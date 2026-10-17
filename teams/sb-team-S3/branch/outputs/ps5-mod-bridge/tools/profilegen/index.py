"""Whole-image indexes, built once and shared by every detector.

* ``StringTable``    : exact ASCII / UTF-16LE string lookup ("this string, delimited by NULs").
* ``RipXrefIndex``   : every ``lea/mov reg,[rip+disp32]`` style reference, sorted by target, so
                       "who references address X" is a bisect, not a scan.
* ``FunctionMap``    : function starts from the frame-pointer prologue (``push rbp; mov rbp,rsp``),
                       so "which function contains this address" is a bisect.
* ``CallIndex``      : ``call rel32`` / ``jmp rel32`` edges whose target is a function start.
* ``Decoder``        : thin optional wrapper over capstone (``Decoder.available``).

The indexes are byte-level (a regex finds candidate encodings and the displacement arithmetic is
checked); a candidate that is really the middle of another instruction is possible, which is why
detectors only trust *counts and agreement across many sites*, never one site, and verify the
interesting ones with the decoder.
"""
from __future__ import annotations

import re
import struct
from array import array
from bisect import bisect_left, bisect_right
from typing import Dict, Iterator, List, Optional, Sequence, Tuple

from .elf import ElfImage

try:  # capstone is optional: detectors degrade (and say so) without it.
    import capstone as _cs
    from capstone import x86 as _cs_x86
    from capstone import x86_const as _cs_c
except Exception:  # pragma: no cover - exercised only on machines without capstone
    _cs = None
    _cs_x86 = None
    _cs_c = None

PROLOGUE = b"\x55\x48\x89\xe5"          # push rbp ; mov rbp, rsp  (clang -fno-omit-frame-pointer)
MODRM_RIP = b"\x05\x0d\x15\x1d\x25\x2d\x35\x3d"      # mod=00 rm=101 for every reg field

_REG_NAMES64 = ["rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
                "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"]

KIND_LEA = 0
KIND_LOAD = 1
KIND_STORE = 2
_OPCODE_KIND = {0x8D: KIND_LEA, 0x8B: KIND_LOAD, 0x89: KIND_STORE}


# ---------------------------------------------------------------------------------------------
class StringTable:
    """Exact string lookup in the file-backed part of the image.

    A string "starts at" va when the preceding code unit is NUL (so the match is the whole string
    from its beginning) and the code unit after it is NUL.  Tail-merged strings (a literal whose
    start lies inside a longer literal) are found with ``anywhere=True``.
    """

    def __init__(self, elf: ElfImage) -> None:
        self.elf = elf

    def find_ascii(self, text: str, anywhere: bool = False) -> List[int]:
        needle = text.encode("ascii")
        return self._find(needle, 1, anywhere)

    def find_utf16(self, text: str, anywhere: bool = False) -> List[int]:
        needle = text.encode("utf-16-le")
        return self._find(needle, 2, anywhere)

    def _find(self, needle: bytes, unit: int, anywhere: bool) -> List[int]:
        """Hits in the readable data segments; only if there are none, in the code/unreadable ones."""
        for segs in (self.elf.data_segments, self.elf.other_segments):
            out = self._find_in(segs, needle, unit, anywhere)
            if out:
                return out
        return []

    def _find_in(self, segs, needle: bytes, unit: int, anywhere: bool) -> List[int]:
        pad = b"\x00" * unit
        out: List[int] = []
        data = self.elf.data
        for s in segs:
            lo, hi = s.offset, s.offset + s.filesz
            probe = needle if anywhere else pad + needle
            pos = lo
            while True:
                i = data.find(probe, pos, hi)
                if i < 0:
                    break
                pos = i + 1
                start = i if anywhere else i + unit
                end = start + len(needle)
                if end + unit > hi:
                    continue
                if bytes(data[end:end + unit]) != pad:
                    continue
                if unit == 2 and (s.vaddr + (start - lo)) & 1:
                    continue
                out.append(s.vaddr + (start - lo))
        return out

    def _tiered(self, scan) -> list:
        """Run ``scan(segments)`` on the readable data segments, and on the rest only if that found nothing."""
        for segs in (self.elf.data_segments, self.elf.other_segments):
            out = scan(segs)
            if out:
                return out
        return []

    def find_ascii_containing(self, fragment: str, limit: int = 64) -> List[Tuple[int, str]]:
        """NUL-delimited ASCII strings that CONTAIN ``fragment`` (start vaddr, full text)."""
        needle = fragment.encode("ascii")
        data = self.elf.data

        def scan(segs):
            res: List[Tuple[int, str]] = []
            for s in segs:
                lo, hi = s.offset, s.offset + s.filesz
                pos = lo
                while len(res) < limit:
                    i = data.find(needle, pos, hi)
                    if i < 0:
                        break
                    pos = i + 1
                    a = i
                    while a > lo and i - a < 256 and data[a - 1] != 0:
                        a -= 1
                    b = i + len(needle)
                    while b < hi and b - i < 256 and data[b] != 0:
                        b += 1
                    text = bytes(data[a:b])
                    if all(0x20 <= c < 0x7F for c in text):
                        res.append((s.vaddr + (a - lo), text.decode("ascii")))
            return res
        return self._tiered(scan)

    def find_utf16_containing(self, fragment: str, limit: int = 64) -> List[Tuple[int, str]]:
        """NUL-delimited UTF-16LE strings that contain ``fragment`` (start vaddr, full text)."""
        needle = fragment.encode("utf-16-le")
        data = self.elf.data

        def scan(segs):
            res: List[Tuple[int, str]] = []
            for s in segs:
                lo, hi = s.offset, s.offset + s.filesz
                pos = lo
                while len(res) < limit:
                    i = data.find(needle, pos, hi)
                    if i < 0:
                        break
                    pos = i + 1
                    if (i - lo) & 1:
                        continue
                    a = i
                    while a - 2 >= lo and i - a < 512 and data[a - 2:a] != b"\x00\x00":
                        a -= 2
                    va = s.vaddr + (a - lo)
                    text = self.elf.utf16(va, 256)
                    if text and fragment in text:
                        res.append((va, text))
            return res
        return self._tiered(scan)

    def find_ascii_prefix(self, prefix: str, limit: int = 64) -> List[Tuple[int, str]]:
        """NUL-delimited ASCII strings that START with ``prefix`` (full string returned)."""
        needle = b"\x00" + prefix.encode("ascii")
        data = self.elf.data

        def scan(segs):
            res: List[Tuple[int, str]] = []
            for s in segs:
                lo, hi = s.offset, s.offset + s.filesz
                pos = lo
                while len(res) < limit:
                    i = data.find(needle, pos, hi)
                    if i < 0:
                        break
                    pos = i + 1
                    j = i + 1
                    k = j
                    while k < hi and k - j < 256 and data[k] != 0:
                        k += 1
                    res.append((s.vaddr + (j - lo), bytes(data[j:k]).decode("latin-1")))
            return res
        return self._tiered(scan)


# ---------------------------------------------------------------------------------------------
class RipXrefIndex:
    """All ``lea/mov r,[rip+disp32]`` references in executable segments, sorted by target."""

    _RX = re.compile(rb"[\x40-\x4f]?[\x8d\x8b\x89][" + re.escape(MODRM_RIP) + rb"]", re.S)

    def __init__(self, elf: ElfImage) -> None:
        self.elf = elf
        sites = array("q")
        targets = array("q")
        kinds = array("B")
        regs = array("B")
        unpack = struct.Struct("<i").unpack_from
        data = elf.data
        for seg in elf.exec_segments:
            lo, hi = seg.offset, seg.offset + seg.filesz
            delta = seg.vaddr - seg.offset
            for m in self._RX.finditer(data, lo, hi):
                s, e = m.span()
                if e + 4 > hi:
                    continue
                disp = unpack(data, e)[0]
                modrm = data[e - 1]
                opcode = data[e - 2]
                rex = data[s] if e - s == 3 else 0
                sites.append(s + delta)
                targets.append(e + 4 + disp + delta)
                kinds.append(_OPCODE_KIND[opcode])
                regs.append(((modrm >> 3) & 7) | (8 if rex & 4 else 0))
        order = sorted(range(len(targets)), key=targets.__getitem__)
        self.targets = array("q", (targets[i] for i in order))
        self.sites = array("q", (sites[i] for i in order))
        self.kinds = array("B", (kinds[i] for i in order))
        self.regs = array("B", (regs[i] for i in order))

    def __len__(self) -> int:
        return len(self.targets)

    def refs_to(self, target: int, kinds: Optional[Sequence[int]] = None) -> List[int]:
        lo = bisect_left(self.targets, target)
        hi = bisect_right(self.targets, target)
        if kinds is None:
            return sorted(self.sites[lo:hi])
        ks = set(kinds)
        return sorted(self.sites[i] for i in range(lo, hi) if self.kinds[i] in ks)

    def count_to(self, target: int, kinds: Optional[Sequence[int]] = None) -> int:
        lo = bisect_left(self.targets, target)
        hi = bisect_right(self.targets, target)
        if kinds is None:
            return hi - lo
        ks = set(kinds)
        return sum(1 for i in range(lo, hi) if self.kinds[i] in ks)

    def iter_range(self, lo_va: int, hi_va: int) -> Iterator[Tuple[int, int, int, int]]:
        """(target, site, kind, reg) for every reference whose target is in [lo_va, hi_va)."""
        lo = bisect_left(self.targets, lo_va)
        hi = bisect_left(self.targets, hi_va)
        for i in range(lo, hi):
            yield self.targets[i], self.sites[i], self.kinds[i], self.regs[i]

    def targets_histogram(self, kinds: Optional[Sequence[int]] = None) -> Dict[int, int]:
        """target -> number of references (optionally filtered by kind)."""
        ks = None if kinds is None else set(kinds)
        hist: Dict[int, int] = {}
        t = self.targets
        k = self.kinds
        for i in range(len(t)):
            if ks is not None and k[i] not in ks:
                continue
            hist[t[i]] = hist.get(t[i], 0) + 1
        return hist


# ---------------------------------------------------------------------------------------------
class FunctionMap:
    """Function starts for clang/PS5 code.

    Two kinds of start are combined:

    * 16-byte aligned addresses that follow ``int3`` padding (every function the linker padded);
    * frame-pointer prologues (``push rbp; mov rbp,rsp``) that are NOT merely the shrink-wrapped
      tail of such a function: a prologue within ``SHRINK_WRAP_WINDOW`` bytes after an aligned
      start with no ``ret`` in between belongs to that earlier start (a function that tests an
      argument and returns early *before* it builds its frame, e.g. ``test rdi,rdi; je ret;
      push rbp; mov rbp,rsp``).

    The map is only trusted when starts are dense enough (``usable``)."""

    SHRINK_WRAP_WINDOW = 0x40
    _PAD_RUN = re.compile(rb"\xcc+(?=[^\xcc])", re.S)

    def __init__(self, elf: ElfImage, prologue: bytes = PROLOGUE) -> None:
        self.elf = elf
        starts = array("q")
        exec_bytes = 0
        data = elf.data
        for seg in elf.exec_segments:
            lo, hi = seg.offset, seg.offset + seg.filesz
            exec_bytes += seg.filesz
            delta = seg.vaddr - seg.offset
            aligned: List[int] = []
            for m in self._PAD_RUN.finditer(data, lo, hi):
                p = m.end()
                if (p + delta) % 16 == 0:
                    aligned.append(p)
            merged = set(aligned)
            i = data.find(prologue, lo, hi)
            while i != -1:
                if i not in merged:
                    k = bisect_right(aligned, i) - 1
                    shrink_wrapped = False
                    if k >= 0 and i - aligned[k] <= self.SHRINK_WRAP_WINDOW:
                        shrink_wrapped = b"\xc3" not in data[aligned[k]:i]
                    if not shrink_wrapped:
                        merged.add(i)
                i = data.find(prologue, i + 1, hi)
            for p in sorted(merged):
                starts.append(p + delta)
        self.starts = array("q", sorted(starts))
        self.exec_bytes = exec_bytes
        self._set = None

    @property
    def usable(self) -> bool:
        # one function per <= 4 KiB on average is a generous floor for compiled C++
        return len(self.starts) > 0 and self.exec_bytes / len(self.starts) < 4096

    def __len__(self) -> int:
        return len(self.starts)

    def is_start(self, va: int) -> bool:
        if self._set is None:
            self._set = set(self.starts)
        return va in self._set

    def containing(self, va: int) -> Optional[int]:
        """Start of the function that contains ``va`` (nearest prologue at or before it)."""
        i = bisect_right(self.starts, va) - 1
        return self.starts[i] if i >= 0 else None

    def next_start(self, va: int) -> Optional[int]:
        i = bisect_right(self.starts, va)
        return self.starts[i] if i < len(self.starts) else None

    def span(self, start: int) -> Tuple[int, int]:
        n = self.next_start(start)
        return start, (n if n is not None else start + 0x1000)


# ---------------------------------------------------------------------------------------------
class CallIndex:
    """``call rel32`` (E8) and ``jmp rel32`` (E9) edges whose target is a function start."""

    def __init__(self, elf: ElfImage, funcs: FunctionMap) -> None:
        self.elf = elf
        self.funcs = funcs
        starts = funcs._set if funcs._set is not None else set(funcs.starts)
        funcs._set = starts
        sites = array("q")
        targets = array("q")
        kinds = array("B")           # 0 call, 1 jmp
        unpack = struct.Struct("<i").unpack_from
        data = elf.data
        rx = re.compile(rb"[\xe8\xe9]", re.S)
        for seg in elf.exec_segments:
            lo, hi = seg.offset, seg.offset + seg.filesz
            delta = seg.vaddr - seg.offset
            for m in rx.finditer(data, lo, hi - 4):
                s = m.start()
                tgt = s + 5 + unpack(data, s + 1)[0] + delta
                if tgt in starts:
                    sites.append(s + delta)
                    targets.append(tgt)
                    kinds.append(0 if data[s] == 0xE8 else 1)
        order = sorted(range(len(targets)), key=targets.__getitem__)
        self.targets = array("q", (targets[i] for i in order))
        self.sites = array("q", (sites[i] for i in order))
        self.kinds = array("B", (kinds[i] for i in order))

    def __len__(self) -> int:
        return len(self.targets)

    def callers(self, target: int, calls_only: bool = True) -> List[int]:
        lo = bisect_left(self.targets, target)
        hi = bisect_right(self.targets, target)
        return sorted(self.sites[i] for i in range(lo, hi) if not calls_only or self.kinds[i] == 0)

    def caller_count(self, target: int) -> int:
        return bisect_right(self.targets, target) - bisect_left(self.targets, target)

    def callers_by_function(self, target: int) -> Dict[int, List[int]]:
        out: Dict[int, List[int]] = {}
        for site in self.callers(target):
            f = self.funcs.containing(site)
            out.setdefault(f if f is not None else -1, []).append(site)
        return out


# ---------------------------------------------------------------------------------------------
class Decoder:
    """Optional capstone wrapper.  ``Decoder.available`` is False when capstone is missing."""

    available = _cs is not None

    def __init__(self, elf: ElfImage) -> None:
        if _cs is None:
            raise RuntimeError("capstone is not installed")
        self.elf = elf
        self.md = _cs.Cs(_cs.CS_ARCH_X86, _cs.CS_MODE_64)
        self.md.detail = True

    def decode(self, va: int, nbytes: int = 64, count: int = 0) -> list:
        raw = self.elf.read_upto(va, nbytes)
        if not raw:
            return []
        return list(self.md.disasm(raw, va, count))

    MAX_FUNCTION_BYTES = 0x10000

    def decode_function(self, start: int, end: int) -> list:
        """Linear decode of [start, end) (capped) stopping at undecodable bytes."""
        raw = self.elf.read_upto(start, min(max(0, end - start), self.MAX_FUNCTION_BYTES))
        return list(self.md.disasm(raw, start)) if raw else []

    def reg_name(self, reg_id: int) -> str:
        return self.md.reg_name(reg_id) or ""


def reg_index(name: str) -> Optional[int]:
    return _REG_NAMES64.index(name) if name in _REG_NAMES64 else None


def reg_name64(idx: int) -> str:
    return _REG_NAMES64[idx]


class ImageIndex:
    """Facade that builds each index at most once and records how long it took."""

    def __init__(self, elf: ElfImage, timer=None) -> None:
        self.elf = elf
        self.strings = StringTable(elf)
        self._xrefs: Optional[RipXrefIndex] = None
        self._funcs: Optional[FunctionMap] = None
        self._calls: Optional[CallIndex] = None
        self._decoder: Optional[Decoder] = None
        self.timings: Dict[str, float] = {}
        self._timer = timer

    def _timed(self, name, fn):
        import time
        t0 = time.time()
        v = fn()
        self.timings[name] = round(time.time() - t0, 3)
        return v

    @property
    def xrefs(self) -> RipXrefIndex:
        if self._xrefs is None:
            self._xrefs = self._timed("index.xrefs", lambda: RipXrefIndex(self.elf))
        return self._xrefs

    @property
    def funcs(self) -> FunctionMap:
        if self._funcs is None:
            self._funcs = self._timed("index.functions", lambda: FunctionMap(self.elf))
        return self._funcs

    @property
    def calls(self) -> CallIndex:
        if self._calls is None:
            funcs = self.funcs
            self._calls = self._timed("index.calls", lambda: CallIndex(self.elf, funcs))
        return self._calls

    @property
    def decoder(self) -> Optional[Decoder]:
        if self._decoder is None and Decoder.available:
            self._decoder = Decoder(self.elf)
        return self._decoder
