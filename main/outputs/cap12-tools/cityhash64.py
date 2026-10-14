#!/usr/bin/env python3
"""Faithful pure-python port of CityHash64 (Google cityhash v1.1, MIT license;
reference city.cc fetched to outputs/cap12-tools/city.cc and sha-recorded).

Purpose (capability #12, Mac-side research only): reproduce UE IoStore
lower_utf16_cityhash derivations against the game's own TOC ids, the same
derivation the pinned retoc source (name_map.rs / lib.rs) performs.

Verified locally against (see cap12-research-notes.md):
  - container id oracle: CityHash64(utf16le(lower(basename))) == FIoContainerId
    for the Leon mod utoc and base pakchunk0-ps5.utoc.
  - shader-library chunk-id oracle: matches type-8 chunk ids in the game TOCs
    for names of the form "<library>-<format>" (exact matches recorded in the
    research notes; no guessing in code).
"""

MASK = (1 << 64) - 1
K0 = 0xc3a5c85c97cb3127
K1 = 0xb492b66fbe98f273
K2 = 0x9ae16a3b2f90404f
K3 = 0xc949d7c7509e6557  # only used by CityHash128; kept for reference fidelity


def _rot(val, shift):
    return val if shift == 0 else ((val >> shift) | (val << (64 - shift))) & MASK


def _shift_mix(val):
    return (val ^ (val >> 47)) & MASK


def _fetch64(s, i=0):
    return int.from_bytes(s[i:i + 8], 'little')


def _fetch32(s, i=0):
    return int.from_bytes(s[i:i + 4], 'little')


def _bswap64(v):
    return int.from_bytes((v & MASK).to_bytes(8, 'little'), 'big')


def _hash_len16(u, v, mul):
    a = ((u ^ v) * mul) & MASK
    a ^= a >> 47
    b = ((v ^ a) * mul) & MASK
    b ^= b >> 47
    b = (b * mul) & MASK
    return b


def _weak_hash_len32_with_seeds(w, x, y, z, a, b):
    a = (a + w) & MASK
    b = _rot((b + a + z) & MASK, 21)
    c = a
    a = (a + x + y) & MASK
    b = (b + _rot(a, 44)) & MASK
    return ((a + z) & MASK, (b + c) & MASK)


def _weak_hash_len32_with_seeds_s(s, a, b):
    return _weak_hash_len32_with_seeds(
        _fetch64(s, 0), _fetch64(s, 8), _fetch64(s, 16), _fetch64(s, 24), a, b)


def city_hash64(s: bytes) -> int:
    ln = len(s)
    if ln <= 32:
        if ln <= 16:
            return _hash_len0to16(s, ln)
        return _hash_len17to32(s, ln)
    if ln <= 64:
        return _hash_len33to64(s, ln)
    return _hash_len_over64(s, ln)


def _hash_len0to16(s, ln):
    if ln >= 8:
        mul = (K2 + ln * 2) & MASK
        a = (_fetch64(s) + K2) & MASK
        b = _fetch64(s, ln - 8)
        c = ((_rot(b, 37) * mul) + a) & MASK
        d = ((_rot(a, 25) + b) * mul) & MASK
        return _hash_len16(c, d, mul)
    if ln >= 4:
        mul = (K2 + ln * 2) & MASK
        a = _fetch32(s)
        return _hash_len16(ln + (a << 3), _fetch32(s, ln - 4), mul)
    if ln > 0:
        a = s[0]
        b = s[ln >> 1]
        c = s[ln - 1]
        y = a + (b << 8)
        z = ln + (c << 2)
        return (_shift_mix((y * K2 ^ z * K0) & MASK) * K2) & MASK
    return K2


def _hash_len17to32(s, ln):
    mul = (K2 + ln * 2) & MASK
    a = (_fetch64(s) * K1) & MASK
    b = _fetch64(s, 8)
    c = (_fetch64(s, ln - 8) * mul) & MASK
    d = (_fetch64(s, ln - 16) * K2) & MASK
    return _hash_len16(
        ((_rot((a + b) & MASK, 43) + _rot(c, 30) + d) & MASK),
        ((a + _rot((b + K2) & MASK, 18) + c) & MASK),
        mul)


def _hash_len33to64(s, ln):
    mul = (K2 + ln * 2) & MASK
    a = (_fetch64(s) * K2) & MASK
    b = _fetch64(s, 8)
    c = _fetch64(s, ln - 24)
    d = _fetch64(s, ln - 32)
    e = (_fetch64(s, 16) * K2) & MASK
    f = (_fetch64(s, 24) * 9) & MASK
    g = _fetch64(s, ln - 8)
    h = (_fetch64(s, ln - 16) * mul) & MASK
    u = (_rot((a + g) & MASK, 43) + ((_rot(b, 30) + c) & MASK) * 9) & MASK
    v = ((((a + g) & MASK) ^ d) + f + 1) & MASK
    w = (_bswap64(((u + v) & MASK) * mul) + h) & MASK
    x = (_rot((e + f) & MASK, 42) + c) & MASK
    y = ((_bswap64(((v + w) & MASK) * mul) + g) * mul) & MASK
    z = (e + f + c) & MASK
    a2 = (_bswap64((((x + z) & MASK) * mul + y) & MASK) + b) & MASK
    b2 = (_shift_mix((((z + a2) & MASK) * mul + d + h) & MASK) * mul) & MASK
    return (b2 + x) & MASK


def _hash_len_over64(s, ln):
    x = _fetch64(s, ln - 40)
    y = (_fetch64(s, ln - 16) + _fetch64(s, ln - 56)) & MASK
    z = _hash_len16((_fetch64(s, ln - 48) + ln) & MASK, _fetch64(s, ln - 24), 0x9ddfea08eb382d69)
    v = _weak_hash_len32_with_seeds_s(s[ln - 64:], ln, z)
    w = _weak_hash_len32_with_seeds_s(s[ln - 32:], (y + K1) & MASK, x)
    x = (x * K1 + _fetch64(s)) & MASK

    ln = (ln - 1) & ~63
    i = 0
    while ln != 0:
        x = (_rot((x + y + v[0] + _fetch64(s, i + 8)) & MASK, 37) * K1) & MASK
        y = (_rot((y + v[1] + _fetch64(s, i + 48)) & MASK, 42) * K1) & MASK
        x ^= w[1]
        y = (y + v[0] + _fetch64(s, i + 40)) & MASK
        z = ((_rot((z + w[0]) & MASK, 33)) * K1) & MASK
        v = _weak_hash_len32_with_seeds_s(s[i:], (v[1] * K1) & MASK, (x + w[0]) & MASK)
        w = _weak_hash_len32_with_seeds_s(s[i + 32:], (z + w[1]) & MASK, (y + _fetch64(s, i + 16)) & MASK)
        z, x = x, z
        i += 64
        ln -= 64
    return _hash_len16(
        (_hash_len16(v[0], w[0], 0x9ddfea08eb382d69) + ((_shift_mix(y) * K1) & MASK) + z) & MASK,
        (_hash_len16(v[1], w[1], 0x9ddfea08eb382d69) + x) & MASK,
        0x9ddfea08eb382d69)


def lower_utf16_cityhash(name: str) -> int:
    """UE derivation (retoc name_map.rs:18): hash(lower(name) as UTF-16LE)."""
    low = name.lower().encode('utf-16-le')
    return city_hash64(low)


if __name__ == '__main__':
    import sys
    for arg in sys.argv[1:]:
        print(f"{arg}\t{lower_utf16_cityhash(arg):016x}")
