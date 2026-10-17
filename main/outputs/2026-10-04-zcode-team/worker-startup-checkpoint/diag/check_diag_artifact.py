#!/usr/bin/env python3
"""check_diag_artifact.py — STARTUP CHECKPOINT variant (worker-startup-
checkpoint, team 2026-10-04). Vendored from worker-v3-startup-diag/
check_ingame_artifact.py (derived-origin: native-full-runtime-aligned-entry-1/
check_artifact.py).

Changes vs igd1: MODULE follows the composed runtime's module name
(modbridge_supplier_composition); the dynsym import gate is an EXACT
equality against expected-imports.json (name/bind/type per row), whose
oracle is the variant's ground-truth bare PRX (igd1 run1 for sdg1-min,
composition-4 bare-sdk for sdg2-imp). The forbidden-identity gate applies
ONLY to the min variant (the imp variant intentionally carries the full
composition-4 surface). Everything else (fself container law, per-phdr
segment equality, init/fini slot counts, SDK 0x11000000 stamp, explicit
DT_INIT/DT_FINI, loader-unowned arrays) is checked exactly as in v1/v2/igd1.
Local checks only: no native code is executed."""
import hashlib
import json
import struct as s
import subprocess
import sys
from pathlib import Path

out = Path(sys.argv[1]).resolve()
MODULE = "modbridge_supplier_composition"

profile = json.loads((out / "profile.json").read_text())
variant = profile["variant"]
minimal = bool(profile.get("minimal_variant", False))
expected = json.loads((out / "expected-imports.json").read_text())
EXPECTED_ROWS = {(r["name"], r["bind"], r["type"]) for r in expected["und"]}

FORBIDDEN = [
    "sceKernelGetModuleList", "sceKernelGetModuleInfo",
    "sceKernelGetExecutableModuleHandle",
    "dlopen", "dlclose", "dlsym", "__dlopen", "__dlclose", "__dlsym",
    "_DYNAMIC", "__DYNAMIC"]

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

# Section-level init/fini evidence.
shoff = s.unpack_from("<Q", bare, 40)[0]
shnum, shstr = s.unpack_from("<HH", bare, 60)
sections = [s.unpack_from("<II4QII2Q", bare, shoff + 64 * i) for i in range(shnum)]
st = sections[shstr]
strings = bare[st[4]:st[4] + st[5]]
named = {strings[x[0]:].split(b"\0")[0].decode(): x for x in sections}
init_slots = named[".init_array"][5] // 8
fini_slots = named[".fini_array"][5] // 8
assert init_slots == 1, init_slots
assert fini_slots == 1, fini_slots

# dynsym import table from the bare PRX: EXACT equality with the oracle.
readelf = Path("/WORKSPACE/.codex/worktrees/translation-core/what/outputs/"
               "ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-readelf")
r = subprocess.run([str(readelf), "-s", "--wide", str(out / "bare-sdk" / f"{MODULE}.prx")],
                   capture_output=True, text=True)
assert r.returncode == 0, r.stderr
syms = {}
imports_dyn = []
for line in r.stdout.splitlines():
    parts = line.split()
    if len(parts) >= 8 and parts[0].rstrip(":").isdigit():
        _, value, size, typ, bind, vis, ndx, name = parts[:8]
        syms[name] = (typ, bind, ndx, value)
        if ndx == "UND" and name:
            imports_dyn.append((name, bind, typ))
got_rows = set(imports_dyn)
assert got_rows == EXPECTED_ROWS, (
    f"dynsym surface mismatch: extra={sorted(got_rows - EXPECTED_ROWS)[:8]} "
    f"missing={sorted(EXPECTED_ROWS - got_rows)[:8]} "
    f"got={len(got_rows)} want={len(EXPECTED_ROWS)}")
assert all("#" in n for n, _b, _t in imports_dyn), "unencoded dynsym name"
if minimal:
    for name in syms:
        if name in FORBIDDEN:
            raise AssertionError(f"forbidden identity present in dynsym: {name}")

# Boundaries live in the combined object (DynamicWriter consumes them; the
# final PRX carries only dynsym): verify there, on the SAME object that was
# packaged (hash-pinned by the receipt).
rc = subprocess.run([str(readelf), "-s", "--wide", str(out / "combined-diag.o")],
                    capture_output=True, text=True)
assert rc.returncode == 0, rc.stderr
osyms = {}
for line in rc.stdout.splitlines():
    parts = line.split()
    if len(parts) >= 8 and parts[0].rstrip(":").isdigit():
        osyms[parts[7]] = (parts[3], parts[4], parts[6], parts[1])
for bn in ("__init_array_start", "__init_array_end", "__fini_array_start", "__fini_array_end"):
    assert bn in osyms and osyms[bn][2] != "UND", bn
assert "module_start" in osyms and osyms["module_start"][0] == "FUNC"
assert "module_stop" in osyms and osyms["module_stop"][0] == "FUNC"
if minimal:
    for name in osyms:
        if name in FORBIDDEN:
            raise AssertionError(f"forbidden symbol in combined object: {name}")

# Program headers: SDK stamp + explicit DT_INIT/DT_FINI, arrays not loader-owned.
phdrs = [s.unpack_from("<II6Q", bare, phoff + 56 * i) for i in range(phnum)]
module = [p for p in phdrs if p[0] == 0x61000002]
assert len(module) == 1
assert s.unpack_from("<I", bare, module[0][2] + 20)[0] == 0x11000000
dynamic = next(p for p in phdrs if p[0] == 2)
tags = dict(s.unpack_from("<QQ", bare, dynamic[2] + i)
            for i in range(0, dynamic[5], 16))
assert tags[12] != 0 and tags[13] != 0  # explicit DT_INIT / DT_FINI
assert all(tags.get(t, 0) == 0 for t in [25, 26, 27, 28])

result = dict(status="LOCAL_PACKAGE_CHECKS_PASSED",
              variant=variant,
              bytes=len(wrapped),
              sha256=hashlib.sha256(wrapped).hexdigest(),
              bare_sha256=hashlib.sha256(bare).hexdigest(),
              wrapped_segments_equal=segments, init_slots=init_slots,
              fini_slots=fini_slots, explicit_start_stop=True,
              loader_array_ownership=False, sdk="0x11000000",
              import_policy=("composition-4 exact surface (oracle equality)"
                             if not minimal else
                             "7-verbatim-console-proven-only (oracle equality)"),
              oracle=expected["oracle"],
              dynsym_import_count=len(imports_dyn),
              forbidden_symbol_gate=("passed" if minimal else "not-applicable (full surface by design)"),
              native_executed=False)
(out / "artifact-checks.json").write_text(json.dumps(result, indent=1) + "\n")
print(json.dumps(result))
