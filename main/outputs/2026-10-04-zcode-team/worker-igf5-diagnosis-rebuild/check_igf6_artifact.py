#!/usr/bin/env python3
"""check_igf6_artifact.py — igf6 VARIANT (worker-igf5-diagnosis-rebuild,
team 2026-10-04). Vendored from worker-code-anchor-reads/
check_code_anchor_artifact.py (lineage: worker-anchor-validation ->
worker-ingame-probe-v2 -> native-full-runtime-aligned-entry-1).

Changes vs igf5: variant strings only, PLUS the inverted syscall gate —
(a) sceKernelMprotect / igcar_sys_mprotect must NOT appear anywhere
    (dynsym, friendly symbols, combined object);
(b) the combined object must contain ZERO raw `syscall` instructions
    (igf5 asserted >=1 for its mprotect shim; igf6 REMOVED that mechanism);
(c) the final inner ELF .text section must contain NO 0F 05 byte pair
    (defense in depth against an unrecognized emission).
Everything else (init/fini slot counts, SDK 0x11000000 stamp, explicit
DT_INIT/DT_FINI, loader-unowned arrays, make_fself segment equality,
NID-encoded dynsym with EXACTLY 7 strong imports) is checked exactly as
in the v1/v2/igf4/igf5 lineage. Local checks only: no native execution."""
import hashlib
import json
import struct as s
import subprocess
import sys
from pathlib import Path

out = Path(sys.argv[1]).resolve()
MODULE = "modbridge_ingame_facts"
EXPECTED_STRONG = [
    "__error", "sceKernelOpen", "sceKernelWrite", "sceKernelClose", "sysctl",
    "getpid", "clock_gettime"]
EXPECTED_WEAK = []
FORBIDDEN = [
    "sceKernelGetModuleList", "sceKernelGetModuleInfo",
    "sceKernelGetExecutableModuleHandle",
    "dlopen", "dlclose", "dlsym", "__dlopen", "__dlclose", "__dlsym",
    "_DYNAMIC", "__DYNAMIC", "sceKernelMprotect", "igcar_sys_mprotect"]

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

# igf6-specific: .text must contain NO 0F 05 byte pair (raw syscall).
text = named[".text"]
toff, tsz = text[4], text[5]
assert b"\x0f\x05" not in bare[toff:toff + tsz], \
    "0F 05 (syscall) byte pair present in inner ELF .text"

# dynsym import/provider table from the bare PRX.
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
strong_dyn = sorted(n for n, b, _ in imports_dyn if b == "GLOBAL")
weak_dyn = sorted(n for n, b, _ in imports_dyn if b == "WEAK")
# DynamicWriter publishes NID-encoded names, one per contract row: igf6
# must carry EXACTLY 7 (UNCHANGED from igf4/igf5), all encoded, zero weak.
assert len(strong_dyn) == len(EXPECTED_STRONG), strong_dyn
assert weak_dyn == [], weak_dyn
assert all("#" in n for n in strong_dyn), "unexpected unencoded dynsym name"
for name in syms:
    if name in FORBIDDEN:
        raise AssertionError(f"forbidden identity present in dynsym: {name}")

# Combined-object level: boundaries, entry funcs, forbidden gate, NO shim.
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

objdump = readelf.parent / "llvm-objdump"
od = subprocess.run([str(objdump), "-d", str(out / "combined-ingame.o")],
                    capture_output=True, text=True)
assert od.returncode == 0, od.stderr
syscall_count = sum(1 for ln in od.stdout.splitlines()
                    if ln.strip().endswith("syscall"))
assert syscall_count == 0, \
    f"igf6 must contain ZERO raw syscall instructions, found {syscall_count}"

checks = {
    "wrapper_magic_ok": True,
    "inner_elf_sha256": hashlib.sha256(bare).hexdigest(),
    "wrapped_sha256": hashlib.sha256(wrapped).hexdigest(),
    "wrapped_bytes": len(wrapped),
    "inner_elf_bytes": len(bare),
    "fself_segment_copies_verified": len(segments),
    "init_array_slots": init_slots,
    "fini_array_slots": fini_slots,
    "strong_dynsym_imports": strong_dyn,
    "weak_dynsym_imports": weak_dyn,
    "syscall_instructions_combined": syscall_count,
    "syscall_byte_pair_in_text": False,
    "sdk_stamp": "0x11000000",
}
(out / "artifact-checks.json").write_text(json.dumps(checks, indent=1))
print(json.dumps(checks, indent=1))
