#!/usr/bin/env python3
"""Texture tiling probe (Mac-only, read-only, generic; no game, mod or asset names).

For every cooked Texture2D package it can read (local .utoc/.ucas pairs, or a .utoc plus a
folder of chunks the integrator fetched read-only with ftp_range_fetch.py) it prints:
  * the platform-data header fields: SizeX, SizeY, PackedData (bit 30 = HasOptData),
    pixel format, and when present the optional layout block {ExtData, NumMipsInTail};
  * which pixel layout the mip data really has, decided from the PIXELS: two consecutive
    mips are decoded (mean value per 4x4 block) under each layout hypothesis and the
    smaller mip is compared with a 2x2 downsample of the larger one. The right layout
    gives a near-perfect match; a wrong one gives noise.
Hypotheses: LINEAR (row-major, unpadded) and the public AMD GFX10 "standard swizzle"
modes 256B_S / 4KB_S / 64KB_S (AddrSwizzleMode 1 / 5 / 9). Their address equations and the
mip-tail placement are the public Mesa addrlib ones (gfx10SwizzlePattern.h 1-pipe rows: the S
modes have no pipe/bank XOR, the same for every hardware config; gfx10addrlib.cpp mip tail).
Nothing here uses Sony or Epic material. Texture bytes are only read.

Exit 1 when zero textures were checked.
"""
import argparse
import collections
import json
import os
import struct
import sys

sys.dont_write_bytecode = True

# address bits from the element-size bit upward (public addrlib GFX10_SW_*_S, 1 pipe; identical for all pipes)
S_EQ = {
    (1, 8): 'X0 Y0 Y1 X1 X2', (1, 16): 'Y0 Y1 X0 X1',
    (5, 8): 'X0 Y0 Y1 X1 X2 Y2 X3 Y3 X4', (5, 16): 'Y0 Y1 X0 X1 Y2 X2 Y3 X3',
    (9, 8): 'X0 Y0 Y1 X1 X2 Y2 X3 Y3 X4 Y4 X5 Y5 X6', (9, 16): 'Y0 Y1 X0 X1 Y2 X2 Y3 X3 Y4 X4 Y5 X5',
}
BLOCK_LOG2 = {1: 8, 5: 12, 9: 16}
MODE_NAME = {0: 'LINEAR', 1: 'SW_256B_S', 5: 'SW_4KB_S', 9: 'SW_64KB_S'}
BLOCK_BYTES = {'PF_DXT1': 8, 'PF_BC4': 8, 'PF_DXT3': 16, 'PF_DXT5': 16, 'PF_BC5': 16, 'PF_BC6H': 16, 'PF_BC7': 16}
GOOD, MARGIN = 0.75, 0.3   # decided: best match >= GOOD and >= MARGIN above every other hypothesis


# ---------------------------------------------------------------- public addrlib rules
def equation(mode, eb):
    return [(t[0], int(t[1:])) for t in S_EQ[(mode, eb)].split()]


def tile_dims(eq):
    return 1 << sum(1 for a, _ in eq if a == 'X'), 1 << sum(1 for a, _ in eq if a == 'Y')


def element_index(eq, x, y, tw, th, pitch_tiles, eb, blk_log2):
    a = 0
    for k, (ax, b) in enumerate(eq):
        a |= (((x if ax == 'X' else y) >> b) & 1) << k
    tile = (y // th) * pitch_tiles + (x // tw)
    return (tile << (blk_log2 - (eb.bit_length() - 1))) + a


def max_mips_in_tail(blk_log2):
    return blk_log2 - 4 if blk_log2 > 11 else 1 + (1 << (blk_log2 - 9))


def tail_layout(mode, eb, w_el, h_el, nmips):
    """-> (first mip in tail or None, {mip: (x0, y0) element origin inside the tail block})."""
    if mode not in BLOCK_LOG2 or BLOCK_LOG2[mode] < 12:
        return None, {}
    blk = BLOCK_LOG2[mode]
    tw, th = tile_dims(equation(mode, eb))
    tail_w, tail_h = tw // 2, th          # GetMipTailDim, thin 2D: width halved
    maxin = max_mips_in_tail(blk)
    first = None
    for i in range(nmips):
        mw, mh = max(1, w_el >> i), max(1, h_el >> i)
        if mw <= tail_w and mh <= tail_h and nmips - i <= maxin:
            first = i
            break
    if first is None:
        return None, {}
    b256 = {8: (8, 4), 16: (4, 4)}[eb]
    out = {}
    for i in range(first, nmips):
        m = maxin - 1 - (i - first)
        off = (16 << m) if m > 6 else (m << 8)
        mx = sum(((off >> (9 + 2 * k)) & 1) << k for k in range(6))
        my = sum(((off >> (8 + 2 * k)) & 1) << k for k in range(6))
        out[i] = (mx * b256[0], my * b256[1])
    return first, out


# ---------------------------------------------------------------- block decoders (mean value)
def _bc4_mean(b):
    a0, a1 = b[0], b[1]
    pal = [a0, a1] + ([((7 - i) * a0 + i * a1) / 7 for i in range(1, 7)] if a0 > a1 else
                      [((5 - i) * a0 + i * a1) / 5 for i in range(1, 5)] + [0, 255])
    bits = int.from_bytes(b[2:8], 'little')
    return sum(pal[(bits >> (3 * k)) & 7] for k in range(16)) / 16


def _rgb565(c):
    return ((c >> 11) & 31) * 8, ((c >> 5) & 63) * 4, (c & 31) * 8


def _bc1_mean(b):
    c0, c1 = struct.unpack_from('<HH', b, 0)
    p, q = _rgb565(c0), _rgb565(c1)
    return tuple((u + v) / 2 for u, v in zip(p, q))


_BC7 = [(3, 4, 0, 0, 4), (2, 6, 0, 0, 6), (3, 6, 0, 0, 5), (2, 6, 0, 0, 7),
        (1, 0, 2, 1, 5), (1, 0, 2, 0, 7), (1, 0, 0, 0, 7), (2, 6, 0, 0, 5)]


def _bc7_mean(b):
    v = int.from_bytes(b, 'little')
    mode = 0
    while mode < 8 and not (v >> mode) & 1:
        mode += 1
    if mode == 8:
        return (0, 0, 0)
    ns, pb, rb, isb, cb = _BC7[mode]
    p = mode + 1 + pb + rb + isb
    out = []
    for _c in range(3):
        s = 0
        for _k in range(2 * ns):
            s += ((v >> p) & ((1 << cb) - 1)) << (8 - cb)
            p += cb
        out.append(s / (2 * ns))
    return tuple(out)


def block_value(fmt):
    return {'PF_DXT1': _bc1_mean, 'PF_BC4': lambda b: (_bc4_mean(b),),
            'PF_BC5': lambda b: (_bc4_mean(b[:8]), _bc4_mean(b[8:16])),
            'PF_DXT5': lambda b: (_bc4_mean(b[:8]),) + _bc1_mean(b[8:16]),
            'PF_DXT3': lambda b: _bc1_mean(b[8:16]), 'PF_BC7': _bc7_mean}.get(fmt)


# ---------------------------------------------------------------- layout read + score
def read_grid(data, w_el, h_el, eb, mode, pitch_el=None, origin=(0, 0)):
    """Element grid of one mip under `mode` (0 = linear). None if the data is too short."""
    if mode == 0:
        if len(data) < w_el * h_el * eb:
            return None
        return [[data[(y * w_el + x) * eb:(y * w_el + x + 1) * eb] for x in range(w_el)] for y in range(h_el)]
    eq = equation(mode, eb)
    tw, th = tile_dims(eq)
    blk = BLOCK_LOG2[mode]
    pt = max(1, -(-(pitch_el or w_el) // tw))
    g = []
    for y in range(h_el):
        row = []
        for x in range(w_el):
            e = element_index(eq, x + origin[0], y + origin[1], tw, th, pt, eb, blk)
            if (e + 1) * eb > len(data):
                return None
            row.append(data[e * eb:(e + 1) * eb])
        g.append(row)
    return g


def _values(g, f):
    return [[f(c) for c in r] for r in g]


def _down(v):
    h, w = len(v) // 2, len(v[0]) // 2
    n = len(v[0][0])
    return [[tuple((v[2 * y][2 * x][c] + v[2 * y][2 * x + 1][c] + v[2 * y + 1][2 * x][c] + v[2 * y + 1][2 * x + 1][c]) / 4
                   for c in range(n)) for x in range(w)] for y in range(h)]


def corr(a, b):
    xs = [p for r in a for v in r for p in v]
    ys = [q for r in b for v in r for q in v]
    n = len(xs)
    if n < 8 or n != len(ys):
        return None
    mx, my = sum(xs) / n, sum(ys) / n
    sxy = sum((p - mx) * (q - my) for p, q in zip(xs, ys))
    sxx = sum((p - mx) ** 2 for p in xs)
    syy = sum((q - my) ** 2 for q in ys)
    return round(sxy / (sxx * syy) ** 0.5, 4) if sxx and syy else None


# ---------------------------------------------------------------- cooked header
def platform_header(x, names):
    """Texture platform-data fields from an export payload -> dict or None."""
    for pf in [n for n in names if n.startswith('PF_')]:
        needle = struct.pack('<i', len(pf) + 1) + pf.encode() + b'\0'
        at = x.find(needle)
        if at < 12:
            continue
        sx, sy, packed = struct.unpack_from('<iiI', x, at - 12)
        if not (0 < sx <= 16384 and 0 < sy <= 16384):
            continue
        q = at + len(needle)
        h = {'format': pf, 'size': [sx, sy], 'packed': packed, 'has_opt_data': bool(packed & (1 << 30)),
             'ext_data': None, 'num_mips_in_tail': 0}
        if h['has_opt_data']:
            h['ext_data'], h['num_mips_in_tail'] = struct.unpack_from('<II', x, q)
            q += 8
        h['first_mip'], h['num_mips'] = struct.unpack_from('<ii', x, q)
        return h
    return None


def mip_payloads(z, x, bulk):
    """{mip: bytes} from the package bulk-data map (entry k = mip k): inline entries are
    offsets into the export payload, separate-file entries offsets into the bulk chunk."""
    out = {}
    for k, (so, _dso, ss, fl) in enumerate(z.bulk_data):
        if ss <= 0:
            continue
        if fl & 0x100:                    # payload in the separate bulk file
            if bulk is not None and so + ss <= len(bulk):
                out[k] = bulk[so:so + ss]
        elif so + ss <= len(x):
            out[k] = x[so:so + ss]
    return out


def judge(h, mips):
    """Pick the decisive mip pair and score every hypothesis."""
    eb = BLOCK_BYTES.get(h['format'])
    vf = block_value(h['format'])
    if eb is None or vf is None:
        return {'verdict': 'unsupported format'}
    fm = max(0, h.get('first_mip') or 0)          # the stored chain starts at mip FirstMipToSerialize
    w0, h0 = max(1, -(-max(1, h['size'][0] >> fm) // 4)), max(1, -(-max(1, h['size'][1] >> fm) // 4))
    n = h['num_mips']
    res = {}
    # smallest mip pair whose smaller mip has >= 64x64 blocks (one whole 64 KiB tile of 16-byte
    # blocks: the full in-tile equation is tested), else >= 16x16, else >= 4x4
    pairs = []
    for lim in (64, 16, 4):
        pairs += [m for m in range(n - 2, -1, -1) if m not in pairs
                  and min(max(1, w0 >> (m + 1)), max(1, h0 >> (m + 1))) >= lim]
    for mode in (0, 1, 5, 9):
        best = None
        first, tail = tail_layout(mode, eb, w0, h0, n) if mode else (None, {})
        for m in pairs:
            wa, ha, wb, hb = max(1, w0 >> m), max(1, h0 >> m), max(1, w0 >> (m + 1)), max(1, h0 >> (m + 1))
            if wa != 2 * wb or ha != 2 * hb:
                continue

            def grid(i, w, hh):
                if mode and first is not None and i >= first:
                    d = mips.get(first)
                    return read_grid(d, w, hh, eb, mode, pitch_el=tile_dims(equation(mode, eb))[0],
                                     origin=tail[i]) if d is not None else None
                d = mips.get(i)
                return read_grid(d, w, hh, eb, mode) if d is not None else None
            ga, gb = grid(m, wa, ha), grid(m + 1, wb, hb)
            if ga is None or gb is None:
                continue
            c = corr(_down(_values(ga, vf)), _values(gb, vf))
            if c is not None:
                best = {'mips': [m, m + 1], 'match': c}
                break
        res[MODE_NAME[mode]] = best
    scored = {k: v['match'] for k, v in res.items() if v}
    if not scored:
        return {'verdict': 'no decodable mip pair', 'scores': res}
    # the question asked: do the pixels have the layout the HEADER claims, against the main
    # alternative (64 KiB swizzle for a linear claim, linear for a swizzled claim)?
    claimed = h['ext_data'] if h['has_opt_data'] else 0
    if claimed not in MODE_NAME:
        return {'verdict': f'header claims layout {claimed}: not a mode this probe knows', 'scores': res}
    cn = MODE_NAME[claimed]
    alt = 'SW_64KB_S' if claimed == 0 else 'LINEAR'
    c, o = scored.get(cn), scored.get(alt)
    if c is None:
        verdict = f'claimed {cn} does not fit the data'
    elif o is None:
        verdict = (f'{cn} (header and pixels agree; {alt} does not fit the data size)' if c >= GOOD
                   else 'undecided')
    elif c >= GOOD and c - o >= MARGIN:
        verdict = f'{cn} (header and pixels agree)'
    elif o >= GOOD and o - c >= MARGIN:
        verdict = f'{alt} (CONTRADICTS the header, which claims {cn})'
    elif c >= GOOD and o >= GOOD:
        verdict = 'undecided (texture too uniform: both layouts fit)'
    else:
        verdict = 'undecided'
    return {'verdict': verdict, 'claimed': cn, 'scores': res}


# ---------------------------------------------------------------- drivers
def probe_container(uf, utoc, ucas=None, fetched=None, limit=None):
    t = uf.parse_toc(open(utoc, 'rb').read())
    rd = uf.ChunkReader(t, ucas) if ucas else None
    rows = {}
    for row, cid in enumerate(t.chunk_ids):
        rows.setdefault(cid[:8], {})[cid[11]] = row

    def chunk(row):
        if rd is not None:
            return rd.read_row(row)
        p = os.path.join(fetched, t.chunk_ids[row].hex() + '.bin')
        if not os.path.isfile(p):
            return None
        d = open(p, 'rb').read()
        if len(d) != t.lengths[row] or uf.io_hash(d) != t.hashes[row]:
            return None                    # only whole, IoHash-verified chunks are used
        return d
    out = []
    for key, kinds in sorted(rows.items()):
        if 1 not in kinds:
            continue
        d = chunk(kinds[1])
        if d is None:
            continue
        try:
            z = uf.parse_zen_package(d)
        except uf.FormatError:
            continue
        if not any(n.startswith('PF_') for n in z.names):
            continue
        top = [e for e in z.exports if 'chunk_offset' in e]
        x = uf.export_data(d, z, top[-1]) if top else b''
        h = platform_header(x, z.names)
        if h is None:
            continue
        bulk = chunk(kinds[2]) if 2 in kinds else None
        mips = mip_payloads(z, x, bulk)
        r = {'package': z.name, **h, 'mips_with_data': sorted(mips), 'bulk_chunk': 2 in kinds,
             'bulk_read': bulk is not None}
        r.update(judge(h, mips))
        out.append(r)
        if limit and len(out) >= limit:
            break
    return out


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--formats-lib', required=True, help='folder holding ue_formats.py (read-only import)')
    ap.add_argument('--local', nargs=2, action='append', default=[], metavar=('UTOC', 'UCAS'))
    ap.add_argument('--fetched', nargs=2, action='append', default=[], metavar=('UTOC', 'CHUNK_DIR'))
    ap.add_argument('--out', required=True)
    a = ap.parse_args(argv)
    sys.path.insert(0, a.formats_lib)
    import ue_formats as uf  # noqa: E402
    rows = []
    for utoc, ucas in a.local:
        rows += probe_container(uf, utoc, ucas=ucas)
    for utoc, d in a.fetched:
        rows += probe_container(uf, utoc, fetched=d)
    hdr = collections.Counter((r['has_opt_data'], r['ext_data'], r['num_mips_in_tail'] > 0) for r in rows)
    ver = collections.Counter(r['verdict'].split(' ')[0] for r in rows)
    with open(a.out, 'w') as fh:
        json.dump({'tool': 'cap12-texture-tiling-probe-1', 'label': 'Mac-only', 'textures': rows,
                   'header_histogram': {str(k): v for k, v in hdr.items()}, 'verdicts': dict(ver)}, fh, indent=1)
    print(f'texture_tiling_probe: {len(rows)} textures checked')
    for (opt, ext, tail), v in sorted(hdr.items(), key=lambda kv: -kv[1]):
        print(f'  {v:5d}  HasOptData={int(opt)} ExtData={ext} ({MODE_NAME.get(ext, "?") if opt else "no layout block"}) '
              f'mip tail={"yes" if tail else "no"}')
    print('  pixel verdicts:', dict(ver))
    if not rows:
        print('texture_tiling_probe: FAIL (zero textures checked)')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
