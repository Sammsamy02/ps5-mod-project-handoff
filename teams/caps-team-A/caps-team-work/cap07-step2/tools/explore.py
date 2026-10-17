#!/usr/bin/env python3
"""explore.py - ad-hoc exploration helpers (string -> referencing function).

Usage: python3 -I explore.py IMAGE utf16 "text"
       python3 -I explore.py IMAGE ascii  "text"
       python3 -I explore.py IMAGE dis 0xVADDR 0xLEN
       python3 -I explore.py IMAGE refs 0xTARGET
       python3 -I explore.py IMAGE ptr 0xVADDR [count]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from elfimg import Image  # noqa: E402

import capstone  # noqa: E402


def func_start(img, va, back=0x4000):
    """Heuristic: nearest preceding 'push rbp; mov rbp,rsp' (55 48 89 e5)."""
    lo = max(0, va - back)
    blob = img.read(lo, va - lo + 1)
    i = blob.rfind(b"\x55\x48\x89\xe5")
    return None if i < 0 else lo + i


def dis(img, va, n, out=print):
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    code = img.read(va, n)
    for ins in md.disasm(code, va):
        note = ""
        if "rip" in ins.op_str:
            for op in ins.operands:
                if op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP:
                    t = ins.address + ins.size + op.mem.disp
                    note = f"  ; -> {t:#x}"
        out(f"{ins.address:#x}: {ins.mnemonic:8s} {ins.op_str}{note}")


def main():
    img = Image(sys.argv[1])
    cmd = sys.argv[2]
    if cmd in ("utf16", "ascii"):
        text = sys.argv[3].encode().decode("unicode_escape")
        needle = text.encode("utf-16-le" if cmd == "utf16" else "ascii")
        for s in img.find_all(needle):
            refs = img.rip_refs(s)
            print(f"string @ {s:#x}: refs={[hex(r) for r in refs]} "
                  f"funcs={[hex(func_start(img, r) or 0) for r in refs]}")
    elif cmd == "dis":
        dis(img, int(sys.argv[3], 16), int(sys.argv[4], 16))
    elif cmd == "refs":
        t = int(sys.argv[3], 16)
        for r in img.rip_refs(t):
            print(hex(r), hex(func_start(img, r) or 0))
    elif cmd == "ptr":
        va = int(sys.argv[3], 16)
        cnt = int(sys.argv[4]) if len(sys.argv) > 4 else 1
        for k in range(cnt):
            print(f"{va + 8 * k:#x}: {img.ptr(va + 8 * k):#x}")


if __name__ == "__main__":
    main()
