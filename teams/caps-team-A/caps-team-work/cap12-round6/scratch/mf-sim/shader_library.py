#!/usr/bin/env python3
"""Read-only parser/validator for the UE5 IoStore shader code library manifest
(IoStore chunk type 8 'ShaderCodeLibrary', version 1).

Format facts from public retoc shader_library.rs:20-153,235-300,1029-1057 (MIT,
retained at outputs/2026-10-12-caps-prep-chatgpt/cap12/manifest-format-audit/
public-retoc-source/). Independent Python implementation; no Epic/Sony source.

Layout: u32 version (=1), then seven u32-counted arrays:
  shader_map_hashes[20B], shader_hashes[20B], shader_group_chunk_ids[12B],
  shader_map_entries{u32 indices_offset, u32 num_shaders},
  shader_entries{u64 packed: freq 4b | group index 30b | offset-in-group 30b},
  shader_group_entries{u32 indices_offset, u32 num_shaders, u32 uncompressed, u32 compressed},
  shader_indices[u32].
Type-9 group chunk id = SHA1(for each member in group order: shader_hash ||
u32le(member uncompressed size)) || format_name)[0:11] + type byte 9.

Nothing here decompresses or interprets shader code. Fails closed.
"""
import hashlib
import struct

MAX_COUNT = 4_000_000


class LibError(ValueError):
    pass


def need(ok, why):
    if not ok:
        raise LibError(why)


class Library:
    def __init__(self, data: bytes):
        self.size = len(data)
        need(len(data) >= 4 + 7 * 4, 'manifest too short')
        self.version = struct.unpack_from('<I', data, 0)[0]
        need(self.version == 1, f'unsupported shader library version {self.version}')
        p = 4

        def arr(width):
            nonlocal p
            need(p + 4 <= len(data), 'truncated array count')
            n = struct.unpack_from('<I', data, p)[0]; p += 4
            need(n <= MAX_COUNT, 'array count over cap')
            need(p + n * width <= len(data), 'truncated array body')
            out = [data[p + i * width:p + (i + 1) * width] for i in range(n)]
            p += n * width
            return out
        self.map_hashes = arr(20)
        self.shader_hashes = arr(20)
        self.group_ids = arr(12)
        self.map_entries = [struct.unpack('<II', e) for e in arr(8)]
        self.shader_entries = [struct.unpack('<Q', e)[0] for e in arr(8)]
        self.group_entries = [struct.unpack('<IIII', e) for e in arr(16)]
        self.indices = [struct.unpack('<I', e)[0] for e in arr(4)]
        self.trailing = len(data) - p
        need(self.trailing == 0, f'{self.trailing} unparsed trailing bytes')
        self._map_index = {h: i for i, h in enumerate(self.map_hashes)}

    # packed shader entry fields
    @staticmethod
    def freq(packed):
        return packed & 0xF

    @staticmethod
    def group_index(packed):
        return (packed >> 4) & ((1 << 30) - 1)

    @staticmethod
    def offset_in_group(packed):
        return (packed >> 34) & ((1 << 30) - 1)

    def validate(self):
        """Structural consistency checks. Returns dict of counts checked."""
        need(len(self.map_hashes) == len(self.map_entries), 'map hash/entry count differ')
        need(len(self.shader_hashes) == len(self.shader_entries), 'shader hash/entry count differ')
        need(len(self.group_ids) == len(self.group_entries), 'group id/entry count differ')
        need(len(set(self.map_hashes)) == len(self.map_hashes), 'duplicate shader map hash')
        need(len(set(self.shader_hashes)) == len(self.shader_hashes), 'duplicate shader hash')
        need(len(set(self.group_ids)) == len(self.group_ids), 'duplicate group id')
        ns, ni, ng = len(self.shader_entries), len(self.indices), len(self.group_entries)
        for off, cnt in self.map_entries:
            need(cnt > 0 and off + cnt <= ni, 'map index range out of bounds')
            for k in range(off, off + cnt):
                need(self.indices[k] < ns, 'map references missing shader')
        member_of = [None] * ns
        for gi, (off, cnt, usize, csize) in enumerate(self.group_entries):
            need(cnt > 0 and off + cnt <= ni, 'group index range out of bounds')
            need(0 < csize <= usize or csize == usize, 'bad group sizes')
            for k in range(off, off + cnt):
                si = self.indices[k]
                need(si < ns, 'group references missing shader')
                need(self.group_index(self.shader_entries[si]) == gi, 'shader/group membership mismatch')
                need(self.offset_in_group(self.shader_entries[si]) < usize, 'shader offset beyond group size')
                need(member_of[si] is None, 'shader in two groups')
                member_of[si] = gi
        need(all(m is not None for m in member_of), 'shader not in any group')
        for t in self.group_ids:
            need(t[11] & 0x3F == 9, 'group chunk id not type 9 (ShaderCode)')
        return {'shader_maps': len(self.map_hashes), 'shaders': ns, 'groups': ng, 'indices': ni}

    def group_members(self, gi):
        off, cnt, usize, _ = self.group_entries[gi]
        members = sorted(((self.offset_in_group(self.shader_entries[self.indices[k]]), self.indices[k])
                          for k in range(off, off + cnt)))
        out = []
        for j, (o, si) in enumerate(members):
            end = members[j + 1][0] if j + 1 < len(members) else usize
            need(end > o, 'non-positive member size')
            out.append((si, o, end - o))
        return out

    def group_id_for(self, gi, format_name: bytes):
        h = hashlib.sha1()
        for si, _o, size in self.group_members(gi):
            h.update(self.shader_hashes[si])
            h.update(struct.pack('<I', size))
        h.update(format_name)
        return h.digest()[:11] + bytes([9])

    def check_group_ids(self, format_candidates):
        """Recompute group chunk ids; return the candidate format string that
        reproduces ALL of them, else None (fail closed). Candidates are bytes."""
        for cand in format_candidates:
            if all(self.group_id_for(gi, cand)[:11] == self.group_ids[gi][:11]
                   for gi in range(len(self.group_entries))):
                return cand
        return None

    def shader_map(self, map_hash: bytes):
        """Shaders of one shader map: list of dicts, or None if the map is absent."""
        i = self._map_index.get(bytes(map_hash))
        if i is None:
            return None
        off, cnt = self.map_entries[i]
        out = []
        for k in range(off, off + cnt):
            si = self.indices[k]
            pk = self.shader_entries[si]
            gi = self.group_index(pk)
            out.append({'shader_index': si, 'shader_hash': self.shader_hashes[si].hex(),
                        'frequency': self.freq(pk), 'group_index': gi,
                        'group_chunk_id': self.group_ids[gi].hex(),
                        'offset_in_group': self.offset_in_group(pk),
                        'group_uncompressed': self.group_entries[gi][2],
                        'group_compressed': self.group_entries[gi][3]})
        return out


def build(maps, groups, format_name: bytes):
    """Synthetic writer for host tests only.

    maps:   list of (map_hash20, [shader indices])
    groups: list of [(shader_hash20, freq, size)] -- member order = layout order
    """
    shader_hashes, shader_entries, group_ids, group_entries, indices = [], [], [], [], []
    for gi, members in enumerate(groups):
        start = len(indices)
        off = 0
        h = hashlib.sha1()
        for sh, fq, size in members:
            si = len(shader_hashes)
            shader_hashes.append(sh)
            shader_entries.append((fq & 0xF) | (gi << 4) | (off << 34))
            indices.append(si)
            h.update(sh); h.update(struct.pack('<I', size))
            off += size
        h.update(format_name)
        group_ids.append(h.digest()[:11] + bytes([9]))
        group_entries.append((start, len(members), off, off))
    map_entries = []
    for _mh, sis in maps:
        map_entries.append((len(indices), len(sis)))
        indices.extend(sis)

    def arr(items, fmt=None):
        b = struct.pack('<I', len(items))
        for it in items:
            b += struct.pack(fmt, *it) if fmt else it
        return b
    return (struct.pack('<I', 1) + arr([m for m, _ in maps]) + arr(shader_hashes) + arr(group_ids)
            + arr(map_entries, '<II') + arr([(e,) for e in shader_entries], '<Q')
            + arr(group_entries, '<IIII') + arr([(i,) for i in indices], '<I'))
