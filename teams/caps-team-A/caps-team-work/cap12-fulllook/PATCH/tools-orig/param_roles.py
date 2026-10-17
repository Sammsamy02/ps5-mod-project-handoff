#!/usr/bin/env python3
"""Map a mod material instance's texture parameters onto a base parent's
parameters (Mac-only, generic; no game, mod or asset names).

Order of trust:
  exact     same parameter name in the base parent AND a fetched base instance
            binds a texture to that name (so it is a texture slot): the engine
            binds by name, nothing is renamed.
  role      different names, same role, renamed in the derived package. Only for
            roles whose channel layout is fixed and confirmed by TWO sources:
              base_color  (RGB colour)  1 name tokens on both sides,
                                        2 texture data on both sides: the mod texture
                                          bound to it is a 3/4-channel colour format,
                                          and a base texture bound to the base
                                          parameter is sRGB colour in the game's
                                          asset registry
              normal      (tangent XY)  1 name tokens, 2 mod texture is a 2-channel
                                          BC5 normal format and a base texture bound
                                          to the base parameter is TC_Normalmap
  never     packed masks (ORM / MRO / RMA ...): the channel ORDER is known from the
            name only (one source) and a rename cannot swizzle channels -> not
            remapped; the base parent's own default for that slot is used and the
            loss is disclosed.
Texture bindings are decoded from the instance's own export bytes
(TextureParameterValues: u32 count, then per entry 4 header bytes, FName
(8), 5 bytes, i32 import index, 16-byte GUID; 37-byte stride), a layout read from
the data (mod PC cook and base PS5 cook agree). Ambiguous or missing -> no
binding (fail closed).
"""
import re
import struct

TOKENS = re.compile(r'[a-z0-9]+')
BASE_COLOR = {'basecolor', 'basecolour', 'albedo', 'diffuse'}
NORMAL = {'normal', 'normals', 'normalmap'}
FILLER = {'texture', 'tex', 'map', 'sampler'}
PACKED_LETTERS = set('ormash')
COLOR_FORMATS = {'PF_DXT1', 'PF_DXT3', 'PF_DXT5', 'PF_BC7', 'PF_B8G8R8A8', 'PF_R8G8B8A8', 'PF_FloatRGBA'}
NORMAL_FORMATS = {'PF_BC5'}
STRIDE = 37
PF = re.compile(rb'(PF_[A-Za-z0-9]+)\x00')


def role_of(name):
    """-> 'base_color' | 'normal' | 'packed:<LETTERS>' | None. Filler words
    (texture/tex/map/sampler) are ignored; any other extra word (tint, strength,
    wet, detail ...) means a different parameter, so no role."""
    core = ''.join(w for w in TOKENS.findall(name.lower()) if w not in FILLER)
    if core in BASE_COLOR:
        return 'base_color'
    if core in NORMAL:
        return 'normal'
    if 3 <= len(core) <= 4 and set(core) <= PACKED_LETTERS and len(set(core)) == len(core):
        return 'packed:' + core.upper()
    return None


def texture_bindings(pkg):
    """{parameter name: imported package} from a zen MIC's TextureParameterValues,
    or (None, why)."""
    top = [e for e in pkg.exports if e['outer'][0] == 'Null']
    if len(top) != 1:
        return None, 'not exactly one top-level export'
    b = pkg.export_bytes(top[0])
    if b is None:
        return None, 'no export data (header-only fetch)'
    imp = {}
    for i, ref in enumerate(pkg.imports):
        if ref[0] == 'PackageImport' and ref[1][0] < len(pkg.imported_packages):
            imp[i] = pkg.imported_packages[ref[1][0]]
    runs = []
    for o in range(0, max(0, len(b) - 4 - STRIDE + 1)):
        n = struct.unpack_from('<I', b, o)[0]
        if not 1 <= n <= 64 or o + 4 + n * STRIDE > len(b):
            continue
        ent, hdr = {}, b[o + 4:o + 8]
        for k in range(n):
            e = o + 4 + k * STRIDE
            ni, nn = struct.unpack_from('<II', b, e + 4)
            pi = struct.unpack_from('<i', b, e + 17)[0]
            if b[e:e + 4] != hdr or nn != 0 or ni >= len(pkg.names) or pkg.names[ni].startswith('/') \
                    or pi >= 0 or (-pi - 1) not in imp or pkg.names[ni] in ent:
                ent = None
                break
            ent[pkg.names[ni]] = imp[-pi - 1]
        if ent:
            runs.append(ent)
    if len(runs) != 1:
        return None, f'{len(runs)} texture-parameter arrays found (need exactly 1)'
    return runs[0], None


def pixel_format(pkg):
    """Single PF_* token in a cooked texture package's export data, or None."""
    toks = set()
    for e in pkg.exports:
        blob = pkg.export_bytes(e) or b''
        toks.update(m.group(1).decode() for m in PF.finditer(blob))
    return next(iter(toks)) if len(toks) == 1 else None


def mod_evidence(role, fmt):
    if role == 'normal':
        return fmt in NORMAL_FORMATS
    if role == 'base_color':
        return fmt in COLOR_FORMATS
    return False


def base_evidence(role, tags):
    if not tags:
        return False
    cs = tags.get('CompressionSettings')
    if role == 'normal':
        return cs == 'TC_Normalmap'
    if role == 'base_color':
        return tags.get('SRGB') == 'True' and cs not in ('TC_Normalmap', 'TC_Masks', 'TC_Grayscale', 'TC_Alpha',
                                                          'TC_VectorDisplacementmap', 'TC_DistanceFieldFont')
    return False


def map_params(params, mod_bind, mod_formats, base_names, base_bind, tex_tags, mod_names):
    """params: mod instance parameter names. mod_bind: {param: mod texture package}
    mod_formats: {mod texture package: PF_*}. base_names: names of the base parent.
    base_bind: {base param: set(base texture packages)} from fetched base instances.
    tex_tags: callable(package) -> registry tags or None.
    -> (mapping {param: {to, how, evidence}}, lost {param: why})"""
    mapping, lost = {}, {}
    base_set = set(base_names)
    for p in params:
        if p in base_set:
            if (base_bind or {}).get(p):
                mapping[p] = {'to': p, 'how': 'exact'}
            else:
                lost[p] = 'same name in the base parent, but no fetched base instance binds a texture to it'
            continue
        r = role_of(p)
        if r is None:
            lost[p] = 'no known role and no same-named base parameter'
            continue
        if r.startswith('packed:'):
            others = sorted(q for q in base_set if (role_of(q) or '').startswith('packed:'))
            lost[p] = (f'packed mask {r[7:]}: channel order has one source (the name) and a rename cannot '
                       f'swizzle channels; base packed slots {others or "none"} keep their defaults')
            continue
        qs = sorted(q for q in base_set if role_of(q) == r and q not in mod_names)
        if not qs:
            lost[p] = f'base parent has no {r} parameter'
            continue
        tex = mod_bind.get(p) if mod_bind else None
        fmt = mod_formats.get(tex) if tex else None
        if not mod_evidence(r, fmt):
            lost[p] = f'{r}: mod texture format {fmt or "unknown"} does not confirm the layout (second source missing)'
            continue
        ok = [q for q in qs if any(base_evidence(r, tex_tags(t)) for t in (base_bind or {}).get(q, ()))]
        if len(ok) != 1:
            lost[p] = (f'{r}: {len(ok)} base parameters {qs} confirmed by a bound base texture '
                       '(need exactly 1; second source missing or ambiguous)')
            continue
        q = ok[0]
        mapping[p] = {'to': q, 'how': 'role:' + r,
                      'evidence': {'mod_texture': tex, 'mod_format': fmt,
                                   'base_textures': sorted(t for t in base_bind[q] if base_evidence(r, tex_tags(t)))}}
    targets = {}
    for p, v in mapping.items():
        targets.setdefault(v['to'], []).append(p)
    for q, ps in targets.items():
        if len(ps) > 1:
            for p in ps:
                del mapping[p]
                lost[p] = f'{len(ps)} mod parameters {sorted(ps)} would map to the same base parameter {q}'
    return mapping, lost
