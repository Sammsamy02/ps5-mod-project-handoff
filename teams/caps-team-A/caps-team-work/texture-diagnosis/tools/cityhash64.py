"""CityHash64 (v1.1 algorithm), pure Python, independent implementation.

Used only to (a) self-check against the FName hashes stored in every Zen
package name batch (algorithm id 0xC1640000 = CityHash64 of the lower-case
name bytes), and (b) derive Zen public export hashes for substitution
planning. Validated at runtime against real stored hashes; never trusted
blind (see tests/test_cityhash.py).
"""
M = (1 << 64) - 1
K0 = 0xC3A5C85C97CB3127
K1 = 0xB492B66FBE98F273
K2 = 0x9AE16A3B2F90404F
KMUL = 0x9DDFEA08EB382D69


def _f64(s, i):
    return int.from_bytes(s[i:i + 8], "little")


def _f32(s, i):
    return int.from_bytes(s[i:i + 4], "little")


def _rot(v, n):
    return v if n == 0 else ((v >> n) | (v << (64 - n))) & M


def _smix(v):
    return v ^ (v >> 47)


def _bswap(v):
    return int.from_bytes(v.to_bytes(8, "little"), "big")


def _h16(u, v, mul=KMUL):
    a = ((u ^ v) * mul) & M
    a ^= a >> 47
    b = ((v ^ a) * mul) & M
    b ^= b >> 47
    return (b * mul) & M


def _len0to16(s, n):
    if n >= 8:
        mul = (K2 + n * 2) & M
        a = (_f64(s, 0) + K2) & M
        b = _f64(s, n - 8)
        c = (_rot(b, 37) * mul + a) & M
        d = ((_rot(a, 25) + b) * mul) & M
        return _h16(c, d, mul)
    if n >= 4:
        mul = (K2 + n * 2) & M
        a = _f32(s, 0)
        return _h16((n + (a << 3)) & M, _f32(s, n - 4), mul)
    if n > 0:
        a, b, c = s[0], s[n >> 1], s[n - 1]
        y = (a + (b << 8)) & 0xFFFFFFFF
        z = (n + (c << 2)) & 0xFFFFFFFF
        return (_smix(((y * K2) ^ (z * K0)) & M) * K2) & M
    return K2


def _len17to32(s, n):
    mul = (K2 + n * 2) & M
    a = (_f64(s, 0) * K1) & M
    b = _f64(s, 8)
    c = (_f64(s, n - 8) * mul) & M
    d = (_f64(s, n - 16) * K2) & M
    return _h16((_rot((a + b) & M, 43) + _rot(c, 30) + d) & M,
                (a + _rot((b + K2) & M, 18) + c) & M, mul)


def _len33to64(s, n):
    mul = (K2 + n * 2) & M
    a = (_f64(s, 0) * K2) & M
    b = _f64(s, 8)
    c = _f64(s, n - 24)
    d = _f64(s, n - 32)
    e = (_f64(s, 16) * K2) & M
    f = (_f64(s, 24) * 9) & M
    g = _f64(s, n - 8)
    h = (_f64(s, n - 16) * mul) & M
    u = (_rot((a + g) & M, 43) + (_rot(b, 30) + c) * 9) & M
    v = (((a + g) & M) ^ d) + f + 1 & M
    w = (_bswap(((u + v) * mul) & M) + h) & M
    x = (_rot((e + f) & M, 42) + c) & M
    y = ((_bswap(((v + w) * mul) & M) + g) * mul) & M
    z = (e + f + c) & M
    a = (_bswap(((x + z) * mul + y) & M) + b) & M
    b = (_smix(((z + a) * mul + d + h) & M) * mul) & M
    return (b + x) & M


def _weak(s, i, a, b):
    w, x, y, z = _f64(s, i), _f64(s, i + 8), _f64(s, i + 16), _f64(s, i + 24)
    a = (a + w) & M
    b = _rot((b + a + z) & M, 21)
    c = a
    a = (a + x + y) & M
    b = (b + _rot(a, 44)) & M
    return (a + z) & M, (b + c) & M


def cityhash64(s: bytes) -> int:
    n = len(s)
    if n <= 32:
        return _len0to16(s, n) if n <= 16 else _len17to32(s, n)
    if n <= 64:
        return _len33to64(s, n)
    x = _f64(s, n - 40)
    y = (_f64(s, n - 16) + _f64(s, n - 56)) & M
    z = _h16((_f64(s, n - 48) + n) & M, _f64(s, n - 24))
    v = _weak(s, n - 64, n, z)
    w = _weak(s, n - 32, (y + K1) & M, x)
    x = (x * K1 + _f64(s, 0)) & M
    rem = (n - 1) & ~63
    p = 0
    while True:
        x = (_rot((x + y + v[0] + _f64(s, p + 8)) & M, 37) * K1) & M
        y = (_rot((y + v[1] + _f64(s, p + 48)) & M, 42) * K1) & M
        x ^= w[1]
        y = (y + v[0] + _f64(s, p + 40)) & M
        z = (_rot((z + w[0]) & M, 33) * K1) & M
        v = _weak(s, p, (v[1] * K1) & M, (x + w[0]) & M)
        w = _weak(s, p + 32, (z + w[1]) & M, (y + _f64(s, p + 16)) & M)
        z, x = x, z
        p += 64
        rem -= 64
        if rem == 0:
            break
    return _h16((_h16(v[0], w[0]) + _smix(y) * K1 + z) & M, (_h16(v[1], w[1]) + x) & M)


def fname_hash(name: str) -> int:
    """Hash stored in Zen name batches (lower-case; ASCII bytes or UTF-16LE)."""
    low = name.lower()
    try:
        return cityhash64(low.encode("ascii"))
    except UnicodeEncodeError:
        return cityhash64(low.encode("utf-16-le"))
