#!/usr/bin/env python3
"""Annotated linear disassembler for the Townfall PS5 eboot.

Resolves rip-relative operands to (a) known strings from `strings -t x`,
(b) call targets within the image. Usage:
   python3 eboot_disasm.py <start_vaddr> <length> [--strings /tmp/eboot_strings.txt]
"""
import bisect
import sys

import capstone

EB = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
D = 0x4000


def load_strings(path):
    entries = []
    with open(path) as f:
        for line in f:
            parts = line.strip().split(None, 1)
            if len(parts) == 2:
                try:
                    entries.append((int(parts[0], 16) - D, parts[1]))
                except ValueError:
                    pass
    entries.sort()
    return entries


class Annotator:
    def __init__(self, strings_path):
        self.blob = open(EB, "rb").read()
        self.str_vaddrs = [v for v, _ in strings_path] if False else None
        entries = load_strings(strings_path)
        self.str_vaddrs = [v for v, _ in entries]
        self.str_texts = {v: t for v, t in entries}

    def string_at(self, vaddr):
        i = bisect.bisect_right(self.str_vaddrs, vaddr) - 1
        if i < 0:
            return None
        start = self.str_vaddrs[i]
        text = self.str_texts[start]
        if start <= vaddr < start + len(text) + 1:
            off = vaddr - start
            return text[off:] if off else text
        return None

    def disasm(self, start, length):
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        md.detail = True
        code = self.blob[start + D : start + D + length]
        for ins in md.disasm(code, start):
            note = ""
            for op in ins.operands:
                if op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP:
                    target = ins.address + ins.size + op.mem.disp
                    s = self.string_at(target)
                    if s is not None:
                        note = f'  ; "{s[:80]}"'
                    else:
                        note = f"  ; -> {target:#x}"
                elif op.type == capstone.x86.X86_OP_IMM and ins.group(capstone.x86.X86_GRP_JUMP) or (
                    ins.mnemonic == "call" and op.type == capstone.x86.X86_OP_IMM
                ):
                    note = f"  ; -> {op.imm:#x}"
            yield f"{ins.address:#010x}  {ins.mnemonic:<8} {ins.op_str}{note}"


def main():
    start = int(sys.argv[1], 16)
    length = int(sys.argv[2], 16)
    strings_file = "/tmp/eboot_strings.txt"
    if "--strings" in sys.argv:
        strings_file = sys.argv[sys.argv.index("--strings") + 1]
    ann = Annotator(strings_file)
    for line in ann.disasm(start, length):
        print(line)


if __name__ == "__main__":
    main()
