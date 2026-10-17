#!/usr/bin/env python3
"""check_observer_artifact.py — worker-observer-packaging variant.

Vendored from worker-startup-checkpoint/diag/check_diag_artifact.py (derived
origin: native-full-runtime-aligned-entry-1/check_artifact.py), adapted for
modbridge_runtime_aligned_observer:

- MODULE is the observer module name; the fself container law, per-phdr
  segment equality, SDK 0x11000000 stamp, explicit DT_INIT/DT_FINI and
  loader-unowned arrays are checked exactly as in the frozen lineage and the
  diag checkpoint probe.
- init/fini slot counts are RECORDED (frozen lineage carried 232/2; the
  observer swaps two probe units, so counts are reported, not hard-pinned;
  >0 required) and compared against build-2 for determinism when present.
- The dynsym import gate is EXACT equality with the frozen 191-row contract
  (import-contract.json, sha256-pinned), verified on decoded NID identities:
  the Packager publishes dynsym names in the on-device long form
  "<sce-nid>#<library>#<module>" (SceNid.Compute: salted SHA-1, 8-byte
  reversal, 11-char base64, '/'->'-'), identical to the console-proven frozen
  lineage PRX, so raw dynstr text is never compared.
- The combined object is checked for the typed import declarations the
  harness requires (every contract import carried as a typed UND reference)
  plus module_start/module_stop FUNC definitions.
Local checks only: no native code is executed.
"""
import base64
import hashlib
import json
import re
import struct as s
import subprocess
import sys
from pathlib import Path

base = Path(__file__).resolve().parent
root = base.parents[2]  # .../what
out = (base / sys.argv[1]).resolve()
MODULE = "modbridge_runtime_aligned_observer"

CONTRACT = (root / ".superpowers/native-dependency-spike/"
                 "native-full-runtime-aligned-entry-1/import-contract.json")
CONTRACT_SHA256 = "58d94bc65997d1a9fac839cf126dc323806214893df7a20d3e435341aceab86c"
assert hashlib.sha256(CONTRACT.read_bytes()).hexdigest() == CONTRACT_SHA256
contract_names = [r["ImportSymbol"]["Name"] for r in
                  json.loads(CONTRACT.read_text())["contract"]["imports"]]
assert len(contract_names) == 191

readelf = Path(root / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/"
                     "LLVM-21.1.8-macOS-ARM64/bin/llvm-readelf")

# ---- SceNid.Compute mirror (verified against encoder ground truth:
# malloc->gQX+4GDQjpM, module_start->BaOKcng8g88)
NID_SUFFIX = bytes([0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1,
                    0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30])

def sce_nid(name: str) -> str:
    digest = hashlib.sha1(name.encode() + NID_SUFFIX).digest()
    return base64.b64encode(digest[:8][::-1]).decode()[:11].replace("/", "-")

NID_LONG_RE = re.compile(r"^(.{11})#(.+)#(.+)$")

# ---- fself container law (frozen check_artifact.py, verbatim structure)
bare = (out / "bare-sdk" / f"{MODULE}.prx").read_bytes()
wrapped = (out / f"{MODULE}.prx").read_bytes()
assert bare[:4] == b"\x7fELF" and wrapped[:4] == bytes.fromhex("4f153d1d")
count = s.unpack_from("<I2HQ2H4x", wrapped, 8)[4]
elf_at = 32 + 32 * count
phoff = s.unpack_from("<Q", bare, 32)[0]
phnum = s.unpack_from("<H", bare, 56)[0]
header_end = max(64, phoff + 56 * phnum)
expected_header = bytearray(bare[:header_end])
# Pinned SDK make_fself.py deliberately removes the embedded section count.
s.pack_into("<H", expected_header, 60, 0)
assert wrapped[elf_at:elf_at + header_end] == expected_header
info_at = (elf_at + header_end + 15) & ~15
assert s.unpack_from("<4Q32s", wrapped, info_at)[4] == hashlib.sha256(bare).digest()
segments = []
for j in range(1, count, 2):
    props, offset, stored, _ = s.unpack_from("<4Q", wrapped, 32 + 32 * j)
    index = (props >> 20) & 65535
    ph = s.unpack_from("<II6Q", bare, phoff + 56 * index)
    assert stored == ph[5]
    assert wrapped[offset:offset + stored] == bare[ph[2]:ph[2] + ph[5]]
    segments.append({"index": index, "type": hex(ph[0]), "bytes": stored})

# ---- section evidence
shoff = s.unpack_from("<Q", bare, 40)[0]
shnum, shstr = s.unpack_from("<HH", bare, 60)
sections = [s.unpack_from("<II4QII2Q", bare, shoff + 64 * i) for i in range(shnum)]
st = sections[shstr]
strings = bare[st[4]:st[4] + st[5]]
named = {strings[x[0]:].split(b"\0")[0].decode(): x for x in sections}
init_slots = named[".init_array"][5] // 8
fini_slots = named[".fini_array"][5] // 8
assert init_slots > 0 and fini_slots > 0, (init_slots, fini_slots)
for name in [".eh_frame", ".eh_frame_hdr", ".gcc_except_table"]:
    assert named[name][5] > 0

# ---- program headers: SDK stamp + explicit DT_INIT/DT_FINI, arrays not loader-owned
phdrs = [s.unpack_from("<II6Q", bare, phoff + 56 * i) for i in range(phnum)]
module = [p for p in phdrs if p[0] == 0x61000002]
assert len(module) == 1
sdk_stamp = s.unpack_from("<I", bare, module[0][2] + 20)[0]
assert sdk_stamp == 0x11000000, hex(sdk_stamp)
dynamic = next(p for p in phdrs if p[0] == 2)
tags = dict(s.unpack_from("<QQ", bare, dynamic[2] + i)
            for i in range(0, dynamic[5], 16))
assert tags[12] != 0 and tags[13] != 0  # explicit DT_INIT / DT_FINI
assert all(tags.get(t, 0) == 0 for t in [25, 26, 27, 28])

# ---- dynsym import surface: EXACT equality with the frozen 191-row contract
# on decoded NID identities.
r = subprocess.run([str(readelf), "-s", "--wide", str(out / "bare-sdk" / f"{MODULE}.prx")],
                   capture_output=True, text=True)
assert r.returncode == 0, r.stderr
imports_dyn = []
bind_hist = {}
for line in r.stdout.splitlines():
    parts = line.split()
    if len(parts) >= 8 and parts[0].rstrip(":").isdigit():
        typ, bind, _vis, ndx, name = parts[3:8]
        if ndx == "UND" and name:
            imports_dyn.append({"raw": name, "bind": bind, "type": typ})
            bind_hist[bind] = bind_hist.get(bind, 0) + 1
assert len(imports_dyn) == 191, f"dynsym UND count {len(imports_dyn)} != 191"
assert bind_hist == {"GLOBAL": 191}, bind_hist
nid_to_name = {}
for n in contract_names:
    nid = sce_nid(n)
    assert nid not in nid_to_name, f"NID collision for {n}"
    nid_to_name[nid] = n
decoded = {}
malformed = []
for row in imports_dyn:
    m = NID_LONG_RE.match(row["raw"])
    if not m:
        malformed.append(row["raw"])
        continue
    head, lib, mod = m.groups()
    assert head in nid_to_name, f"unknown NID import: {row['raw']}"
    decoded.setdefault(nid_to_name[head], row["raw"])
assert not malformed, malformed
assert set(decoded) == set(contract_names), (
    f"extra={sorted(set(decoded) - set(contract_names))[:8]} "
    f"missing={sorted(set(contract_names) - set(decoded))[:8]}")
assert all("#" in raw for raw in decoded.values())

# ---- combined object: typed import declarations + entry exports
combined = Path(json.loads((out / "build-receipt.json").read_text())
                ["combined_object"]["path"])
rc = subprocess.run([str(readelf), "-s", "--wide", str(combined)],
                    capture_output=True, text=True)
assert rc.returncode == 0, rc.stderr
osyms = {}
for line in rc.stdout.splitlines():
    parts = line.split()
    if len(parts) >= 8 and parts[0].rstrip(":").isdigit():
        osyms.setdefault(parts[7], []).append((parts[3], parts[4], parts[6]))
typed = {}
for name in contract_names:
    rows = osyms.get(name, [])
    kinds = {t for t, b, n in rows if n == "UND" and b == "GLOBAL"}
    assert kinds, f"contract import never referenced: {name}"
    assert "FUNC" in kinds or "OBJECT" in kinds, f"typed reference missing: {name} {kinds}"
    typed[name] = sorted(kinds)
# Boundary names are UND in the relocatable combined object by design: the
# DynamicWriter provides them exactly as the frozen lineage harness accepts
# (boundaries set); the exact-equality dynsym gate above already proves none
# of them leaked into the final import table.
for bn in ("__init_array_start", "__init_array_end",
           "__fini_array_start", "__fini_array_end"):
    assert bn not in decoded, f"boundary symbol leaked into import table: {bn}"
assert any(t == "FUNC" and n != "UND" for t, _b, n in osyms["module_start"]), "module_start export"
assert any(t == "FUNC" and n != "UND" for t, _b, n in osyms["module_stop"]), "module_stop export"

# ---- determinism cross-check vs build-2 when available
twin = base / "build-2" / f"{MODULE}.prx"
identical_twin = None
if twin.is_file() and out.name != "build-2":
    identical_twin = hashlib.sha256(twin.read_bytes()).hexdigest() == \
                     hashlib.sha256(wrapped).hexdigest()
    assert identical_twin, "build-2 PRX is NOT byte-identical to build-1"

result = dict(status="LOCAL_PACKAGE_CHECKS_PASSED",
              module=MODULE,
              bytes=len(wrapped),
              sha256=hashlib.sha256(wrapped).hexdigest(),
              bare_sha256=hashlib.sha256(bare).hexdigest(),
              wrapped_segments_equal=segments,
              init_slots=init_slots, fini_slots=fini_slots,
              frozen_lineage_init_slots=232, frozen_lineage_fini_slots=2,
              explicit_start_stop=True, loader_array_ownership=False,
              sdk=hex(sdk_stamp),
              dynsym_import_count=len(imports_dyn),
              dynsym_bind_histogram=bind_hist,
              import_policy="frozen 191-row contract exact equality "
                            "(decoded SceNid long-form identities)",
              contract_sha256=CONTRACT_SHA256,
              combined_object_sha256=hashlib.sha256(combined.read_bytes()).hexdigest(),
              typed_reference_spot_check={k: typed[k] for k in
                                          ("sceKernelOpen", "malloc", "stat", "__cxa_finalize")},
              build2_byte_identical=identical_twin,
              native_executed=False)
(out / "artifact-checks.json").write_text(json.dumps(result, indent=1) + "\n")
print(json.dumps(result))
