#!/usr/bin/env python3
"""Material domain / blend mode / shading model, read from TWO independent
sources in the game's own files (Mac-only, read-only, generic UE5; no game,
mod or asset names).

Base-game material (candidate parent):
  source 1  the game's AssetRegistry.bin tags of the master Material
            (MaterialDomain, BlendMode, ShadingModel, ShadingModels). For a
            MaterialInstanceConstant the master is found through the registry's
            'Parent' tags; where the instance's header was fetched, its imports
            must name the same parent (else: fail closed).
  source 2  the candidate's OWN compiled PS5 shader maps: container-header store
            -> shader library manifest -> per-shader type hashes (the game's
            ShaderTypeInfo .stinfo file, one entry per library shader). The set of
            (shader type, frequency) pairs is compared with every OTHER game
            material that compiles exactly the same set (leave-one-out peers,
            the candidate and its own master excluded). The value is read only
            when >= MIN_PEERS peers exist and all of them have the same
            (domain, blend group, lit/unlit) in the registry.
  readable = both sources give the same (domain, blend group, lit/unlit).
  Note: source 2 is read from the candidate's own shader maps, but it is
  interpreted with OTHER materials' registry tags; a registry error repeated
  across all peers would pass both. The candidate's own tags never feed source 2.
  The exact lit shading model (DefaultLit vs Cloth vs Subsurface ...) is NOT
  visible in compiled shader types (measured on the game's 418 materials):
  it has one source only, so it is never used to accept; it is disclosed, and
  special models (Hair, Eye, SingleLayerWater, ThinTranslucent) refuse a
  parent on that one source (refusing is the fail-closed direction).

Mod material (PC-cooked, inline PC shader maps):
  reader A  shader type hashes found in the mod material's inline shader map
            (same hashes as the game's .stinfo). A (domain, blend group) class of
            the game is excluded when the mod has a type that no game material of
            that class compiles, or lacks a type that ALL game materials of that
            class compile and some other class never does (a class marker).
  reader B  the base-pass uniform buffer named in the mod's PC shader code:
            'OpaqueBasePass' (opaque or masked) or 'TranslucentBasePass'.
  masked  = B says opaque/masked and A keeps only masked (surface).
  opaque  is NOT provable this way: masked game materials that compile no
            masked-only shader types exist, so "A keeps opaque and masked" is
            returned as readable=False with unproven_opaque=True (fail closed).
            reparent_plan accepts it only with --accept-unproven-opaque (an
            explicit user decision; disclosed: a masked cut-out would be filled).
  Every surface blend group must have >= MIN_CLASS game materials, or reader A
  cannot rule groups in or out and the class is unreadable.
"""
import collections
import struct
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cityhash  # noqa: E402

MIN_PEERS = 3          # other materials with the identical compiled type set
MIN_CLASS = 10         # game materials needed before a class is used for mod reading
SURFACE = 'MD_Surface'
OPAQUE, MASKED, TRANSLUCENT = 'opaque', 'masked', 'translucent'
SPECIAL_MODELS = ('MSM_Hair', 'MSM_Eye', 'MSM_SingleLayerWater', 'MSM_ThinTranslucent')
FROM_EXPRESSION = 'MSM_FromMaterialExpression'
MATERIAL_AR_CLASS = '/Script/Engine.Material'


class ClassError(ValueError):
    pass


def need(ok, why):
    if not ok:
        raise ClassError(why)


def load_stinfo(data):
    """ShaderTypeInfo: u32 count, then per library shader: u32 n + n u64 type hashes."""
    need(len(data) >= 4, 'stinfo too short')
    n = struct.unpack_from('<I', data, 0)[0]
    p, out = 4, []
    for _ in range(n):
        need(p + 4 <= len(data), 'stinfo truncated')
        k = struct.unpack_from('<I', data, p)[0]
        p += 4
        need(0 < k <= 64 and p + 8 * k <= len(data), 'stinfo bad entry')
        out.append(struct.unpack_from('<%dQ' % k, data, p))
        p += 8 * k
    need(p == len(data), 'stinfo trailing bytes')
    return out


class TypeIndex:
    """(library, stinfo) pairs + the game's container-header store -> per-package
    compiled shader type signature."""

    def __init__(self, pairs, store):
        self.libs = []
        for lib, st in pairs:
            need(len(st) == len(lib.shader_hashes),
                 f'stinfo has {len(st)} entries but its library has {len(lib.shader_hashes)} shaders')
            self.libs.append((lib, st))
        need(self.libs, 'no library with shader type info')
        self.store = store
        self.universe = {t for _, st in self.libs for v in st for t in v}

    def signature(self, package):
        e = self.store.get(cityhash.ue_name_hash(package))
        if e is None:
            return None, 'not in any loaded container-header store'
        if not e['shader_maps']:
            return None, 'no target shader maps'
        sig = set()
        for mh in e['shader_maps']:
            for lib, st in self.libs:
                sm = lib.shader_map(mh)
                if sm is not None:
                    for s in sm:
                        sig.update((t, s['frequency']) for t in st[s['shader_index']])
                    break
            else:
                return None, 'a shader map is in no library that has shader type info'
        return frozenset(sig), None


def shading_bits(registry):
    """bit -> model name, learned from materials whose ShadingModel is a single
    model (ShadingModels field == 1 << bit). Data-derived, not hard-coded."""
    seen = collections.defaultdict(set)
    for a in registry.assets:
        if a['class'] != MATERIAL_AR_CLASS:
            continue
        sm, f = a['tags'].get('ShadingModel'), _field(a['tags'])
        if sm and sm != FROM_EXPRESSION and f and f & (f - 1) == 0:
            seen[f.bit_length() - 1].add(sm)
    return {b: next(iter(v)) for b, v in seen.items() if len(v) == 1}


def _field(tags):
    v = tags.get('ShadingModels') or ''
    if '=' not in v:
        return None
    try:
        return int(v.split('=', 1)[1].strip(' )'))
    except ValueError:
        return None


def ar_class(tags, bits):
    """Registry tags -> {domain, blend, blend_group, lit, models, special} or (None, why)."""
    dm, bm, sm = tags.get('MaterialDomain'), tags.get('BlendMode'), tags.get('ShadingModel')
    if not (dm and bm and sm):
        return None, 'registry has no MaterialDomain/BlendMode/ShadingModel tags'
    if sm == FROM_EXPRESSION:
        f = _field(tags)
        if not f:
            return None, 'ShadingModels field unreadable'
        models = set()
        for b in range(f.bit_length()):
            if f >> b & 1:
                if b not in bits:
                    return None, f'shading model bit {b} has no name in this registry'
                models.add(bits[b])
    else:
        models = {sm}
    unlit = 'MSM_Unlit' in models
    lit = 'unlit' if models == {'MSM_Unlit'} else ('lit' if not unlit else 'lit+unlit')
    group = {'BLEND_Opaque': OPAQUE, 'BLEND_Masked': MASKED}.get(bm, TRANSLUCENT)
    return {'domain': dm, 'blend': bm, 'blend_group': group, 'lit': lit, 'models': sorted(models),
            'special': sorted(models & set(SPECIAL_MODELS))}, None


def key_of(c):
    return (c['domain'], c['blend_group'], c['lit'])


class Peers:
    """Population: every registry Material with readable tags and a signature."""

    def __init__(self, registry, typeidx):
        self.bits = shading_bits(registry)
        self.by_sig = collections.defaultdict(list)
        self.pop = []
        for a in registry.assets:
            if a['class'] != MATERIAL_AR_CLASS:
                continue
            c, _ = ar_class(a['tags'], self.bits)
            sig, _ = typeidx.signature(a['package'])
            if c and sig:
                self.by_sig[sig].append((a['package'], key_of(c)))
                self.pop.append((a['package'], c, sig))
        need(self.pop, 'zero game materials with registry tags and shader type signatures')

    def value(self, sig, exclude):
        others = [k for p, k in self.by_sig.get(sig, ()) if p not in exclude]
        if len(others) < MIN_PEERS:
            return None, f'only {len(others)} other game materials compile the same shader types (need {MIN_PEERS})'
        cnt = collections.Counter(others)
        if len(cnt) > 1:
            return None, 'game materials with the same shader types differ: ' + \
                ', '.join(f'{"/".join(k)} x{n}' for k, n in cnt.most_common())
        return {'value': others[0], 'peers': len(others)}, None

    # ---- mod side (reader A) ----
    def type_presence(self):
        n = collections.Counter()
        pres = collections.defaultdict(collections.Counter)
        for _p, c, sig in self.pop:
            k = (c['domain'], c['blend_group'])
            n[k] += 1
            for t in {t for t, _f in sig}:
                pres[t][k] += 1
        return n, pres


def master_of(package, registry, headers, depth=16):
    """Follow registry 'Parent' tags to a Material. Where a header was fetched,
    its imported packages must contain the registry parent. -> (master, chain) or (None, why)."""
    chain, p = [package], package
    for _ in range(depth):
        a = registry.asset(p)
        if a is None:
            return None, f'{p} is not in the asset registry'
        if a['class'] == MATERIAL_AR_CLASS:
            return p, chain
        par = registry.parent_package(p)
        if par is None:
            return None, f'{p} has no registry parent'
        h = headers.get(p)
        if h is not None and par not in h.get('imports', []):
            return None, f'registry parent of {p} ({par}) is not imported by its fetched header'
        chain.append(par)
        p = par
    return None, 'parent chain too deep'


def base_props(package, registry, typeidx, peers, headers):
    """Two-source class of a base candidate. Always returns a dict; 'readable'
    False carries 'why'."""
    out = {'readable': False}
    m, chain = master_of(package, registry, headers)
    if m is None:
        out['why'] = chain
        return out
    out['master'], out['chain'] = m, chain
    c1, why = ar_class(registry.asset(m)['tags'], peers.bits)
    if c1 is None:
        out['why'] = 'source 1 (asset registry): ' + why
        return out
    out.update(c1)
    sig, why = typeidx.signature(package)
    if sig is None:
        out['why'] = 'source 2 (compiled shader types): ' + why
        return out
    v2, why = peers.value(sig, set(chain))
    if v2 is None:
        out['why'] = 'source 2 (compiled shader types): ' + why
        return out
    out['sources'] = {'asset_registry': '/'.join(key_of(c1)), 'shader_type_peers': '/'.join(v2['value']),
                      'peers': v2['peers']}
    if v2['value'] != key_of(c1):
        out['why'] = (f"sources disagree: asset registry says {'/'.join(key_of(c1))}, "
                      f"compiled shader types say {'/'.join(v2['value'])}")
        return out
    out['readable'] = True
    return out


def _opaque_types(peers):
    return {t for _p, c, sig in peers.pop if c['blend_group'] == OPAQUE for t, _f in sig}


def _masked_sets(peers):
    return [{t for t, _f in sig} for _p, c, sig in peers.pop if (c['domain'], c['blend_group']) == (SURFACE, MASKED)]


def mod_props(blob, peers):
    """Two-reader class of a mod material from its own package bytes (inline PC
    shader maps). Returns dict with 'readable', 'blend_group', 'why'."""
    n, pres = peers.type_presence()
    classes = [k for k, v in n.items() if v >= MIN_CLASS]
    universe = set(pres)
    hashes = set()
    for i in range(len(blob) - 7):
        v = struct.unpack_from('<Q', blob, i)[0]
        if v in universe:
            hashes.add(v)
    out = {'readable': False, 'type_hashes_found': len(hashes)}
    if not hashes:
        out['why'] = 'reader A: no known shader type hashes in the mod material'
        return out
    markers = {k: {t for t in universe if pres[t][k] == n[k] and any(pres[t][o] == 0 for o in classes)}
               for k in classes}
    keep = []
    for k in classes:
        if any(pres[t][k] == 0 for t in hashes):
            continue                       # mod compiles a type no game material of k compiles
        if markers[k] - hashes:
            continue                       # mod lacks a type every game material of k compiles
        keep.append(k)
    out['reader_a_classes'] = sorted('/'.join(k) for k in keep)
    has_t, has_o = b'TranslucentBasePass' in blob, b'OpaqueBasePass' in blob
    if has_t and not has_o:
        b, out['reader_b'] = TRANSLUCENT, 'translucent base pass'
    elif has_o and not has_t:
        b, out['reader_b'] = (OPAQUE, MASKED), 'opaque/masked base pass'
    else:
        b, out['reader_b'] = None, 'no single base-pass buffer'
    if b is None:
        out['why'] = 'reader B: base-pass uniform buffer not found (or both kinds)'
        return out
    missing = [g for g in (OPAQUE, MASKED, TRANSLUCENT) if (SURFACE, g) not in classes]
    if missing:
        out['why'] = (f'reader A: fewer than {MIN_CLASS} game surface materials of group(s) {missing}, '
                      'so they cannot be ruled in or out')
        return out
    if not keep or any(k[0] != SURFACE for k in keep):
        out['why'] = 'reader A: not provably a surface material (' + (', '.join(out['reader_a_classes']) or 'no class') + ')'
        return out
    groups = {k[1] for k in keep}
    if isinstance(b, tuple):
        if TRANSLUCENT in groups:
            out['why'] = 'reader A cannot exclude translucent although reader B says opaque/masked'
            return out
        if groups == {MASKED}:
            out.update(readable=True, blend_group=MASKED)
        elif OPAQUE in groups:
            # opaque vs masked: neither reader can prove it (masked game materials without
            # masked-only shader types exist) -> fail closed; the caller may accept it only on
            # an explicit user decision (reparent_plan --accept-unproven-opaque)
            n_m = sum(1 for t_set in _masked_sets(peers) if not (t_set - _opaque_types(peers)))
            out.update(unproven_opaque=True,
                       why=('opaque vs masked not provable by two readers: the mod compiles no masked-only '
                            f'shader types, but {n_m} game masked materials do not either'))
        else:
            out['why'] = 'reader A excludes opaque and masked'
        return out
    if groups == {TRANSLUCENT}:
        out.update(readable=True, blend_group=TRANSLUCENT)
    else:
        out['why'] = 'readers disagree: B says translucent, A keeps ' + ', '.join(sorted(groups))
    return out
