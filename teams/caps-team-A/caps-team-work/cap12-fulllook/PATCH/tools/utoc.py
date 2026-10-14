#!/usr/bin/env python3
"""Minimal read-only UE5 IoStore .utoc (version 8) reader for Mac-only analysis.

Independent implementation of public format facts (retoc lib.rs / CUE4Parse
FIoStoreTocHeader, both retained locally). Fails closed on anything it does not
understand: wrong magic, unsupported version, encryption, signatures, size
mismatch. Never writes anything.
"""
import struct

MAGIC = b'-==--==--==--==-'
TYPE_NAMES = {0: 'Invalid', 1: 'ExportBundleData', 2: 'BulkData', 3: 'OptionalBulkData',
              4: 'MemoryMappedBulkData', 5: 'ScriptObjects', 6: 'ContainerHeader',
              7: 'ExternalFile', 8: 'ShaderCodeLibrary', 9: 'ShaderCode',
              10: 'PackageStoreEntry', 11: 'DerivedData', 12: 'EditorDerivedData',
              13: 'PackageResource'}
FLAG_COMPRESSED, FLAG_ENCRYPTED, FLAG_SIGNED, FLAG_INDEXED = 1, 2, 4, 8


class TocError(ValueError):
    pass


def need(ok, why):
    if not ok:
        raise TocError(why)


def _fstring(b, pos):
    n = struct.unpack_from('<i', b, pos)[0]
    pos += 4
    if n == 0:
        return '', pos
    if n < 0:
        raw = b[pos:pos - 2 * n]
        need(len(raw) == -2 * n, 'truncated utf16 FString')
        return raw.decode('utf-16-le').rstrip('\0'), pos - 2 * n
    raw = b[pos:pos + n]
    need(len(raw) == n, 'truncated FString')
    return raw.decode('utf-8', 'replace').rstrip('\0'), pos + n


class Toc:
    def __init__(self, data: bytes):
        need(len(data) >= 144, 'file shorter than TOC header')
        need(data[:16] == MAGIC, 'bad TOC magic')
        (self.version, _r0, _r1, hdr_size, self.entry_count, self.block_count, block_entry_size,
         self.method_count, self.method_len, self.block_size, self.dir_size, self.partition_count,
         self.container_id) = struct.unpack_from('<BBHIIIIIIIIIQ', data, 16)
        need(self.version == 8, f'unsupported TOC version {self.version}')
        need(hdr_size == 144 and block_entry_size == 12, 'unexpected header/block entry size')
        self.flags = data[80]
        self.seed_count, self.partition_size, self.nophash_count = struct.unpack_from("<IQI", data, 84)
        need(not self.flags & FLAG_ENCRYPTED, 'encrypted TOC unsupported')
        need(not self.flags & FLAG_SIGNED, 'signed TOC unsupported')
        need(self.entry_count <= 4_000_000 and self.block_count <= 16_000_000, 'implausible counts')
        p = 144
        ids = data[p:p + 12 * self.entry_count]; p += 12 * self.entry_count
        ol = data[p:p + 10 * self.entry_count]; p += 10 * self.entry_count
        p += 4 * self.seed_count + 4 * self.nophash_count
        blocks_at = p; p += 12 * self.block_count
        names = data[p:p + self.method_count * self.method_len]; p += self.method_count * self.method_len
        self.methods = ['None'] + [names[i * self.method_len:(i + 1) * self.method_len].split(b'\0')[0].decode()
                                   for i in range(self.method_count)]
        dir_at = p; p += self.dir_size
        meta_at = p; p += 24 * self.entry_count
        need(p == len(data), f'TOC size mismatch: computed {p}, file {len(data)}')
        self._data = data
        self._blocks_at = blocks_at
        self.chunk_ids = [ids[12 * i:12 * i + 12] for i in range(self.entry_count)]
        self.offlen = []
        for i in range(self.entry_count):
            e = ol[10 * i:10 * i + 10]
            self.offlen.append((int.from_bytes(e[0:5], 'big'), int.from_bytes(e[5:10], 'big')))
        self.metas = [(data[meta_at + 24 * i:meta_at + 24 * i + 20].hex(), data[meta_at + 24 * i + 20])
                      for i in range(self.entry_count)]
        self.files = self._directory(data[dir_at:dir_at + self.dir_size]) if self.dir_size else {}

    def block(self, i):
        need(0 <= i < self.block_count, 'block index out of range')
        e = self._data[self._blocks_at + 12 * i:self._blocks_at + 12 * i + 12]
        off = int.from_bytes(e[0:5], 'little')
        csize = int.from_bytes(e[5:8], 'little')
        usize = int.from_bytes(e[8:11], 'little')
        return off, csize, usize, e[11]

    @staticmethod
    def chunk_type(cid: bytes) -> int:
        return cid[11] & 0x3f

    def rows_of_type(self, t):
        return [i for i, c in enumerate(self.chunk_ids) if self.chunk_type(c) == t]

    def physical_ranges(self, row):
        """Physical (offset, length) pieces for a chunk; only uncompressed blocks supported."""
        off, length = self.offlen[row]
        if length == 0:
            return []
        first, last = off // self.block_size, (off + length - 1) // self.block_size
        out = []
        for bi in range(first, last + 1):
            boff, csize, usize, method = self.block(bi)
            need(method == 0 and csize == usize, 'compressed block: not supported by this reader')
            lo = max(off, bi * self.block_size) - bi * self.block_size
            hi = min(off + length, bi * self.block_size + usize) - bi * self.block_size
            need(0 <= lo < hi <= usize, 'chunk range outside block')
            out.append((boff + lo, hi - lo))
        return out

    def read_chunk(self, row, ucas_reader):
        """ucas_reader(offset, length) -> bytes."""
        return b''.join(ucas_reader(o, n) for o, n in self.physical_ranges(row))

    def _directory(self, d):
        mount, p = _fstring(d, 0)
        n = struct.unpack_from('<I', d, p)[0]; p += 4
        dirs = [struct.unpack_from('<4I', d, p + 16 * i) for i in range(n)]; p += 16 * n
        n = struct.unpack_from('<I', d, p)[0]; p += 4
        files = [struct.unpack_from('<3I', d, p + 12 * i) for i in range(n)]; p += 12 * n
        n = struct.unpack_from('<I', d, p)[0]; p += 4
        strings = []
        for _ in range(n):
            s, p = _fstring(d, p)
            strings.append(s)
        need(p == len(d), 'directory index not fully consumed')
        NONE = 0xffffffff
        out = {}

        def walk(di, prefix, depth):
            need(depth < 256, 'directory nesting too deep')
            name, child, _sib, ffile = dirs[di]
            path = prefix if name == NONE else prefix + strings[name] + '/'
            f = ffile
            while f != NONE:
                fname, nxt, user = files[f]
                out[user] = path + strings[fname]
                f = nxt
            c = child
            while c != NONE:
                walk(c, path, depth + 1)
                c = dirs[c][2]
        if dirs:
            walk(0, mount, 0)
        return out


def load(path):
    with open(path, 'rb') as f:
        return Toc(f.read())
