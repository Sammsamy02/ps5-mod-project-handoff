#!/usr/bin/env python3
"""PS5 GPU tile order for block-compressed textures (Mac-only, generic, lossless, fail closed).

The game's PS5 textures store their blocks in the AMD GFX10 "standard swizzle" order
(Team B, cap12-layout-invisible NOTES.md Q1: game pixels + public addrlib), with the small
mips packed into one tail block. PC mods store blocks row by row. This module is the ONLY
place that knows the tile order: it re-lays linear block data into that order (tile) and
back (detile). Bytes are only moved, never changed; padding is zero.

Public sources only (Mesa addrlib, MIT; copy with checksums in Team B's evidence folder):
  gfx10SwizzlePattern.h  GFX10_SW_4K_S / 64K_S PATINFO rows + NIBBLE01/2/3 tables (the S
                         modes have no pipe/bank XOR: identical on every AMD configuration),
  gfx10addrlib.cpp       ComputeSurfaceInfoMacroTiled (pitch/height padding to whole tiles,
                         first mip in tail, tail offsets), GetMaxNumMipsInTail,
  addrlib2.cpp           GetMipTailDim (thin: width halved), Block256_2d.
No Sony or Epic material.

GATES (fail closed; all must hold before anything is tiled):
  RULE      the swizzle mode and NumMipsInTail are chosen by the GAME's own rule, learned
            from its textures and checked from two sources:
              (1) the game's header census (ps5_layout_census.json, built by
                  game_layout_census.py from IoHash-verified game chunks): the rule must
                  agree with EVERY census row (mode AND NumMipsInTail), and
              (2) public addrlib: the equations and tail placement below, and its
                  preferred-mode search (HwlGetPreferredSurfaceSetting: larger blocks are
                  preferred while the padded size stays within budget) whose zero-padding
                  case is the rule: the largest S block (64KB, then 4KB) whose tile fits
                  the texture's header size in elements; otherwise 256B.
  SETTLED   the decision must be shown on the GAME's own textures: at least one VERIFIED
            census row (chunk IoHash == base TOC) with the same block size, the same mode,
            and the same FIT CLASS (whether its size reaches the 64KB and the 4KB tile on
            each axis), whose single pixel verdict says its pixels agree with its header.
            So a size whose fit class the game data does not show (e.g. one axis below the
            64KB tile, the other above) is refused rather than guessed, even where
            another plausible rule (addrlib's padded-size budget search) would agree.
            Any census row whose pixels CONTRADICT its header fails the whole census.
            256B / linear: not handled -> refused.
  SHAPE     power-of-two size, full mip chain, at least one mip above the tail.
"""

# address bits of the element index inside one block (tile), lowest first, from the public
# NIBBLE tables (bits below the element size are 0); key (AddrSwizzleMode, bytes per element)
EQUATIONS = {
    (5, 8): 'X0 Y0 Y1 X1 X2 Y2 X3 Y3 X4',                  # SW_4KB_S,  8 B: NIBBLE01[3] + NIBBLE2[4]
    (5, 16): 'Y0 Y1 X0 X1 Y2 X2 Y3 X3',                    # SW_4KB_S, 16 B: NIBBLE01[4] + NIBBLE2[5]
    (9, 8): 'X0 Y0 Y1 X1 X2 Y2 X3 Y3 X4 Y4 X5 Y5 X6',      # SW_64KB_S, 8 B: + NIBBLE3[4]
    (9, 16): 'Y0 Y1 X0 X1 Y2 X2 Y3 X3 Y4 X4 Y5 X5',        # SW_64KB_S, 16 B: + NIBBLE3[5]
}
BLOCK_LOG2 = {5: 12, 9: 16}
MODE_NAME = {5: 'SW_4KB_S', 9: 'SW_64KB_S'}
BLOCK256 = {8: (8, 4), 16: (4, 4)}            # Block256_2d[log2(bytes per element)]
ELEMENT_BYTES = {'PF_DXT1': 8, 'PF_BC4': 8, 'PF_DXT3': 16, 'PF_DXT5': 16, 'PF_BC5': 16, 'PF_BC7': 16}

MIN_CENSUS_ROWS = 50
PIXEL_AGREE = 'header and pixels agree'


class TilingRefused(ValueError):
    pass


def equation(mode, eb):
    """[('X'|'Y', bit), ...] lowest address bit first."""
    if (mode, eb) not in EQUATIONS:
        raise TilingRefused(f'no public equation for mode {mode} with {eb}-byte elements')
    return [(t[0], int(t[1:])) for t in EQUATIONS[(mode, eb)].split()]


def tile_dims(mode, eb):
    eq = equation(mode, eb)
    return 1 << sum(a == 'X' for a, _ in eq), 1 << sum(a == 'Y' for a, _ in eq)


def max_mips_in_tail(blk_log2):
    return (blk_log2 - 4) if blk_log2 > 11 else 1 + (1 << (blk_log2 - 9))


def _offsets(mode, eb):
    """Per-axis contribution to the element index inside a tile (bits are disjoint, so OR = sum)."""
    eq = equation(mode, eb)
    tw, th = tile_dims(mode, eb)
    ox = [0] * tw
    oy = [0] * th
    for k, (ax, b) in enumerate(eq):
        if ax == 'X':
            for x in range(tw):
                if (x >> b) & 1:
                    ox[x] |= 1 << k
        else:
            for y in range(th):
                if (y >> b) & 1:
                    oy[y] |= 1 << k
    return ox, oy, tw, th


def chain_layout(mode, eb, w_el, h_el, nmips):
    """Public addrlib placement for a thin 2D mip chain.
    -> {'first_tail': f or None, 'mips': [{'w','h','pitch','height','size'} for mips < f],
        'tail': {mip: (x0, y0)} element origins inside the tail block, 'block': bytes per tile}."""
    blk = BLOCK_LOG2[mode]
    tw, th = tile_dims(mode, eb)
    tail_w, tail_h = tw // 2, th                  # GetMipTailDim, thin: width halved
    maxin = max_mips_in_tail(blk)
    first = None
    mips = []
    for i in range(nmips):
        mw, mh = max(1, w_el >> i), max(1, h_el >> i)
        if mw <= tail_w and mh <= tail_h and nmips - i <= maxin:
            first = i
            break
        pitch = -(-mw // tw) * tw
        height = -(-mh // th) * th
        mips.append({'w': mw, 'h': mh, 'pitch': pitch, 'height': height, 'size': pitch * height * eb})
    tail = {}
    if first is not None:
        bw, bh = BLOCK256[eb]
        for i in range(first, nmips):
            m = maxin - 1 - (i - first)
            off = (16 << m) if m > 6 else (m << 8)
            mx = sum(((off >> (9 + 2 * k)) & 1) << k for k in range(6))
            my = sum(((off >> (8 + 2 * k)) & 1) << k for k in range(6))
            if blk & 1:
                raise TilingRefused('odd block-size modes are not handled')
            tail[i] = (mx * bw, my * bh)
            mw, mh = max(1, w_el >> i), max(1, h_el >> i)
            if tail[i][0] + mw > tw or tail[i][1] + mh > th:
                raise TilingRefused(f'tail mip {i} does not fit the tail block')
    return {'first_tail': first, 'mips': mips, 'tail': tail, 'block': 1 << blk, 'tile': (tw, th)}


def _place(dst, base, src, mw, mh, eb, ox, oy, tw, th, pitch_tiles, x0=0, y0=0, inverse=False):
    tile_bytes = tw * th * eb
    for y in range(mh):
        yy = y + y0
        rowt = (yy // th) * pitch_tiles
        oyv = oy[yy % th]
        lin = y * mw * eb
        for x in range(mw):
            xx = x + x0
            a = base + (rowt + xx // tw) * tile_bytes + (ox[xx % tw] | oyv) * eb
            s = lin + x * eb
            if inverse:
                src[s:s + eb] = dst[a:a + eb]
            else:
                dst[a:a + eb] = src[s:s + eb]


def block_dims(mode, eb):
    """2D element dims of one S-mode block for any element size (public addrlib: the
    element bits alternate X/Y, X gets the extra bit). Equals tile_dims() for 8/16 bytes."""
    b = {9: 16, 5: 12, 1: 8}[mode] - (eb.bit_length() - 1)
    return 1 << ((b + 1) // 2), 1 << (b // 2)


def rule_predict(eb, size_el, mip0_el, nmips):
    """The game's rule: (ExtData, NumMipsInTail) for a texture whose header size and stored
    mip 0 are given in elements."""
    for mode in (9, 5):
        tw, th = block_dims(mode, eb)
        if size_el[0] >= tw and size_el[1] >= th:
            blk = BLOCK_LOG2[mode]
            tail_w, tail_h = tw // 2, th
            for i in range(nmips):
                if max(1, mip0_el[0] >> i) <= tail_w and max(1, mip0_el[1] >> i) <= tail_h and \
                        nmips - i <= max_mips_in_tail(blk):
                    return mode, nmips - i
            return mode, 0
    return 1, 0


def fit_class(eb, size_el):
    """Whether a size reaches the 64KB and the 4KB tile on each axis (the rule's decision inputs)."""
    out = []
    for mode in (9, 5):
        tw, th = block_dims(mode, eb)
        out += [size_el[0] >= tw, size_el[1] >= th]
    return tuple(out)


def census_check(census):
    """Every census row must follow the rule, and no row's pixels may contradict its header.
    -> (rows checked, evidence {(eb, mode, fit class): [verified packages with ONE pixel verdict
    'header and pixels agree']}) or TilingRefused."""
    rows = census.get('rows') or []
    if len(rows) < MIN_CENSUS_ROWS:
        raise TilingRefused(f'layout census has {len(rows)} rows (need >= {MIN_CENSUS_ROWS})')
    evidence = {}
    for r in rows:
        eb, px = r['element_bytes'], r['pixels_per_element']
        size_el = (-(-r['size'][0] // px), -(-r['size'][1] // px))
        mip0 = (-(-r['mip0'][0] // px), -(-r['mip0'][1] // px))
        got = rule_predict(eb, size_el, mip0, r['mips'])
        if got != (r['ext_data'], r['num_mips_in_tail']):
            raise TilingRefused(f"rule contradicted by game texture {r['package']} ({r['format']} {r['size']}): "
                                f"rule {got}, game {(r['ext_data'], r['num_mips_in_tail'])}")
        verdicts = r.get('pixel_verdicts') or []
        if any('CONTRADICTS' in v or 'does not fit' in v.split(';')[0] for v in verdicts):
            raise TilingRefused(f"game texture {r['package']}: a pixel verdict contradicts its header {verdicts}")
        if r.get('verified') is True and len(set(verdicts)) == 1 and PIXEL_AGREE in verdicts[0]:
            evidence.setdefault((eb, r['ext_data'], fit_class(eb, size_el)), []).append(r['package'])
    return len(rows), evidence


def choose(fmt, width, height, nmips, census=None):
    """The PS5 layout a derived texture must use, or TilingRefused (all gates)."""
    if census is None:
        raise TilingRefused('no game layout census given')
    eb = ELEMENT_BYTES.get(fmt)
    if eb is None:
        raise TilingRefused(f'{fmt}: not a block format this module tiles')
    if width & (width - 1) or height & (height - 1) or width < 4 or height < 4:
        raise TilingRefused(f'{fmt} {width}x{height}: not a power-of-two size of at least one block')
    full = max(width, height).bit_length()
    if nmips != full:
        raise TilingRefused(f'{fmt} {width}x{height}: {nmips} mips, a full chain has {full}')
    n_rows, evidence = census_check(census)
    w_el, h_el = width // 4, height // 4
    mode, tail_n = rule_predict(eb, (w_el, h_el), (w_el, h_el), nmips)
    if mode not in BLOCK_LOG2:
        raise TilingRefused(f'{fmt} {width}x{height}: the game rule gives {mode} (256B), not handled')
    shown = evidence.get((eb, mode, fit_class(eb, (w_el, h_el))))
    if not shown:
        raise TilingRefused(f'{fmt} {width}x{height}: no verified game texture with {eb}-byte blocks in '
                            f'{MODE_NAME[mode]} of the same fit class whose pixels agree with its header '
                            '(not settled by game data)')
    lay = chain_layout(mode, eb, w_el, h_el, nmips)
    n_tail = 0 if lay['first_tail'] is None else nmips - lay['first_tail']
    if n_tail != tail_n:
        raise TilingRefused(f'internal: tail count {n_tail} vs rule {tail_n}')
    if not lay['mips']:
        raise TilingRefused('whole chain in the tail: not handled')
    return {'mode': mode, 'mode_name': MODE_NAME[mode], 'ext_data': mode, 'num_mips_in_tail': n_tail,
            'element_bytes': eb, 'chain': lay,
            'game_layout_source': f'game rule: {n_rows}/{n_rows} census rows agree (mode + NumMipsInTail)',
            'order_source': f'{len(shown)} verified game texture(s) with {eb}-byte blocks in {MODE_NAME[mode]}, same '
                            'fit class, pixels agree: ' + ', '.join(sorted(shown)[:3])}


def tile(fmt, width, height, linear_mips, choice):
    """linear_mips: [bytes] per mip, row-major blocks. -> [bytes] per mip in the PS5 form:
    tiled (padded) mips before the tail; at the first tail mip one whole tail block; later
    tail mips empty."""
    eb = choice['element_bytes']
    lay = choice['chain']
    ox, oy, tw, th = _offsets(choice['mode'], eb)
    out = []
    w_el, h_el = -(-width // 4), -(-height // 4)
    f = lay['first_tail']
    for i, data in enumerate(linear_mips):
        mw, mh = max(1, w_el >> i), max(1, h_el >> i)
        if len(data) != mw * mh * eb:
            raise TilingRefused(f'mip {i}: {len(data)} bytes, {mw}x{mh} blocks need {mw * mh * eb}')
        if f is None or i < f:
            m = lay['mips'][i]
            buf = bytearray(m['size'])
            _place(buf, 0, data, mw, mh, eb, ox, oy, tw, th, m['pitch'] // tw)
            out.append(bytes(buf))
        elif i == f:
            out.append(None)
        else:
            out.append(b'')
    if f is not None:
        blockbuf = bytearray(lay['block'])
        for i in range(f, len(linear_mips)):
            mw, mh = max(1, w_el >> i), max(1, h_el >> i)
            x0, y0 = lay['tail'][i]
            _place(blockbuf, 0, linear_mips[i], mw, mh, eb, ox, oy, tw, th, 1, x0, y0)
        out[f] = bytes(blockbuf)
    return out


def detile(fmt, width, height, ps5_mips, choice):
    """Inverse of tile(): PS5-form mips -> linear row-major mips (this module's own check;
    the derive step also uses an independent de-tiler)."""
    eb = choice['element_bytes']
    lay = choice['chain']
    ox, oy, tw, th = _offsets(choice['mode'], eb)
    w_el, h_el = -(-width // 4), -(-height // 4)
    f = lay['first_tail']
    out = []
    for i in range(len(ps5_mips)):
        mw, mh = max(1, w_el >> i), max(1, h_el >> i)
        lin = bytearray(mw * mh * eb)
        if f is None or i < f:
            m = lay['mips'][i]
            if len(ps5_mips[i]) != m['size']:
                raise TilingRefused(f'mip {i}: {len(ps5_mips[i])} bytes, tiled size {m["size"]}')
            _place(ps5_mips[i], 0, lin, mw, mh, eb, ox, oy, tw, th, m['pitch'] // tw, inverse=True)
        else:
            blockbuf = ps5_mips[f]
            if len(blockbuf) != lay['block'] or (i > f and ps5_mips[i] != b''):
                raise TilingRefused(f'tail mip {i}: bad tail block or non-empty later tail mip')
            x0, y0 = lay['tail'][i]
            _place(blockbuf, 0, lin, mw, mh, eb, ox, oy, tw, th, 1, x0, y0, inverse=True)
        out.append(bytes(lin))
    return out
