#!/usr/bin/env python3
"""Plan, for every mod material that has no target-platform shaders, what the
overlay does with it, using only data from the files (Mac-only). Generic: no
game, mod or asset names are hard-coded.

Candidate base parents (all must hold, fail closed):
  * a fetched, verified base package header (package_fetch_plan.py check),
  * NOT overridden by the mod (the mod's copy would win at load),
  * the game's own container-header store lists it WITH target shader maps,
  * exactly one top-level Material / MaterialInstanceConstant export (fetched
    header + global ScriptObjects); new_export = that export's own name,
  * skeletal-mesh use evidence: a fetched base SkeletalMesh header imports it,
    directly or through base MaterialInstanceConstant parents (fetched headers,
    or the game's asset-registry 'Parent' tags for unfetched instances).
    Morph-target / cloth usage is NOT checked (disclosed gap).
  * ROUND 2: its material class is read from TWO sources (material_class.py:
    the game's asset registry + its own compiled PS5 shader types) and both
    agree; the domain is Surface; it is lit; it has no special shading model
    (Hair, Eye, SingleLayerWater, ThinTranslucent); its blend group is opaque or
    masked. Anything else is excluded and the reason is written out.

Per mod material:
  MIC with own parameters P(m): the mod master's class is read with two readers
     (material_class.mod_props). Translucent or unreadable -> known-checker.
     "Opaque, but not provable against masked" is unreadable too, unless the
     user decides --accept-unproven-opaque (every affected row says so).
     Only candidates of the SAME blend group are allowed (opaque mod instance ->
     opaque parent; masked -> masked). Others are listed as refused with why.
     Parameters are mapped by param_roles.map_params (exact name, or a role
     rename with two sources; packed masks never). Best candidate: most mapped,
     then most exact, then a Material before an instance (an instance would add
     its own textures for unmapped slots), then fewer names, then name.
     coverage 1 -> 'reparent'; 0<coverage<1 -> 'reparent-partial' (only with
     --allow-partial; every lost parameter is listed with its reason).
  MIC with NO own parameters, mod Materials: not matched (as round 1).
  not matched -> 'known-checker', or 'restore-base' only with --allow-restore-base
     AND the package overrides a base package proven to have target shader maps.
Exit 1 if zero materials were checked or zero candidates survived.
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import asset_registry  # noqa: E402
import cityhash  # noqa: E402
import container_header  # noqa: E402
import material_class as mc  # noqa: E402
import mod_material_audit as mma  # noqa: E402
import param_roles as pr  # noqa: E402
import shader_library as sl  # noqa: E402
import zen  # noqa: E402

MATERIAL_CLASSES = (mma.MATERIAL, mma.MIC)
SKELETAL_MESH = '/Script/Engine/SkeletalMesh'
AR_MIC = '/Script/Engine.MaterialInstanceConstant'


def _rb(p):
    with open(p, 'rb') as fh:
        return fh.read()


def _jl(p):
    with open(p) as fh:
        return json.load(fh)


def _jd(obj, p):
    with open(p, 'w') as fh:
        json.dump(obj, fh, indent=1)


def param_names(names, export, package):
    return sorted({n for n in names if n not in (export, package) and not n.startswith('/')})


def skeletal_use(checked, registry=None, max_depth=8):
    """package -> sorted list of base SkeletalMesh packages whose fetched headers
    import it directly or via base MIC parents (fetched headers, else registry)."""
    by = {r['package']: r for r in checked}
    used = {}
    for r in checked:
        if not any(e['class'] == SKELETAL_MESH for e in (r.get('exports') or [])):
            continue
        frontier, seen = list(r['imports']), set()
        for _ in range(max_depth):
            nxt = []
            for i in frontier:
                if i in seen:
                    continue
                seen.add(i)
                used.setdefault(i, set()).add(r['package'])
                h = by.get(i)
                if h and any(e['class'] == mma.MIC for e in (h.get('exports') or [])):
                    nxt.extend(h['imports'])
                elif h is None and registry is not None:
                    a = registry.asset(i)
                    if a and a['class'] == AR_MIC and registry.parent_package(i):
                        nxt.append(registry.parent_package(i))
            frontier = nxt
    return {k: sorted(v) for k, v in used.items()}


def base_gate(props):
    """None if a two-source class allows this parent at all, else why not."""
    if not props.get('readable'):
        return 'class not readable from two sources: ' + props.get('why', '?')
    if props['domain'] != mc.SURFACE:
        return f"domain {props['domain']} is not a surface material"
    if props['lit'] != 'lit':
        return f"shading is {props['lit']} (mod materials need a lit parent)"
    if props['special']:
        return f"special shading model {props['special']} (one source suffices to refuse)"
    if props['blend_group'] not in (mc.OPAQUE, mc.MASKED):
        return f"blend {props['blend']} (translucent family)"
    return None


def candidates(checked, store, mod_names, registry=None, props_of=None):
    used = skeletal_use(checked, registry)
    cands, excluded, details = {}, {}, {}
    for r in checked:
        pkg = r['package']
        pid = cityhash.ue_name_hash(pkg)
        mats = [e for e in (r.get('exports') or []) if e['class'] in MATERIAL_CLASSES]
        why = None
        if pkg in mod_names:
            why = 'overridden by the mod'
        elif pid not in store:
            why = 'store unknown (its container header not loaded)'
        elif not store[pid]['shader_maps']:
            why = 'no target shader maps'
        elif r.get('exports') is None:
            why = 'no export class info'
        elif len(mats) != 1:
            why = 'not exactly one Material/MaterialInstanceConstant export'
        elif pkg not in used:
            why = 'no skeletal-mesh use evidence'
        props = None
        if why is None:
            if props_of is None:
                why = 'no material class reader (asset registry + shader types) given'
            else:
                props = props_of(pkg)
                why = base_gate(props)
        if why is None:
            cands[pkg] = {'names': r['names'], 'export': mats[0]['name'], 'class': mats[0]['class'],
                          'verified': r['verified'], 'skeletal_mesh_evidence': used[pkg], 'props': props}
            continue
        excluded.setdefault(why if props is None else why.split(':')[0], []).append(pkg)
        if props is not None:
            details[pkg] = why
    return cands, excluded, details


MIN_PARTIAL_COVERAGE = 0.5   # a partial parent must carry at least half of the mod's parameters


def rank(pkg, o):
    return (-o['mapped'], -o['exact'], o['is_mic'], o['names'], pkg)


def best_option(options):
    return min(options.items(), key=lambda kv: rank(*kv), default=None)


def choose(params, options, allow_partial):
    """options: {package: {'mapped': int, 'exact': int, 'is_mic': bool, 'names': int}}
    -> (package or None, coverage)"""
    best = best_option(options)
    if best is None or not params or best[1]['mapped'] == 0:
        return None, 0.0
    coverage = best[1]['mapped'] / len(params)
    if coverage < 1 and (not allow_partial or coverage < MIN_PARTIAL_COVERAGE):
        return None, coverage
    return best[0], coverage


class Context:
    """What plan() needs besides the audit: mod class reader, base texture
    bindings, registry texture tags, mod texture formats."""

    def __init__(self, mod_class, base_bind, tex_tags, mod_formats, accept_unproven_opaque=False):
        self.mod_class, self.base_bind, self.tex_tags, self.mod_formats = mod_class, base_bind, tex_tags, mod_formats
        self.accept_unproven_opaque = accept_unproven_opaque


def mod_master(audit_by, name, depth=16):
    for _ in range(depth):
        m = audit_by.get(name)
        if m is None:
            return None
        if m['class'] == 'Material':
            return name
        name = m.get('parent')
    return None


def plan(audit, mod_packages, cands, allow_partial, allow_restore_base=False, ctx=None):
    if ctx is None:
        raise ValueError('plan() needs a Context (two-source class + parameter evidence)')
    rows = []
    audit_by = {m['package']: m for m in audit['materials']}
    for m in audit['materials']:
        if m['verdict'] not in ('NO_TARGET_SHADERS', 'INHERITS_MISSING'):
            continue
        pk = mod_packages[m['package']]
        params = param_names(pk.names, m['export'], m['package'])
        row = {'mod_package': m['package'], 'class': m['class'], 'verdict': m['verdict'],
               'old_parent': m.get('parent'), 'params': params, 'overrides_base': m['overrides_base'],
               'base_had_target_shader_maps': m.get('base_had_target_shader_maps')}
        target, cov, why = None, 0.0, None
        if m['class'] == 'MaterialInstanceConstant' and m['verdict'] == 'INHERITS_MISSING' and params:
            master = mod_master(audit_by, m['package'])
            mp = ctx.mod_class(master) if master else {'readable': False, 'why': 'mod master not found'}
            if not mp.get('readable') and mp.get('unproven_opaque') and ctx.accept_unproven_opaque:
                mp = dict(mp, readable=True, blend_group=mc.OPAQUE,
                          note='USER DECISION (--accept-unproven-opaque): opaque is not proven by two readers; '
                               'if the mod material is really masked, its cut-outs are filled in')
            row['mod_master'] = master
            row['mod_class'] = {k: mp[k] for k in ('readable', 'blend_group', 'why', 'note', 'reader_a_classes',
                                                   'reader_b', 'unproven_opaque') if k in mp}
            if not mp.get('readable'):
                why = 'mod material class not readable from two readers: ' + mp.get('why', '?')
            elif mp['blend_group'] not in (mc.OPAQUE, mc.MASKED):
                why = f"{mp['blend_group']} mod material: no parent class handled"
            else:
                bind, _ = pr.texture_bindings(pk)
                options, maps, refused = {}, {}, {}
                for c, cd in cands.items():
                    names = cd['names']
                    if cd['props']['blend_group'] != mp['blend_group']:
                        if set(params) & set(names) or any(pr.role_of(q) for q in names):
                            refused[c] = (f"{cd['props']['blend_group']} parent for a {mp['blend_group']} mod "
                                          f"instance ({cd['props']['sources']['asset_registry']}, both sources)")
                        continue
                    mapping, lost = pr.map_params(params, bind or {}, ctx.mod_formats, names,
                                                  ctx.base_bind(c), ctx.tex_tags, set(pk.names))
                    maps[c] = (mapping, lost)
                    options[c] = {'mapped': len(mapping), 'exact': sum(v['how'] == 'exact' for v in mapping.values()),
                                  'is_mic': cd['class'] == mma.MIC, 'names': len(names)}
                if refused:
                    row['refused_parents'] = refused
                target, cov = choose(params, options, allow_partial)
                if target is None:
                    best = best_option(options)
                    need_txt = (f'partial needs --allow-partial and coverage >= {MIN_PARTIAL_COVERAGE}'
                                if cov < 1 else '')
                    why = 'no allowed candidate maps its parameters' if not best or not best[1]['mapped'] else (
                        f"best allowed candidate {best[0]} maps {best[1]['mapped']}/{len(params)} parameters "
                        f"({need_txt}); lost: {maps[best[0]][1]}")
                else:
                    row['param_map'], row['lost_params'] = maps[target]
                    row['param_renames'] = {k: v['to'] for k, v in maps[target][0].items() if v['how'] != 'exact'}
        elif m['class'] == 'MaterialInstanceConstant':
            why = 'no own parameters: look comes only from the mod parent, which cannot be emulated'
        else:
            why = 'mod Material without target shaders: graph cannot be rebuilt without a shader compiler'
        row['coverage'] = round(cov, 3)
        if target:
            c = cands[target]
            p = c['props']
            row.update({'action': 'reparent' if cov == 1 else 'reparent-partial', 'new_parent': target,
                        'new_export': c['export'], 'new_parent_class': c['class'],
                        'candidate_verified': c['verified'], 'skeletal_mesh_evidence': c['skeletal_mesh_evidence'],
                        'parent_class': {'domain': p['domain'], 'blend': p['blend'], 'lit': p['lit'],
                                         'sources': p['sources']},
                        'look_notes': [f"parent shading models {p['models']} come from one source (asset "
                                       'registry) and the mod model is not readable: lighting may differ'],
                        'store_entry_repoint': {'package_id': f"{cityhash.ue_name_hash(m['package']):016x}",
                                                'from_package_id': f"{cityhash.ue_name_hash(m['parent']):016x}",
                                                'to_package_id': f'{cityhash.ue_name_hash(target):016x}'}})
            if row['mod_class'].get('note'):
                row['look_notes'].append(row['mod_class']['note'])
            if p['blend_group'] == mc.MASKED:
                row['look_notes'].append('masked parent: the cut-out shape comes from the base parent graph, '
                                         'not from the mod')
            if row.get('lost_params'):
                row['look_notes'].append('lost parameters keep the base parent defaults: '
                                         + ', '.join(sorted(row['lost_params'])))
        elif allow_restore_base and m['overrides_base'] and m.get('base_had_target_shader_maps') is True:
            row.update({'action': 'restore-base', 'why': why + '; base copy restored (mod intent lost here)'})
        else:
            row.update({'action': 'known-checker', 'why': why, 'parameterless': not params})
        rows.append(row)
    return rows


def load_type_index(libraries, stinfos, store):
    if len(libraries) != len(stinfos):
        raise ValueError('--library and --stinfo must pair up (same count, same order)')
    for lp, sp in zip(libraries, stinfos):
        lib = os.path.basename(lp).split('.', 1)[0]
        if f'-{lib}-' not in os.path.basename(sp):
            raise ValueError(f'stinfo {os.path.basename(sp)} does not name library {lib}')
    pairs = [(sl.Library(_rb(lp)), mc.load_stinfo(_rb(sp))) for lp, sp in zip(libraries, stinfos)]
    return mc.TypeIndex(pairs, store)


def fetched_packages(dirs, checked):
    """package -> ZenPackage for whole-chunk (IoHash-verified) fetched files."""
    out = {}
    for r in checked:
        if r.get('verified') != 'iohash':
            continue
        name = f'{cityhash.ue_name_hash(r["package"]):016x}.hdr.bin'
        for d in dirs:
            p = os.path.join(d, name)
            if os.path.exists(p):
                try:
                    out[r['package']] = zen.ZenPackage(_rb(p))
                except (zen.ZenError, ValueError, IndexError):
                    pass
                break
    return out


def build_context(registry, peers, mod_pk, whole, headers, accept_unproven_opaque=False):
    mod_cache = {}

    def mod_class(name):
        if name not in mod_cache:
            p = mod_pk.get(name)
            top = [e for e in p.exports if e['outer'][0] == 'Null'] if p else []
            blob = p.export_bytes(top[0]) if len(top) == 1 else None
            mod_cache[name] = mc.mod_props(blob, peers) if blob else {'readable': False, 'why': 'no export data'}
        return mod_cache[name]

    binds = {}
    for pkg, z in whole.items():
        b, _ = pr.texture_bindings(z)
        if not b:
            continue
        m, chain = mc.master_of(pkg, registry, headers)
        for node in (chain if m else [pkg]):
            d = binds.setdefault(node, {})
            for q, t in b.items():
                d.setdefault(q, set()).add(t)

    def base_bind(cand):
        return binds.get(cand, {})

    def tex_tags(pkg):
        a = registry.asset(pkg)
        return a['tags'] if a else None

    formats = {}
    for n, p in mod_pk.items():
        f = pr.pixel_format(p)
        if f:
            formats[n] = f
    return Context(mod_class, base_bind, tex_tags, formats, accept_unproven_opaque)


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--audit', required=True)
    ap.add_argument('--mod-utoc', required=True)
    ap.add_argument('--mod-ucas', required=True)
    ap.add_argument('--checked-headers', nargs='+', required=True,
                    help='package_fetch_plan.py check outputs (round 1, round 2, ...)')
    ap.add_argument('--fetched-dir', nargs='+', required=True,
                    help='directories holding the fetched <package id>.hdr.bin files')
    ap.add_argument('--base-header', nargs='+', required=True, help='EVERY container header of the game')
    ap.add_argument('--asset-registry', required=True, help="the game's own AssetRegistry.bin")
    ap.add_argument('--library', nargs='+', required=True, help='fetched shader library manifests (type-8)')
    ap.add_argument('--stinfo', nargs='+', required=True, help='matching ShaderTypeInfo .stinfo files, same order')
    ap.add_argument('--allow-partial', action='store_true')
    ap.add_argument('--allow-restore-base', action='store_true')
    ap.add_argument('--accept-unproven-opaque', action='store_true',
                    help='USER DECISION ONLY: treat a mod material whose opaque-vs-masked class has no second '
                         'source as opaque (disclosed in every row it affects)')
    ap.add_argument('--out', required=True)
    a = ap.parse_args(argv)
    audit = _jl(a.audit)
    pk, _libs = mma.load_mod(a.mod_utoc, a.mod_ucas)
    store = {}
    for hp in a.base_header:
        store.update(container_header.parse(_rb(hp))[1])
    checked = [r for p in a.checked_headers for r in _jl(p)['checked'] if 'verified' in r]
    headers = {r['package']: r for r in checked}
    registry = asset_registry.load(a.asset_registry)
    typeidx = load_type_index(a.library, a.stinfo, store)
    peers = mc.Peers(registry, typeidx)

    def props_of(pkg):
        return mc.base_props(pkg, registry, typeidx, peers, headers)
    cands, excluded, details = candidates(checked, store, set(pk), registry, props_of)
    whole = fetched_packages(a.fetched_dir, checked)
    ctx = build_context(registry, peers, pk, whole, headers, a.accept_unproven_opaque)
    rows = plan(audit, pk, cands, a.allow_partial, a.allow_restore_base, ctx)
    _jd({'label': 'Mac-only', 'inputs': {'asset_registry_assets': registry.counts['assets'],
                                         'class_population': len(peers.pop),
                                         'whole_chunk_base_packages': len(whole)},
         'candidates': {k: {x: v[x] for x in v if x != 'names'} for k, v in cands.items()},
         'excluded': excluded, 'excluded_class_detail': details, 'rows': rows}, a.out)
    acts = {}
    for r in rows:
        acts[r['action']] = acts.get(r['action'], 0) + 1
    print(f'reparent_plan: checked {len(rows)} materials against {len(checked)} fetched headers '
          f'({len(peers.pop)} game materials in the class population) -> {len(cands)} candidates; '
          f'excluded {({k: len(v) for k, v in excluded.items()})}; actions {acts}')
    ok = bool(rows) and bool(cands)
    print('reparent_plan:', 'DONE' if ok else 'FAIL (zero materials or zero candidates)')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
