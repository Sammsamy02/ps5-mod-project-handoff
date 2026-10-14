"""Synthetic PS5-shaped ELF images and a tiny x86-64 assembler, for the profilegen unit tests.

The detectors work on instruction *shapes*, so the tests assemble just those shapes.  Nothing here
comes from a real game: the addresses are arbitrary and chosen only so that vaddr != file offset in
the same way a PS5 image is laid out (a different delta per segment, a BSS tail, a segment that
sits before its own vaddr in the file).
"""
from __future__ import annotations

import struct
from typing import Dict, List, Optional, Tuple

RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI = range(8)
R8, R9, R10, R11, R12, R13, R14, R15 = range(8, 16)
REG_NAMES = {n: i for i, n in enumerate("rax rcx rdx rbx rsp rbp rsi rdi r8 r9 r10 r11 r12 r13 r14 r15".split())}


class Asm:
    """A few x86-64 encodings, enough for the detector fixtures.  Targets are absolute vaddrs or labels."""

    def __init__(self, base: int) -> None:
        self.base = base
        self.buf = bytearray()
        self.labels: Dict[str, int] = {}
        self.fix: List[Tuple[int, str, str]] = []        # (buffer offset of disp32, label, kind)

    # -- position ------------------------------------------------------------------------
    @property
    def va(self) -> int:
        return self.base + len(self.buf)

    def label(self, name: str) -> int:
        self.labels[name] = self.va
        return self.va

    def raw(self, data: bytes) -> "Asm":
        self.buf += data
        return self

    def align(self, n: int = 16, fill: int = 0xCC) -> "Asm":
        while self.va % n:
            self.buf.append(fill)
        return self

    def int3(self, n: int = 1) -> "Asm":
        self.buf += b"\xcc" * n
        return self

    # -- encoding helpers ----------------------------------------------------------------
    @staticmethod
    def _rex(w: int, r: int, x: int, b: int, force: bool = False) -> bytes:
        v = 0x40 | (w << 3) | ((r >> 3) << 2) | ((x >> 3) << 1) | (b >> 3)
        return bytes([v]) if (v != 0x40 or force) else b""

    def _rel32(self, target, insn_end_offset: int, kind: str = "rel") -> bytes:
        """disp32 placeholder or value; ``insn_end_offset`` = bytes from the disp32 start to the instruction end."""
        if isinstance(target, str):
            self.fix.append((len(self.buf), target, "%s:%d" % (kind, insn_end_offset)))
            return b"\x00\x00\x00\x00"
        disp = target - (self.va + 4 + insn_end_offset)
        return struct.pack("<i", disp)

    def _mem(self, reg: int, base: int, disp: int) -> bytes:
        """modrm (+SIB) + disp32 for [base + disp]."""
        modrm = 0x80 | ((reg & 7) << 3) | (base & 7)
        out = bytes([modrm])
        if (base & 7) == 4:
            out += b"\x24"
        return out + struct.pack("<i", disp)

    # -- instructions --------------------------------------------------------------------
    def push(self, r: int) -> "Asm":
        self.buf += (b"\x41" if r >= 8 else b"") + bytes([0x50 + (r & 7)])
        return self

    def pop(self, r: int) -> "Asm":
        self.buf += (b"\x41" if r >= 8 else b"") + bytes([0x58 + (r & 7)])
        return self

    def ret(self) -> "Asm":
        self.buf += b"\xc3"
        return self

    def prologue(self) -> "Asm":
        self.buf += b"\x55\x48\x89\xe5"             # push rbp ; mov rbp, rsp
        return self

    def epilogue(self) -> "Asm":
        self.buf += b"\x5d\xc3"                     # pop rbp ; ret
        return self

    def mov_rr(self, dst: int, src: int) -> "Asm":
        self.buf += self._rex(1, src, 0, dst, True) + b"\x89" + bytes([0xC0 | ((src & 7) << 3) | (dst & 7)])
        return self

    def mov_rr32(self, dst: int, src: int) -> "Asm":
        self.buf += self._rex(0, src, 0, dst) + b"\x89" + bytes([0xC0 | ((src & 7) << 3) | (dst & 7)])
        return self

    def mov_r32_imm(self, dst: int, imm: int) -> "Asm":
        self.buf += self._rex(0, 0, 0, dst) + bytes([0xB8 + (dst & 7)]) + struct.pack("<I", imm & 0xFFFFFFFF)
        return self

    def xor_r32(self, r: int) -> "Asm":
        self.buf += self._rex(0, r, 0, r) + b"\x31" + bytes([0xC0 | ((r & 7) << 3) | (r & 7)])
        return self

    def lea_rip(self, dst: int, target) -> "Asm":
        self.buf += self._rex(1, dst, 0, 0, True) + b"\x8d" + bytes([((dst & 7) << 3) | 5])
        self.buf += self._rel32(target, 0)
        return self

    def mov_load_rip(self, dst: int, target) -> "Asm":
        self.buf += self._rex(1, dst, 0, 0, True) + b"\x8b" + bytes([((dst & 7) << 3) | 5])
        self.buf += self._rel32(target, 0)
        return self

    def mov_store_rip(self, target, src: int) -> "Asm":
        self.buf += self._rex(1, src, 0, 0, True) + b"\x89" + bytes([((src & 7) << 3) | 5])
        self.buf += self._rel32(target, 0)
        return self

    def lea_rm(self, dst: int, base: int, disp: int) -> "Asm":
        self.buf += self._rex(1, dst, 0, base, True) + b"\x8d" + self._mem(dst, base, disp)
        return self

    def load64(self, dst: int, base: int, disp: int = 0) -> "Asm":
        self.buf += self._rex(1, dst, 0, base, True) + b"\x8b" + self._mem(dst, base, disp)
        return self

    def store64(self, base: int, disp: int, src: int) -> "Asm":
        self.buf += self._rex(1, src, 0, base, True) + b"\x89" + self._mem(src, base, disp)
        return self

    def load32(self, dst: int, base: int, disp: int = 0) -> "Asm":
        self.buf += self._rex(0, dst, 0, base) + b"\x8b" + self._mem(dst, base, disp)
        return self

    def store32(self, base: int, disp: int, src: int) -> "Asm":
        self.buf += self._rex(0, src, 0, base) + b"\x89" + self._mem(src, base, disp)
        return self

    def store32_imm(self, base: int, disp: int, imm: int) -> "Asm":
        self.buf += self._rex(0, 0, 0, base) + b"\xc7" + self._mem(0, base, disp) + struct.pack("<I", imm & 0xFFFFFFFF)
        return self

    def cmp32_mem_reg(self, base: int, disp: int, reg: int) -> "Asm":
        self.buf += self._rex(0, reg, 0, base) + b"\x39" + self._mem(reg, base, disp)
        return self

    def cmp8_mem_imm(self, base: int, disp: int, imm: int) -> "Asm":
        self.buf += self._rex(0, 0, 0, base) + b"\x80" + self._mem(7, base, disp) + bytes([imm & 0xFF])
        return self

    def test8_mem_imm(self, base: int, disp: int, imm: int) -> "Asm":
        self.buf += self._rex(0, 0, 0, base) + b"\xf6" + self._mem(0, base, disp) + bytes([imm & 0xFF])
        return self

    def shr_r32(self, r: int, imm: int) -> "Asm":
        self.buf += self._rex(0, 0, 0, r) + b"\xc1" + bytes([0xE8 | (r & 7), imm & 0xFF])
        return self

    def add_r64_imm8(self, r: int, imm: int) -> "Asm":
        self.buf += self._rex(1, 0, 0, r, True) + b"\x83" + bytes([0xC0 | (r & 7), imm & 0xFF])
        return self

    def cmp_r64_imm8(self, r: int, imm: int) -> "Asm":
        self.buf += self._rex(1, 0, 0, r, True) + b"\x83" + bytes([0xF8 | (r & 7), imm & 0xFF])
        return self

    def call(self, target) -> "Asm":
        self.buf.append(0xE8)
        self.buf += self._rel32(target, 0)
        return self

    def jmp(self, target) -> "Asm":
        self.buf.append(0xE9)
        self.buf += self._rel32(target, 0)
        return self

    def call_mem(self, base: int, disp: int) -> "Asm":
        self.buf += self._rex(0, 0, 0, base) + b"\xff" + self._mem(2, base, disp)
        return self

    def movzx_al(self, dst: int) -> "Asm":
        self.buf += self._rex(0, dst, 0, 0, True) + b"\x0f\xb6" + bytes([0xC0 | ((dst & 7) << 3)])
        return self

    def shl_r32(self, r: int, imm: int) -> "Asm":
        self.buf += self._rex(0, 0, 0, r) + b"\xc1" + bytes([0xE0 | (r & 7), imm & 0xFF])
        return self

    def or_rr32(self, dst: int, src: int) -> "Asm":
        self.buf += self._rex(0, src, 0, dst) + b"\x09" + bytes([0xC0 | ((src & 7) << 3) | (dst & 7)])
        return self


    def load64_sib(self, dst: int, base: int, index: int, scale: int, disp: int = 0) -> "Asm":
        """mov dst, [base + index*scale + disp32]"""
        self.buf += self._rex(1, dst, index, base, True) + b"\x8b" + self._sib(dst, base, index, scale, disp)
        return self

    def store64_sib(self, base: int, index: int, scale: int, disp: int, src: int) -> "Asm":
        self.buf += self._rex(1, src, index, base, True) + b"\x89" + self._sib(src, base, index, scale, disp)
        return self

    def _sib(self, reg: int, base: int, index: int, scale: int, disp: int) -> bytes:
        ss = {1: 0, 2: 1, 4: 2, 8: 3}[scale]
        return bytes([0x84 | ((reg & 7) << 3), (ss << 6) | ((index & 7) << 3) | (base & 7)]) + struct.pack("<i", disp)

    def movsxd_load(self, dst: int, base: int, disp: int) -> "Asm":
        self.buf += self._rex(1, dst, 0, base, True) + b"\x63" + self._mem(dst, base, disp)
        return self

    def cmp8_rip_imm(self, target, imm: int) -> "Asm":
        self.buf += b"\x80\x3d"
        self.buf += self._rel32(target, 1)
        self.buf += bytes([imm & 0xFF])
        return self

    def mov8_rip_imm(self, target, imm: int) -> "Asm":
        self.buf += b"\xc6\x05"
        self.buf += self._rel32(target, 1)
        self.buf += bytes([imm & 0xFF])
        return self

    def cmp16_mem_imm(self, base: int, disp: int, imm: int) -> "Asm":
        self.buf += b"\x66" + self._rex(0, 0, 0, base) + b"\x83" + self._mem(7, base, disp) + bytes([imm & 0xFF])
        return self

    def test_rr(self, a: int, b: int) -> "Asm":
        self.buf += self._rex(1, b, 0, a, True) + b"\x85" + bytes([0xC0 | ((b & 7) << 3) | (a & 7)])
        return self

    def test32_mem_imm(self, base: int, disp: int, imm: int) -> "Asm":
        self.buf += self._rex(0, 0, 0, base) + b"\xf7" + self._mem(0, base, disp) + struct.pack("<I", imm & 0xFFFFFFFF)
        return self

    def store16_imm(self, base: int, disp: int, imm: int) -> "Asm":
        self.buf += b"\x66" + self._rex(0, 0, 0, base) + b"\xc7" + self._mem(0, base, disp) + struct.pack("<H", imm & 0xFFFF)
        return self

    def inc_mem32(self, base: int, disp: int) -> "Asm":
        self.buf += self._rex(0, 0, 0, base) + b"\xff" + self._mem(0, base, disp)
        return self

    # -- finish --------------------------------------------------------------------------
    def finish(self, extern: Optional[Dict[str, int]] = None) -> bytes:
        table = dict(extern or {})
        table.update(self.labels)
        for off, name, kind in self.fix:
            _k, tail = kind.split(":")
            target = table[name]
            disp = target - (self.base + off + 4 + int(tail))
            self.buf[off:off + 4] = struct.pack("<i", disp)
        self.fix = []
        return bytes(self.buf)


def utf16(s: str) -> bytes:
    return s.encode("utf-16-le") + b"\x00\x00"


def ascii0(s: str) -> bytes:
    return s.encode("ascii") + b"\x00"


# ---------------------------------------------------------------------------------------------
class Seg:
    def __init__(self, va: int, data: bytes, flags: int, memsz: Optional[int] = None, delta: int = 0x4000,
                 p_type: int = 1) -> None:
        self.va, self.data, self.flags = va, bytes(data), flags
        self.memsz = memsz if memsz is not None else len(data)
        self.delta, self.p_type = delta, p_type


def build_elf(segs: List[Seg], notes: Optional[List[bytes]] = None, relas: Optional[List[Tuple[int, int]]] = None,
              e_type: int = 0xFE10, osabi: int = 9, procparam: Optional[bytes] = None) -> bytes:
    """Assemble an ELF64 image: ``segs`` are PT_LOADs placed at ``file offset = vaddr + delta``.

    ``relas`` (offset, addend) become R_X86_64_RELATIVE entries in an extra flags-0 PT_LOAD whose
    file offset is *below* its vaddr, like the SCE dynlib data of a real image; PT_DYNAMIC points at it.
    """
    notes = notes or []
    phdrs: List[Tuple[int, int, int, int, int, int, int, int]] = []        # type flags off va pa filesz memsz align
    placed: List[Tuple[int, bytes]] = []
    for s in segs:
        off = s.va + s.delta
        phdrs.append((s.p_type, s.flags, off, s.va, s.va, len(s.data), s.memsz, 0x1000))
        placed.append((off, s.data))
    if relas is not None:
        dyn_va = 0x40000
        dyn_off = 0x20000
        rela_blob = b"".join(struct.pack("<QQq", o, 8, a) for o, a in relas)
        dyn_entries = [(7, dyn_va + 0x100), (8, len(rela_blob)), (9, 24), (0x6FFFFFF9, len(relas)), (0, 0)]
        dyn_blob = b"".join(struct.pack("<qQ", t, v) for t, v in dyn_entries)
        blob = dyn_blob.ljust(0x100, b"\x00") + rela_blob
        phdrs.append((1, 0, dyn_off, dyn_va, dyn_va, len(blob), len(blob), 0x1000))
        phdrs.append((2, 4, dyn_off, dyn_va, dyn_va, len(dyn_blob), len(dyn_blob), 8))
        placed.append((dyn_off, blob))
    if notes:
        nb = b"".join(notes)
        off = 0x30000
        phdrs.append((4, 4, off, 0x50000, 0x50000, len(nb), len(nb), 4))
        placed.append((off, nb))
    if procparam is not None:
        off = 0x38000
        phdrs.append((0x61000001, 4, off, 0x58000, 0x58000, len(procparam), len(procparam), 8))
        placed.append((off, procparam))
    size = max(o + len(d) for o, d in placed)
    img = bytearray(size)
    ph_off = 0x40
    hdr = bytearray(b"\x7fELF" + bytes([2, 1, 1, osabi]) + b"\x00" * 8)
    hdr += struct.pack("<HHIQQQIHHHHHH", e_type, 62, 1, 0x80, ph_off, 0, 0, 64, 56, len(phdrs), 64, 0, 0)
    img[0:len(hdr)] = hdr
    for i, (t, fl, off, va, pa, filesz, memsz, al) in enumerate(phdrs):
        img[ph_off + i * 56: ph_off + (i + 1) * 56] = struct.pack("<IIQQQQQQ", t, fl, off, va, pa, filesz, memsz, al)
    for off, data in placed:
        assert off >= ph_off + len(phdrs) * 56, "segment overlaps the program headers"
        img[off:off + len(data)] = data
    return bytes(img)


def gnu_note(desc: bytes) -> bytes:
    name = b"GNU\x00"
    return struct.pack("<III", len(name), len(desc), 3) + name + desc.ljust((len(desc) + 3) & ~3, b"\x00")
