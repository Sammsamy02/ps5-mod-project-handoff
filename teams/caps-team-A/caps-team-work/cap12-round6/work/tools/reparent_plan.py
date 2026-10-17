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
ROUND 4 (full look, Mac-only):
  * reader C (material_props.py): blend mode and shading model from the
    material's OWN property block, decoder learned from the game's materials
    against the registry (leave-one-out for base parents). Base parent model =
    registry AND reader C (two files) -> 'models_two_source'. Mod model = reader
    C enum AND bit field (two fields) whose blend group must agree with readers
    A/B; if A/B could not tell opaque from masked, reader C must not contradict
    the user decision (--accept-unproven-opaque). An unreadable mod model
    refuses EVERY parent (A/B cannot exclude a special model); a special model
    named only by the bit field (per-pixel enum) refuses the same-model route.
  * special shading models (Hair, Eye, SingleLayerWater, ThinTranslucent): a mod
    instance whose model is special may take ONLY a Material parent with the
    SAME model set (two sources each side) and blend group; a non-special mod
    instance never takes a special parent; an unreadable mod model never takes a
    special parent. Never crosses models; refusing is the default.
  * slot gate + packed same-order + colour+alpha rules (param_roles.py) apply
    to every mapped parameter (virtual-texture slots are lost).
  * ranking among allowed parents: most mapped, then part-role match
    (part_roles.py), then shading-model match (0 same set, 1 parent set contains
    the mod model, 2 other/unknown), then most exact, Material before instance,
    fewer names, name.
ROUND 5 (derived textures, --derive-textures, Mac-only):
  * PS5 tile layout (round 5b, Team B finding): EVERY texture a shipped instance
    binds must ship as a derived copy in the game's PS5 tile layout (ps5_tiling.py,
    texture_package.derive_ps5: channels kept, or R<->B swapped for the mirror
    route). A row any of whose bound textures fails derive_ok(texture, channels)
    (e.g. tile order or game layout for its size not proven) is refused.
  * per-slot sampler proof (material_defaults.py): a slot whose OWN default
    texture (the master's default-texture array, pairing cross-checked against
    the game's bound textures on >= MIN_PAIR_CHECKS slots) is plain has a
    plain sampler; this second source replaces the class-wide default rule
    when available (the class-wide rule cannot decide when the master has a
    virtual default for another slot of the same class).
  * a packed mask whose base slot has the R<->B mirror channel order (ORM ->
    MRO) is KEPT through a DERIVED texture (param_roles 'derived:swap_rb'):
    the overlay ships a copy of the mod texture package with red and blue
    exchanged on the stored blocks (bcn_swap.py, exact; proven by two
    decoders in derive_substitutes.py). The derived copy keeps the texture's
    path, so the gate derive_ok() requires: the texture is a mod-only package
    (no base package of that id), its layout is proven by texture_package.py
    (header bulk map + export mip table), the swap succeeds on every block,
    and EVERY mod package importing it is a mod material that cannot draw with
    its own shaders (audit verdict NO_TARGET_SHADERS / INHERITS_MISSING).
    Rows list them under 'texture_derivations'. A post-pass refuses the plan
    (exit 1) if a derived texture would also be read unswapped by a reparented
    row.
Exit 1 if zero materials were checked, zero candidates survived or reader C
could not be learned.
"""
import argparse
import hashlib
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import asset_registry  # noqa: E402
import cityhash  # noqa: E402
import container_header  # noqa: E402
import material_class as mc  # noqa: E402
import mod_material_audit as mma  # noqa: E402
import material_props as mprops  # noqa: E402
import param_roles as pr  # noqa: E402
import part_roles  # noqa: E402
import texture_layout as tlay  # noqa: E402
import utoc  # noqa: E402
import shader_library as sl  # noqa: E402
import zen  # noqa: E402
import bcn_swap  # noqa: E402
import texture_package as tpkg  # noqa: E402
import material_defaults as mdef  # noqa: E402
import ps5_tiling  # noqa: E402

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


def base_gate(props, allow_special=False):
    """None if a two-source class allows this parent at all, else why not.
    allow_special: let special-model parents through (the same-model route
    checks them per mod instance)."""
    if not props.get('readable'):
        return 'class not readable from two sources: ' + props.get('why', '?')
    if props['domain'] != mc.SURFACE:
        return f"domain {props['domain']} is not a surface material"
    if props['lit'] != 'lit':
        return f"shading is {props['lit']} (mod materials need a lit parent)"
    if props['special'] and not allow_special:
        return f"special shading model {props['special']} (one source suffices to refuse)"
    if props['blend_group'] not in (mc.OPAQUE, mc.MASKED):
        return f"blend {props['blend']} (translucent family)"
    return None


def candidates(checked, store, mod_names, registry=None, props_of=None, model_c=None):
    """model_c: callable(package, props) -> reader C result for the parent's
    master (leave-one-out), or None. Special-model parents are kept only when
    reader C confirms the registry model set (two sources)."""
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
                props = dict(props_of(pkg))
                why = base_gate(props, allow_special=model_c is not None)
        if why is None:
            rc = model_c(pkg, props) if model_c else None
            props['reader_c'] = rc
            two = bool(rc and rc.get('readable') and rc['models'] == props['models']
                       and rc['blend'] == props['blend'])
            props['models_two_source'] = two
            if rc and rc.get('readable') and not two:
                why = (f"sources disagree on the shading model: registry {props['blend']} {props['models']}, "
                       f"reader C {rc['blend']} {rc['models']}")
            elif props['special'] and not (two and mats[0]['class'] == mma.MATERIAL):
                why = (f"special shading model {props['special']} not confirmed by two sources on a Material "
                       f"({(rc or {}).get('why') or 'instance parent'})")
        if why is None:
            cands[pkg] = {'names': r['names'], 'export': mats[0]['name'], 'class': mats[0]['class'],
                          'verified': r['verified'], 'skeletal_mesh_evidence': used[pkg], 'props': props}
            continue
        excluded.setdefault(why if props is None else why.split(':')[0], []).append(pkg)
        if props is not None:
            details[pkg] = why
    return cands, excluded, details


MIN_PAIR_CHECKS = 2          # per-slot default pairing must agree with the game's bindings on >= 2 slots
MIN_PARTIAL_COVERAGE = 0.5   # a partial parent must carry at least half of the mod's parameters


def rank(pkg, o):
    return (-o['mapped'], o.get('role_score', 2), o.get('model_score', 2), -o['exact'], o['is_mic'], o['names'], pkg)


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

    def __init__(self, mod_class, base_bind, tex_tags, mod_formats, accept_unproven_opaque=False,
                 mod_c=None, mod_tex=None, defaults=None, derive_ok=None, slot_default=None):
        self.mod_class, self.base_bind, self.tex_tags, self.mod_formats = mod_class, base_bind, tex_tags, mod_formats
        self.derive_ok = derive_ok                 # None -> derived textures off
        self.slot_default = slot_default or (lambda cand, q, info=False: None)
        self.accept_unproven_opaque = accept_unproven_opaque
        self.mod_c = mod_c or (lambda name: {'readable': False, 'why': 'no reader C given'})
        self.mod_tex = mod_tex                     # None -> no mod texture is proven -> nothing maps
        self.defaults = defaults or (lambda cand: [])


def mod_master(audit_by, name, depth=16):
    for _ in range(depth):
        m = audit_by.get(name)
        if m is None:
            return None
        if m['class'] == 'Material':
            return name
        name = m.get('parent')
    return None


GROUP_OF_BLEND = {'BLEND_Opaque': mc.OPAQUE, 'BLEND_Masked': mc.MASKED}


def model_score(mod_models, parent_props):
    """0 same model set (two sources each side), 1 parent set contains the mod
    model(s), 2 other or unknown."""
    if not mod_models:
        return 2
    pm = set(parent_props.get('models') or ())
    if set(mod_models) == pm and parent_props.get('models_two_source'):
        return 0
    return 1 if set(mod_models) <= pm else 2


def _mod_names(pk, bind, master):
    return [pk.name if hasattr(pk, 'name') else None, master] + sorted((bind or {}).values())


def plan(audit, mod_packages, cands, allow_partial, allow_restore_base=False, ctx=None):
    if ctx is None:
        raise ValueError('plan() needs a Context (two-source class + parameter evidence)')
    rows = []
    audit_by = {m['package']: m for m in audit['materials']}
    special_set = set(mc.SPECIAL_MODELS)
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
            mcr = ctx.mod_c(master) if master else {'readable': False, 'why': 'mod master not found'}
            c_group = GROUP_OF_BLEND.get(mcr.get('blend'), mc.TRANSLUCENT) if mcr.get('readable') else None
            if not mp.get('readable') and mp.get('unproven_opaque') and ctx.accept_unproven_opaque:
                if c_group not in (None, mc.OPAQUE):
                    mp = dict(mp, why=f"user decision refused: reader C (own property block) says {mcr['blend']}")
                else:
                    mp = dict(mp, readable=True, blend_group=mc.OPAQUE,
                              note='USER DECISION (--accept-unproven-opaque): opaque is not proven by two readers; '
                                   'if the mod material is really masked, its cut-outs are filled in' +
                                   ('; reader C (the material\'s own property block) also reads opaque'
                                    if c_group == mc.OPAQUE else ''))
            row['mod_master'] = master
            row['mod_class'] = {k: mp[k] for k in ('readable', 'blend_group', 'why', 'note', 'reader_a_classes',
                                                   'reader_b', 'unproven_opaque') if k in mp}
            row['mod_model'] = {k: mcr[k] for k in ('readable', 'blend', 'models', 'model_enum', 'why') if k in mcr}
            mod_models = mcr['models'] if mcr.get('readable') else None
            mod_special = sorted(set(mod_models or ()) & special_set)
            if not mp.get('readable'):
                why = 'mod material class not readable from two readers: ' + mp.get('why', '?')
            elif mp['blend_group'] not in (mc.OPAQUE, mc.MASKED):
                why = f"{mp['blend_group']} mod material: no parent class handled"
            elif not mcr.get('readable'):
                # readers A/B never see the shading model: without reader C a special (hair, eye ...)
                # model cannot be excluded, so no parent is allowed (fail closed)
                why = 'mod shading model unreadable (reader C): ' + str(mcr.get('why') or '?')
            elif mod_special and mcr.get('field_only'):
                why = (f"mod special model {mod_special} named only by the bit field (per-pixel enum): "
                       'not two fields, same-model route refused')
            elif c_group is not None and c_group != mp['blend_group']:
                why = (f"mod readers disagree: shader types/base pass say {mp['blend_group']}, "
                       f"reader C (own property block) says {mcr['blend']}")
            else:
                bind, _ = pr.texture_bindings(pk)
                part = part_roles.role(mod_models, *_mod_names(pk, bind, master))
                row['part_role'] = part
                options, maps, refused = {}, {}, {}
                for c, cd in cands.items():
                    names = cd['names']
                    cp = cd['props']
                    if cp['blend_group'] != mp['blend_group']:
                        if set(params) & set(names) or any(pr.role_of(q) for q in names):
                            refused[c] = (f"{cp['blend_group']} parent for a {mp['blend_group']} mod "
                                          f"instance ({cp['sources']['asset_registry']}, both sources)")
                        continue
                    if cp.get('special') or mod_special:
                        if not mod_special:
                            refused[c] = (f"special shading model {cp['special']} parent for a "
                                          f"{'/'.join(mod_models or ['unreadable'])} mod instance (never cross)")
                            continue
                        if not cp.get('special'):
                            refused[c] = (f"{'/'.join(cp['models'])} parent for a {'/'.join(mod_special)} "
                                          'mod instance (never cross shading models)')
                            continue
                        if not (cp.get('models_two_source') and set(cp['models']) == set(mod_models)
                                and cd['class'] == mma.MATERIAL):
                            refused[c] = (f"same-model route needs the SAME model set from two sources on a "
                                          f"Material: parent {cp['models']} vs mod {mod_models}")
                            continue
                    mapping, lost = pr.map_params(params, bind or {}, ctx.mod_formats, names,
                                                  ctx.base_bind(c), ctx.tex_tags, set(pk.names),
                                                  mod_tex=ctx.mod_tex, defaults=ctx.defaults(c),
                                                  mod_masked=mp['blend_group'] == mc.MASKED,
                                                  derive_ok=getattr(ctx, 'derive_ok', None),
                                                  slot_default=(lambda q, _c=c: ctx.slot_default(_c, q))
                                                  if hasattr(ctx, 'slot_default') else None)
                    maps[c] = (mapping, lost)
                    prole = part_roles.role(cp['models'] if cp.get('models_two_source') else None,
                                            c, cp.get('master'))
                    options[c] = {'mapped': len(mapping), 'exact': sum(v['how'] == 'exact' for v in mapping.values()),
                                  'is_mic': cd['class'] == mma.MIC, 'names': len(names),
                                  'role_score': part_roles.score(part, prole), 'parent_role': prole,
                                  'model_score': model_score(mod_models, cp)}
                if refused:
                    row['refused_parents'] = refused
                row['options_evaluated'] = len(options)
                row['considered'] = [{'parent': k, 'mapped': v['mapped'], 'role_score': v['role_score'],
                                      'model_score': v['model_score'], 'lost': maps[k][1]}
                                     for _r, k, v in sorted((rank(k, v), k, v) for k, v in options.items())[:6]]
                target, cov = choose(params, options, allow_partial)
                if target is None:
                    best = best_option(options)
                    need_txt = (f'partial needs --allow-partial and coverage >= {MIN_PARTIAL_COVERAGE}'
                                if cov < 1 else '')
                    why = ('no allowed candidate' if not best else
                           f"best allowed candidate {best[0]} maps 0/{len(params)} parameters; lost: {maps[best[0]][1]}"
                           ) if not best or not best[1]['mapped'] else (
                        f"best allowed candidate {best[0]} maps {best[1]['mapped']}/{len(params)} parameters "
                        f"({need_txt}); lost: {maps[best[0]][1]}")
                    if mod_special and not options:
                        why = (f"no game parent with the same {'/'.join(mod_special)} shading model and blend "
                               'from two sources')
                    blockers = sorted({w.split('derived PS5 texture refused: ', 1)[1].rstrip("'}\" ")
                                       for _m, lost_ in maps.values() for w in lost_.values()
                                       if 'derived PS5 texture refused: ' in w})
                    if blockers:
                        row['ps5_layout_blockers'] = blockers
                        why = ('no derived PS5-layout copy possible for a texture the parent would use: '
                               + ' | '.join(blockers) + '; ' + why)
                else:
                    der, why_t = (None, None)
                    if getattr(ctx, 'derive_ok', None) is not None:
                        # every texture the shipped instance binds must be a derived PS5 copy
                        der, why_t = derivations(maps[target][0], bind or {}, ctx)
                    if why_t:
                        row['considered_parent'] = target
                        target, cov, why = None, 0.0, why_t
                    else:
                        row['param_map'], row['lost_params'] = maps[target]
                        row['param_renames'] = {k: v['to'] for k, v in maps[target][0].items()
                                                if v['how'] != 'exact'}
                        if der is not None:
                            row['texture_derivations'] = der
                        if hasattr(ctx, 'slot_default'):
                            proofs = {}
                            for v in maps[target][0].values():
                                inf = ctx.slot_default(target, v['to'], True) if ctx.slot_default else None
                                if inf:
                                    proofs[v['to']] = inf
                            if proofs:
                                row['slot_proofs'] = proofs
                        row['ranking'] = {k: options[target][k] for k in ('role_score', 'model_score', 'parent_role')}
                        row['runner_up'] = sorted(((rank(k, v), k) for k, v in options.items() if k != target))[:1]
                        row['runner_up'] = row['runner_up'][0][1] if row['runner_up'] else None
        elif m['class'] == 'MaterialInstanceConstant':
            why = 'no own parameters: look comes only from the mod parent, which cannot be emulated'
        else:
            why = 'mod Material without target shaders: graph cannot be rebuilt without a shader compiler'
        row['coverage'] = round(cov, 3)
        if target:
            c = cands[target]
            p = c['props']
            mm = row['mod_model']
            notes = []
            if p.get('models_two_source') and mm.get('readable'):
                if set(mm['models']) == set(p['models']):
                    notes.append(f"same shading model {p['models']} (two sources each side)")
                else:
                    notes.append(f"shading model differs: mod {mm['models']} (reader C), parent {p['models']} "
                                 '(registry + reader C): lighting differs from PC')
            else:
                notes.append(f"parent shading models {p['models']} from "
                             f"{'two sources' if p.get('models_two_source') else 'one source (asset registry)'}; "
                             f"mod model {mm.get('models') if mm.get('readable') else 'not readable'}: "
                             'lighting may differ')
            if row['ranking']['role_score'] == 3:
                notes.append(f"role mismatch: {row['part_role']['role']} part on a "
                             f"{row['ranking']['parent_role']['role']} parent (no allowed parent of its role maps "
                             'as many parameters)')
            row.update({'action': 'reparent' if cov == 1 else 'reparent-partial', 'new_parent': target,
                        'new_export': c['export'], 'new_parent_class': c['class'],
                        'candidate_verified': c['verified'], 'skeletal_mesh_evidence': c['skeletal_mesh_evidence'],
                        'parent_class': {'domain': p['domain'], 'blend': p['blend'], 'lit': p['lit'],
                                         'models': p['models'], 'models_two_source': p.get('models_two_source'),
                                         'sources': p['sources']},
                        'look_notes': notes,
                        'store_entry_repoint': {'package_id': f"{cityhash.ue_name_hash(m['package']):016x}",
                                                'from_package_id': f"{cityhash.ue_name_hash(m['parent']):016x}",
                                                'to_package_id': f'{cityhash.ue_name_hash(target):016x}'}})
            if row['mod_class'].get('note'):
                row['look_notes'].append(row['mod_class']['note'])
            if p['blend_group'] == mc.MASKED:
                row['look_notes'].append('masked parent: the cut-out shape comes from the base parent graph, '
                                         'not from the mod')
            for d in row.get('texture_derivations', []):
                if d['channels'] == 'swap_rb':
                    row['look_notes'].append(f"{d['parameter']} kept through a DERIVED texture: {d['texture']} with red "
                                             f"and blue exchanged ({d['from_letters']} -> {d['to_letters']}, exact block "
                                             f"swap) for the parent's {d['slot']} slot, PS5 tile layout")
            if row.get('texture_derivations'):
                row['look_notes'].append(f"{len(row['texture_derivations'])} bound textures ship as derived copies in the "
                                         "game's PS5 tile layout (lossless re-order)")
            if row.get('lost_params'):
                row['look_notes'].append('lost parameters keep the base parent defaults: '
                                         + ', '.join(sorted(row['lost_params'])))
        elif allow_restore_base and m['overrides_base'] and m.get('base_had_target_shader_maps') is True:
            row.update({'action': 'restore-base', 'why': why + '; base copy restored (mod intent lost here)'})
        else:
            row.update({'action': 'known-checker', 'why': why, 'parameterless': not params})
        rows.append(row)
    derived_consistency(rows, mod_packages)
    return rows


class PlanRefused(ValueError):
    pass


def derivations(mapping, bind, ctx):
    """Derived PS5 copies for EVERY texture the instance binds: mapped ones with their
    channels ('swap_rb' for the mirror route, else 'keep'); unmapped ones 'keep' (still
    imported by the shipped instance). -> (list, None) or (None, why)."""
    out = []
    by_tex = {}
    for prm, tex in sorted(bind.items()):
        v = mapping.get(prm)
        if v and v['how'].startswith('derived:'):
            d = dict(v['derive'], parameter=prm, slot=v['to'], channels='swap_rb', layout='ps5')
        else:
            d = {'texture': tex, 'op': 'keep', 'channels': 'keep', 'layout': 'ps5', 'parameter': prm,
                 'slot': v['to'] if v else None, 'format': ctx.mod_formats.get(tex)}
            why = ctx.derive_ok(tex, 'keep')
            if why:
                return None, f'bound texture {tex} ({prm}) has no derived PS5 copy: {why}'
        if tex in by_tex and by_tex[tex] != d['channels']:
            return None, f'texture {tex} is bound twice with different channel operations'
        by_tex[tex] = d['channels']
        out.append(d)
    if not out:
        return None, 'instance binds no texture'
    return out, None


def derived_consistency(rows, mod_packages):
    """A derived (swapped) texture replaces the mod's own copy for EVERY reader.
    Refuse the plan when a reparented row would read one of them unswapped
    (mapped exactly or by another role) or with another operation."""
    ops = {}
    for r in rows:
        for d in r.get('texture_derivations', []):
            ops.setdefault(d['texture'], set()).add((d['op'], d.get('channels'), d.get('layout')))
    bad = [t for t, o in ops.items() if len(o) != 1]
    if bad:
        raise PlanRefused(f'derived textures with conflicting operations: {sorted(bad)}')
    for r in rows:
        if not r.get('action', '').startswith('reparent'):
            continue
        pk = mod_packages.get(r['mod_package'])
        bind, _ = pr.texture_bindings(pk) if pk is not None else ({}, None)
        mine = {d['texture'] for d in r.get('texture_derivations', [])}
        for prm, tex in (bind or {}).items():
            if ops and tex not in mine:
                raise PlanRefused(f"{r['mod_package']}: bound texture {tex} ({prm}) has no derived copy in the row")


def make_derive_ok(mod_pk, audit, store, mod_toc, mod_read, census=None):
    """derive_ok(texture) -> None if a DERIVED copy of that mod texture package
    may replace the mod's own for every consumer, else why not (fail closed)."""
    audit_by = {m['package']: m for m in audit['materials']}
    importers = {}
    for n, p in mod_pk.items():
        for i in p.imported_packages:
            importers.setdefault(i, set()).add(n)
    rows_of = {}
    for i, cid in enumerate(mod_toc.chunk_ids):
        rows_of.setdefault(int.from_bytes(cid[:8], 'little'), {})[cid[8:].hex()] = i
    cache = {}

    def derive_ok(tex, channels='swap_rb'):
        key = (tex, channels)
        if key in cache:
            return cache[key]
        why = None
        pid = cityhash.ue_name_hash(tex)
        chunks = rows_of.get(pid, {})
        users = sorted(importers.get(tex, ()))
        if pid in store:
            why = 'the mod texture has the id of a base package (base materials would read the swapped copy)'
        elif tex not in mod_pk:
            why = 'mod texture package missing'
        elif not users:
            why = 'no mod package imports it'
        elif set(chunks) - {'00000001', '00000002'}:
            why = f'texture has chunk kinds {sorted(chunks)} (optional / memory-mapped bulk data unsupported)'
        else:
            for u in users:
                a = audit_by.get(u)
                if a is None or a['class'] not in ('Material', 'MaterialInstanceConstant'):
                    why = f'consumer {u} is not an audited mod material'
                    break
                if a['verdict'] not in ('NO_TARGET_SHADERS', 'INHERITS_MISSING'):
                    why = f'consumer {u} can draw with its own shaders ({a["verdict"]}) and expects the original order'
                    break
        if why is None:
            try:
                chunk = mod_toc.read_chunk(chunks['00000001'], mod_read)
                bulk = mod_toc.read_chunk(chunks['00000002'], mod_read) if '00000002' in chunks else None
                tpkg.derive_ps5(chunk, bulk, bcn_swap.swap_rb if channels == 'swap_rb' else keep_channels,
                                lambda f, w, h, n: ps5_tiling.choose(f, w, h, n, census))
            except ps5_tiling.TilingRefused as e:
                why = f'PS5 tile layout: {e}'
            except (tpkg.LayoutError, bcn_swap.SwapRefused, zen.ZenError, KeyError) as e:
                why = f'texture not derivable: {e}'
        cache[key] = why
        return why
    return derive_ok


def keep_channels(fmt, data):
    return data, {'format': fmt, 'blocks': None, 'endpoint_order_fixed': 0, 'bc7_rotation_remapped': 0}


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


def build_context(registry, peers, mod_pk, whole, headers, accept_unproven_opaque=False,
                  reader_c=None, mod_toc=None, mod_read=None, derive_ok=None, use_slot_defaults=False):
    """reader_c: material_props.ReaderC; mod_toc/mod_read: the mod .utoc (utoc.Toc)
    and a ucas reader, for the mod textures' bulk-data layout proof."""
    mod_cache = {}
    c_cache = {}

    def mod_c(name):
        if reader_c is None:
            return {'readable': False, 'why': 'no reader C'}
        if name not in c_cache:
            p = mod_pk.get(name)
            blob = mprops.top_export_bytes(p) if p else None
            c_cache[name] = reader_c.mod(blob) if blob else {'readable': False, 'why': 'no export data'}
        return c_cache[name]

    bulk_rows = tlay.mod_bulk_index(mod_toc) if mod_toc is not None else {}
    tex_cache = {}

    def mod_tex(tex):
        if mod_toc is None:
            return None
        if tex not in tex_cache:
            p = mod_pk.get(tex)
            rows_ = bulk_rows.get(cityhash.ue_name_hash(tex), [])
            if p is None:
                tex_cache[tex] = {'proven': False, 'why': 'mod texture package missing'}
            else:
                fact = tlay.plain_mips(p, [mod_toc.offlen[r][1] for r in rows_])
                if fact.get('proven') and fact['format'] in tlay.ALPHA_FORMATS and mod_read is not None:
                    fact['cutout'] = tlay.cutout_alpha(p, [mod_toc.read_chunk(r, mod_read) for r in rows_])
                tex_cache[tex] = fact
        return tex_cache[tex]

    def defaults(cand):
        m, chain = mc.master_of(cand, registry, headers)
        h = headers.get(m) if m else None
        out = []
        for i in (h or {}).get('imports', []):
            a = registry.asset(i)
            if a and a['class'].startswith('/Script/Engine.Texture'):
                out.append(a['tags'])
        return out

    pair_cache = {}

    def slot_default(cand, q, info=False):
        """Registry tags of slot q's own default texture in cand's master, or None
        (pairing not decodable or not cross-checked on >= MIN_PAIR_CHECKS bound slots)."""
        m, _chain = mc.master_of(cand, registry, headers)
        if not m or m not in whole:
            return None
        if m not in pair_cache:
            pairs, _why = mdef.slot_defaults(whole[m])
            n = 0
            if pairs is not None:
                n, bad = mdef.validate(pairs, binds.get(m, {}), lambda t: pr.tex_class(tex_tags(t)),
                                       lambda t: (tex_tags(t) or {}).get('VirtualTextureStreaming'))
                if bad or n < MIN_PAIR_CHECKS:
                    pairs = None
            pair_cache[m] = (pairs, n)
        pairs, n = pair_cache[m]
        t = pairs.get(q) if pairs else None
        if info:
            return {'master': m, 'default_texture': t,
                    'virtual': (tex_tags(t) or {}).get('VirtualTextureStreaming'),
                    'cross_checked_slots': n} if t else None
        return tex_tags(t) if t else None

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
    return Context(mod_class, base_bind, tex_tags, formats, accept_unproven_opaque,
                   mod_c=mod_c, mod_tex=mod_tex, defaults=defaults, derive_ok=derive_ok,
                   slot_default=slot_default if use_slot_defaults else None)


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
    ap.add_argument('--layout-census', default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                            'ps5_layout_census.json'),
                    help="the game's own PS5 texture layouts (game_layout_census.py output)")
    ap.add_argument('--derive-textures', action='store_true',
                    help='ROUND 5: keep R<->B mirror-order packed masks through a DERIVED (exactly swapped) texture')
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
    whole = fetched_packages(a.fetched_dir, checked)
    train = mprops.training_set(whole, registry, mc.MATERIAL_AR_CLASS)
    rc = mprops.ReaderC(train, peers.bits)
    print(f'reparent_plan: reader C trained on {rc.full.n_train} game materials: '
          + (f'learned {rc.full.found} (decodable {rc.full.n_used})' if rc.full.ok else 'NOT learned: ' + str(rc.full.why)))
    if not rc.full.ok:
        print('reparent_plan: FAIL (reader C not learned from the fetched game Materials)')
        return 1

    def model_c(pkg, props):
        m = props.get('master')
        z = whole.get(m) if m else None
        if z is None:
            return {'readable': False, 'why': 'master export not fetched whole'}
        return rc.base(m, mprops.top_export_bytes(z), set(props.get('chain') or [m]))
    cands, excluded, details = candidates(checked, store, set(pk), registry, props_of, model_c)
    mod_toc = utoc.load(a.mod_utoc)
    mread = mma.reader(a.mod_ucas)
    census = json.load(open(a.layout_census)) if a.derive_textures else None
    dok = make_derive_ok(pk, audit, store, mod_toc, mread, census) if a.derive_textures else None
    ctx = build_context(registry, peers, pk, whole, headers, a.accept_unproven_opaque,
                        reader_c=rc, mod_toc=mod_toc, mod_read=mread, derive_ok=dok,
                        use_slot_defaults=a.derive_textures)
    try:
        rows = plan(audit, pk, cands, a.allow_partial, a.allow_restore_base, ctx)
    except PlanRefused as e:
        print(f'reparent_plan: FAIL (derived textures): {e}')
        return 1
    n_two = sum(bool(c['props'].get('models_two_source')) for c in cands.values())
    print(f'reparent_plan: {n_two}/{len(cands)} candidates have a two-source shading model '
          '(registry + reader C)')
    ders = sorted({d['texture'] for r in rows for d in r.get('texture_derivations', [])})
    swp = sorted({d['texture'] for r in rows for d in r.get('texture_derivations', []) if d['channels'] == 'swap_rb'})
    blocked = sum(1 for r in rows if 'no derived PS5 copy' in str(r.get('why')))
    print(f'reparent_plan: derived textures {"on" if a.derive_textures else "off"}: {len(ders)} derived PS5-layout '
          f'texture(s), {len(swp)} of them with an exact R<->B swap; {blocked} instance(s) refused because a bound '
          'texture has no derived PS5 copy')
    _jd({'label': 'Mac-only', 'derive_textures': bool(a.derive_textures),
         'derive_rules': ({'ps5_layout': 'every bound texture of a shipped instance ships as a derived PS5-layout copy',
                           'per_slot_sampler_proof': 'material_defaults pairing used as second source for every slot',
                           'layout_census_rows': len(census['rows']),
                           'layout_census_sha256': hashlib.sha256(open(a.layout_census, 'rb').read()).hexdigest()}
                          if a.derive_textures else None),
         'inputs': {'asset_registry_assets': registry.counts['assets'],
                                         'class_population': len(peers.pop),
                                         'whole_chunk_base_packages': len(whole),
                                         'reader_c': {'trained': rc.full.n_train, 'decodable': rc.full.n_used,
                                                      'fields': {k: list(v) for k, v in rc.full.found.items()},
                                                      'raw_sizes': rc.full.raw_learned}},
         'candidates': {k: {x: v[x] for x in v if x != 'names'} for k, v in cands.items()},
         'excluded': excluded, 'excluded_class_detail': details, 'rows': rows}, a.out)
    acts = {}
    for r in rows:
        acts[r['action']] = acts.get(r['action'], 0) + 1
    print(f'reparent_plan: checked {len(rows)} materials against {len(checked)} fetched headers '
          f'({len(peers.pop)} game materials in the class population) -> {len(cands)} candidates; '
          f'excluded {({k: len(v) for k, v in excluded.items()})}; actions {acts}')
    evaluated = [r for r in rows if 'options_evaluated' in r]
    pairs = sum(r['options_evaluated'] for r in evaluated)
    print(f'reparent_plan: parameter/slot gate evaluated {pairs} (mod instance, parent) pairs for '
          f'{len(evaluated)} instances')
    ok = bool(rows) and bool(cands) and (pairs > 0 or not evaluated)
    print('reparent_plan:', 'DONE' if ok else 'FAIL (zero materials, zero candidates or zero gate evaluations)')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
