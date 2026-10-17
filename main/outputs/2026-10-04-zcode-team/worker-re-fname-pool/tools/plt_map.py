#!/usr/bin/env python3
"""TIER1: build PLT stub -> import symbol map.

PS5 eboot: PLT stubs are `ff 25 disp32` (jmp [rip+got]) near the end of
LOAD0; the GOT slot is filled by R_X86_64_JUMP_SLOT (type 7) / GLOB_DAT
(type 6) relocations carrying the import symbol. We enumerate stubs by
scanning the last 2MB of LOAD0, then map got slot -> symbol via lief.
"""
import json
import struct

import lief
import capstone as cs

BIN = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
DELTA = 0x400000
LOAD0_FILE_END = 0x7479B1C

lief.logging.disable()
elf = lief.ELF.parse(BIN)

got2sym = {}
for r in elf.relocations:
    sym = r.symbol
    if sym is not None and sym.name:
        got2sym[int(r.address)] = sym.name

print(f"gotted relocs with symbols: {len(got2sym)}", file=__import__('sys').stderr)

LOAD0_VADDR_BASE = 0x0
LOAD0_OFF_BASE = 0x4000  # phdr: offset 0x4000 <-> vaddr 0
WIN_LO = LOAD0_FILE_END - 0x200000  # file vaddr window start
with open(BIN, "rb") as f:
    f.seek(WIN_LO - LOAD0_VADDR_BASE + LOAD0_OFF_BASE)
    blob = f.read(0x200000)
base = WIN_LO  # file vaddr of blob start

md = cs.Cs(cs.CS_ARCH_X86, cs.CS_MODE_64)
stub2sym = {}
i = 0
while True:
    j = blob.find(b"\xff\x25", i)
    if j < 0:
        break
    i = j + 1
    if j + 6 > len(blob):
        continue
    disp = struct.unpack_from("<i", blob, j + 2)[0]
    got = base + j + 6 + disp
    if got in got2sym:
        stub = base + j
        stub2sym[hex(stub + DELTA)] = got2sym[got]

out = {"stub_count": len(stub2sym), "stubs": stub2sym}
with open("plt_map.json", "w") as f:
    json.dump(out, f, indent=1)
print(f"stubs resolved: {len(stub2sym)}")
for k in list(stub2sym)[:20]:
    print(k, stub2sym[k])
