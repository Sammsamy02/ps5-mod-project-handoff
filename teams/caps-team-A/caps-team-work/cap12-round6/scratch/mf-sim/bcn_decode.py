#!/usr/bin/env python3
"""Reference decoder for BC1 / BC2 / BC3 / BC7 blocks and 8-bit RGBA pixels
(Mac-only, read-only, generic). Written separately from bcn_swap.py (no shared
code) from the public Microsoft D3D10/11 block-compression specification; it is
one of the two independent decoders of the R<->B swap proof (the other is the
GPU: metal_bcn_check.swift). Its BC7 tables are cross-checked against the GPU
in the tests.

decode_block(fmt, block) -> 64 bytes: 16 texels x RGBA8, texel order row-major
inside the 4x4 block. decode(fmt, data) yields one 64-byte string per block
(per 4x4 group for the uncompressed formats: one texel per 'block').

swap_proof(fmt, before, after) -> facts: decodes EVERY block of both inputs
and checks after == before with R and B exchanged on every texel (all 16
texels of every block, i.e. also the padding texels of mips smaller than 4x4).
"""

BC7_P2 = [
    0xCCCC, 0x8888, 0xEEEE, 0xECC8, 0xC880, 0xFEEC, 0xFEC8, 0xEC80, 0xC800, 0xFFEC, 0xFE80, 0xE800,
    0xFFE8, 0xFF00, 0xFFF0, 0xF000, 0xF710, 0x008E, 0x7100, 0x08CE, 0x008C, 0x7310, 0x3100, 0x8CCE,
    0x088C, 0x3110, 0x6666, 0x366C, 0x17E8, 0x0FF0, 0x718E, 0x399C, 0xAAAA, 0xF0F0, 0x5A5A, 0x33CC,
    0x3C3C, 0x55AA, 0x9696, 0xA55A, 0x73CE, 0x13C8, 0x324C, 0x3BDC, 0x6996, 0xC33C, 0x9966, 0x0660,
    0x0272, 0x04E4, 0x4E40, 0x2720, 0xC936, 0x936C, 0x39C6, 0x639C, 0x9336, 0x9CC6, 0x817E, 0xE718,
    0xCCF0, 0x0FCC, 0x7744, 0xEE22]

BC7_P3 = [
    "0011001102212222", "0001001122112221", "0000200122112211", "0222002200110111",
    "0000000011221122", "0011001100220022", "0022002211111111", "0011001122112211",
    "0000000011112222", "0000111111112222", "0000111122222222", "0012001200120012",
    "0112011201120112", "0122012201220122", "0011011211221222", "0011200122002220",
    "0001001101121122", "0111001120012200", "0000112211221122", "0022002200221111",
    "0111011102220222", "0001000122212221", "0000001101220122", "0000110022102210",
    "0122012200110000", "0012001211222222", "0110122112210110", "0000011012211221",
    "0022110211020022", "0110011020022222", "0011012201220011", "0000200022112221",
    "0000000211221222", "0222002200120011", "0011001200220222", "0120012001200120",
    "0000111122220000", "0120120120120120", "0120201212010120", "0011220011220011",
    "0011112222000011", "0101010122222222", "0000000021212121", "0022112200221122",
    "0022001100220011", "0220122102201221", "0101222222220101", "0000212121212121",
    "0101010101012222", "0222011102220111", "0002111200021112", "0000211221122112",
    "0222011101110222", "0002111211120002", "0110011001102222", "0000000021122112",
    "0110011022222222", "0022001100110022", "0022112211220022", "0000000000002112",
    "0002000100020001", "0222122202221222", "0101222222222222", "0111201122012220"]

BC7_A2 = [15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
          15, 2, 8, 2, 2, 8, 8, 15, 2, 8, 2, 2, 8, 8, 2, 2,
          15, 15, 6, 8, 2, 8, 15, 15, 2, 8, 2, 2, 2, 15, 15, 6,
          6, 2, 6, 8, 15, 15, 2, 2, 15, 15, 15, 15, 15, 2, 2, 15]
BC7_A3B = [3, 3, 15, 15, 8, 3, 15, 15, 8, 8, 6, 6, 6, 5, 3, 3,
           3, 3, 8, 15, 3, 3, 6, 10, 5, 8, 8, 6, 8, 5, 15, 15,
           8, 15, 3, 5, 6, 10, 8, 15, 15, 3, 15, 5, 15, 15, 15, 15,
           3, 15, 5, 5, 5, 8, 5, 10, 5, 10, 8, 13, 15, 12, 3, 3]
BC7_A3C = [15, 8, 8, 3, 15, 15, 3, 8, 15, 15, 15, 15, 15, 15, 15, 8,
           15, 8, 15, 3, 15, 8, 15, 8, 3, 15, 6, 10, 15, 15, 10, 8,
           15, 3, 15, 10, 10, 8, 9, 10, 6, 15, 8, 15, 3, 6, 6, 8,
           15, 3, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 3, 15, 15, 8]
WEIGHTS = {2: (0, 21, 43, 64), 3: (0, 9, 18, 27, 37, 46, 55, 64),
           4: (0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64)}
# mode: subsets, partition bits, rotation bits, idx-select bits, colour bits, alpha bits,
#       per-endpoint p-bits, shared p-bits, index bits, second index bits
MODES = [(3, 4, 0, 0, 4, 0, 1, 0, 3, 0), (2, 6, 0, 0, 6, 0, 0, 1, 3, 0),
         (3, 6, 0, 0, 5, 0, 0, 0, 2, 0), (2, 6, 0, 0, 7, 0, 1, 0, 2, 0),
         (1, 0, 2, 1, 5, 6, 0, 0, 2, 3), (1, 0, 2, 0, 7, 8, 0, 0, 2, 2),
         (1, 0, 0, 0, 7, 7, 1, 0, 4, 0), (2, 6, 0, 0, 5, 5, 1, 0, 2, 0)]


class DecodeError(ValueError):
    pass


def _subset_of(ns, part, i):
    if ns == 1:
        return 0
    if ns == 2:
        return (BC7_P2[part] >> i) & 1
    return ord(BC7_P3[part][i]) - 48


def _anchors(ns, part):
    if ns == 1:
        return (0,)
    if ns == 2:
        return (0, BC7_A2[part])
    return (0, BC7_A3B[part], BC7_A3C[part])


def _expand(v, bits):
    v <<= 8 - bits
    return v | (v >> bits)


def _bc7(block):
    v = int.from_bytes(block, 'little')
    mode = None
    for m in range(8):
        if v >> m & 1:
            mode = m
            break
    if mode is None:
        return bytes(64)  # reserved: the spec decodes all channels to zero
    ns, pbits, rbits, isbits, cbits, abits, epb, spb, ib, ib2 = MODES[mode]
    pos = mode + 1

    def take(n):
        nonlocal pos
        x = (v >> pos) & ((1 << n) - 1)
        pos += n
        return x
    part = take(pbits)
    rot = take(rbits)
    isel = take(isbits)
    ne = 2 * ns
    ep = [[0, 0, 0, 255] for _ in range(ne)]
    for ch in range(3):
        for e in range(ne):
            ep[e][ch] = take(cbits)
    if abits:
        for e in range(ne):
            ep[e][3] = take(abits)
    pv = [0] * ne
    if epb:
        for e in range(ne):
            pv[e] = take(1)
    elif spb:
        for s in range(ns):
            b = take(1)
            pv[2 * s] = pv[2 * s + 1] = b
    total_c = cbits + (1 if (epb or spb) else 0)
    total_a = abits + (1 if (epb or spb) else 0) if abits else 0
    for e in range(ne):
        for ch in range(3):
            x = ep[e][ch]
            if epb or spb:
                x = (x << 1) | pv[e]
            ep[e][ch] = _expand(x, total_c)
        if abits:
            x = ep[e][3]
            if epb or spb:
                x = (x << 1) | pv[e]
            ep[e][3] = _expand(x, total_a)
    anc = _anchors(ns, part)
    sub = [_subset_of(ns, part, i) for i in range(16)]
    idx1 = []
    for i in range(16):
        n = ib - 1 if i in anc else ib
        idx1.append(take(n))
    idx2 = []
    if ib2:
        for i in range(16):
            n = ib2 - 1 if i == 0 else ib2
            idx2.append(take(n))
    if pos != 128:
        raise DecodeError(f'BC7 mode {mode}: {pos} bits consumed, need 128')
    out = bytearray(64)
    for i in range(16):
        s = sub[i]
        e0, e1 = ep[2 * s], ep[2 * s + 1]
        if ib2:
            if isel:
                ci, cw = idx2[i], WEIGHTS[ib2]
                ai, aw = idx1[i], WEIGHTS[ib]
            else:
                ci, cw = idx1[i], WEIGHTS[ib]
                ai, aw = idx2[i], WEIGHTS[ib2]
        else:
            ci, cw = idx1[i], WEIGHTS[ib]
            ai, aw = ci, cw
        px = [((64 - cw[ci]) * e0[c] + cw[ci] * e1[c] + 32) >> 6 for c in range(3)]
        px.append(((64 - aw[ai]) * e0[3] + aw[ai] * e1[3] + 32) >> 6)
        if rot:
            px[3], px[rot - 1] = px[rot - 1], px[3]
        out[4 * i:4 * i + 4] = bytes(px)
    return bytes(out)


def _rgb565(c):
    r, g, b = c >> 11, (c >> 5) & 63, c & 31
    return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))


def _colour(b, force_four):
    c0 = int.from_bytes(b[0:2], 'little')
    c1 = int.from_bytes(b[2:4], 'little')
    bits = int.from_bytes(b[4:8], 'little')
    p0, p1 = _rgb565(c0), _rgb565(c1)
    if force_four or c0 > c1:
        pal = [p0 + (255,), p1 + (255,),
               tuple((2 * p0[k] + p1[k]) // 3 for k in range(3)) + (255,),
               tuple((p0[k] + 2 * p1[k]) // 3 for k in range(3)) + (255,)]
    else:
        pal = [p0 + (255,), p1 + (255,), tuple((p0[k] + p1[k]) // 2 for k in range(3)) + (255,), (0, 0, 0, 0)]
    return [pal[(bits >> (2 * i)) & 3] for i in range(16)]


def _bc3_alpha(b):
    a0, a1 = b[0], b[1]
    bits = int.from_bytes(b[2:8], 'little')
    if a0 > a1:
        pal = [a0, a1] + [((7 - k) * a0 + k * a1) // 7 for k in range(1, 7)]
    else:
        pal = [a0, a1] + [((5 - k) * a0 + k * a1) // 5 for k in range(1, 5)] + [0, 255]
    return [pal[(bits >> (3 * i)) & 7] for i in range(16)]


def decode_block(fmt, block):
    if fmt == 'PF_DXT1':
        px = _colour(block, False)
        return b''.join(bytes(p) for p in px)
    if fmt == 'PF_DXT3':
        col = _colour(block[8:16], True)
        al = int.from_bytes(block[0:8], 'little')
        return b''.join(bytes(p[:3]) + bytes(((al >> (4 * i)) & 15) * 17 for _ in (0,)) for i, p in enumerate(col))
    if fmt == 'PF_DXT5':
        col = _colour(block[8:16], True)
        al = _bc3_alpha(block[0:8])
        return b''.join(bytes(p[:3]) + bytes((al[i],)) for i, p in enumerate(col))
    if fmt == 'PF_BC7':
        return _bc7(block)
    if fmt == 'PF_B8G8R8A8':
        return bytes((block[2], block[1], block[0], block[3]))
    if fmt == 'PF_R8G8B8A8':
        return bytes(block[0:4])
    raise DecodeError(f'no decoder for {fmt}')


BLOCK = {'PF_DXT1': 8, 'PF_DXT3': 16, 'PF_DXT5': 16, 'PF_BC7': 16, 'PF_B8G8R8A8': 4, 'PF_R8G8B8A8': 4}


def decode(fmt, data):
    bb = BLOCK.get(fmt)
    if bb is None:
        raise DecodeError(f'no decoder for {fmt}')
    if len(data) % bb:
        raise DecodeError('not a whole number of blocks')
    for o in range(0, len(data), bb):
        yield decode_block(fmt, data[o:o + bb])


def _rb_swapped(texels):
    t = bytearray(texels)
    t[0::4] = texels[2::4]
    t[2::4] = texels[0::4]
    return bytes(t)


def swap_proof(fmt, before, after):
    """Decode every block of both; after must equal before with R<->B on every texel."""
    bb = BLOCK.get(fmt)
    if bb is None:
        raise DecodeError(f'no decoder for {fmt}')
    if len(before) != len(after) or len(before) % bb or not before:
        raise DecodeError('inputs differ in size, are empty, or are not whole blocks')
    texels = 0
    differs = 0
    bad = 0
    first_bad = None
    seen = {}
    for o in range(0, len(before), bb):
        a = before[o:o + bb]
        b = after[o:o + bb]
        key = a + b
        res = seen.get(key)
        if res is None:
            da = decode_block(fmt, a)
            db = decode_block(fmt, b)
            res = (db == _rb_swapped(da), sum(1 for i in range(0, len(da), 4) if da[i] != da[i + 2]))
            if len(seen) < 1 << 20:
                seen[key] = res
        n = 16 if bb > 4 else 1
        texels += n
        differs += res[1]
        if not res[0]:
            bad += 1
            if first_bad is None:
                first_bad = o
    return {'decoder': 'bcn_decode.py (reference, D3D spec)', 'format': fmt, 'blocks': len(before) // bb,
            'texels_compared': texels, 'texels_where_r_differs_from_b': differs,
            'mismatched_blocks': bad, 'first_mismatch_offset': first_bad, 'exact': bad == 0 and texels > 0}
