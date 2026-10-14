#!/usr/bin/env python3
"""Reference (oracle) for the LOSSLESS linear <-> console-tiled texture conversion (Mac-only).

This is the executable form of the spec in NOTES.md ("Conversion spec"). It is a reference for
Team A's implementation and for its tests, NOT the converter: it reads and writes bytes in
memory only and never touches packages, mod files or the console.

Layout rules (public AMD addrlib, GFX10, the S "standard swizzle" modes, no pipe/bank XOR):
  * elements: 4x4 blocks for BCn (DXT1/BC4 = 8 bytes, BC5/BC7/DXT3/DXT5 = 16 bytes), pixels otherwise;
    mip i of a stored chain is max(1, w0_el >> i) x max(1, h0_el >> i) elements, w0_el/h0_el being
    the FIRST STORED mip (SizeX >> FirstMipToSerialize, then /4 rounded up for BCn);
  * mode 1 SW_256B_S (micro-tiled: no mip tail), 5 SW_4KB_S, 9 SW_64KB_S; tile = the element
    rectangle the mode's equation covers (texture_tiling_probe.S_EQ, checked against the public
    header for every pipe configuration);
  * a mip above the tail is stored as its own surface: pitch and height rounded up to whole tiles,
    tiles row-major, inside a tile the element at (x, y) goes to the equation's offset; padding = 0;
  * 4 KB / 64 KB modes: the first mip whose size fits (tile_w/2, tile_h) with <= maxMipsInTail
    mips left starts the tail; ALL tail mips go into ONE tile-sized block at the public addrlib
    origins (texture_tiling_probe.tail_layout); the later tail mips store 0 bytes; unused = 0.
Linear (PC) layout: elements row-major, unpadded, one buffer per mip.
"""
import sys

sys.dont_write_bytecode = True
import os  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import texture_tiling_probe as tp  # noqa: E402

PIXEL_BYTES = {'PF_B8G8R8A8': 4, 'PF_R8G8B8A8': 4, 'PF_G8': 1, 'PF_G16': 2, 'PF_FloatRGBA': 8,
               'PF_A32B32G32R32F': 16, 'PF_R16F': 2, 'PF_R32_FLOAT': 4}
BLK = {1: 8, 5: 12, 9: 16}


class TileError(ValueError):
    pass


def elements(fmt, sx, sy, first_mip=0):
    """-> (bytes per element, w0_el, h0_el) of the first STORED mip."""
    w, h = max(1, sx >> first_mip), max(1, sy >> first_mip)
    if fmt in tp.BLOCK_BYTES:
        return tp.BLOCK_BYTES[fmt], max(1, -(-w // 4)), max(1, -(-h // 4))
    if fmt in PIXEL_BYTES:
        return PIXEL_BYTES[fmt], w, h
    raise TileError('format %s not covered' % fmt)


def _equation(mode, eb):
    """Public equation for any element size (1..16 bytes); probe has 8/16, others are derived by the
    tests from the public header and passed in via EXTRA_EQ when needed."""
    key = (mode, eb)
    if key in tp.S_EQ:
        return tp.equation(mode, eb)
    if key in EXTRA_EQ:
        return [(t[0], int(t[1:])) for t in EXTRA_EQ[key].split()]
    raise TileError('no public equation loaded for mode %d, %d-byte elements' % key)


EXTRA_EQ = {}   # (mode, eb) -> 'X0 Y0 ...' for 1/2/4-byte elements (filled from the public header by callers)


def layout(fmt, sx, sy, first_mip, nmips, mode):
    """-> dict: eb, tile (w, h), blk_log2, first_tail (or None), tail origins, per-mip dims and
    stored byte sizes (exactly what the console cook stores)."""
    if mode not in BLK:
        raise TileError('mode %s not covered' % mode)
    eb, w0, h0 = elements(fmt, sx, sy, first_mip)
    eq = _equation(mode, eb)
    tw, th = tp.tile_dims(eq)
    blk = BLK[mode]
    if tw * th * eb != 1 << blk:
        raise TileError('equation does not cover one block')
    first, origins = (None, {})
    if blk >= 12:
        first, origins = _tail(eb, tw, th, blk, w0, h0, nmips)
    dims = [(max(1, w0 >> i), max(1, h0 >> i)) for i in range(nmips)]
    sizes = []
    for i, (w, h) in enumerate(dims):
        if first is not None and i >= first:
            sizes.append((1 << blk) if i == first else 0)
        else:
            sizes.append(-(-w // tw) * tw * (-(-h // th) * th) * eb)
    return {'eb': eb, 'eq': eq, 'tile': (tw, th), 'blk_log2': blk, 'first_tail': first, 'origins': origins,
            'dims': dims, 'sizes': sizes, 'num_mips_in_tail': (nmips - first) if first is not None else 0}


def _tail(eb, tw, th, blk, w0, h0, nmips):
    tail_w, tail_h, maxin = tw // 2, th, tp.max_mips_in_tail(blk)
    first = None
    for i in range(nmips):
        if max(1, w0 >> i) <= tail_w and max(1, h0 >> i) <= tail_h and nmips - i <= maxin:
            first = i
            break
    if first is None:
        return None, {}
    b256 = {1: (16, 16), 2: (16, 8), 4: (8, 8), 8: (8, 4), 16: (4, 4)}[eb]
    out = {}
    for i in range(first, nmips):
        m = maxin - 1 - (i - first)
        off = (16 << m) if m > 6 else (m << 8)
        mx = sum(((off >> (9 + 2 * k)) & 1) << k for k in range(6))
        my = sum(((off >> (8 + 2 * k)) & 1) << k for k in range(6))
        out[i] = (mx * b256[0], my * b256[1])
    return first, out


def _in_tile_table(eq, tw, th):
    tab = [0] * (tw * th)
    for y in range(th):
        for x in range(tw):
            a = 0
            for k, (ax, b) in enumerate(eq):
                a |= (((x if ax == 'X' else y) >> b) & 1) << k
            tab[y * tw + x] = a
    if sorted(tab) != list(range(tw * th)):
        raise TileError('equation is not a permutation of the tile')
    return tab


def _map(w, h, tw, th, tab, pitch_tiles, origin=(0, 0)):
    """element (x, y) of a mip -> element index in its tiled surface (or in the tail block)."""
    ox, oy = origin
    per = tw * th
    for y in range(h):
        yy = y + oy
        for x in range(w):
            xx = x + ox
            yield y * w + x, ((yy // th) * pitch_tiles + xx // tw) * per + tab[(yy % th) * tw + (xx % tw)]


def tile_chain(linear, fmt, sx, sy, first_mip, mode):
    """linear: [bytes per stored mip, PC layout] -> [bytes per stored mip, console layout]."""
    L = layout(fmt, sx, sy, first_mip, len(linear), mode)
    eb, (tw, th) = L['eb'], L['tile']
    tab = _in_tile_table(L['eq'], tw, th)
    out = []
    for i, (w, h) in enumerate(L['dims']):
        src = linear[i]
        if len(src) != w * h * eb:
            raise TileError('mip %d: linear size %d, expected %d' % (i, len(src), w * h * eb))
        first = L['first_tail']
        if first is not None and i > first:
            out.append(b'')
            continue
        if first is not None and i == first:
            dst = bytearray(1 << L['blk_log2'])
            for j in range(first, len(L['dims'])):
                wj, hj = L['dims'][j]
                s = linear[j]
                if len(s) != wj * hj * eb:
                    raise TileError('tail mip %d: linear size %d, expected %d' % (j, len(s), wj * hj * eb))
                for a, b in _map(wj, hj, tw, th, tab, 1, L['origins'][j]):
                    if (b + 1) * eb > len(dst):
                        raise TileError('tail mip %d does not fit the tail block' % j)
                    dst[b * eb:(b + 1) * eb] = s[a * eb:(a + 1) * eb]
            out.append(bytes(dst))
            continue
        pt = -(-w // tw)
        dst = bytearray(L['sizes'][i])
        for a, b in _map(w, h, tw, th, tab, pt):
            dst[b * eb:(b + 1) * eb] = src[a * eb:(a + 1) * eb]
        out.append(bytes(dst))
    return out


def detile_chain(stored, fmt, sx, sy, first_mip, mode):
    """stored: [bytes per stored mip, console layout] -> [bytes per stored mip, PC linear layout]."""
    L = layout(fmt, sx, sy, first_mip, len(stored), mode)
    eb, (tw, th) = L['eb'], L['tile']
    tab = _in_tile_table(L['eq'], tw, th)
    if [len(s) for s in stored] != L['sizes']:
        raise TileError('stored sizes %s != rule %s' % ([len(s) for s in stored], L['sizes']))
    out = []
    first = L['first_tail']
    for i, (w, h) in enumerate(L['dims']):
        if first is not None and i >= first:
            src, pt, org = stored[first], 1, L['origins'][i]
        else:
            src, pt, org = stored[i], -(-w // tw), (0, 0)
        dst = bytearray(w * h * eb)
        for a, b in _map(w, h, tw, th, tab, pt, org):
            dst[a * eb:(a + 1) * eb] = src[b * eb:(b + 1) * eb]
        out.append(bytes(dst))
    return out


def header_fields(packed_pc, mode, num_mips_in_tail):
    """PackedData and the optional block to write: bit 30 (HasOptData) set, other bits kept;
    OptData = u32 ExtData (= the public AddrSwizzleMode number), u32 NumMipsInTail."""
    if packed_pc & (1 << 30):
        raise TileError('texture already carries a layout block')
    return packed_pc | (1 << 30), mode.to_bytes(4, 'little') + num_mips_in_tail.to_bytes(4, 'little')
