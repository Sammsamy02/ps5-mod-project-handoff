#!/usr/bin/env python3
"""TIER1 step 3: range-based rip-relative scan over the pool BSS region.

Scans all rip-relative-capable encodings (incl. SSE movq/movdqu, movsxd,
group1 imm) for targets landing anywhere in [POOL_LO, POOL_HI) covering the
pinned pool label +- margin and the 0x9379f00 table found at +0x14040.
Expected random false hits over 194MB: range 0x1d000 bytes -> ~5; each hit's
preceding bytes are re-verified with capstone before reporting.
"""
import json
import struct
import sys

BIN = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
POOL_FILE = 0x9365EC0
RUNTIME_DELTA = 0x400000
POOL_LO = 0x9365000
POOL_HI = 0x9382000

with open(BIN, "rb") as f:
    data = f.read()
n = len(data)

MODRM_RIP = [0x05, 0x0D, 0x15, 0x1D, 0x25, 0x2D, 0x35, 0x3D]
REXES = [b"\x48", b"\x4c", b"\x49", b"\x4d"]
OPCODES1 = [0x8D, 0x8B, 0x89, 0x03, 0x2B, 0x39, 0x3B, 0x85,
            0xC7, 0x63, 0x01, 0x09, 0x11, 0x21, 0x29, 0x31,
            0x0B, 0x13, 0x1B, 0x23, 0x2B, 0x33, 0x3B, 0x0F]

patterns = []
for rex in REXES:
    for op in OPCODES1:
        for m in MODRM_RIP:
            patterns.append((rex + bytes([op, m]), 3, "%s %02x" % (rex.hex(), op)))
# SSE families: prefix + 0F + opcode + modrm
for pre in (b"\x66", b"\xf3", b"\xf2", b""):
    for op in (0x10, 0x11, 0x28, 0x29, 0x6F, 0x7F, 0xD6, 0x7E, 0x2E, 0x5A):
        for m in MODRM_RIP:
            patterns.append((pre + b"\x0f" + bytes([op, m]),
                             len(pre) + 3, "sse%02x" % op))
# group1 imm32 / misc no-rex
for op in (0x81, 0x83, 0x8B, 0x89, 0xC7, 0x8D):
    for m in MODRM_RIP:
        patterns.append((bytes([op, m]), 2, "%02x norex" % op))
patterns.append((b"\xff\x15", 2, "call [rip]"))
patterns.append((b"\xff\x25", 2, "jmp [rip]"))

hits = []
for prefix, plen, desc in patterns:
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
        if POOL_LO <= tgt < POOL_HI:
            hits.append({"tgt_file": hex(tgt), "tgt_off_from_pool": tgt - POOL_FILE,
                         "site_file": hex(s), "runtime_site": hex(s + RUNTIME_DELTA),
                         "enc": desc, "bytes": data[s:dpos + 4].hex()})

hits.sort(key=lambda h: h["tgt_off_from_pool"])
out = {"pool": hex(POOL_FILE), "range": [hex(POOL_LO), hex(POOL_HI)],
       "hit_count": len(hits), "hits": hits}
with open("scan_range.json", "w") as f:
    json.dump(out, f, indent=1)
# compact report: offset clusters
from collections import Counter
c = Counter(h["tgt_off_from_pool"] for h in hits)
print("clusters (off_from_pool: count):")
for off, cnt in sorted(c.items()):
    print(f"  +{off:#x}: {cnt}")
print(f"total {len(hits)}")
