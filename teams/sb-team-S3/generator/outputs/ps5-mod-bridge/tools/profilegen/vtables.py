"""Vtables of a PIE image: runs of consecutive code pointers written by RELATIVE relocations.

In a position-independent PS5 executable the pointers inside a vtable are *not* in the file:
the loader writes them from ``R_X86_64_RELATIVE`` relocations (``*(base + off) = base + addend``)
and the on-disk bytes are zero.  Reading vtables therefore means walking the RELA table.

An Itanium-ABI vtable is ``[offset_to_top (0, no reloc), typeinfo* (reloc to data), fn0, fn1, ...]``
and the object's vptr points at ``fn0``.  So a vtable shows up as a *run* of relocations at
consecutive 8-byte offsets whose addends are executable addresses; the run starts right after the
typeinfo pointer (which is a reloc to non-executable data).  ``Run.slots[k]`` is the function at
byte offset ``k * 8`` from the vptr.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import List

from .elf import ElfImage


@dataclass
class Run:
    offset: int          # vaddr of slot 0 (where the relocation is applied)
    slots: List[int]     # function vaddrs


def find_runs(elf: ElfImage, min_len: int = 16) -> List[Run]:
    offs, adds = elf.relative_relocs()
    ex = [(s.vaddr, s.file_end_va) for s in elf.exec_segments]
    if len(ex) == 1:
        lo, hi = ex[0]
        is_exec = lambda a: lo <= a < hi
    else:
        is_exec = lambda a: any(l <= a < h for l, h in ex)
    runs: List[Run] = []
    cur_start = None
    cur: List[int] = []
    prev = -16
    for i in range(len(offs)):
        o, a = offs[i], adds[i]
        if is_exec(a) and (cur and o == prev + 8 or not cur):
            if not cur:
                cur_start = o
            cur.append(a)
            prev = o
            continue
        if len(cur) >= min_len:
            runs.append(Run(cur_start, cur))
        cur = []
        if is_exec(a):
            cur_start, cur, prev = o, [a], o
    if len(cur) >= min_len:
        runs.append(Run(cur_start, cur))
    return runs
