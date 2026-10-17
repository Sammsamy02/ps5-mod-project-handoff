#!/usr/bin/env python3
"""Drift-proof recursive-descent disassembler for one function.

Usage: python3 rdis.py <file_vaddr_hex> <max_bytes_hex>
Decodes from the given file vaddr; follows intra-function jcc/jmp targets,
does not follow calls (records them), stops chains on ret/int3/jmp-out.
"""
import struct
import sys

import capstone as cs

from vmap import v2f

BIN = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
DELTA = 0x400000
with open(BIN, "rb") as f:
    data = f.read()

md = cs.Cs(cs.CS_ARCH_X86, cs.CS_MODE_64)
md.detail = True


def read_at(vaddr: int, n: int):
    off = v2f(vaddr)
    if off is None:
        return None
    return data[off:off + n]


def disasm_func(start: int, limit: int = 0x800):
    seen = {}
    work = [start]
    calls = []
    while work:
        addr = work.pop()
        while True:
            if addr in seen or addr >= 0x7479b1c:
                break
            code = read_at(addr, 16)
            ins = next(md.disasm(code, addr + DELTA), None)
            if ins is None:
                break
            if (ins.address - DELTA) > start + limit:
                break
            seen[addr] = ins
            m = ins.mnemonic
            if m in ("ret", "int3", "ud2", "(bad)"):
                break
            if m == "jmp":
                op = ins.operands[0]
                if op.type == cs.x86.X86_OP_IMM:
                    tgt = op.imm - DELTA
                    if start <= tgt < start + limit:
                        addr = tgt
                        continue
                break
            if m.startswith("j"):
                op = ins.operands[0]
                if op.type == cs.x86.X86_OP_IMM:
                    tgt = op.imm - DELTA
                    if start <= tgt < start + limit:
                        work.append(tgt)
                addr += ins.size
                continue
            if m == "call":
                op = ins.operands[0]
                if op.type == cs.x86.X86_OP_IMM:
                    calls.append((hex(ins.address + DELTA), hex(op.imm)))
                addr += ins.size
                continue
            addr += ins.size
    return seen, calls


start = int(sys.argv[1], 16)
limit = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x800
seen, calls = disasm_func(start, limit)


def ann(ins):
    extra = ""
    for op in ins.operands:
        if op.type == cs.x86.X86_OP_MEM and op.mem.base == cs.x86.X86_REG_RIP:
            tgt = ins.address + ins.size + op.mem.disp
            extra += f"  ; rip->{tgt:#x} (file {tgt - DELTA:#x})"
    return extra


for a in sorted(seen):
    ins = seen[a]
    print(f"{ins.address:#x}  (f {ins.address - DELTA:#x})  {ins.bytes.hex():<22} "
          f"{ins.mnemonic} {ins.op_str}{ann(ins)}")
print("--- calls:", calls)
