#!/usr/bin/env python3
"""Per-slot DEFAULT texture of a cooked base Material (Mac-only, read-only, generic).

Why: a texture slot can only take a plain (non-virtual) mod texture if its
sampler is not a virtual-texture sampler. A virtual sampler needs a virtual
default texture, so a slot whose OWN default texture is plain has a plain
sampler. This gives a per-slot second source where the class-wide rule
(param_roles.slot_gate: all defaults of the class plain) cannot decide because
the master also has a virtual default for some OTHER slot.

Layout read from the data (the material's cached expression data; no schema
is assumed beyond what is checked):
  * parameter run: u32 N, then N entries of 15 bytes, each starting with an
    FName (name-map index, number 0) of a parameter name, then an association
    byte (0..2) and an int32 parameter index (-1 for global parameters);
  * default-texture array: u32 N (same N), then N soft object paths of 20 bytes:
    FName package path, FName asset name, empty FString (i32 0); the asset name
    must be the last component of the package path, and the package must be
    one of the material's own imported packages.
  The k-th parameter of the run takes the k-th default. Exactly one run may
  have the array's N among the runs whose names are not package paths;
  otherwise the pairing is ambiguous and nothing is returned (fail closed).
The caller must still cross-check the pairing against the game's data
(validate()): for every slot the game's own instances bind textures to, the
paired default must be of the same texture class (normal / colour / linear).
"""
import struct

ENTRY = 15
SOFT = 20


def _runs(b, names):
    """[(offset, [parameter names])] of 15-byte FName entry runs preceded by their count."""
    out = []
    nn = len(names)
    for o in range(0, len(b) - 4 - ENTRY):
        n = struct.unpack_from('<I', b, o)[0]
        if not 1 <= n <= 256 or o + 6 + ENTRY * n > len(b):
            continue
        got = []
        for k in range(n):
            e = o + 6 + ENTRY * k
            i, num = struct.unpack_from('<II', b, e)
            assoc = b[e + 8]
            idx = struct.unpack_from('<i', b, e + 9)[0]
            if num != 0 or i >= nn or names[i].startswith('/') or assoc > 2 or idx < -1 or idx > 255:
                got = None
                break
            got.append(names[i])
        if got and len(set(got)) == len(got):
            out.append((o, got))
    return out


def _soft_arrays(b, names, imported):
    """[(offset, [package paths])] of soft-object-path arrays naming imported packages."""
    out = []
    nn = len(names)
    for o in range(0, len(b) - 4 - SOFT):
        n = struct.unpack_from('<I', b, o)[0]
        if not 1 <= n <= 256 or o + 4 + SOFT * n > len(b):
            continue
        got = []
        for k in range(n):
            e = o + 4 + SOFT * k
            pi, pn, ai, an, sub = struct.unpack_from('<IIIIi', b, e)
            if pn or an or sub or pi >= nn or ai >= nn:
                got = None
                break
            pkg, asset = names[pi], names[ai]
            if not pkg.startswith('/') or pkg.rsplit('/', 1)[-1] != asset or pkg not in imported:
                got = None
                break
            got.append(pkg)
        if got:
            out.append((o, got))
    return out


def slot_defaults(pkg):
    """-> ({parameter: default texture package}, None) or (None, why)."""
    top = [e for e in pkg.exports if e['outer'][0] == 'Null']
    if len(top) != 1:
        return None, 'not exactly one top-level export'
    b = pkg.export_bytes(top[0])
    if b is None:
        return None, 'no export data (header-only fetch)'
    imported = set(pkg.imported_packages)
    arrays = _soft_arrays(b, pkg.names, imported)
    if len(arrays) != 1:
        return None, f'{len(arrays)} default-texture arrays found (need exactly 1)'
    ao, defaults = arrays[0]
    runs = [(o, r) for o, r in _runs(b, pkg.names) if len(r) == len(defaults) and o < ao]
    if len(runs) != 1:
        return None, f'{len(runs)} parameter runs of {len(defaults)} entries before the default array (need 1)'
    return dict(zip(runs[0][1], defaults)), None


def validate(pairs, bound, tex_class, virtual=None):
    """Cross-check with the game's data: bound {slot: set(game textures)},
    tex_class(package) -> 'normal'|'colour'|'linear'|None; virtual(package) ->
    'True'|'False'|None (registry VirtualTextureStreaming). Every bound slot of the
    pairing must have a default of the same class AND the same virtual-texture
    status as ALL its bound textures (a sampler's default and its textures share
    the virtual-texture type), so two same-class slots swapped in the pairing are
    caught when their virtual status differs.
    -> (checked slot count, None) or (n, why)."""
    n = 0
    for q, ts in sorted(bound.items()):
        if q not in pairs:
            continue
        classes = {tex_class(t) for t in ts}
        d = tex_class(pairs[q])
        if None in classes or d is None or classes != {d}:
            return n, f'slot {q}: default {pairs[q].rsplit("/", 1)[-1]} is {d}, bound game textures are {sorted(map(str, classes))}'
        if virtual is not None:
            vs = {virtual(t) for t in ts}
            dv = virtual(pairs[q])
            if None in vs or dv is None or vs != {dv}:
                return n, (f'slot {q}: default {pairs[q].rsplit("/", 1)[-1]} virtual={dv}, bound game textures '
                           f'virtual={sorted(map(str, vs))}')
        n += 1
    if n == 0:
        return 0, 'no bound slot to cross-check the pairing'
    return n, None
