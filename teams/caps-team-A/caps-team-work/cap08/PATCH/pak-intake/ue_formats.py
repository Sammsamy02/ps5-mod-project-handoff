#!/usr/bin/env python3
"""Bounded readers for the UE5 IoStore / Zen formats used by whole-chunk intake.

Mac-only, stdlib only, Python 3.9+. Everything here is ENGINE-level format
knowledge (UE5 IoStore TOC v8, container header v5, Zen package summary for
container-header v5 = UE 5.6, ScriptObjects). Nothing is game- or mod-specific.

Format sources (public, cited, not copied):
  * retoc (MIT) zen.rs / container_header.rs / name_map.rs, retained at
    .superpowers/native-dependency-spike/unreal-dependency-compatibility-1/source/retoc/
  * CUE4Parse (Apache-2.0) FExportMapEntry.cs / FPackageObjectIndex.cs /
    FIoContainerHeader.cs, same retained tree.
  * CityHash64 v1.1 and BLAKE3 public specifications (re-implemented here).
Every reader refuses (FormatError) on anything outside the admitted profile:
unknown versions, compression, encryption, truncation, bad offsets.
"""
import hashlib
import struct
from typing import Dict, List, Optional, Sequence, Tuple

M64 = (1 << 64) - 1
M32 = (1 << 32) - 1


class FormatError(ValueError):
    """Input is outside the admitted, verified format profile (fail closed)."""


def need(ok: bool, why: str) -> None:
    if not ok:
        raise FormatError(why)


# ---------------------------------------------------------------- CityHash64 v1.1
_K0 = 0xC3A5C85C97CB3127
_K1 = 0xB492B66FBE98F273
_K2 = 0x9AE16A3B2F90404F


def _f64(s: bytes, p: int) -> int:
    return struct.unpack_from("<Q", s, p)[0]


def _f32(s: bytes, p: int) -> int:
    return struct.unpack_from("<I", s, p)[0]


def _rot(v: int, sh: int) -> int:
    return v if sh == 0 else ((v >> sh) | (v << (64 - sh))) & M64


def _smix(v: int) -> int:
    return v ^ (v >> 47)


def _bswap(v: int) -> int:
    return int.from_bytes(v.to_bytes(8, "little"), "big")


def _hl16(u: int, v: int, mul: int = 0x9DDFEA08EB382D69) -> int:
    a = ((u ^ v) * mul) & M64
    a ^= a >> 47
    b = ((v ^ a) * mul) & M64
    b ^= b >> 47
    return (b * mul) & M64


def _hl0to16(s: bytes) -> int:
    n = len(s)
    if n >= 8:
        mul = (_K2 + n * 2) & M64
        a = (_f64(s, 0) + _K2) & M64
        b = _f64(s, n - 8)
        c = (_rot(b, 37) * mul + a) & M64
        d = ((_rot(a, 25) + b) * mul) & M64
        return _hl16(c, d, mul)
    if n >= 4:
        mul = (_K2 + n * 2) & M64
        a = _f32(s, 0)
        return _hl16((n + (a << 3)) & M64, _f32(s, n - 4), mul)
    if n > 0:
        a, b, c = s[0], s[n >> 1], s[n - 1]
        y = a + (b << 8)
        z = n + (c << 2)
        return (_smix(((y * _K2) ^ (z * _K0)) & M64) * _K2) & M64
    return _K2


def _hl17to32(s: bytes) -> int:
    n = len(s)
    mul = (_K2 + n * 2) & M64
    a = (_f64(s, 0) * _K1) & M64
    b = _f64(s, 8)
    c = (_f64(s, n - 8) * mul) & M64
    d = (_f64(s, n - 16) * _K2) & M64
    return _hl16((_rot((a + b) & M64, 43) + _rot(c, 30) + d) & M64,
                 (a + _rot((b + _K2) & M64, 18) + c) & M64, mul)


def _hl33to64(s: bytes) -> int:
    n = len(s)
    mul = (_K2 + n * 2) & M64
    a = (_f64(s, 0) * _K2) & M64
    b = _f64(s, 8)
    c = _f64(s, n - 24)
    d = _f64(s, n - 32)
    e = (_f64(s, 16) * _K2) & M64
    f = (_f64(s, 24) * 9) & M64
    g = _f64(s, n - 8)
    h = (_f64(s, n - 16) * mul) & M64
    u = (_rot((a + g) & M64, 43) + (_rot(b, 30) + c) * 9) & M64
    v = (((a + g) & M64) ^ d) + f + 1 & M64
    w = (_bswap(((u + v) * mul) & M64) + h) & M64
    x = (_rot((e + f) & M64, 42) + c) & M64
    y = ((_bswap(((v + w) * mul) & M64) + g) * mul) & M64
    z = (e + f + c) & M64
    a = (_bswap(((x + z) * mul + y) & M64) + b) & M64
    b = (_smix(((z + a) * mul + d + h) & M64) * mul) & M64
    return (b + x) & M64


def _weak32(s: bytes, p: int, a: int, b: int) -> Tuple[int, int]:
    w, x, y, z = _f64(s, p), _f64(s, p + 8), _f64(s, p + 16), _f64(s, p + 24)
    a = (a + w) & M64
    b = _rot((b + a + z) & M64, 21)
    c = a
    a = (a + x + y) & M64
    b = (b + _rot(a, 44)) & M64
    return (a + z) & M64, (b + c) & M64


def cityhash64(s: bytes) -> int:
    n = len(s)
    if n <= 16:
        return _hl0to16(s)
    if n <= 32:
        return _hl17to32(s)
    if n <= 64:
        return _hl33to64(s)
    x = _f64(s, n - 40)
    y = (_f64(s, n - 16) + _f64(s, n - 56)) & M64
    z = _hl16((_f64(s, n - 48) + n) & M64, _f64(s, n - 24))
    v = _weak32(s, n - 64, n, z)
    w = _weak32(s, n - 32, (y + _K1) & M64, x)
    x = (x * _K1 + _f64(s, 0)) & M64
    left = (n - 1) & ~63
    p = 0
    while True:
        x = (_rot((x + y + v[0] + _f64(s, p + 8)) & M64, 37) * _K1) & M64
        y = (_rot((y + v[1] + _f64(s, p + 48)) & M64, 42) * _K1) & M64
        x ^= w[1]
        y = (y + v[0] + _f64(s, p + 40)) & M64
        z = (_rot((z + w[0]) & M64, 33) * _K1) & M64
        v = _weak32(s, p, (v[1] * _K1) & M64, (x + w[0]) & M64)
        w = _weak32(s, p + 32, (z + w[1]) & M64, (y + _f64(s, p + 16)) & M64)
        z, x = x, z
        p += 64
        left -= 64
        if left == 0:
            break
    return _hl16((_hl16(v[0], w[0]) + _smix(y) * _K1 + z) & M64,
                 (_hl16(v[1], w[1]) + x) & M64)


def package_id_of(package_name: str) -> int:
    """FPackageId = CityHash64(lowercase(name) as UTF-16LE) (retoc lib.rs:743)."""
    return cityhash64(package_name.lower().encode("utf-16-le"))


# ---------------------------------------------------------------- BLAKE3 (IoHash)
_B3_IV = (0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
          0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19)
_B3_PERM = (2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8)
_CHUNK_START, _CHUNK_END, _PARENT, _ROOT = 1, 2, 4, 8


def _b3_schedules():
    order = list(range(16))
    out = []
    for _ in range(7):
        out.append(tuple(order))
        order = [order[_B3_PERM[i]] for i in range(16)]
    return tuple(out)


_B3_SCHED = _b3_schedules()


def _b3_compress(cv, m, counter, block_len, flags):
    s = [cv[0], cv[1], cv[2], cv[3], cv[4], cv[5], cv[6], cv[7],
         _B3_IV[0], _B3_IV[1], _B3_IV[2], _B3_IV[3],
         counter & M32, (counter >> 32) & M32, block_len, flags]
    for sch in _B3_SCHED:
        mm = [m[i] for i in sch]
        for (a, b, c, d, i) in ((0, 4, 8, 12, 0), (1, 5, 9, 13, 2), (2, 6, 10, 14, 4),
                                (3, 7, 11, 15, 6), (0, 5, 10, 15, 8), (1, 6, 11, 12, 10),
                                (2, 7, 8, 13, 12), (3, 4, 9, 14, 14)):
            sa = (s[a] + s[b] + mm[i]) & M32
            sd = s[d] ^ sa
            sd = ((sd >> 16) | (sd << 16)) & M32
            sc = (s[c] + sd) & M32
            sb = s[b] ^ sc
            sb = ((sb >> 12) | (sb << 20)) & M32
            sa = (sa + sb + mm[i + 1]) & M32
            sd ^= sa
            sd = ((sd >> 8) | (sd << 24)) & M32
            sc = (sc + sd) & M32
            sb ^= sc
            sb = ((sb >> 7) | (sb << 25)) & M32
            s[a], s[b], s[c], s[d] = sa, sb, sc, sd
    for i in range(8):
        s[i] ^= s[i + 8]
        s[i + 8] ^= cv[i]
    return s


def _b3_chunk_output(data: bytes, counter: int):
    """Returns (cv_or_None, last_block_words, last_len, last_flags, cv_in)."""
    cv = _B3_IV
    n = len(data)
    nblocks = max(1, (n + 63) // 64)
    for bi in range(nblocks):
        block = data[bi * 64:(bi + 1) * 64]
        blen = len(block)
        flags = 0
        if bi == 0:
            flags |= _CHUNK_START
        if bi == nblocks - 1:
            flags |= _CHUNK_END
            words = struct.unpack("<16I", block.ljust(64, b"\0"))
            return cv, words, blen, flags, counter
        words = struct.unpack("<16I", block)
        cv = tuple(_b3_compress(cv, words, counter, 64, flags)[:8])
    raise AssertionError("unreachable")


def blake3(data: bytes, out_len: int = 32) -> bytes:
    need(out_len <= 32, "blake3 helper supports <=32 output bytes")
    n = len(data)
    nchunks = max(1, (n + 1023) // 1024)
    stack: List[Tuple[int, ...]] = []
    final = None
    for ci in range(nchunks):
        chunk = data[ci * 1024:(ci + 1) * 1024]
        cv_in, words, blen, flags, counter = _b3_chunk_output(chunk, ci)
        if ci == nchunks - 1:
            final = (cv_in, words, counter, blen, flags)
            break
        cv = tuple(_b3_compress(cv_in, words, counter, blen, flags)[:8])
        total = ci + 1
        while total & 1 == 0:
            left = stack.pop()
            cv = tuple(_b3_compress(_B3_IV, left + cv, 0, 64, _PARENT)[:8])
            total >>= 1
        stack.append(cv)
    cv_in, words, counter, blen, flags = final
    if not stack:
        out = _b3_compress(cv_in, words, 0, blen, flags | _ROOT)
    else:
        cv = tuple(_b3_compress(cv_in, words, counter, blen, flags)[:8])
        while len(stack) > 1:
            left = stack.pop()
            cv = tuple(_b3_compress(_B3_IV, left + cv, 0, 64, _PARENT)[:8])
        left = stack.pop()
        out = _b3_compress(_B3_IV, left + cv, 0, 64, _PARENT | _ROOT)
    return struct.pack("<16I", *out)[:out_len]


def io_hash(data: bytes) -> bytes:
    """FIoHash = first 20 bytes of BLAKE3 (TOC v8 ReplaceIoChunkHashWithIoHash)."""
    return blake3(data, 20)


# ---------------------------------------------------------------- name batch
FNAME_HASH_ALGORITHM_ID = 0xC1640000


def read_name_batch(d: bytes, pos: int, end: int, cap: int = 1 << 20) -> Tuple[List[str], int]:
    need(pos + 4 <= end, "name batch truncated (count)")
    num = struct.unpack_from("<I", d, pos)[0]
    pos += 4
    if num == 0:
        return [], pos
    need(num <= cap, "name batch count over cap")
    need(pos + 12 <= end, "name batch truncated (header)")
    nbytes, algo = struct.unpack_from("<IQ", d, pos)
    pos += 12
    need(algo == FNAME_HASH_ALGORITHM_ID, "unknown name hash algorithm %x" % algo)
    need(pos + 10 * num <= end, "name batch truncated (hashes/lengths)")
    pos += 8 * num
    lens = [struct.unpack_from(">h", d, pos + 2 * i)[0] for i in range(num)]
    pos += 2 * num
    start = pos
    names = []
    for ln in lens:
        if ln < 0:  # UTF-16 (retoc name_map.rs:61)
            units = -32768 - ln
            units = -units if units < 0 else units
            k = units * 2
            need(pos + k <= end, "name string truncated")
            names.append(d[pos:pos + k].decode("utf-16-le"))
        else:
            k = ln
            need(pos + k <= end, "name string truncated")
            names.append(d[pos:pos + k].decode("latin-1"))
        pos += k
    need(pos - start == nbytes, "name batch byte count mismatch")
    return names, pos


# ---------------------------------------------------------------- UTOC
TOC_MAGIC = b"-==--==--==--==-"
TOC_VERSION_ADMITTED = 8  # ReplaceIoChunkHashWithIoHash
CHUNK_TYPES = {0: "Invalid", 1: "ExportBundleData", 2: "BulkData", 3: "OptionalBulkData",
               4: "MemoryMappedBulkData", 5: "ScriptObjects", 6: "ContainerHeader",
               7: "ExternalFile", 8: "ShaderCodeLibrary", 9: "ShaderCode",
               10: "PackageStoreEntry", 11: "DerivedData", 12: "EditorDerivedData",
               13: "PackageResource"}
CONTAINER_FLAG_COMPRESSED, CONTAINER_FLAG_ENCRYPTED = 1, 2
CONTAINER_FLAG_SIGNED, CONTAINER_FLAG_INDEXED = 4, 8


class Toc:
    def __init__(self):
        self.header: Dict[str, object] = {}
        self.chunk_ids: List[bytes] = []
        self.offsets: List[int] = []
        self.lengths: List[int] = []
        self.blocks: List[Tuple[int, int, int, int]] = []
        self.hashes: List[bytes] = []
        self.meta_flags: List[int] = []
        self.files: Dict[int, str] = {}  # toc row -> virtual path
        self.mount_point = ""
        self.row_of: Dict[bytes, int] = {}

    # chunk id helpers
    @staticmethod
    def chunk_type(cid: bytes) -> int:
        return cid[11]

    @staticmethod
    def chunk_object_id(cid: bytes) -> int:
        return struct.unpack_from("<Q", cid, 0)[0]

    def physical_ranges(self, row: int) -> List[Tuple[int, int]]:
        """(ucas offset, length) list for the stored blocks of one chunk."""
        bs = self.header["compression_block_size"]
        off, ln = self.offsets[row], self.lengths[row]
        need(ln > 0, "zero-length chunk")
        first, last = off // bs, (off + ln - 1) // bs
        need(last < len(self.blocks), "chunk block range outside block table")
        out = []
        for bi in range(first, last + 1):
            boff, csize, usize, method = self.blocks[bi]
            need(method == 0 and csize == usize, "compressed block refused")
            lo = max(off, bi * bs) - bi * bs
            hi = min(off + ln, bi * bs + usize) - bi * bs
            need(0 <= lo < hi <= usize, "chunk slice outside block")
            out.append((boff + lo, hi - lo))
        return out


def parse_toc(d: bytes) -> Toc:
    need(len(d) >= 144 and d[:16] == TOC_MAGIC, "not a UTOC (magic)")
    t = Toc()
    ver = d[16]
    need(ver == TOC_VERSION_ADMITTED, "UTOC version %d not admitted (only 8)" % ver)
    (hsize, n, nblocks, bentry, ncomp, complen, bsize, dirsize, nparts) = struct.unpack_from("<9I", d, 20)
    container_id = struct.unpack_from("<Q", d, 56)[0]
    key_guid = d[64:80]
    flags = d[80]
    nseeds = struct.unpack_from("<I", d, 84)[0]
    part_size = struct.unpack_from("<Q", d, 88)[0]
    nnoph = struct.unpack_from("<I", d, 96)[0]
    need(hsize == 144, "UTOC header size")
    need(bentry == 12, "block entry size")
    need(ncomp == 0 and complen == 32, "compression methods declared: refused (stored-only profile)")
    need(flags & (CONTAINER_FLAG_COMPRESSED | CONTAINER_FLAG_ENCRYPTED | CONTAINER_FLAG_SIGNED) == 0,
         "compressed/encrypted/signed container refused")
    need(nparts == 1, "multi-partition container refused")
    need(bsize > 0 and n <= 4_000_000 and nblocks <= 64_000_000, "UTOC counts out of bounds")
    t.header = dict(version=ver, entry_count=n, block_count=nblocks, compression_block_size=bsize,
                    directory_index_size=dirsize, container_id=container_id,
                    key_guid=key_guid.hex(), container_flags=flags, perfect_hash_seeds=nseeds,
                    chunks_without_perfect_hash=nnoph, partition_size=part_size)
    p = 144
    need(p + 12 * n + 10 * n <= len(d), "UTOC truncated (ids/offsets)")
    for i in range(n):
        cid = d[p + 12 * i:p + 12 * i + 12]
        t.chunk_ids.append(cid)
    p += 12 * n
    for i in range(n):
        b = d[p + 10 * i:p + 10 * i + 10]
        t.offsets.append(int.from_bytes(b[0:5], "big"))
        t.lengths.append(int.from_bytes(b[5:10], "big"))
    p += 10 * n
    p += 4 * nseeds + 4 * nnoph
    need(p + 12 * nblocks <= len(d), "UTOC truncated (blocks)")
    for i in range(nblocks):
        raw = d[p + 12 * i:p + 12 * i + 12]
        boff = int.from_bytes(raw[0:5], "little")
        csz = int.from_bytes(raw[5:8], "little")
        usz = int.from_bytes(raw[8:11], "little")
        t.blocks.append((boff, csz, usz, raw[11]))
    p += 12 * nblocks
    p += ncomp * complen
    dir_start = p
    if flags & CONTAINER_FLAG_INDEXED and dirsize:
        need(p + dirsize <= len(d), "UTOC truncated (directory index)")
        _parse_directory_index(d, p, p + dirsize, t)
    p = dir_start + dirsize
    need(p + 24 * n == len(d), "UTOC meta table size mismatch (%d vs %d)" % (p + 24 * n, len(d)))
    for i in range(n):
        t.hashes.append(d[p + 24 * i:p + 24 * i + 20])
        t.meta_flags.append(d[p + 24 * i + 20])
    for i, cid in enumerate(t.chunk_ids):
        need(cid not in t.row_of, "duplicate chunk id in TOC")
        t.row_of[cid] = i
    return t


def _fstring(d: bytes, p: int, end: int) -> Tuple[str, int]:
    need(p + 4 <= end, "FString truncated")
    ln = struct.unpack_from("<i", d, p)[0]
    p += 4
    if ln == 0:
        return "", p
    if ln < 0:
        k = -ln * 2
        need(p + k <= end, "FString truncated")
        return d[p:p + k - 2].decode("utf-16-le"), p + k
    need(p + ln <= end, "FString truncated")
    return d[p:p + ln - 1].decode("latin-1"), p + ln


def _parse_directory_index(d: bytes, p: int, end: int, t: Toc) -> None:
    mount, p = _fstring(d, p, end)
    nd = struct.unpack_from("<I", d, p)[0]
    p += 4
    need(p + 16 * nd <= end, "dir entries truncated")
    dirs = [struct.unpack_from("<4I", d, p + 16 * i) for i in range(nd)]
    p += 16 * nd
    nf = struct.unpack_from("<I", d, p)[0]
    p += 4
    need(p + 12 * nf <= end, "file entries truncated")
    files = [struct.unpack_from("<3I", d, p + 12 * i) for i in range(nf)]
    p += 12 * nf
    ns = struct.unpack_from("<I", d, p)[0]
    p += 4
    strings = []
    for _ in range(ns):
        s, p = _fstring(d, p, end)
        strings.append(s)
    need(p == end, "directory index size mismatch")
    t.mount_point = mount

    def name(i):
        return "" if i == M32 else strings[i]

    stack = [(0, "")] if nd else []
    seen = set()
    while stack:
        di, path = stack.pop()
        need(di < nd and di not in seen, "bad directory tree")
        seen.add(di)
        dname, child, sib, ff = dirs[di]
        here = path + (name(dname) + "/" if dname != M32 else "")
        fi = ff
        while fi != M32:
            need(fi < nf, "bad file entry")
            fname, nxt, row = files[fi]
            t.files[row] = here + name(fname)
            fi = nxt
        if sib != M32:
            stack.append((sib, path))
        if child != M32:
            stack.append((child, here))


class ChunkReader:
    """Reads chunks from a UCAS file through the TOC block table (stored-only)."""

    def __init__(self, toc: Toc, ucas_path: Optional[str] = None,
                 ranges: Optional[Dict[int, bytes]] = None):
        self.toc = toc
        self.path = ucas_path
        self.ranges = ranges or {}  # physical offset -> bytes (integrator-fetched)
        self.fh = open(ucas_path, "rb") if ucas_path else None

    def close(self):
        if self.fh:
            self.fh.close()

    def read_row(self, row: int, limit: Optional[int] = None) -> bytes:
        parts = []
        got = 0
        for off, ln in self.toc.physical_ranges(row):
            if limit is not None and got >= limit:
                break
            if self.fh:
                self.fh.seek(off)
                b = self.fh.read(ln)
                need(len(b) == ln, "UCAS truncated")
            else:
                b = self._from_ranges(off, ln)
            parts.append(b)
            got += ln
        data = b"".join(parts)
        return data if limit is None else data[:limit]

    def _from_ranges(self, off: int, ln: int) -> bytes:
        for base, blob in self.ranges.items():
            if base <= off and off + ln <= base + len(blob):
                return blob[off - base:off - base + ln]
        raise FormatError("physical range %d+%d not present in fetched ranges" % (off, ln))

    def read_verified(self, row: int) -> bytes:
        data = self.read_row(row)
        need(len(data) == self.toc.lengths[row], "chunk length mismatch")
        need(io_hash(data) == self.toc.hashes[row], "chunk IoHash mismatch (row %d)" % row)
        return data


# ---------------------------------------------------------------- container header v5
CONTAINER_HEADER_SIGNATURE = 0x496F436E
CONTAINER_HEADER_VERSION_ADMITTED = 5  # SoftPackageReferencesOffset (UE 5.6)


class ContainerHeader:
    def __init__(self):
        self.version = 0
        self.container_id = 0
        self.store: Dict[int, Dict[str, list]] = {}
        self.optional_store: Dict[int, Dict[str, list]] = {}


def _store_entries(d: bytes, p: int, end: int, ids: List[int]) -> Tuple[Dict[int, dict], int]:
    need(p + 4 <= end, "store entries size truncated")
    size = struct.unpack_from("<i", d, p)[0]
    p += 4
    base = p
    need(0 <= size and base + size <= end and size >= 16 * len(ids), "store entries buffer bounds")
    out = {}
    for i, pid in enumerate(ids):
        e = base + 16 * i
        res = {}
        for key, field, width in (("imported_packages", e, 8), ("shader_map_hashes", e + 8, 20)):
            cnt, rel = struct.unpack_from("<II", d, field)
            vals = []
            if cnt:
                at = field + rel
                need(base <= at and at + cnt * width <= base + size, "CArrayView outside store buffer")
                for k in range(cnt):
                    if width == 8:
                        vals.append(struct.unpack_from("<Q", d, at + 8 * k)[0])
                    else:
                        vals.append(d[at + 20 * k:at + 20 * k + 20].hex())
            res[key] = vals
        need(pid not in out, "duplicate store package id")
        out[pid] = res
    return out, base + size


def parse_container_header(d: bytes) -> ContainerHeader:
    need(len(d) >= 20, "container header truncated")
    sig, ver = struct.unpack_from("<II", d, 0)
    need(sig == CONTAINER_HEADER_SIGNATURE, "container header signature")
    need(ver == CONTAINER_HEADER_VERSION_ADMITTED, "container header version %d not admitted (only 5)" % ver)
    h = ContainerHeader()
    h.version = ver
    h.container_id = struct.unpack_from("<Q", d, 8)[0]
    p = 16
    end = len(d)
    for target in ("store", "optional_store"):
        need(p + 4 <= end, "package id array truncated")
        n = struct.unpack_from("<i", d, p)[0]
        p += 4
        need(0 <= n <= 4_000_000 and p + 8 * n <= end, "package id array bounds")
        ids = list(struct.unpack_from("<%dQ" % n, d, p))
        p += 8 * n
        entries, p = _store_entries(d, p, end, ids)
        setattr(h, target, entries)
    return h


# ---------------------------------------------------------------- object index
NULL_INDEX = M64
KIND_EXPORT, KIND_SCRIPT, KIND_PACKAGE = 0, 1, 2


def obj_kind(raw: int) -> str:
    if raw == NULL_INDEX:
        return "Null"
    k = raw >> 62
    need(k < 3, "invalid package object index type")
    return ("Export", "ScriptImport", "PackageImport")[k]


# ---------------------------------------------------------------- Zen package (UE 5.6)
class ZenPackage:
    def __init__(self):
        self.name = ""
        self.names: List[str] = []
        self.flags = 0
        self.header_size = 0
        self.imported_public_export_hashes: List[int] = []
        self.import_map: List[int] = []
        self.exports: List[Dict[str, object]] = []
        self.imported_package_names: List[str] = []
        self.bulk_data: List[Tuple[int, int, int, int]] = []


def parse_zen_package(d: bytes) -> ZenPackage:
    """Container-header-v5 (UE 5.6) unversioned Zen package header."""
    need(len(d) >= 64, "package shorter than summary")
    has_ver, hsize, nidx, nnum, flags, cooked = struct.unpack_from("<6I", d, 0)
    need(has_ver == 0, "versioned zen package refused (cooked games ship unversioned)")
    need(64 <= hsize <= len(d), "zen header size outside chunk")
    iph, imp, exp, bundle, deph, depe, ipn = struct.unpack_from("<7i", d, 24)
    cimp, cexp = struct.unpack_from("<2i", d, 52)
    order = [60, iph, imp, exp, cimp, cexp, bundle, deph, depe, ipn, hsize]
    need(order == sorted(order), "zen summary offsets not monotonic")
    need((imp - iph) % 8 == 0 and (exp - imp) % 8 == 0 and (cimp - exp) % 72 == 0,
         "zen import/export region misaligned")
    z = ZenPackage()
    z.flags = flags
    z.header_size = hsize
    z.names, p = read_name_batch(d, 60, iph, cap=65536)
    need(len(z.names) > 0, "empty package name map")
    # bulk data map (5.4+: u64 pad count + pad, then i64 size, entries 32 B)
    if p + 8 <= iph:
        padn = struct.unpack_from("<Q", d, p)[0]
        if padn < 16 and p + 8 + padn + 8 <= iph:
            q = p + 8 + padn
            bsz = struct.unpack_from("<q", d, q)[0]
            q += 8
            if 0 <= bsz and bsz % 32 == 0 and q + bsz == iph:
                for k in range(bsz // 32):
                    so, dso, ss, fl = struct.unpack_from("<qqqI", d, q + 32 * k)
                    z.bulk_data.append((so, dso, ss, fl))
    z.imported_public_export_hashes = list(struct.unpack_from("<%dQ" % ((imp - iph) // 8), d, iph))
    z.import_map = list(struct.unpack_from("<%dQ" % ((exp - imp) // 8), d, imp))
    for raw in z.import_map:
        obj_kind(raw)

    def mapped(index, number):
        need(index >> 30 == 0, "non-package mapped name")
        need(index < len(z.names), "mapped name index out of range")
        return z.names[index] + ("_%d" % (number - 1) if number else "")

    z.name = mapped(nidx, nnum)
    count = (cimp - exp) // 72
    need(0 < count <= 65536, "export count out of bounds")
    for i in range(count):
        at = exp + 72 * i
        (soff, ssize, oidx, onum, outer, cls, sup, tmpl, pub, oflags, ff) = struct.unpack_from("<QQIIQQQQQIB", d, at)
        for raw in (outer, cls, sup, tmpl):
            k = obj_kind(raw)
            if k == "Export":
                need((raw & M32) < count, "local export index out of bounds")
            if k == "PackageImport":
                need((raw & M32) < len(z.imported_public_export_hashes), "public hash index out of bounds")
        z.exports.append(dict(index=i, name=mapped(oidx, onum), serial_offset=soff, serial_size=ssize,
                              outer=outer, cls=cls, super=sup, template=tmpl, public_export_hash=pub,
                              object_flags=oflags, filter_flags=ff))
    nb = (deph - bundle) // 8 if deph > 0 else 0
    need(nb == 2 * count, "export bundle entries != 2 x exports")
    cursor = hsize
    serialized = set()
    for k in range(nb):
        li, cmd = struct.unpack_from("<II", d, bundle + 8 * k)
        need(li < count and cmd in (0, 1), "bad export bundle entry")
        if cmd == 1:
            need(li not in serialized, "export serialized twice")
            serialized.add(li)
            z.exports[li]["chunk_offset"] = cursor
            cursor += int(z.exports[li]["serial_size"])
    need(len(serialized) == count, "not every export has a Serialize command")
    z.export_payload_end = cursor
    if ipn > 0 and ipn < hsize:
        names, q = read_name_batch(d, ipn, hsize, cap=65536)
        need(q + 4 * len(names) <= hsize, "imported package name numbers truncated")
        nums = struct.unpack_from("<%di" % len(names), d, q) if names else ()
        z.imported_package_names = [n + ("_%d" % (k - 1) if k else "") for n, k in zip(names, nums)]
    for raw in z.import_map:
        if obj_kind(raw) == "PackageImport":
            pkg_i = (raw & ((1 << 62) - 1)) >> 32
            need(pkg_i < max(len(z.imported_package_names), 1 << 30), "imported package index")
            need((raw & M32) < len(z.imported_public_export_hashes), "imported public hash index")
    return z


def package_import_ref(raw: int) -> Tuple[int, int]:
    """(imported package index, imported public-export-hash index)."""
    return (raw & ((1 << 62) - 1)) >> 32, raw & M32


def export_data(d: bytes, z: ZenPackage, e: Dict[str, object]) -> bytes:
    """Export serial bytes. Zen chunks lay exports out right after the header in
    export-bundle Serialize order (retoc zen.rs export bundle handling)."""
    lo = int(e["chunk_offset"])
    hi = lo + int(e["serial_size"])
    need(z.header_size <= lo <= hi <= len(d), "export serial range outside chunk")
    return d[lo:hi]


# ---------------------------------------------------------------- ScriptObjects
class ScriptObjects:
    def __init__(self):
        self.entries: Dict[int, Tuple[str, int, int]] = {}  # global index -> (name, outer, cdo class)

    def path(self, raw: int) -> Optional[str]:
        parts = []
        cur = raw
        seen = set()
        while cur != NULL_INDEX:
            if cur not in self.entries or cur in seen or len(parts) > 64:
                return None
            seen.add(cur)
            name, outer, _ = self.entries[cur]
            parts.append(name)
            cur = outer
        parts.reverse()
        if len(parts) == 1:
            return parts[0]
        return parts[0] + "." + ":".join(parts[1:])


def parse_script_objects(d: bytes) -> ScriptObjects:
    names, p = read_name_batch(d, 0, len(d), cap=4_000_000)
    need(len(names) > 0, "script objects: empty global name batch")
    need(p + 4 <= len(d), "script objects: count truncated")
    n = struct.unpack_from("<i", d, p)[0]
    p += 4
    need(0 < n and p + 32 * n <= len(d), "script objects: entries out of bounds")
    so = ScriptObjects()
    for i in range(n):
        idx, num, g, outer, cdo = struct.unpack_from("<IIQQQ", d, p + 32 * i)
        need((idx & ((1 << 30) - 1)) < len(names), "script object name index")
        nm = names[idx & ((1 << 30) - 1)] + ("_%d" % (num - 1) if num else "")
        need(g not in so.entries, "duplicate script object index")
        so.entries[g] = (nm, outer, cdo)
    return so


def read_bytes(path: str) -> bytes:
    with open(path, "rb") as f:
        return f.read()


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()
