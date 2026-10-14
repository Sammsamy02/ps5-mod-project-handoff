#!/usr/bin/env python3
"""Read-only UE5 IoStore container header (chunk type 6) store-entry reader.

Reads only: magic, version, container id, package ids, and per-package store
entries {ImportedPackages: u64[], ShaderMapHashes: 20B[]} (CArrayView = u32
count + u32 offset relative to the view itself). Public format facts: retained
CUE4Parse FFilePackageStoreEntry.cs:23-25 and retoc container_header.rs.
The later sections (optional/redirect/localized/soft refs) are not needed and
are not interpreted.
"""
import struct


class HeaderError(ValueError):
    pass


def need(ok, why):
    if not ok:
        raise HeaderError(why)


def parse(data: bytes):
    need(len(data) >= 24, 'container header too short')
    magic, version, cid = struct.unpack_from('<IIQ', data, 0)
    need(magic == 0x496F436E, f'bad container header magic {magic:08x}')  # 'nCoI'
    need(version == 5, f'unsupported container header version {version}')
    n = struct.unpack_from('<I', data, 16)[0]
    need(n <= 2_000_000 and 20 + 8 * n + 4 <= len(data), 'bad package count')
    ids = struct.unpack_from('<%dQ' % n, data, 20)
    p = 20 + 8 * n
    ssize = struct.unpack_from('<I', data, p)[0]; p += 4
    store = data[p:p + ssize]
    need(len(store) == ssize and ssize >= 16 * n, 'store truncated')
    out = {}
    for i in range(n):
        e = 16 * i
        ic, io, sc, so = struct.unpack_from('<4I', store, e)
        imports = struct.unpack_from('<%dQ' % ic, store, e + io) if ic else ()
        need(not ic or e + io + 8 * ic <= ssize, 'imports out of range')
        need(not sc or e + 8 + so + 20 * sc <= ssize, 'shader hashes out of range')
        hashes = tuple(store[e + 8 + so + 20 * k:e + 8 + so + 20 * (k + 1)] for k in range(sc))
        out[ids[i]] = {'imports': imports, 'shader_maps': hashes}
    return cid, out
