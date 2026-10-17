#!/usr/bin/env python3
"""Find all e8 rel32 call sites in LOAD0 targeting a given set of functions."""
import struct
import sys

BIN = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
DELTA = 0x400000
LOAD0 = (0x0, 0x7479B1C)

targets = set(int(a, 16) for a in sys.argv[1:])
with open(BIN, "rb") as f:
    f.seek(LOAD0[0] + 0x4000)
    blob = f.read(LOAD0[1])

callers = {hex(t + DELTA): [] for t in targets}
n = len(blob)
pos = 0
while True:
    p = blob.find(b"\xe8", pos)
    if p < 0 or p + 5 > n:
        break
    pos = p + 1
    rel = struct.unpack_from("<i", blob, p + 1)[0]
    site = p
    tgt = site + 5 + rel
    if tgt in targets:
        callers[hex(tgt + DELTA)].append(hex(site + DELTA))

for k in sorted(callers):
    print(k, "callers:", len(callers[k]), callers[k][:40])
