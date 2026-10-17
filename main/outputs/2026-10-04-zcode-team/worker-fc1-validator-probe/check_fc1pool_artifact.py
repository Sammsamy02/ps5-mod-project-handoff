#!/usr/bin/env python3
"""check_fc1pool_artifact.py — worker-fc1-validator-probe variant (fc1pool1,
team 2026-10-04). Vendored from worker-record-sink-addon/check_qdrv_artifact.py
(derived-origin: worker-startup-checkpoint/diag/check_diag_artifact.py, itself
derived from native-full-runtime-aligned-entry-1/check_artifact.py).

Changes vs the qdrv1 variant: MODULE is this probe's own module name
(modbridge_fc1pool); the dynsym import gate is an EXACT name-equality against
expected-imports.json after stripping the loader-side NID encoding (each
decoded prefix set must equal the 8 verbatim frozen-contract rows exactly —
no extra, no missing). The forbidden-identity gate applies to the dynsym
surface AND the combined object. init/fini slot law (1/1 via the boundaries.S
sentinels), fself container law, per-phdr segment equality, SDK 0x11000000
stamp, explicit DT_INIT/DT_FINI, loader-unowned arrays: checked exactly as in
the lineage. Local checks only: no native code is executed."""
import hashlib
import json
import struct as s
import subprocess
import sys
from pathlib import Path

out = Path(sys.argv[1]).resolve()
MODULE = "modbridge_fc1pool"

profile = json.loads((out / "profile.json").read_text())
expected = json.loads((out / "expected-imports.json").read_text())
EXPECTED_NAMES = set(expected["names"])


def sce_nid(name: str) -> str:
    """The pinned SharpProspero SceNid algorithm (source/tools/SharpProspero.Prx/
    SceNid.cs, vendored spike): 11-char id = base64url of the REVERSED first
    8 bytes of SHA1(ASCII(name) + fixed 16-byte suffix), '/' -> '-'. The
    oracle is therefore INDEPENDENT of the artifact under test."""
    import base64
    import hashlib
    suffix = bytes([0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1,
                    0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30])
    digest = hashlib.sha1(name.encode("ascii") + suffix).digest()
    value = digest[:8][::-1]
    return base64.b64encode(value).decode()[:11].replace("/", "-")


EXPECTED_NIDS = {sce_nid(n): n for n in EXPECTED_NAMES}

FORBIDDEN = [
    "sceKernelGetModuleList", "sceKernelGetModuleInfo",
    "sceKernelGetExecutableModuleHandle",
    "dlopen", "dlclose", "dlsym", "__dlopen", "__dlclose", "__dlsym",
    "_DYNAMIC", "__DYNAMIC", "sceKernelMprotect", "scePthreadMprotect",
    "igcar_sys_mprotect"]

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

# Section-level init/fini evidence (boundaries.S sentinels).
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

# dynsym import table from the bare PRX: EXACT equality (decoded names).
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
got_nids = {}
for name, _b, _t in imports_dyn:
    if "#" in name:
        fields = name.split("#")
        nid = fields[0]
        assert len(nid) == 11 and len(fields) in (2, 3), \
            f"unexpected dynsym name spelling: {name}"
        got_nids[nid] = EXPECTED_NIDS.get(nid, "?")
        continue
    assert len(name) == 11, f"unexpected dynsym name spelling: {name}"
    got_nids[name] = EXPECTED_NIDS.get(name, "?")
assert set(got_nids) == set(EXPECTED_NIDS), (
    f"dynsym surface mismatch: extra={sorted(set(got_nids) - set(EXPECTED_NIDS))[:8]} "
    f"missing={sorted(set(EXPECTED_NIDS) - set(got_nids))[:8]} "
    f"got={len(got_nids)} want={len(EXPECTED_NIDS)}")
decoded_names = set(got_nids.values())
assert "?" not in decoded_names, "dynsym contains an unknown NID"
assert decoded_names == EXPECTED_NAMES, "decoded NID set != expected import set"
for nid, plain in got_nids.items():
    if plain in FORBIDDEN:
        raise AssertionError(f"forbidden identity present in dynsym: {plain}")

# Module-name patch evidence: the bare SDK ELF carries fc1pool strings and NO
# residual Packager-identity string.
n_new = bare.count(b"modbridge_fc1pool")
n_old = bare.count(b"modbridge_ingame_facts")
assert n_new >= 1, f"module-name patch missing (found {n_new} 'modbridge_fc1pool')"
assert n_old == 0, f"residual Packager identity string: {n_old}"

# Boundaries live in the combined object (DynamicWriter consumes them; the
# final PRX carries only dynsym): verify there, on the SAME object that was
# packaged (hash-pinned by the receipt).
rc = subprocess.run([str(readelf), "-s", "--wide", str(out / "combined-ingame.o")],
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
              variant=profile.get("variant"),
              bytes=len(wrapped),
              sha256=hashlib.sha256(wrapped).hexdigest(),
              bare_sha256=hashlib.sha256(bare).hexdigest(),
              wrapped_segments_equal=segments, init_slots=init_slots,
              fini_slots=fini_slots, explicit_start_stop=True,
              loader_array_ownership=False, sdk="0x11000000",
              module_name_patch={"modbridge_fc1pool": n_new,
                                 "residual_modbridge_ingame_facts": n_old},
              import_policy=("8 verbatim frozen-contract rows (exact decoded-"
                             "name equality; igf 7 + nanosleep)"),
              forbidden_gate="dynsym + combined object")
(out / "artifact-checks.json").write_text(json.dumps(result, indent=1) + "\n")
print(json.dumps({k: result[k] for k in ("status", "bytes", "sha256",
                                         "import_policy")}))
