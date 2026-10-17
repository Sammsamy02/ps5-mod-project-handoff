"""Small capstone helpers shared by the decode-based detectors."""
from __future__ import annotations

from typing import Iterable, List, Set

_R64 = ["rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi"] + ["r%d" % i for i in range(8, 16)]
CALLER_SAVED = {"rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11"}

_SUB = {}
for _i, _n in enumerate(["ax", "cx", "dx", "bx", "sp", "bp", "si", "di"]):
    _SUB[_n] = _R64[_i]
    _SUB["e" + _n] = _R64[_i]
    _SUB["r" + _n] = _R64[_i]
for _n, _r in (("al", "rax"), ("cl", "rcx"), ("dl", "rdx"), ("bl", "rbx"), ("spl", "rsp"), ("bpl", "rbp"),
               ("sil", "rsi"), ("dil", "rdi"), ("ah", "rax"), ("ch", "rcx"), ("dh", "rdx"), ("bh", "rbx")):
    _SUB[_n] = _r
for _i in range(8, 16):
    for _suffix in ("", "d", "w", "b"):
        _SUB["r%d%s" % (_i, _suffix)] = "r%d" % _i


def canon64(name: str) -> str:
    """Map any x86-64 general register name (eax, r14d, dil, ...) to its 64-bit name."""
    return _SUB.get(name, name)


class Regs:
    """Name lookups + read/written register sets for a capstone instruction."""

    def __init__(self, decoder) -> None:
        self.dec = decoder

    def name(self, rid: int) -> str:
        return canon64(self.dec.reg_name(rid)) if rid else ""

    def written(self, ins) -> Set[str]:
        try:
            _r, w = ins.regs_access()
        except Exception:
            return set()
        return {self.name(r) for r in w}

    def read(self, ins) -> Set[str]:
        try:
            r, _w = ins.regs_access()
        except Exception:
            return set()
        return {self.name(x) for x in r}


class Access:
    """One ``[alias + disp]`` memory access found by ``field_accesses``."""

    __slots__ = ("addr", "mnemonic", "disp", "size", "indexed", "scale", "reads", "writes", "lock")

    def __init__(self, addr, mnemonic, disp, size, indexed, scale, reads, writes, lock):
        self.addr, self.mnemonic, self.disp, self.size = addr, mnemonic, disp, size
        self.indexed, self.scale, self.reads, self.writes, self.lock = indexed, scale, reads, writes, lock


def field_accesses(ctx_dec, regs: Regs, ins: list, alias: Iterable[str], stop_at_ret: bool = True,
                   max_ins: int = 100000) -> List[Access]:
    """Accesses through a pointer (and its register copies) over a linear instruction list.

    ``alias`` names the registers that hold the object pointer at the start.  Register copies are
    followed; a register that is written by anything else stops being an alias; calls clobber the
    caller-saved registers.
    """
    from capstone import x86 as xo
    CS_AC_READ, CS_AC_WRITE = 1, 2
    alias = set(alias)
    out: List[Access] = []
    for n, i in enumerate(ins):
        if n >= max_ins or not alias:
            break
        mn = i.mnemonic
        ops = i.operands
        for op in ops:
            if op.type == xo.X86_OP_MEM and op.mem.base and regs.name(op.mem.base) in alias:
                out.append(Access(i.address, mn, op.mem.disp, op.size, op.mem.index != 0, op.mem.scale,
                                  bool(op.access & CS_AC_READ), bool(op.access & CS_AC_WRITE),
                                  mn.startswith("lock") or bytes(i.bytes[:1]) == b"\xf0"))
        written = regs.written(i)
        if mn == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_REG and \
                regs.name(ops[1].reg) in alias and ops[0].size == 8:
            written = written - {regs.name(ops[0].reg)}
            alias.add(regs.name(ops[0].reg))
        alias -= written
        if mn == "call":
            alias -= CALLER_SAVED
        if stop_at_ret and mn in ("ret", "jmp") and len(ops) == 0:
            break
    return out
