#!/usr/bin/env python3
"""TIER1 step 2 (pure python, no numpy): rip-relative + absolute reference
scan for the FName pool struct.

Method: rip-relative refs to a global in x86-64 System V code are built from
REX + opcode + ModRM(mod=00, rm=101) + disp32. For each candidate pattern
(opcode prefix with matching ModRM byte) found at position s, the
displacement occupies [s+len(prefix), s+len(prefix)+4) and RIP (end of
instruction) == s+len(prefix)+4, so target = s+len(prefix)+4+int32(disp).
bytes.find runs at C speed; expected random false hits per target ~0.

Coverage: lea r64,[rip+d] (48/4c/49/4d 8d), mov r64,[rip+d] (8b family),
mov [rip+d],r64 (89 family), 32-bit mov variants, mov [rip+d],imm
(c7/48 c7/49 c7 05), call/jmp [rip+d] (ff 15/25), cmp [rip+d] (83/39/3b
families), test/and/or/xor/add/sub [rip+d] immediates and reg forms with
common reg fields. Uncovered exotic encodings are documented as a limit.
"""
import json
import struct
import sys

import lief

BIN = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"

POOL_FILE = 0x9365EC0
RUNTIME_DELTA = 0x400000
FIELD_OFFS = [0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x20, 0x14040, 0x14044]
TARGETS = {POOL_FILE + o: "pool+%#x" % o for o in FIELD_OFFS}

with open(BIN, "rb") as f:
    data = f.read()
n = len(data)
print(f"file bytes: {n:#x}", file=sys.stderr)

MODRM_RIP = [0x05, 0x0D, 0x15, 0x1D, 0x25, 0x2D, 0x35, 0x3D]

patterns = []  # (prefix_bytes, description)
for rex in (b"\x48", b"\x4c", b"\x49", b"\x4d"):
    for m in MODRM_RIP:
        patterns.append((rex + b"\x8d" + bytes([m]), "lea r64,[rip+d]"))
        patterns.append((rex + b"\x8b" + bytes([m]), "mov r64,[rip+d]"))
        patterns.append((rex + b"\x89" + bytes([m]), "mov [rip+d],r64"))
        patterns.append((rex + b"\x03" + bytes([m]), "add r64,[rip+d]"))
        patterns.append((rex + b"\x2b" + bytes([m]), "sub r64,[rip+d]"))
        patterns.append((rex + b"\x39" + bytes([m]), "cmp [rip+d],r64"))
        patterns.append((rex + b"\x3b" + bytes([m]), "cmp r64,[rip+d]"))
        patterns.append((rex + b"\x85" + bytes([m]), "test [rip+d],r64"))
        patterns.append((rex + b"\xc7" + bytes([m]), "mov [rip+d],imm32"))
for m in MODRM_RIP:
    patterns.append((b"\x8b" + bytes([m]), "mov r32,[rip+d]"))
    patterns.append((b"\x89" + bytes([m]), "mov [rip+d],r32"))
    patterns.append((b"\xc7" + bytes([m]), "mov [rip+d],imm32"))
    patterns.append((b"\x83" + bytes([m]), "cmp [rip+d],imm8"))
    patterns.append((b"\xff" + bytes([m]), "INC/DEC/CALL/JMP [rip+d]"))
patterns.append((b"\xff\x15", "call [rip+d]"))
patterns.append((b"\xff\x25", "jmp [rip+d]"))
patterns.append((b"\xa1", "mov eax,moffs32") )
patterns.append((b"\xa3", "mov moffs32,eax"))
# movabs-style 64-bit immediate materializations checked separately below.

hits = []
seen = set()
for prefix, desc in patterns:
    start = 0
    while True:
        s = data.find(prefix, start)
        if s < 0:
            break
        start = s + 1
        dpos = s + len(prefix)
        if dpos + 4 > n:
            break
        disp = struct.unpack_from("<i", data, dpos)[0]
        rip = dpos + 4
        tgt = rip + disp
        rec = TARGETS.get(tgt)
        if rec is not None and (dpos, rec) not in seen:
            seen.add((dpos, rec))
            hits.append({"target": hex(tgt), "field": rec, "kind": desc,
                         "site_file": hex(s), "site_runtime": hex(s + RUNTIME_DELTA),
                         "ctx_bytes": data[max(0, s - 4): dpos + 4].hex()})

# absolute 64-bit occurrences anywhere in file (data slots / movabs imm64)
for tv, kindname in ((POOL_FILE, "abs64 file-vaddr"),
                     (POOL_FILE + RUNTIME_DELTA, "abs64 runtime")):
    pat = struct.pack("<Q", tv)
    start = 0
    while True:
        p = data.find(pat, start)
        if p < 0:
            break
        start = p + 1
        hits.append({"target": hex(tv), "field": kindname, "kind": "abs64 in file",
                     "site_file": hex(p), "site_runtime": hex(p + RUNTIME_DELTA),
                     "ctx_bytes": data[max(0, p - 8): p + 8].hex()})

print(f"code/data hits: {len(hits)}", file=sys.stderr)

# relocations with addend or offset near pool fields (PS5 RELA: addends are
# file-space vaddrs before load-base relocation)
lief.logging.disable()
elf = lief.ELF.parse(BIN)
rel_hits = []
lo_t = POOL_FILE - 8
hi_t = POOL_FILE + 0x14044 + 8
count = 0
for r in elf.relocations:
    count += 1
    a = r.addend
    off = getattr(r, "address", None)
    if off is None:
        continue
    if a is not None and lo_t <= a <= hi_t:
        rel_hits.append({"r_offset": hex(off), "addend": hex(a),
                         "type": int(r.type), "note": "addend near pool field"})
    if lo_t <= off <= hi_t:
        rel_hits.append({"r_offset": hex(off),
                         "addend": hex(a) if a is not None else None,
                         "type": int(r.type), "note": "reloc targets pool slot"})
print(f"relocations scanned: {count}, near-pool: {len(rel_hits)}", file=sys.stderr)

out = {"pool_file_vaddr": hex(POOL_FILE), "runtime": hex(POOL_FILE + RUNTIME_DELTA),
       "hits": sorted(hits, key=lambda h: (h["target"], h.get("site_file", ""))),
       "relocation_hits": rel_hits}
with open("scan_refs.json", "w") as f:
    json.dump(out, f, indent=1)
for h in out["hits"]:
    print(json.dumps(h))
print("--- reloc ---")
for r in rel_hits[:60]:
    print(json.dumps(r))
