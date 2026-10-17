#!/usr/bin/env python3
"""Read-only annotated disassembly of the pinned eboot. Usage: dis.py <vaddr> [len] | dis.py --func <vaddr-inside> [len]"""
import sys
sys.path.insert(0, "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/phasea-tools")
import pak_vtable_pins as P
IMG = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
P.load_image(IMG, True)
args = sys.argv[1:]
func = False
if args and args[0] == "--func":
    func = True; args = args[1:]
va = int(args[0], 0); n = int(args[1], 0) if len(args) > 1 else 0x100
if func:
    o = P.v2o(va)
    # walk back to a function start: 'push rbp; mov rbp,rsp' preceded by int3/ret padding
    s = o
    while s > o - 0x4000:
        if P.BLOB[s:s+4] == b"\x55\x48\x89\xe5" and P.BLOB[s-1] in (0xcc, 0xc3):
            break
        s -= 1
    start = va - (o - s)
    print(f"; function start {start:#x} (contains {va:#x})")
    va = start
P.disasm_window(va, n)
