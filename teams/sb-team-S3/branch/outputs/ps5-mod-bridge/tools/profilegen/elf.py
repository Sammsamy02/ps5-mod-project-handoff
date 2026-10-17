"""PS5 (Prospero) ELF loader: program headers, vaddr <-> file offset, read helpers.

A decrypted PS5 executable is an ELF64 x86-64 image with e_type 0xFE10 (SCE_DYNEXEC), OSABI 9
(FreeBSD), no usable section headers and Sony-scrambled dynamic symbol names.  Everything the
generator needs is reachable from the *program headers*:

* PT_LOAD segments give the vaddr <-> file-offset mapping.  The delta is per segment (code and
  read-only data may sit at +0x4000 while another segment sits at a negative delta), so a vaddr
  must NEVER be used directly as a file offset.  ``va_to_off`` / ``off_to_va`` are the only conversions.
* PT_NOTE carries the GNU build-id.
* PT_DYNAMIC carries the RELA table; vtables live as R_X86_64_RELATIVE relocations (the file bytes
  of a vtable slot are zero), so ``relative_relocs`` is how vtables are read.
* PT_SCE_PROCPARAM (0x61000001) carries the ``ORBI`` process-parameter block.

``ElfImage`` only needs a bytes-like object (``bytes`` for synthetic fixtures, ``mmap`` for the
194 MB real image); nothing is copied.
"""
from __future__ import annotations

import mmap
import struct
from array import array
from dataclasses import dataclass
from typing import Dict, Iterator, List, Optional, Tuple

PT_NULL = 0
PT_LOAD = 1
PT_DYNAMIC = 2
PT_NOTE = 4
PT_TLS = 7
PT_GNU_EH_FRAME = 0x6474E550
PT_GNU_RELRO = 0x6474E552
PT_SCE_PROCPARAM = 0x61000001

PF_X = 1
PF_W = 2
PF_R = 4

ET_SCE_DYNEXEC = 0xFE10
EM_X86_64 = 62
ELFOSABI_FREEBSD = 9

DT_NULL = 0
DT_RELA = 7
DT_RELASZ = 8
DT_RELAENT = 9
DT_RELACOUNT = 0x6FFFFFF9
R_X86_64_RELATIVE = 8
R_X86_64_64 = 1

NT_GNU_BUILD_ID = 3


# The first four bytes of an encrypted SELF container (the PS4-style header, and the PS5 one).  Nothing in it can be
# analysed; the executable has to be decrypted first (profilegen never decrypts and never touches a console).
SELF_MAGIC = b"\x4f\x15\x3d\x1d"
SELF_MAGICS = (SELF_MAGIC, b"\x54\x14\xf5\xee")
# Generous upper bound for a vaddr (1 TiB): anything above is a corrupt program header, and
# keeps the address arithmetic of the indexes inside 64-bit signed arrays.
MAX_VADDR = 1 << 40


class ElfError(ValueError):
    pass


class EncryptedSelfError(ElfError):
    """The input is an encrypted SELF container, not a decrypted ELF."""


def check_not_encrypted_self(head: bytes) -> None:
    magic = bytes(head[:4])
    if magic in SELF_MAGICS:
        raise EncryptedSelfError(
            "the executable is an encrypted PS4/PS5 SELF (magic %s), not a decrypted ELF: nothing was analysed; "
            "decrypt it first (profilegen does not decrypt and does not contact a console)" % " ".join("%02x" % b for b in magic))


@dataclass(frozen=True)
class Segment:
    index: int
    p_type: int
    flags: int
    offset: int
    vaddr: int
    filesz: int
    memsz: int
    align: int

    @property
    def readable(self) -> bool:
        return bool(self.flags & PF_R)

    @property
    def writable(self) -> bool:
        return bool(self.flags & PF_W)

    @property
    def executable(self) -> bool:
        return bool(self.flags & PF_X)

    @property
    def file_end_va(self) -> int:
        return self.vaddr + self.filesz

    @property
    def mem_end_va(self) -> int:
        return self.vaddr + self.memsz

    @property
    def delta(self) -> int:
        """file offset = vaddr + delta (per segment)."""
        return self.offset - self.vaddr


@dataclass(frozen=True)
class Note:
    name: str
    n_type: int
    desc: bytes
    segment_index: int
    file_offset: int


class ElfImage:
    def __init__(self, data) -> None:
        self.data = data
        self.size = len(data)
        check_not_encrypted_self(bytes(data[0:4]))
        if self.size < 64 or bytes(data[0:4]) != b"\x7fELF":
            raise ElfError("not an ELF image")
        if data[4] != 2 or data[5] != 1:
            raise ElfError("only ELF64 little-endian images are supported (EI_CLASS %d, EI_DATA %d)" % (data[4], data[5]))
        self.osabi = data[7]
        (self.e_type, self.e_machine, _ver, self.e_entry, self.e_phoff, self.e_shoff, self.e_flags,
         _ehsize, self.e_phentsize, self.e_phnum, self.e_shentsize, self.e_shnum, self.e_shstrndx) = \
            struct.unpack_from("<HHIQQQIHHHHHH", data, 16)
        if self.e_machine != EM_X86_64:
            raise ElfError("e_machine %d is not x86-64 (%d): only x86-64 images are supported" % (self.e_machine, EM_X86_64))
        if self.e_phnum == 0:
            raise ElfError("the image has no program headers (stripped or not an executable image): nothing can be mapped")
        if self.e_phentsize < 56 or self.e_phoff + self.e_phnum * self.e_phentsize > self.size:
            raise ElfError("program header table out of range")
        segs: List[Segment] = []
        for i in range(self.e_phnum):
            p_type, p_flags, p_off, p_va, _pa, p_filesz, p_memsz, p_align = struct.unpack_from(
                "<IIQQQQQQ", data, self.e_phoff + i * self.e_phentsize)
            segs.append(Segment(i, p_type, p_flags, p_off, p_va, p_filesz, p_memsz, p_align))
        self.segments: List[Segment] = segs
        for s in segs:
            if s.p_type != PT_NULL and s.filesz and (s.offset > self.size or s.offset + s.filesz > self.size):
                raise ElfError("program header %d (type 0x%x) runs past the end of the file: truncated or corrupt image" % (
                    s.index, s.p_type))
        self.loads: List[Segment] = [s for s in segs if s.p_type == PT_LOAD and s.memsz > 0]
        for s in self.loads:
            if s.memsz < s.filesz:
                raise ElfError("PT_LOAD %d has p_memsz 0x%x below p_filesz 0x%x" % (s.index, s.memsz, s.filesz))
            if s.vaddr + s.memsz > MAX_VADDR:
                raise ElfError("PT_LOAD %d maps 0x%x..0x%x, beyond any plausible image address" % (
                    s.index, s.vaddr, s.vaddr + s.memsz))
        if not self.loads:
            raise ElfError("the image has no loadable segment (every PT_LOAD is empty or missing): nothing can be mapped")
        by_va = sorted(self.loads, key=lambda s: (s.vaddr, s.index))
        for a, b in zip(by_va, by_va[1:]):
            if a.mem_end_va > b.vaddr:
                raise ElfError("PT_LOAD %d (0x%x..0x%x) and PT_LOAD %d (0x%x..) overlap in memory: the vaddr mapping would "
                               "be ambiguous" % (a.index, a.vaddr, a.mem_end_va, b.index, b.vaddr))
        self._rela_cache: Optional[Tuple[array, array]] = None

    # ------------------------------------------------------------------ construction
    @classmethod
    def open(cls, path: str) -> "ElfImage":
        fh = open(path, "rb")
        try:
            data = mmap.mmap(fh.fileno(), 0, access=mmap.ACCESS_READ)
        except (ValueError, OSError):
            data = fh.read()
        finally:
            fh.close()
        return cls(data)

    # ------------------------------------------------------------------ platform identity
    @property
    def is_ps5_dynexec(self) -> bool:
        return self.e_type == ET_SCE_DYNEXEC and self.e_machine == EM_X86_64

    @property
    def extent(self) -> int:
        """Highest mapped vaddr (max p_vaddr + p_memsz over PT_LOAD)."""
        return max((s.mem_end_va for s in self.loads), default=0)

    @property
    def exec_segments(self) -> List[Segment]:
        return [s for s in self.loads if s.executable]

    @property
    def data_segments(self) -> List[Segment]:
        """Readable, non-executable PT_LOADs: where string literals live (rodata and data)."""
        return [s for s in self.loads if s.readable and not s.executable]

    @property
    def other_segments(self) -> List[Segment]:
        """Everything else (code, unreadable dynlib data): searched only when data segments have no hit."""
        data = {s.index for s in self.data_segments}
        return [s for s in self.loads if s.index not in data]

    # ------------------------------------------------------------------ mapping
    def segment_of(self, va: int) -> Optional[Segment]:
        for s in self.loads:
            if s.vaddr <= va < s.mem_end_va:
                return s
        return None

    def va_to_off(self, va: int) -> Optional[int]:
        """File offset of ``va`` if it is file-backed, else None (BSS / unmapped)."""
        for s in self.loads:
            if s.vaddr <= va < s.file_end_va:
                return s.offset + (va - s.vaddr)
        return None

    def off_to_va(self, off: int) -> Optional[int]:
        for s in self.loads:
            if s.offset <= off < s.offset + s.filesz:
                return s.vaddr + (off - s.offset)
        return None

    def is_mapped(self, va: int) -> bool:
        return self.segment_of(va) is not None

    def is_file_backed(self, va: int) -> bool:
        return self.va_to_off(va) is not None

    def is_exec(self, va: int) -> bool:
        s = self.segment_of(va)
        return s is not None and s.executable and va < s.file_end_va

    def is_bss(self, va: int) -> bool:
        s = self.segment_of(va)
        return s is not None and va >= s.file_end_va

    def is_writable(self, va: int) -> bool:
        s = self.segment_of(va)
        return s is not None and s.writable

    # ------------------------------------------------------------------ reads
    def read(self, va: int, n: int) -> Optional[bytes]:
        """``n`` bytes at ``va``; None unless the whole range is file-backed in ONE segment."""
        if n < 0:
            return None
        for s in self.loads:
            if s.vaddr <= va and va + n <= s.file_end_va:
                o = s.offset + (va - s.vaddr)
                return bytes(self.data[o:o + n])
        return None

    def read_upto(self, va: int, n: int) -> bytes:
        """Like ``read`` but clamps to the end of the file-backed range (b'' if none)."""
        for s in self.loads:
            if s.vaddr <= va < s.file_end_va:
                o = s.offset + (va - s.vaddr)
                return bytes(self.data[o:o + min(n, s.file_end_va - va)])
        return b""

    def u8(self, va: int) -> Optional[int]:
        b = self.read(va, 1)
        return None if b is None else b[0]

    def u16(self, va: int) -> Optional[int]:
        b = self.read(va, 2)
        return None if b is None else struct.unpack("<H", b)[0]

    def u32(self, va: int) -> Optional[int]:
        b = self.read(va, 4)
        return None if b is None else struct.unpack("<I", b)[0]

    def i32(self, va: int) -> Optional[int]:
        b = self.read(va, 4)
        return None if b is None else struct.unpack("<i", b)[0]

    def u64(self, va: int) -> Optional[int]:
        b = self.read(va, 8)
        return None if b is None else struct.unpack("<Q", b)[0]

    def cstring(self, va: int, maxlen: int = 256) -> Optional[str]:
        """NUL-terminated ASCII string at ``va`` (printable ASCII only), else None."""
        raw = self.read_upto(va, maxlen + 1)
        end = raw.find(b"\x00")
        if end < 0:
            return None
        raw = raw[:end]
        if not raw or any(c < 0x20 or c > 0x7E for c in raw):
            return None
        return raw.decode("ascii")

    def utf16(self, va: int, maxchars: int = 256) -> Optional[str]:
        """NUL-terminated UTF-16LE string at ``va`` (BMP printable ASCII subset), else None."""
        raw = self.read_upto(va, maxchars * 2 + 2)
        out = []
        for i in range(0, len(raw) - 1, 2):
            c = raw[i] | (raw[i + 1] << 8)
            if c == 0:
                return "".join(out) if out else None
            if c < 0x20 or c > 0x7E:
                return None
            out.append(chr(c))
        return None

    # ------------------------------------------------------------------ whole-image search
    def find_all_offsets(self, needle: bytes, start: int = 0, end: Optional[int] = None) -> Iterator[int]:
        """Every file offset where ``needle`` occurs (overlapping allowed)."""
        data = self.data
        end = self.size if end is None else end
        i = data.find(needle, start, end)
        while i != -1:
            yield i
            i = data.find(needle, i + 1, end)

    def find_all_vas(self, needle: bytes) -> List[int]:
        """Vaddrs of every occurrence of ``needle`` lying fully inside one file-backed segment."""
        out: List[int] = []
        for s in self.loads:
            lo, hi = s.offset, s.offset + s.filesz
            for o in self.find_all_offsets(needle, lo, hi):
                if o + len(needle) <= hi:
                    out.append(s.vaddr + (o - lo))
        return out

    # ------------------------------------------------------------------ notes / dynamic / relocs
    def notes(self) -> List[Note]:
        out: List[Note] = []
        for s in self.segments:
            if s.p_type != PT_NOTE or s.filesz == 0:
                continue
            base = s.offset
            pos = 0
            while pos + 12 <= s.filesz:
                namesz, descsz, n_type = struct.unpack_from("<III", self.data, base + pos)
                pos += 12
                if pos + ((namesz + 3) & ~3) + ((descsz + 3) & ~3) > s.filesz:
                    break                                       # a note that does not fit its segment is corrupt: stop
                name_raw = bytes(self.data[base + pos:base + pos + namesz])
                pos += (namesz + 3) & ~3
                desc = bytes(self.data[base + pos:base + pos + descsz])
                pos += (descsz + 3) & ~3
                if namesz == 0 and descsz == 0 and n_type == 0:
                    continue                      # empty filler record
                out.append(Note(name_raw.rstrip(b"\x00").decode("latin-1"), n_type, desc, s.index,
                                base + pos - ((descsz + 3) & ~3)))
        return out

    def dynamic_tags(self) -> List[Tuple[int, int]]:
        for s in self.segments:
            if s.p_type != PT_DYNAMIC:
                continue
            tags: List[Tuple[int, int]] = []
            for i in range(min(s.filesz, max(0, self.size - s.offset)) // 16):
                tag, val = struct.unpack_from("<qQ", self.data, s.offset + i * 16)
                tags.append((tag, val))
                if tag == DT_NULL:
                    break
            return tags
        return []

    def dynamic(self) -> Dict[int, int]:
        d: Dict[int, int] = {}
        for tag, val in self.dynamic_tags():
            d.setdefault(tag, val)
        return d

    def rela_table(self) -> Optional[Tuple[int, int]]:
        """(file offset, entry count) of the RELA table, if the dynamic section names one."""
        d = self.dynamic()
        va, size = d.get(DT_RELA), d.get(DT_RELASZ)
        ent = d.get(DT_RELAENT, 24)
        if va is None or size is None or ent != 24:
            return None
        off = self.va_to_off(va)
        if off is None or off + size > self.size:
            return None
        return off, size // 24

    def relative_relocs(self) -> Tuple[array, array]:
        """(offsets, addends) of every R_X86_64_RELATIVE reloc, sorted by offset.

        These hold the pointer values the loader will write (``*(base+off) = base+addend``);
        the on-disk bytes at ``off`` are not the pointer, so vtables must be read from here.
        """
        if self._rela_cache is not None:
            return self._rela_cache
        offs, adds = array("Q"), array("q")
        tab = self.rela_table()
        if tab is not None:
            off, count = tab
            blob = bytes(self.data[off:off + count * 24])
            q = memoryview(blob).cast("Q")
            infos = q[1::3]
            ro, ra = q[0::3], memoryview(blob).cast("q")[2::3]
            in_order = True
            prev = -1
            for i in range(count):
                if infos[i] & 0xFFFFFFFF == R_X86_64_RELATIVE:
                    o = ro[i]
                    if o < prev:
                        in_order = False
                    prev = o
                    offs.append(o)
                    adds.append(ra[i])
            if not in_order:                       # real images are sorted; synthetic ones need not be
                order = sorted(range(len(offs)), key=offs.__getitem__)
                offs = array("Q", (offs[i] for i in order))
                adds = array("q", (adds[i] for i in order))
        self._rela_cache = (offs, adds)
        return self._rela_cache

    def procparam(self) -> Optional[Dict[str, int]]:
        for s in self.segments:
            if s.p_type == PT_SCE_PROCPARAM and s.filesz >= 24:
                size, magic, count = struct.unpack_from("<QII", self.data, s.offset)
                if bytes(self.data[s.offset + 8:s.offset + 12]) != b"ORBI":
                    return None
                sdk = struct.unpack_from("<Q", self.data, s.offset + 16)[0]
                return {"size": size, "entry_count": count, "sdk_version_raw": sdk, "vaddr": s.vaddr}
        return None
