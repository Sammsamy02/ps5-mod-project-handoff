#!/usr/bin/env python3
"""Exact red<->blue channel swap on COMPRESSED texture blocks (Mac-only, generic).

Capability #12 "derived texture" (user/integrator decision 2026-10-13): a mod
packed mask whose channel order is the R<->B mirror of the game slot's order
(e.g. ORM = O,R,M in R,G,B versus MRO = M,R,O) is made usable by swapping the
red and blue channels of the mod texture. The swap works on the stored blocks
(no decode / re-encode, never lossy). Output decodes to exactly the input with
R and B exchanged on every texel for any decoder that treats the 5-bit red and
5-bit blue fields the same way AND (BC1-3 blocks whose endpoints get exchanged)
whose 1/3-2/3 blend is mirror-symmetric. The D3D spec does not require bit-exact
BC1-3 interpolation, so exactness is PROVEN for the two decoders used (bcn_decode.py,
spec integer rounding, and the Mac GPU, metal_bcn_check.swift), not for every GPU;
a decoder with an asymmetric blend could differ by at most one 8-bit step on those
blocks. BC7 and 8-bit RGBA are exact for every decoder (integer-specified).

Public format facts only (Microsoft D3D10/11 block-compression specification,
BC1/BC2/BC3/BC7 layouts). No Sony material, no game or mod names.

Formats (UE pixel-format names) and what is done:
  PF_DXT1 (BC1)  colour endpoints RGB565: R (bits 11-15) and B (bits 0-4) of
                 both endpoints are exchanged. The block MODE depends on the
                 unsigned comparison c0 > c1 (4-colour) versus c0 <= c1
                 (3-colour + transparent black), and the swap can flip that
                 comparison. When it does, the two endpoints are exchanged and
                 the indices remapped so the mode and every palette entry stay
                 the same: 4-colour 0<->1, 2<->3; 3-colour 0<->1 (2 = midpoint
                 and 3 = transparent black stay).
  PF_DXT3 (BC2), PF_DXT5 (BC3)
                 alpha half unchanged; colour half as BC1, but the spec decodes
                 it ALWAYS as 4-colour while some decoders use the c0 <= c1
                 rule. The ordering of c0/c1 is preserved, so both readings stay
                 exact; the one case where no single output is exact under both
                 readings (c0 < c1, the swap flips the order, and index 2 or 3 is
                 used) is REFUSED.
  PF_BC7         the R and B endpoint fields (equal width in every mode, p-bits
                 are per endpoint for all channels) are exchanged; in modes 4
                 and 5 the rotation field maps 1 (swap A,R) <-> 3 (swap A,B).
                 Reserved mode (first byte 0) is REFUSED.
  PF_B8G8R8A8, PF_R8G8B8A8
                 bytes 0 and 2 of every pixel are exchanged.
Anything else (BC4/BC5 have no blue channel, BC6H, float formats ...) is REFUSED.
"""

SUPPORTED = ('PF_DXT1', 'PF_DXT3', 'PF_DXT5', 'PF_BC7', 'PF_B8G8R8A8', 'PF_R8G8B8A8')
BLOCK_BYTES = {'PF_DXT1': 8, 'PF_DXT3': 16, 'PF_DXT5': 16, 'PF_BC7': 16, 'PF_B8G8R8A8': 4, 'PF_R8G8B8A8': 4}

# BC7 per mode: (subsets, partition bits, rotation bits, index-selection bits, colour bits)
BC7_MODES = {0: (3, 4, 0, 0, 4), 1: (2, 6, 0, 0, 6), 2: (3, 6, 0, 0, 5), 3: (2, 6, 0, 0, 7),
             4: (1, 0, 2, 1, 5), 5: (1, 0, 2, 0, 7), 6: (1, 0, 0, 0, 7), 7: (2, 6, 0, 0, 5)}


class SwapRefused(ValueError):
    pass


def _swap565(c):
    return ((c & 0x1F) << 11) | (c & 0x07E0) | (c >> 11)


def _colour_block(b, always_four):
    """8-byte BC1-style colour block -> swapped 8 bytes."""
    c0 = b[0] | (b[1] << 8)
    c1 = b[2] | (b[3] << 8)
    idx = b[4] | (b[5] << 8) | (b[6] << 16) | (b[7] << 24)
    s0, s1 = _swap565(c0), _swap565(c1)
    four = c0 > c1
    flips = c0 != c1 and (s0 > s1) != (c0 > c1)
    if always_four and flips and c0 < c1 and {(idx >> (2 * k)) & 3 for k in range(16)} & {2, 3}:
        raise SwapRefused('BC2/BC3 colour block c0 < c1 whose endpoint order flips, with index 2/3: no output is '
                          'exact under both the 4-colour and the 3-colour reading')
    if not flips:
        n0, n1, nidx = s0, s1, idx
    else:
        # comparison flipped: exchange the endpoints and remap the indices
        n0, n1 = s1, s0
        nidx = 0
        for k in range(16):
            x = (idx >> (2 * k)) & 3
            if four:
                y = x ^ 1                    # 0<->1, 2<->3
            else:
                y = x ^ 1 if x < 2 else x    # 0<->1; 2 (midpoint), 3 (transparent) stay
            nidx |= y << (2 * k)
        if not (c0 == c1 or (n0 > n1) == (c0 > c1)):
            raise SwapRefused('internal: endpoint order not preserved')
    return bytes((n0 & 0xFF, n0 >> 8, n1 & 0xFF, n1 >> 8,
                  nidx & 0xFF, (nidx >> 8) & 0xFF, (nidx >> 16) & 0xFF, nidx >> 24))


def bc7_mode(block):
    b0 = block[0]
    for m in range(8):
        if b0 & (1 << m):
            return m
    return None


def _bc7_block(block):
    m = bc7_mode(block)
    if m is None:
        raise SwapRefused('BC7 reserved mode (first byte 0)')
    ns, pb, rb, isb, cb = BC7_MODES[m]
    v = int.from_bytes(block, 'little')
    pos = m + 1 + pb
    if rb:
        rot = (v >> pos) & 3
        nrot = {0: 0, 1: 3, 2: 2, 3: 1}[rot]
        v = (v & ~(3 << pos)) | (nrot << pos)
    start = m + 1 + pb + rb + isb
    w = 2 * ns * cb
    mask = (1 << w) - 1
    r = (v >> start) & mask
    bl = (v >> (start + 2 * w)) & mask
    v &= ~((mask << start) | (mask << (start + 2 * w)))
    v |= (bl << start) | (r << (start + 2 * w))
    return v.to_bytes(16, 'little')


def swap_rb(fmt, data):
    """-> (swapped bytes, facts). Raises SwapRefused for anything not exact."""
    if fmt not in SUPPORTED:
        raise SwapRefused(f'pixel format {fmt} not supported for an exact R<->B swap')
    bb = BLOCK_BYTES[fmt]
    if len(data) % bb:
        raise SwapRefused(f'{len(data)} bytes is not a whole number of {bb}-byte {fmt} blocks')
    out = bytearray(len(data))
    facts = {'format': fmt, 'blocks': len(data) // bb, 'endpoint_order_fixed': 0, 'bc7_rotation_remapped': 0}
    mv = memoryview(data)
    if fmt in ('PF_B8G8R8A8', 'PF_R8G8B8A8'):
        out[:] = data
        out[0::4] = data[2::4]
        out[2::4] = data[0::4]
        return bytes(out), facts
    cache = {}
    for o in range(0, len(data), bb):
        blk = bytes(mv[o:o + bb])
        got = cache.get(blk)
        if got is None:
            if fmt == 'PF_DXT1':
                got = _colour_block(blk, False)
                fixed = got[0:2] != _swap565(blk[0] | (blk[1] << 8)).to_bytes(2, 'little')
            elif fmt in ('PF_DXT3', 'PF_DXT5'):
                got = blk[:8] + _colour_block(blk[8:], True)
                fixed = got[8:10] != _swap565(blk[8] | (blk[9] << 8)).to_bytes(2, 'little')
            else:
                got = _bc7_block(blk)
                m = bc7_mode(blk)
                fixed = m in (4, 5) and ((int.from_bytes(blk, 'little') >> (m + 1)) & 3) in (1, 3)
            got = (got, fixed)
            if len(cache) < 1 << 20:
                cache[blk] = got
        out[o:o + bb] = got[0]
        if got[1]:
            facts['endpoint_order_fixed' if fmt != 'PF_BC7' else 'bc7_rotation_remapped'] += 1
    return bytes(out), facts
