#!/usr/bin/env python3
"""Find relocations (function-pointer slots) and e8/ff15 call sites for
given file-vaddr targets. Usage: python3 ptr_refs.py 247310 247b00 ..."""
import struct
import sys

import lief

BIN = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
DELTA = 0x400000
LOAD0 = (0x0, 0x7479B1C)

targets = set(int(a, 16) for a in sys.argv[1:])

lief.logging.disable()
elf = lief.ELF.parse(BIN)
reloc_hits = []
for r in elf.relocations:
    a = r.addend
    if a is not None and a in targets:
        reloc_hits.append({"slot_vaddr": hex(int(r.address)),
                           "slot_runtime": hex(int(r.address) + DELTA),
                           "addend": hex(a), "type": int(r.type)})
for h in reloc_hits:
    print("RELOC", h)

# ff 15 / ff 25 indirect call sites resolve through GOT; check GOT addends
with open(BIN, "rb") as f:
    f.seek(LOAD0[0] + 0x4000)
    blob = f.read(LOAD0[1])

# slot vaddr -> file offset for data segments: build from phdrs
segs = []
for s in elf.segments:
    if str(s.type).endswith("LOAD"):
        segs.append((int(s.virtual_address), int(s.file_offset),
                     int(s.virtual_address) + int(s.physical_size)))


def v2f(v):
    for va, off, end in segs:
        if va <= v < end:
            return v - va + off
    return None


addend2targets = {int(r.address): r.addend for r in elf.relocations
                  if r.addend is not None and r.addend in targets}
n = len(blob)
pos = 0
while True:
    p = blob.find(b"\xff\x15", pos)
    if p < 0 or p + 6 > n:
        break
    pos = p + 1
    disp = struct.unpack_from("<i", blob, p + 2)[0]
    slot = p + 6 + disp
    if slot in addend2targets:
        print("FF15", hex(p + DELTA), "->slot", hex(slot),
               "target", hex(addend2targets[slot]))
