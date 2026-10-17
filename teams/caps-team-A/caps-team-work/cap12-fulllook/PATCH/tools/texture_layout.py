#!/usr/bin/env python3
"""Mod texture layout facts read from the mod's OWN texture bytes (Mac-only,
read-only, generic; no game, mod or asset names). Texture bytes are never changed.

plain_mips(pkg, bulk_sizes) -> proof that a cooked texture is a plain (non
  virtual-texture) mip chain, from two checks of the same texture:
    1 its export holds exactly one 'PF_*' pixel-format token followed by
      u32 0, u32 mip count, then per streamed mip (u32 index, u32 x, u32 y, u32 1)
      with x, y halving (layout read from the data; every row must fit),
    2 the size of its separate BulkData chunk equals EXACTLY the byte size of
      those streamed mips for that pixel format (4x4 block sizes below).
  A virtual texture stores tiles, not this mip chain, so both checks fail for it
  (fail closed: anything else is 'not proven').

cutout_alpha(...) -> whether the texture's alpha channel is a cut-out mask
  (used as opacity by a masked material), from its smallest streamed mip:
    1 the pixel format carries alpha (DXT5 or 8-bit BGRA/RGBA; DXT1/BC5 do not),
    2 decoded texels: at least CUT_CLEAR of them are clear (alpha < 32/255) AND at
      least CUT_SOLID are solid enough to survive a cut (alpha >= 96/255).
  This is a data heuristic (disclosed in every row that uses it), not a proof
  that the mod material wires alpha to opacity.
"""
import struct

BLOCK_BYTES = {'PF_DXT1': 8, 'PF_BC4': 8, 'PF_DXT3': 16, 'PF_DXT5': 16, 'PF_BC5': 16, 'PF_BC7': 16}
PIXEL_BYTES = {'PF_B8G8R8A8': 4, 'PF_R8G8B8A8': 4}
ALPHA_FORMATS = {'PF_DXT5', 'PF_B8G8R8A8', 'PF_R8G8B8A8'}
CUT_CLEAR = 0.10
CUT_SOLID = 0.05


def mip_bytes(fmt, x, y):
    if fmt in BLOCK_BYTES:
        return max(1, (x + 3) // 4) * max(1, (y + 3) // 4) * BLOCK_BYTES[fmt]
    if fmt in PIXEL_BYTES:
        return x * y * PIXEL_BYTES[fmt]
    return None


def streamed_mips(pkg):
    """-> (pixel format, [(x, y), ...]) or (None, why)."""
    if len(pkg.exports) < 1:
        return None, 'no export'
    blobs = [pkg.export_bytes(e) or b'' for e in pkg.exports]
    hits = [(k, b.find(b'PF_')) for k, b in enumerate(blobs) if b.count(b'PF_') == 1]
    if len(hits) != 1 or sum(b.count(b'PF_') for b in blobs) != 1:
        return None, 'not exactly one pixel-format token'
    k, i = hits[0]
    b = blobs[k]
    j = b.find(b'\x00', i)
    if j < 0:
        return None, 'unterminated pixel-format token'
    fmt = b[i:j].decode('ascii', 'replace')
    q = j + 1
    if q + 8 > len(b):
        return None, 'mip table truncated'
    zero, n = struct.unpack_from('<II', b, q)
    if zero != 0 or not 1 <= n <= 16:
        return None, 'unexpected mip table header'
    q += 8
    mips = []
    for idx in range(n):
        if q + 16 > len(b):
            break
        e = struct.unpack_from('<IIII', b, q)
        if e[0] != idx or e[3] != 1 or not (1 <= e[1] <= 16384 and 1 <= e[2] <= 16384):
            break
        if mips and (e[1] != max(1, mips[-1][0] // 2) or e[2] != max(1, mips[-1][1] // 2)):
            return None, 'mip sizes do not halve'
        mips.append((e[1], e[2]))
        q += 16
    if not mips:
        return None, 'no streamed mip rows'
    return fmt, mips


def plain_mips(pkg, bulk_sizes):
    """bulk_sizes: list of this package's BulkData chunk sizes (uncompressed)."""
    fmt, mips = streamed_mips(pkg)
    if fmt is None:
        return {'proven': False, 'why': mips}
    per = [mip_bytes(fmt, x, y) for x, y in mips]
    if None in per:
        return {'proven': False, 'why': f'pixel format {fmt} has no known block size'}
    if len(bulk_sizes) != 1:
        return {'proven': False, 'why': f'{len(bulk_sizes)} bulk-data chunks (need exactly 1)'}
    if sum(per) != bulk_sizes[0]:
        return {'proven': False, 'why': f'bulk chunk is {bulk_sizes[0]} bytes, streamed mips need {sum(per)}'}
    return {'proven': True, 'format': fmt, 'size': list(mips[0]), 'streamed_mips': len(mips),
            'sources': ['export mip table', 'bulk chunk size == mip-chain size']}


def _dxt5_alpha(block):
    a0, a1 = block[0], block[1]
    bits = int.from_bytes(block[2:8], 'little')
    pal = [a0, a1]
    if a0 > a1:
        pal += [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
    else:
        pal += [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)] + [0, 255]
    return [pal[(bits >> (3 * k)) & 7] for k in range(16)]


def alpha_values(fmt, data):
    if fmt == 'PF_DXT5':
        out = []
        for o in range(0, len(data) - 15, 16):
            out += _dxt5_alpha(data[o:o + 16])
        return out
    if fmt in PIXEL_BYTES:
        return list(data[3::4])
    return None


def cutout_alpha(pkg, bulk):
    """bulk: [bytes] of this package's BulkData chunks."""
    lay = plain_mips(pkg, [len(x) for x in bulk])
    if not lay['proven']:
        return {'cutout': False, 'why': 'layout not proven: ' + lay['why']}
    fmt = lay['format']
    if fmt not in ALPHA_FORMATS:
        return {'cutout': False, 'why': f'{fmt} has no alpha channel'}
    _f, mips = streamed_mips(pkg)
    per = [mip_bytes(fmt, x, y) for x, y in mips]
    last = bulk[0][sum(per[:-1]):]
    vals = alpha_values(fmt, last)
    if not vals:
        return {'cutout': False, 'why': 'no alpha texels decoded'}
    clear = sum(v < 32 for v in vals) / len(vals)
    solid = sum(v >= 96 for v in vals) / len(vals)
    ok = clear >= CUT_CLEAR and solid >= CUT_SOLID
    return {'cutout': ok, 'format': fmt, 'texels': len(vals), 'clear': round(clear, 3), 'solid': round(solid, 3),
            'sources': ['pixel format carries alpha', f'texels: clear {clear:.2f} >= {CUT_CLEAR}, '
                        f'solid {solid:.2f} >= {CUT_SOLID}'] if ok else None,
            'why': None if ok else f'alpha is not a cut-out (clear {clear:.2f}, solid {solid:.2f})'}


def mod_bulk_index(toc):
    """{package id: [row, ...]} of BulkData (type 2) chunks in a mod .utoc."""
    out = {}
    for i, cid in enumerate(toc.chunk_ids):
        if cid[11] == 2:
            out.setdefault(int.from_bytes(cid[:8], 'little'), []).append(i)
    return out
