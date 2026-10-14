#!/usr/bin/env python3
"""elfimg.py - minimal, independent ELF64 image reader for the cap07-step2 pin pass.

Mac-only, read-only. Written fresh for this pass (does NOT import the earlier
phasea-tools/pak_vtable_pins.py, so the two derivations stay independent).

- program headers from the ELF header
- DT_RELA / DT_RELASZ discovered from PT_DYNAMIC (not hard-coded)
- R_X86_64_RELATIVE addends indexed by r_offset (pointer data words are zero
  in the raw file on this image class; real values live only in RELA)
- string search (ascii / utf16le), rip-relative reference scan over text
"""
import bisect
import hashlib
import struct

PT_LOAD = 1
PT_DYNAMIC = 2
DT_NULL, DT_RELA, DT_RELASZ = 0, 7, 8
R_X86_64_RELATIVE = 8


class Image:
    def __init__(self, path):
        with open(path, "rb") as f:
            self.blob = f.read()
        self.sha256 = hashlib.sha256(self.blob).hexdigest()
        if self.blob[:4] != b"\x7fELF":
            raise ValueError("not an ELF image")
        (e_phoff,) = struct.unpack_from("<Q", self.blob, 0x20)
        e_phentsize, e_phnum = struct.unpack_from("<HH", self.blob, 0x36)
        self.phdrs = []
        for i in range(e_phnum):
            t, fl, off, va, _pa, fsz, msz, al = struct.unpack_from(
                "<IIQQQQQQ", self.blob, e_phoff + i * e_phentsize)
            self.phdrs.append(dict(i=i, type=t, flags=fl, off=off, vaddr=va,
                                   filesz=fsz, memsz=msz, align=al))
        self._rela = None
        self._rela_keys = None

    # ---- address mapping -------------------------------------------------
    def v2o(self, va):
        for p in self.phdrs:
            if p["type"] == PT_LOAD and p["vaddr"] <= va < p["vaddr"] + p["filesz"]:
                return p["off"] + (va - p["vaddr"])
        return None

    def o2v(self, off):
        for p in self.phdrs:
            if p["type"] == PT_LOAD and p["off"] <= off < p["off"] + p["filesz"]:
                return p["vaddr"] + (off - p["off"])
        return None

    def seg(self, va):
        for p in self.phdrs:
            if p["type"] == PT_LOAD and p["vaddr"] <= va < p["vaddr"] + p["memsz"]:
                return p
        return None

    def is_exec(self, va):
        p = self.seg(va)
        return bool(p and (p["flags"] & 1))

    def text_segments(self):
        return [p for p in self.phdrs if p["type"] == PT_LOAD and (p["flags"] & 1)]

    def read(self, va, n):
        o = self.v2o(va)
        if o is None:
            return None
        return self.blob[o:o + n]

    def u32(self, va):
        b = self.read(va, 4)
        return None if b is None else struct.unpack("<I", b)[0]

    # ---- dynamic / relocations ----------------------------------------------
    def dynamic(self):
        out = {}
        for p in self.phdrs:
            if p["type"] != PT_DYNAMIC:
                continue
            base = p["off"]
            for k in range(p["filesz"] // 16):
                tag, val = struct.unpack_from("<qQ", self.blob, base + 16 * k)
                if tag == DT_NULL:
                    break
                out.setdefault(tag, val)
        return out

    def rela(self):
        if self._rela is not None:
            return self._rela
        dyn = self.dynamic()
        if DT_RELA not in dyn or DT_RELASZ not in dyn:
            raise ValueError("PT_DYNAMIC has no DT_RELA/DT_RELASZ")
        off = self.v2o(dyn[DT_RELA])
        if off is None:
            off = dyn[DT_RELA]  # some images store a file offset
        size = dyn[DT_RELASZ]
        rel = {}
        for k in range(size // 24):
            r_off, r_info, r_add = struct.unpack_from("<QQq", self.blob, off + 24 * k)
            if (r_info & 0xFFFFFFFF) == R_X86_64_RELATIVE:
                rel[r_off] = r_add
        self._rela = rel
        self._rela_keys = sorted(rel)
        self.rela_location = (dyn[DT_RELA], size)
        return rel

    def ptr(self, va):
        """Pointer-sized data word at va: RELA addend if relocated, else raw."""
        r = self.rela()
        if va in r:
            return r[va]
        b = self.read(va, 8)
        return None if b is None else struct.unpack("<Q", b)[0]

    # ---- searching ---------------------------------------------------------------
    def find_all(self, needle, limit=64):
        hits, start = [], 0
        while len(hits) < limit:
            i = self.blob.find(needle, start)
            if i < 0:
                break
            va = self.o2v(i)
            if va is not None:
                hits.append(va)
            start = i + 1
        return hits

    def rip_refs(self, target, kinds=(b"\x8d", b"\x8b")):
        """All rip-relative disp32 references (REX.W lea/mov [rip+disp]) to target.

        Pattern: 48|4c 8d|8b modrm(mod=00, rm=101) disp32; ref = next_ip + disp.
        """
        hits = []
        for p in self.text_segments():
            data = self.blob[p["off"]:p["off"] + p["filesz"]]
            base = p["vaddr"]
            for rex in (0x48, 0x4C):
                for op in kinds:
                    pat = bytes([rex]) + op
                    s = 0
                    while True:
                        i = data.find(pat, s)
                        if i < 0 or i + 7 > len(data):
                            break
                        modrm = data[i + 2]
                        if (modrm & 0xC7) == 0x05:
                            disp = struct.unpack_from("<i", data, i + 3)[0]
                            if base + i + 7 + disp == target:
                                hits.append(base + i)
                        s = i + 1
        return sorted(hits)
