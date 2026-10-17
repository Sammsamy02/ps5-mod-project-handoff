"""elf64.py - small read-only ELF64 little-endian x86-64 reader (cap05, Mac-only).

Only what the crash-report tools need: program headers, section headers,
symbol tables, RELA relocations and GNU build-id notes. All reads are bounds
checked; any malformed structure raises ElfError (callers fail closed).
No Epic or Sony material; ELF layout is the public System V gABI.
"""
import mmap
import struct


class ElfError(Exception):
    pass


PT_LOAD = 1
PT_NOTE = 4
PF_X = 1
SHT_SYMTAB = 2
SHT_RELA = 4
SHT_NOBITS = 8
SHF_WRITE = 0x1
SHF_ALLOC = 0x2
SHF_EXECINSTR = 0x4
SHF_TLS = 0x400
NT_GNU_BUILD_ID = 3


class Elf:
    def __init__(self, path):
        self.path = str(path)
        self._f = open(self.path, "rb")
        try:
            self.b = mmap.mmap(self._f.fileno(), 0, access=mmap.ACCESS_READ)
        except ValueError as e:
            self._f.close()
            raise ElfError("empty or unmappable file %s: %s" % (path, e))
        b = self.b
        if len(b) < 64 or b[:4] != b"\x7fELF":
            raise ElfError("%s: not an ELF file (packaged SELF? use the bare ELF)" % path)
        if b[4] != 2 or b[5] != 1:
            raise ElfError("%s: not ELF64 little-endian" % path)
        (self.e_type, self.e_machine) = struct.unpack_from("<HH", b, 16)
        if self.e_machine != 62:
            raise ElfError("%s: not x86-64 (e_machine=%d)" % (path, self.e_machine))
        self.e_phoff, self.e_shoff = struct.unpack_from("<QQ", b, 0x20)
        (self.e_phentsize, self.e_phnum, self.e_shentsize, self.e_shnum,
         self.e_shstrndx) = struct.unpack_from("<HHHHH", b, 0x36)
        self._phdrs = None
        self._sections = None

    def close(self):
        try:
            self.b.close()
        finally:
            self._f.close()

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self.close()

    def _need(self, off, n):
        if off < 0 or n < 0 or off + n > len(self.b):
            raise ElfError("%s: read [0x%x,+0x%x) outside file (0x%x bytes)"
                           % (self.path, off, n, len(self.b)))

    def read(self, off, n):
        self._need(off, n)
        return self.b[off:off + n]

    # ---------------------------------------------------------- segments --
    def phdrs(self):
        if self._phdrs is None:
            out = []
            if self.e_phnum and self.e_phentsize != 56:
                raise ElfError("%s: unexpected phentsize %d" % (self.path, self.e_phentsize))
            self._need(self.e_phoff, self.e_phnum * 56)
            for i in range(self.e_phnum):
                (t, fl, off, va, pa, fs, ms, al) = struct.unpack_from(
                    "<IIQQQQQQ", self.b, self.e_phoff + i * 56)
                out.append({"index": i, "type": t, "flags": fl, "offset": off,
                            "vaddr": va, "filesz": fs, "memsz": ms, "align": al})
            self._phdrs = out
        return self._phdrs

    def exec_loads(self):
        return [p for p in self.phdrs() if p["type"] == PT_LOAD and (p["flags"] & PF_X)]

    def vaddr_to_offset(self, va, n=1):
        for p in self.phdrs():
            if p["type"] == PT_LOAD and p["vaddr"] <= va and va + n <= p["vaddr"] + p["filesz"]:
                return p["offset"] + (va - p["vaddr"])
        return None

    def read_va(self, va, n):
        off = self.vaddr_to_offset(va, n)
        if off is None:
            return None
        return self.read(off, n)

    # ---------------------------------------------------------- sections --
    def sections(self):
        if self._sections is None:
            if self.e_shoff == 0:
                self._sections = []
                return self._sections
            if self.e_shentsize != 64:
                raise ElfError("%s: unexpected shentsize %d" % (self.path, self.e_shentsize))
            self._need(self.e_shoff, 64)
            sec0 = struct.unpack_from("<IIQQQQIIQQ", self.b, self.e_shoff)
            shnum = self.e_shnum
            shstrndx = self.e_shstrndx
            # gABI extended numbering: e_shnum==0 -> count in sec0.sh_size;
            # e_shstrndx==SHN_XINDEX -> index in sec0.sh_link.
            if shnum == 0:
                shnum = sec0[5]
            if shstrndx == 0xFFFF:
                shstrndx = sec0[6]
            if shnum == 0:
                self._sections = []
                return self._sections
            self._need(self.e_shoff, shnum * 64)
            raw = []
            for i in range(shnum):
                raw.append(struct.unpack_from("<IIQQQQIIQQ", self.b, self.e_shoff + i * 64))
            self.e_shstrndx = shstrndx
            if self.e_shstrndx >= len(raw):
                raise ElfError("%s: bad shstrndx" % self.path)
            stro = raw[self.e_shstrndx][4]
            strsz = raw[self.e_shstrndx][5]
            self._need(stro, strsz)
            out = []
            for i, (n, t, fl, ad, of, sz, lk, inf, al, es) in enumerate(raw):
                out.append({"index": i, "name": self._cstr(stro, n, strsz), "type": t,
                            "flags": fl, "addr": ad, "offset": of, "size": sz,
                            "link": lk, "info": inf, "align": al or 1, "entsize": es})
            self._sections = out
        return self._sections

    def _cstr(self, base, off, limit=None):
        if limit is not None and off >= limit:
            raise ElfError("%s: string offset 0x%x beyond table" % (self.path, off))
        start = base + off
        end = self.b.find(b"\0", start)
        if end < 0:
            raise ElfError("%s: unterminated string" % self.path)
        return self.b[start:end].decode("utf-8", "replace")

    def symbols(self, kind=SHT_SYMTAB):
        """All symbols of the first section of type `kind` (SHT_SYMTAB or SHT_DYNSYM=11)."""
        secs = self.sections()
        for s in secs:
            if s["type"] == kind:
                if s["link"] >= len(secs):
                    raise ElfError("%s: symtab link out of range" % self.path)
                st = secs[s["link"]]
                self._need(s["offset"], s["size"])
                self._need(st["offset"], st["size"])
                cnt = s["size"] // 24
                xidx = None
                for x in secs:
                    if x["type"] == 18 and x["link"] == s["index"]:   # SHT_SYMTAB_SHNDX
                        self._need(x["offset"], cnt * 4)
                        xidx = x["offset"]
                out = []
                for i in range(cnt):
                    (nm, info, other, shndx, value, size) = struct.unpack_from(
                        "<IBBHQQ", self.b, s["offset"] + i * 24)
                    if shndx == 0xFFFF and xidx is not None:
                        shndx = struct.unpack_from("<I", self.b, xidx + i * 4)[0]
                    out.append({"index": i, "name": self._cstr(st["offset"], nm, st["size"]),
                                "type": info & 0xF, "bind": info >> 4, "shndx": shndx,
                                "value": value, "size": size})
                return out
        return []

    def relas_by_target(self):
        """{target_section_index: [(r_offset, r_type), ...]} for every SHT_RELA."""
        out = {}
        for s in self.sections():
            if s["type"] != SHT_RELA:
                continue
            self._need(s["offset"], s["size"])
            lst = out.setdefault(s["info"], [])
            for i in range(s["size"] // 24):
                (ro, rinfo, _add) = struct.unpack_from("<QQq", self.b, s["offset"] + i * 24)
                lst.append((ro, rinfo & 0xFFFFFFFF))
        return out

    # -------------------------------------------------------------- notes --
    def gnu_build_id(self):
        """Hex GNU build-id from any PT_NOTE (falls back to section notes)."""
        spans = [(p["offset"], p["filesz"]) for p in self.phdrs() if p["type"] == PT_NOTE]
        if not spans:
            spans = [(s["offset"], s["size"]) for s in self.sections() if s["type"] == 7]
        for off, size in spans:
            if size == 0 or off + size > len(self.b):
                continue
            pos = off
            end = off + size
            while pos + 12 <= end:
                namesz, descsz, ntype = struct.unpack_from("<III", self.b, pos)
                name_end = pos + 12 + ((namesz + 3) & ~3)
                desc_end = name_end + ((descsz + 3) & ~3)
                if desc_end > end:
                    break
                name = self.b[pos + 12:pos + 12 + namesz].rstrip(b"\0")
                if ntype == NT_GNU_BUILD_ID and name == b"GNU":
                    return self.b[name_end:name_end + descsz].hex()
                pos = desc_end
        return None
