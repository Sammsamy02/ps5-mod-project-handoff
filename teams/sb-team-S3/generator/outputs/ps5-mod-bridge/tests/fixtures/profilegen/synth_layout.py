"""More x86-64 encodings for the layout-detector fixtures (generic ``op [base+disp]`` forms).

``synth.Asm`` has the encodings the older detectors needed.  The layout detectors look at member accesses of every width
(byte/word/dword/qword loads, stores, immediates, compares, indexed forms), so this subclass adds one generic emitter per
form instead of one method per instruction.  Everything is disp32 / modrm-only; the numbers a test passes are arbitrary and
not any game's.
"""
from __future__ import annotations

import struct
from typing import Optional

from synth import RAX, RBP, RBX, RCX, RDI, RDX, RSI, RSP, Asm  # noqa: F401


class LAsm(Asm):
    # -- generic forms ---------------------------------------------------------------------------
    def _op(self, opcode: bytes, reg: int, base: int, disp: int, w: int = 1, p66: bool = False, imm: bytes = b"") -> "LAsm":
        self.buf += (b"\x66" if p66 else b"") + self._rex(w, reg, 0, base, w == 1 or reg >= 8 or base >= 8) \
            + opcode + self._mem(reg, base, disp) + imm
        return self

    def _op_sib(self, opcode: bytes, reg: int, base: int, index: int, scale: int, disp: int, w: int = 1, p66: bool = False,
                imm: bytes = b"") -> "LAsm":
        self.buf += (b"\x66" if p66 else b"") + self._rex(w, reg, index, base, w == 1 or reg >= 8 or base >= 8 or index >= 8) \
            + opcode + self._sib(reg, base, index, scale, disp) + imm
        return self

    # -- loads / stores --------------------------------------------------------------------------
    def ld(self, dst: int, base: int, disp: int, size: int = 8) -> "LAsm":
        """mov / movzx dst, [base+disp] (sizes 8, 4 = mov; 2, 1 = movzx to a 32-bit register)."""
        if size == 8:
            return self._op(b"\x8b", dst, base, disp, 1)
        if size == 4:
            return self._op(b"\x8b", dst, base, disp, 0)
        if size == 2:
            return self._op(b"\x0f\xb7", dst, base, disp, 0)
        return self._op(b"\x0f\xb6", dst, base, disp, 0)

    def ldx(self, dst: int, base: int, disp: int, size: int = 4) -> "LAsm":
        """movsxd (size 4) / movsx word (size 2) to a 64-bit register."""
        return self._op(b"\x63" if size == 4 else b"\x0f\xbf", dst, base, disp, 1)

    def st(self, base: int, disp: int, src: int, size: int = 8) -> "LAsm":
        if size == 8:
            return self._op(b"\x89", src, base, disp, 1)
        if size == 4:
            return self._op(b"\x89", src, base, disp, 0)
        if size == 2:
            return self._op(b"\x89", src, base, disp, 0, p66=True)
        return self._op(b"\x88", src, base, disp, 0)

    def st_imm(self, base: int, disp: int, imm: int, size: int = 8) -> "LAsm":
        if size == 8:
            return self._op(b"\xc7", 0, base, disp, 1, imm=struct.pack("<i", imm))
        if size == 4:
            return self._op(b"\xc7", 0, base, disp, 0, imm=struct.pack("<I", imm & 0xFFFFFFFF))
        if size == 2:
            return self._op(b"\xc7", 0, base, disp, 0, p66=True, imm=struct.pack("<H", imm & 0xFFFF))
        return self._op(b"\xc6", 0, base, disp, 0, imm=bytes([imm & 0xFF]))

    def ld_sib(self, dst: int, base: int, index: int, scale: int, disp: int = 0, size: int = 8) -> "LAsm":
        if size == 8:
            return self._op_sib(b"\x8b", dst, base, index, scale, disp, 1)
        if size == 2:
            return self._op_sib(b"\x0f\xb7", dst, base, index, scale, disp, 0)
        return self._op_sib(b"\x8b", dst, base, index, scale, disp, 0)

    def call_sib(self, base: int, index: int, scale: int, disp: int = 0) -> "LAsm":
        return self._op_sib(b"\xff", 2, base, index, scale, disp, 0)

    def and_mem8_reg(self, base: int, index: int, src: int) -> "LAsm":
        """and byte [base+index], src8 (src must be one of al cl dl bl)."""
        return self._op_sib(b"\x20", src, base, index, 1, 0, 0)

    # -- compares / tests ------------------------------------------------------------------------
    def cmp_mem_imm(self, base: int, disp: int, imm: int, size: int = 4) -> "LAsm":
        if size == 1:
            return self._op(b"\x80", 7, base, disp, 0, imm=bytes([imm & 0xFF]))
        return self._op(b"\x83", 7, base, disp, 1 if size == 8 else 0, imm=bytes([imm & 0xFF]))

    def test_mem_imm(self, base: int, disp: int, imm: int, size: int = 1) -> "LAsm":
        if size == 1:
            return self._op(b"\xf6", 0, base, disp, 0, imm=bytes([imm & 0xFF]))
        return self._op(b"\xf7", 0, base, disp, 1 if size == 8 else 0, imm=struct.pack("<I", imm & 0xFFFFFFFF))

    def cmp_r_imm(self, reg: int, imm: int, w: int = 1) -> "LAsm":
        self.buf += self._rex(w, 0, 0, reg, w == 1) + b"\x81" + bytes([0xF8 | (reg & 7)]) + struct.pack("<I", imm & 0xFFFFFFFF)
        return self

    def test_r_imm(self, reg: int, imm: int) -> "LAsm":
        self.buf += self._rex(0, 0, 0, reg) + b"\xf7" + bytes([0xC0 | (reg & 7)]) + struct.pack("<I", imm & 0xFFFFFFFF)
        return self

    def or_r32_mem(self, dst: int, base: int, disp: int) -> "LAsm":
        return self._op(b"\x0b", dst, base, disp, 0)

    def mov_r64_imm(self, dst: int, imm: int) -> "LAsm":
        self.buf += self._rex(1, 0, 0, dst, True) + bytes([0xB8 + (dst & 7)]) + struct.pack("<Q", imm & 0xFFFFFFFFFFFFFFFF)
        return self

    def imul_rr(self, dst: int, src: int) -> "LAsm":
        self.buf += self._rex(1, dst, 0, src, True) + b"\x0f\xaf" + bytes([0xC0 | ((dst & 7) << 3) | (src & 7)])
        return self

    def not_r8(self, reg: int) -> "LAsm":
        self.buf += b"\xf6" + bytes([0xD0 | (reg & 7)])
        return self

    def jcc(self, cc: int, target) -> "LAsm":
        """jcc rel32 (cc: 4 je, 5 jne, ...)."""
        self.buf += bytes([0x0F, 0x80 | cc])
        self.buf += self._rel32(target, 0)
        return self

    def mov_rr_(self, dst: int, src: int, w: int = 1) -> "LAsm":
        return self.mov_rr(dst, src) if w == 1 else self.mov_rr32(dst, src)

    def nops(self, n: int) -> "LAsm":
        self.buf += b"\x90" * n
        return self
