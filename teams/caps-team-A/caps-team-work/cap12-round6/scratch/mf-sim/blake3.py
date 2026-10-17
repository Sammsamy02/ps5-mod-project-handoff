#!/usr/bin/env python3
"""Pure-Python BLAKE3 (hash mode only), written from the public BLAKE3 spec.

UE5 IoStore TOC v8 stores FIoHash = first 20 bytes of BLAKE3(chunk bytes) per
chunk. Used here only to verify that bytes the integrator fetches from the
console are exactly the chunk the game's own TOC describes. Slow (pure Python):
fine for shader-library manifests (< 2 MB) and small packages.
Verified in tests: empty-input vector from the spec, and the game's/mod's own
TOC hashes as an independent second source.
"""
IV = (0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
      0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19)
PERM = (2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8)
CHUNK_START, CHUNK_END, PARENT, ROOT = 1, 2, 4, 8
M32 = 0xFFFFFFFF


def _compress(cv, m, counter, blen, flags):
    s = list(cv) + list(IV[:4]) + [counter & M32, (counter >> 32) & M32, blen, flags]
    m = list(m)

    def g(a, b, c, d, x, y):
        s[a] = (s[a] + s[b] + x) & M32
        v = s[d] ^ s[a]; s[d] = ((v >> 16) | (v << 16)) & M32
        s[c] = (s[c] + s[d]) & M32
        v = s[b] ^ s[c]; s[b] = ((v >> 12) | (v << 20)) & M32
        s[a] = (s[a] + s[b] + y) & M32
        v = s[d] ^ s[a]; s[d] = ((v >> 8) | (v << 24)) & M32
        s[c] = (s[c] + s[d]) & M32
        v = s[b] ^ s[c]; s[b] = ((v >> 7) | (v << 25)) & M32

    for r in range(7):
        g(0, 4, 8, 12, m[0], m[1]); g(1, 5, 9, 13, m[2], m[3])
        g(2, 6, 10, 14, m[4], m[5]); g(3, 7, 11, 15, m[6], m[7])
        g(0, 5, 10, 15, m[8], m[9]); g(1, 6, 11, 12, m[10], m[11])
        g(2, 7, 8, 13, m[12], m[13]); g(3, 4, 9, 14, m[14], m[15])
        if r < 6:
            m = [m[i] for i in PERM]
    for i in range(8):
        s[i] ^= s[i + 8]
        s[i + 8] ^= cv[i]
    return s


def _words(block):
    block = block.ljust(64, b'\0')
    return [int.from_bytes(block[4 * i:4 * i + 4], 'little') for i in range(16)]


def _chunk_output(data, counter):
    """Return (cv, block_words, counter, block_len, flags) of the chunk's last block."""
    cv = IV
    blocks = [data[i:i + 64] for i in range(0, len(data), 64)] or [b'']
    for i, b in enumerate(blocks):
        flags = (CHUNK_START if i == 0 else 0) | (CHUNK_END if i == len(blocks) - 1 else 0)
        if i == len(blocks) - 1:
            return (cv, _words(b), counter, len(b), flags)
        cv = tuple(_compress(cv, _words(b), counter, 64, flags)[:8])


def _cv(out):
    cv, w, c, bl, fl = out
    return tuple(_compress(cv, w, c, bl, fl)[:8])


def _parent_output(left, right):
    return (IV, list(left) + list(right), 0, 64, PARENT)


def blake3(data: bytes, length: int = 32) -> bytes:
    data = bytes(data)
    chunks = [data[i:i + 1024] for i in range(0, len(data), 1024)] or [b'']
    stack = []
    out = None
    for idx, ch in enumerate(chunks):
        out = _chunk_output(ch, idx)
        if idx == len(chunks) - 1:
            break
        cv = _cv(out)
        total = idx + 1
        while total & 1 == 0:
            cv = _cv(_parent_output(stack.pop(), cv))
            total >>= 1
        stack.append(cv)
    while stack:
        out = _parent_output(stack.pop(), _cv(out))
    cv, w, c, bl, fl = out
    res = b''
    ctr = 0
    while len(res) < length:
        s = _compress(cv, w, ctr, bl, fl | ROOT)
        res += b''.join(x.to_bytes(4, 'little') for x in s)
        ctr += 1
    return res[:length]


def iohash20(data: bytes) -> str:
    return blake3(data, 20).hex()


if __name__ == '__main__':
    import sys
    for p in sys.argv[1:]:
        with open(p, 'rb') as fh:
            print(iohash20(fh.read()), p)
