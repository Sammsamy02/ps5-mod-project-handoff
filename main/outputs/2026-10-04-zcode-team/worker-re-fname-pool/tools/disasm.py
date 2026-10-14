#!/usr/bin/env python3
"""TIER1 disassembler helper.

Usage: python3 disasm.py <file_off_hex> <len_hex> [--vadj 0x400000]
Disassembles bytes at file offset with vaddr = file_off + vadj, annotating
rip-relative targets and call targets.
"""
import sys
from typing import Optional

import capstone as cs

BIN = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
DELTA = 0x400000


def disasm(off: int, length: int, vadj: int = DELTA,
           stop_at: Optional[int] = None):
    with open(BIN, "rb") as f:
        f.seek(off)
        code = f.read(length)
    md = cs.Cs(cs.CS_ARCH_X86, cs.CS_MODE_64)
    md.detail = True
    lines = []
    for ins in md.disasm(code, off + vadj):
        extra = ""
        for op in ins.operands:
            if op.type == cs.x86.X86_OP_MEM and op.mem.base == cs.x86.X86_REG_RIP:
                tgt = ins.address + ins.size + op.mem.disp
                extra += f"  ; rip->{tgt:#x} (file {tgt - vadj:#x})"
            elif op.type == cs.x86.X86_OP_IMM and ins.mnemonic in (
                "call", "jmp", "je", "jne", "jg", "jl", "jle", "jge", "ja",
                "jb", "jbe", "jae", "js", "jns", "jo", "jno", "jp", "jnp",
            ):
                extra += f"  ; -> {op.imm:#x} (file {op.imm - vadj:#x})"
        lines.append(f"{ins.address:#x}  (file {ins.address - vadj:#x})  "
                     f"{ins.bytes.hex():<24} {ins.mnemonic} {ins.op_str}{extra}")
        if stop_at is not None and ins.address + vadj >= stop_at:
            break
    return lines


if __name__ == "__main__":
    off = int(sys.argv[1], 16)
    length = int(sys.argv[2], 16)
    lines = disasm(off, length)
    print("\n".join(lines))
