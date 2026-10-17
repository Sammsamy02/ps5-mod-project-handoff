#!/usr/bin/env python3
"""Rescan references using the CORRECT pool base 0x9369ec0 (from the lea in
the FName::ToString-style function at 0x243640).

Exact-target scan (rip-rel disp32 families + abs64) for base field offsets,
plus a range scan over [base-0x100, base+0x14000] with cluster report.
"""
import json
import struct
import sys
from collections import Counter

BIN = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
POOL_FILE = 0x9369EC0
DELTA = 0x400000

FIELD_OFFS = [0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x20,
              0x10010, 0x10040, 0x14040]
targets = {POOL_FILE + o: "pool+%#x" % o for o in FIELD_OFFS}

MODRM_RIP = [0x05, 0x0D, 0x15, 0x1D, 0x25, 0x2D, 0x35, 0x3D]
REXES = [b"\x48", b"\x4c", b"\x49", b"\x4d"]
OPS1 = [0x8D, 0x8B, 0x89, 0x03, 0x2B, 0x39, 0x3B, 0x85, 0xC7, 0x63]

with open(BIN, "rb") as f:
    data = f.read()
n = len(data)

patterns = []
for rex in REXES:
    for op in OPS1:
        for m in MODRM_RIP:
            patterns.append((rex + bytes([op, m]), 3))
for pre in (b"\x66", b"\xf3", b"\xf2", b""):
    for op in (0x10, 0x11, 0x28, 0x29, 0x6F, 0x7F, 0xD6, 0x7E):
        for m in MODRM_RIP:
            patterns.append((pre + b"\x0f" + bytes([op, m]), len(pre) + 3))
for op in (0x81, 0x83, 0x8B, 0x89, 0xC7, 0x8D):
    for m in MODRM_RIP:
        patterns.append((bytes([op, m]), 2))
patterns.append((b"\xff\x15", 2))
patterns.append((b"\xff\x25", 2))

exact = []
rng = []
range_lo, range_hi = POOL_FILE - 0x100, POOL_FILE + 0x14000
for prefix, plen in patterns:
    start = 0
    while True:
        s = data.find(prefix, start)
        if s < 0:
            break
        start = s + 1
        dpos = s + plen
        if dpos + 4 > n:
            break
        disp = struct.unpack_from("<i", data, dpos)[0]
        tgt = dpos + 4 + disp
        rec = targets.get(tgt)
        if rec is not None:
            exact.append({"target": hex(tgt), "field": rec,
                          "site_runtime": hex(s + DELTA), "site_file": hex(s),
                          "bytes": data[s:dpos + 4].hex()})
        elif range_lo <= tgt < range_hi:
            rng.append({"tgt_file": hex(tgt), "off": tgt - POOL_FILE,
                        "site_runtime": hex(s + DELTA), "bytes": data[s:dpos + 4].hex()})

print("=== exact field refs ===")
for h in sorted(exact, key=lambda x: (x["field"], x["site_runtime"])):
    print(json.dumps(h))
print("=== range refs (offset clusters) ===")
c = Counter(h["off"] for h in rng)
for off, cnt in sorted(c.items()):
    print(f"  +{off:#x}: {cnt}")
print(f"exact={len(exact)} range={len(rng)}")

with open("scan_correct_base.json", "w") as f:
    json.dump({"pool": hex(POOL_FILE), "exact": exact, "range": rng}, f, indent=1)

# abs64 of runtime address anywhere
for tv in (POOL_FILE, POOL_FILE + DELTA):
    pat = struct.pack("<Q", tv)
    p = data.find(pat)
    while p >= 0:
        print("ABS64", hex(tv), "at file", hex(p), data[p - 8:p + 8].hex())
        p = data.find(pat, p + 1)
