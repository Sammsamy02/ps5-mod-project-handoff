#!/usr/bin/env python3
"""Read-only fast xref: rip-relative LEA/MOV (REX.W 8d/8b modrm rip) to target vaddrs in the pinned eboot text."""
import sys, re, struct
sys.path.insert(0, "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/phasea-tools")
import pak_vtable_pins as P
IMG = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
P.load_image(IMG, True)
targets = {int(a, 0) for a in sys.argv[1:]}
text = [p for p in P.PHDRS if p["type"] == 1 and (p["flags"] & 1)][0]
code = P.BLOB[text["off"]:text["off"] + text["filesz"]]
rx = re.compile(rb'[\x48\x4c][\x8d\x8b][\x05\x0d\x15\x1d\x25\x2d\x35\x3d]', re.S)
for m in rx.finditer(code):
    i = m.start()
    disp = struct.unpack_from("<i", code, i + 3)[0]
    va = text["vaddr"] + i
    t = va + 7 + disp
    if t in targets:
        print(f"{t:#x} <- {va:#x} {'lea' if code[i+1]==0x8d else 'mov'}")
