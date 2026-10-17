"""Minimal pure-Python BLAKE3 (hash mode only), independent implementation.

Used to verify fetched base chunks against the game's own TOC IoHash
(first 20 bytes of BLAKE3-256). Self-checked by tests/test_blake3.py against
every chunk hash stored in a real mod TOC.
"""
import struct

IV = (0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A, 0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19)
PERM = (2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8)
CHUNK_START, CHUNK_END, PARENT, ROOT = 1, 2, 4, 8
M32 = 0xFFFFFFFF


def _g(s, a, b, c, d, x, y):
    s[a] = (s[a] + s[b] + x) & M32
    s[d] = s[d] ^ s[a]; s[d] = ((s[d] >> 16) | (s[d] << 16)) & M32
    s[c] = (s[c] + s[d]) & M32
    s[b] = s[b] ^ s[c]; s[b] = ((s[b] >> 12) | (s[b] << 20)) & M32
    s[a] = (s[a] + s[b] + y) & M32
    s[d] = s[d] ^ s[a]; s[d] = ((s[d] >> 8) | (s[d] << 24)) & M32
    s[c] = (s[c] + s[d]) & M32
    s[b] = s[b] ^ s[c]; s[b] = ((s[b] >> 7) | (s[b] << 25)) & M32


def _compress(cv, block, counter, blen, flags):
    m = list(struct.unpack("<16I", block))
    s = list(cv) + list(IV[:4]) + [counter & M32, (counter >> 32) & M32, blen, flags]
    for r in range(7):
        _g(s, 0, 4, 8, 12, m[0], m[1]); _g(s, 1, 5, 9, 13, m[2], m[3])
        _g(s, 2, 6, 10, 14, m[4], m[5]); _g(s, 3, 7, 11, 15, m[6], m[7])
        _g(s, 0, 5, 10, 15, m[8], m[9]); _g(s, 1, 6, 11, 12, m[10], m[11])
        _g(s, 2, 7, 8, 13, m[12], m[13]); _g(s, 3, 4, 9, 14, m[14], m[15])
        if r < 6:
            m = [m[i] for i in PERM]
    return [s[i] ^ s[i + 8] for i in range(8)] + [s[i + 8] ^ cv[i] for i in range(8)]


def _chunk_output(data, counter):
    """Returns (cv, block, blen, flags) of the chunk's last block (not yet finalized)."""
    cv = list(IV)
    blocks = [data[i:i + 64] for i in range(0, max(len(data), 1), 64)] or [b""]
    for k, blk in enumerate(blocks):
        flags = (CHUNK_START if k == 0 else 0) | (CHUNK_END if k == len(blocks) - 1 else 0)
        padded = blk.ljust(64, b"\0")
        if k == len(blocks) - 1:
            return cv, padded, len(blk), flags, counter
        cv = _compress(cv, padded, counter, 64, flags)[:8]


def blake3(data: bytes, out_len: int = 32) -> bytes:
    if out_len > 32:
        raise ValueError("only up to 32 output bytes supported")
    chunks = [data[i:i + 1024] for i in range(0, len(data), 1024)] or [b""]
    if len(chunks) == 1:
        cv, blk, blen, flags, ctr = _chunk_output(chunks[0], 0)
        out = _compress(cv, blk, ctr, blen, flags | ROOT)
        return struct.pack("<8I", *out[:8])[:out_len]
    stack = []
    for idx, ch in enumerate(chunks[:-1]):
        cv, blk, blen, flags, ctr = _chunk_output(ch, idx)
        new = _compress(cv, blk, ctr, blen, flags)[:8]
        total = idx + 1
        while total & 1 == 0:
            left = stack.pop()
            new = _compress(list(IV), struct.pack("<16I", *(left + new)), 0, 64, PARENT)[:8]
            total >>= 1
        stack.append(new)
    cv, blk, blen, flags, ctr = _chunk_output(chunks[-1], len(chunks) - 1)
    # finalize: fold the last chunk output into the stack right to left
    out_cv, out_blk, out_blen, out_flags, out_ctr = cv, blk, blen, flags, ctr
    while stack:
        child = _compress(out_cv, out_blk, out_ctr, out_blen, out_flags)[:8]
        left = stack.pop()
        out_cv, out_blk, out_blen, out_flags, out_ctr = list(IV), struct.pack("<16I", *(left + child)), 64, PARENT, 0
    out = _compress(out_cv, out_blk, out_ctr, out_blen, out_flags | ROOT)
    return struct.pack("<8I", *out[:8])[:out_len]


def io_hash20(data: bytes) -> bytes:
    return blake3(data, 20)
