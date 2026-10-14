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
ROUND 4 (full look, Mac-only) adds, for EVERY mapping (exact or renamed):
  slot gate  a mod texture may feed a base slot only if
               mod   its layout is a plain mip chain (texture_layout.plain_mips:
                     export mip table + bulk chunk size), i.e. not a virtual texture,
               base  (1) every texture the game binds to that slot is non-virtual in
                     the registry (VirtualTextureStreaming False), and (2) the
                     parent's own default textures (its imports) of that slot's class
                     (normal / colour / linear) are ALL non-virtual and at least one
                     exists: a virtual sampler must have a virtual default of its
                     class, so no virtual sampler of that class exists. A virtual-
                     texture sampler cannot sample a plain mod texture, so a slot
                     failing (1) or (2) is LOST,
               class a BC5 normal map only feeds a normal slot and vice versa.
  letters    for EVERY mapping: if the mod texture name or any bound game
             texture name ends in packed letters (ORM, MRO ...), all of them must
             name the same letters (so an exact name like 'Mask' cannot read an ORM
             texture through an MRO slot).
  packed     a packed mask keeps its slot only with the SAME channel letters in
             the same order (rename allowed, swizzle never): letters from (1) the
             parameter names on both sides and (2) the texture names on both
             sides (the mod texture bound to it; every game texture bound to the
             base slot, which must also be linear in the registry). Any other
             order (ORM vs MRO ...) is lost; texture bytes are never changed.
  colour+alpha  'base_color_alpha' slots (a base-colour word plus alpha/opacity)
             take a mod colour (base_color or base_color_alpha role) only when
             (mod) the material is masked, its texture format carries alpha, its
             alpha is a cut-out (texture_layout.cutout_alpha), and the instance
             binds no other texture named alpha/opacity/mask, and
             (base) every game texture bound to the slot is sRGB AND has an alpha
             channel in the registry. Otherwise the slot is lost.
Texture bindings are decoded from the instance's own export bytes
(TextureParameterValues: u32 count, then per entry 4 header bytes, FName
(8), 5 bytes, i32 import index, 16-byte GUID; 37-byte stride), a layout read from
the data (mod PC cook and base PS5 cook agree). Ambiguous or missing -> no
binding (fail closed).
"""
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import texture_layout as tlay  # noqa: E402

TOKENS = re.compile(r'[a-z0-9]+')
BASE_COLOR = {'basecolor', 'basecolour', 'albedo', 'diffuse'}
NORMAL = {'normal', 'normals', 'normalmap'}
FILLER = {'texture', 'tex', 'map', 'sampler'}
SUBJECT = {'hair', 'fur'}      # the subject of the texture, not its channel content
ALPHA = {'alpha', 'opacity'}
PACKED_LETTERS = set('ormash')
COLOR_FORMATS = {'PF_DXT1', 'PF_DXT3', 'PF_DXT5', 'PF_BC7', 'PF_B8G8R8A8', 'PF_R8G8B8A8', 'PF_FloatRGBA'}
NORMAL_FORMATS = {'PF_BC5'}
STRIDE = 37
PF = re.compile(rb'(PF_[A-Za-z0-9]+)\x00')


def role_of(name):
    """-> 'base_color' | 'base_color_alpha' | 'normal' | 'packed:<LETTERS>' | None.
    Filler words (texture/tex/map/sampler) and subject words (hair/fur) are
    ignored; any other extra word (tint, strength, wet, detail ...) means a
    different parameter, so no role."""
    words = [w for w in TOKENS.findall(name.lower()) if w not in FILLER]
    core = ''.join(w for w in words if w not in SUBJECT)
    if core in BASE_COLOR or core in NORMAL:
        return 'base_color' if core in BASE_COLOR else 'normal'
    if any(w in ALPHA for w in words):
        rest = [w for w in words if w not in ALPHA and w not in SUBJECT]
        if len(rest) == 1 and rest[0] in BASE_COLOR:
            return 'base_color_alpha'
        return None
    if 3 <= len(core) <= 4 and set(core) <= PACKED_LETTERS and len(set(core)) == len(core) \
            and len(words) == 1:
        return 'packed:' + core.upper()
    return None


def name_letters(package):
    """Packed-channel letters from a texture package's own name suffix
    ('/X/T_Body_ORM' -> 'ORM'), or None."""
    last = package.rsplit('/', 1)[-1].rsplit('_', 1)[-1].lower()
    if 3 <= len(last) <= 4 and set(last) <= PACKED_LETTERS and len(set(last)) == len(last):
        return last.upper()
    return None


def tex_class(tags):
    """Registry tags -> 'normal' | 'colour' | 'linear' | None."""
    if not tags or tags.get('CompressionSettings') is None or tags.get('SRGB') not in ('True', 'False'):
        return None
    if tags['CompressionSettings'] == 'TC_Normalmap':
        return 'normal'
    return 'colour' if tags['SRGB'] == 'True' else 'linear'


def slot_gate(q, mod_tex_fact, mod_fmt, base_bind, tex_tags, defaults):
    """None if the mod texture may feed base slot q, else why not (fail closed)."""
    if not mod_tex_fact or not mod_tex_fact.get('proven'):
        return 'mod texture layout not proven plain (could be virtual): ' + \
            ((mod_tex_fact or {}).get('why') or 'not checked')
    bound = sorted((base_bind or {}).get(q, ()))
    if not bound:
        return 'no fetched base instance binds a texture to it'
    tags = [tex_tags(t) for t in bound]
    if any(t is None or t.get('VirtualTextureStreaming') not in ('True', 'False') for t in tags):
        return 'virtual-texture status of a bound game texture unknown'
    if any(t['VirtualTextureStreaming'] == 'True' for t in tags):
        return 'game binds VIRTUAL textures to this slot; a plain mod texture cannot feed a virtual sampler'
    classes = {tex_class(t) for t in tags}
    if len(classes) != 1 or None in classes:
        return f'bound game textures disagree on their class {sorted(map(str, classes))}'
    cls = next(iter(classes))
    same = [d for d in (defaults or []) if d and tex_class(d) == cls]
    if any(d.get('VirtualTextureStreaming') not in ('True', 'False') for d in same):
        return f'virtual-texture status of a {cls} default texture of the parent unknown'
    if any(d['VirtualTextureStreaming'] == 'True' for d in same):
        return (f'parent has a VIRTUAL {cls} default texture, so one of its {cls} samplers is virtual and this '
                f'slot cannot be proven plain (second source missing)')
    if not same:
        return f'parent has no {cls} default texture (second source for a plain {cls} slot missing)'
    if (mod_fmt in NORMAL_FORMATS) != (cls == 'normal'):
        return f'sampler class mismatch: mod {mod_fmt or "unknown"} vs game {cls} slot'
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
    if role == 'base_color_alpha':
        return fmt in tlay.ALPHA_FORMATS
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
    if role == 'base_color_alpha':
        return tags.get('SRGB') == 'True' and tags.get('HasAlphaChannel') == 'True' and cs != 'TC_Normalmap'
    if role == 'base_color':
        return tags.get('SRGB') == 'True' and cs not in ('TC_Normalmap', 'TC_Masks', 'TC_Grayscale', 'TC_Alpha',
                                                          'TC_VectorDisplacementmap', 'TC_DistanceFieldFont')
    return False


def map_params(params, mod_bind, mod_formats, base_names, base_bind, tex_tags, mod_names,
               mod_tex=None, defaults=None, mod_masked=False):
    """params: mod instance parameter names. mod_bind: {param: mod texture package}
    mod_formats: {mod texture package: PF_*}. base_names: names of the base parent.
    base_bind: {base param: set(base texture packages)} from fetched base instances.
    tex_tags: callable(package) -> registry tags or None.
    mod_tex: callable(mod texture package) -> texture_layout facts
             {'proven', 'format', 'cutout': {...}} or None (None -> nothing maps).
    defaults: registry tags of the parent's own default textures.
    mod_masked: the mod material is masked (two readers).
    -> (mapping {param: {to, how, evidence}}, lost {param: why})"""
    mapping, lost = {}, {}
    base_set = set(base_names)
    mod_bind = mod_bind or {}
    base_bind = base_bind or {}

    def gate(p, q):
        tex = mod_bind.get(p)
        if tex is None:
            return 'mod instance binds no texture to it'
        names = [tex] + sorted(base_bind.get(q, ()))
        letters = {name_letters(t) for t in names}
        if letters - {None} and len(letters) != 1:
            return (f'texture names disagree on packed channel letters {sorted(map(str, letters))} '
                    f'({", ".join(t.rsplit("/", 1)[-1] for t in names)})')
        fact = mod_tex(tex) if mod_tex else None
        return slot_gate(q, fact, mod_formats.get(tex), base_bind, tex_tags, defaults)

    alpha_words = {w for q in params if q in mod_bind
                   for w in TOKENS.findall(q.lower()) if w in ALPHA | {'mask', 'opacitymask'}}

    def alpha_ok(p):
        """None if mod parameter p may feed a colour+alpha slot, else why not."""
        tex = mod_bind.get(p)
        fact = (mod_tex(tex) if (mod_tex and tex) else None) or {}
        cut = fact.get('cutout') or {}
        if not mod_masked:
            return 'mod material not masked'
        if mod_formats.get(tex) not in tlay.ALPHA_FORMATS:
            return f'mod texture format {mod_formats.get(tex) or "unknown"} carries no alpha'
        if not cut.get('cutout'):
            return 'mod texture alpha is not a proven cut-out: ' + str(cut.get('why') or 'not checked')
        own = {w for w in TOKENS.findall(p.lower()) if w in ALPHA}
        if alpha_words - own:
            return (f'the mod instance binds another texture named {sorted(alpha_words - own)}: its cut-out may '
                    'come from there, not from this alpha')
        return None

    def packed_ok(p, q, letters):
        tex = mod_bind.get(p)
        if tex is None or name_letters(tex) != letters:
            return f'mod texture name does not repeat the letters {letters} (second source missing)'
        bound = sorted(base_bind.get(q, ()))
        if not bound:
            return 'no fetched base instance binds a texture to it'
        for t in bound:
            if name_letters(t) != letters:
                return f'game texture {t.rsplit("/", 1)[-1]} bound to {q} does not name the letters {letters}'
            if tex_class(tex_tags(t)) != 'linear':
                return f'game texture bound to {q} is not linear (sRGB or unknown)'
        return None

    for p in params:
        r = role_of(p)
        if p in base_set:
            if not base_bind.get(p):
                lost[p] = 'same name in the base parent, but no fetched base instance binds a texture to it'
                continue
            if r and r.startswith('packed:'):
                why = packed_ok(p, p, r[7:])
                if why:
                    lost[p] = f'packed mask {r[7:]} (same name): ' + why
                    continue
            why = gate(p, p)
            if why:
                lost[p] = why
                continue
            mapping[p] = {'to': p, 'how': 'exact'}
            continue
        if r is None:
            lost[p] = 'no known role and no same-named base parameter'
            continue
        if r.startswith('packed:'):
            letters = r[7:]
            same = sorted(q for q in base_set if role_of(q) == r and q not in mod_names)
            others = sorted(q for q in base_set if (role_of(q) or '').startswith('packed:') and role_of(q) != r)
            if not same:
                lost[p] = (f'packed mask {letters}: no base slot with the same channel order; base packed slots '
                           f'{others or "none"} would need a channel swizzle (forbidden: texture bytes are never '
                           'changed); they keep their defaults')
                continue
            ok = []
            whys = {}
            for q in same:
                why = packed_ok(p, q, letters) or gate(p, q)
                if why:
                    whys[q] = why
                else:
                    ok.append(q)
            if len(ok) != 1:
                lost[p] = f'packed mask {letters}: {len(ok)} usable same-order slots {same} (need 1): {whys}'
                continue
            mapping[p] = {'to': ok[0], 'how': 'role:' + r,
                          'evidence': {'mod_texture': mod_bind.get(p), 'letters': letters,
                                       'base_textures': sorted(base_bind[ok[0]])}}
            continue
        tex = mod_bind.get(p)
        fmt = mod_formats.get(tex) if tex else None
        targets = {r}
        alpha_why = alpha_ok(p)
        if r == 'base_color' and alpha_why is None:
            targets.add('base_color_alpha')
        qs = sorted(q for q in base_set if role_of(q) in targets and q not in mod_names)
        if not qs:
            extra = sorted(q for q in base_set if role_of(q) == 'base_color_alpha')
            lost[p] = f'base parent has no {r} parameter' + (
                f' (colour+alpha slot {extra} refused: {alpha_why})' if extra and alpha_why else '')
            continue
        if not mod_evidence(r, fmt):
            lost[p] = f'{r}: mod texture format {fmt or "unknown"} does not confirm the layout (second source missing)'
            continue
        ok, whys = [], {}
        for q in qs:
            rq = role_of(q)
            if rq == 'base_color_alpha' and alpha_why:
                whys[q] = 'colour+alpha slot refused: ' + alpha_why
                continue
            confirmed = [t for t in base_bind.get(q, ()) if base_evidence(rq, tex_tags(t))]
            if not confirmed or len(confirmed) != len(base_bind.get(q, ())):
                whys[q] = (f'{rq}: game textures bound to it do not all confirm the layout '
                           f'({len(confirmed)}/{len(base_bind.get(q, ()))})')
                if rq == 'base_color_alpha':
                    whys[q] += ('; the game binds colour textures WITHOUT an alpha channel here (registry), so the '
                                "game's cut-out does not come from this slot's alpha and the mod's alpha "
                                'cut-out would be ignored')
                continue
            why = gate(p, q)
            if why:
                whys[q] = why
                continue
            ok.append(q)
        if len(ok) != 1:
            lost[p] = (f'{r}: {len(ok)} base parameters of {qs} confirmed by bound base textures and the slot '
                       f'gate (need exactly 1; second source missing or ambiguous): {whys}')
            continue
        q = ok[0]
        mapping[p] = {'to': q, 'how': 'role:' + role_of(q),
                      'evidence': {'mod_texture': tex, 'mod_format': fmt,
                                   'base_textures': sorted(base_bind[q])}}
        if role_of(q) == 'base_color_alpha':
            mapping[p]['evidence']['cutout'] = (mod_tex(tex) or {}).get('cutout')
    targets = {}
    for p, v in mapping.items():
        targets.setdefault(v['to'], []).append(p)
    for q, ps in targets.items():
        if len(ps) > 1:
            for p in ps:
                del mapping[p]
                lost[p] = f'{len(ps)} mod parameters {sorted(ps)} would map to the same base parameter {q}'
    return mapping, lost
