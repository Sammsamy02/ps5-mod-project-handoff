#!/usr/bin/env python3
"""Per-part checker attribution for capability #12 (Mac-only, read-only, generic).

For a mod plan (reparent_plan.py + invisible_route.py output) this lists, from the game's and the mod's
own data:
  A) every SkeletalMesh the mod ships (or overrides): each material slot, the slot's material, how many
     sections / triangles draw it in each LOD (mesh_sections.py), and its status;
  B) every game mesh that a game Blueprint attaches next to a mod mesh (the "character" Blueprints = game
     Blueprints that hard-import a mesh the mod ships), with the materials it imports, when any of those
     materials is overridden by the mod; plus materials those Blueprints import directly.
Status of a material:
  converted          a plan 'reparent' row of the instance route (round 5: game parent + derived textures)
  invisible          a plan 'reparent' row of the invisible route (draws nothing, as on PC)
  invisible-partial  a 'reparent-partial' invisible row (proven for some user classes only)
  placeholder        base_material_route verdict 'placeholder' (cannot be seen; not shipped)
  checker            any other plan row the mod provides (known-checker): the console draws its default
                     material; the plan's reason is copied
  game               not provided by the mod: the game's / engine's own material with console shaders
Every gate prints its count; exit 1 when zero meshes or zero slots were checked.
"""
import argparse
import collections
import json
import os
import sys

sys.dont_write_bytecode = True
TOOL = 'cap12-attribution-1'


def status_of(mat, rows, base, mod_names, inv=None):
    if mat is None:
        return 'empty', 'slot has no material'
    r = rows.get(mat)
    if r is None:
        return ('game', 'game/engine material (console shaders)') if mat not in mod_names else \
               ('checker', 'mod package without a plan row')
    act = r.get('action')
    if act in ('reparent', 'reparent-partial'):
        if r.get('route') == 'invisible':
            if act == 'reparent':
                return 'invisible', 'parent -> %s (draws nothing; users proven: %s)' % (
                    r.get('new_parent', '').rsplit('/', 1)[-1], '+'.join(r.get('proven_user_classes') or []))
            return 'invisible-partial', 'parent -> %s; %s' % (r.get('new_parent', '').rsplit('/', 1)[-1],
                                                              r.get('partial_reason') or 'partial')
        return 'converted', 'parent -> %s, %d derived texture(s)' % (
            r.get('new_parent', '').rsplit('/', 1)[-1], len(r.get('texture_derivations') or []))
    b = base.get(mat)
    if b and b.get('result') == 'placeholder':
        return 'placeholder', b.get('why')
    ir_row = (inv or {}).get(mat)
    if ir_row and ir_row.get('result') == 'refused' and ir_row.get('why'):
        return 'checker', 'invisible route refused: ' + ir_row['why']
    return 'checker', r.get('why') or 'known-checker'


def build(plan, pk, cls_of, game_importers, game_imports, decode, base_report=None):
    rows = {r['mod_package']: r for r in plan['rows']}
    base = {b['package']: b for b in ((base_report or {}).get('materials') or [])}
    inv = {x['mod_package']: x for x in (plan.get('invisible_route', {}).get('rows') or [])}
    mod_names = set(pk)
    meshes = sorted(n for n in pk if cls_of(n) == 'SkeletalMesh')
    out_meshes, nslots = [], 0
    for n in meshes:
        m, err = decode(n)
        ent = {'mesh': n, 'decoded': m is not None, 'error': err}
        if m is not None:
            per = collections.defaultdict(lambda: {'sections': 0, 'triangles': 0})
            for lod in m['lods']:
                for s in lod['sections']:
                    per[s['material_index']]['sections'] += 1
                    per[s['material_index']]['triangles'] += s['triangles']
            ent['lods'] = len(m['lods'])
            ent['slots'] = []
            for s in m['slots']:
                st, why = status_of(s['material'], rows, base, mod_names, inv)
                ent['slots'].append({'slot': s['index'], 'slot_name': s['slot_name'], 'material': s['material'],
                                     'sections': per[s['index']]['sections'],
                                     'triangles': per[s['index']]['triangles'], 'status': st, 'why': why})
                nslots += 1
        out_meshes.append(ent)
    # character Blueprints: game Blueprints that hard-import a mesh the mod ships
    chars = sorted({q for n in meshes for q in game_importers.get(n, ()) if cls_of(q) == 'Blueprint'})
    acc, bp_mats = {}, collections.defaultdict(set)
    for bp in chars:
        for q in sorted(game_imports.get(bp, ())):
            c = cls_of(q)
            if c in ('SkeletalMesh', 'StaticMesh') and q not in mod_names:
                mats = sorted(x for x in game_imports.get(q, ()) if (cls_of(x) or '').startswith('Material'))
                if any(x in mod_names for x in mats):
                    a = acc.setdefault(q, {'mesh': q, 'class': c, 'used_by': [], 'materials': []})
                    a['used_by'].append(bp)
                    if not a['materials']:
                        for x in mats:
                            st, why = status_of(x, rows, base, mod_names, inv)
                            a['materials'].append({'material': x, 'status': st, 'why': why})
            elif (c or '').startswith('Material') and q in mod_names:
                bp_mats[q].add(bp)
    bpm = []
    for x, bps in sorted(bp_mats.items()):
        st, why = status_of(x, rows, base, mod_names, inv)
        bpm.append({'material': x, 'blueprints': sorted(bps), 'status': st, 'why': why})
    return {'tool': TOOL, 'label': 'Mac-only', 'mod_meshes': out_meshes, 'character_blueprints': chars,
            'accessory_meshes': list(acc.values()), 'blueprint_materials': bpm,
            'counts': {'mod_meshes': len(meshes), 'decoded': sum(1 for e in out_meshes if e['decoded']),
                       'slots': nslots, 'character_blueprints': len(chars), 'accessory_meshes': len(acc),
                       'blueprint_materials': len(bpm)}}


def markdown(rep):
    short = lambda p: (p or '-').rsplit('/', 1)[-1]
    L = ['| Mesh (mod ships it) | Slot | Material | Sections / triangles | Status | Why |', '|---|---|---|---|---|---|']
    for e in rep['mod_meshes']:
        if not e['decoded']:
            L.append('| %s | - | - | - | refused | %s |' % (short(e['mesh']), e['error']))
            continue
        for s in e['slots']:
            L.append('| %s | %d %s | %s | %d / %d | %s | %s |' % (short(e['mesh']), s['slot'], s['slot_name'],
                     short(s['material']), s['sections'], s['triangles'], s['status'], s['why'][:120]))
    L += ['', '| Game mesh next to the mod meshes | Kind | Material | Status | Why |', '|---|---|---|---|---|']
    for a in rep['accessory_meshes']:
        for m in a['materials']:
            L.append('| %s | %s | %s | %s | %s |' % (short(a['mesh']), a['class'], short(m['material']), m['status'],
                                                     m['why'][:120]))
    L += ['', '| Material a character Blueprint sets directly | Blueprints | Status | Why |', '|---|---|---|---|']
    for b in rep['blueprint_materials']:
        L.append('| %s | %d | %s | %s |' % (short(b['material']), len(b['blueprints']), b['status'], b['why'][:120]))
    return '\n'.join(L) + '\n'


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--plan', required=True)
    ap.add_argument('--mod-utoc', required=True)
    ap.add_argument('--mod-ucas', required=True)
    ap.add_argument('--asset-registry', required=True)
    ap.add_argument('--base-header', nargs='+', required=True)
    ap.add_argument('--script-utoc', required=True)
    ap.add_argument('--script-ucas', required=True)
    ap.add_argument('--tools-dir', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--md')
    ap.add_argument('--base-report', help="invisible_route.py --base-material-report output (placeholder verdicts)")
    a = ap.parse_args(argv)
    sys.path.insert(0, a.tools_dir)
    import asset_registry
    import cityhash
    import container_header
    import mesh_sections
    import mod_material_audit as mma
    plan = json.load(open(a.plan))
    pk, _ = mma.load_mod(a.mod_utoc, a.mod_ucas)
    script = mma.load_script_objects(a.script_utoc, a.script_ucas)
    reg = asset_registry.load(a.asset_registry)
    store = {}
    for hp in a.base_header:
        store.update(container_header.parse(open(hp, 'rb').read())[1])
    by_id = {cityhash.ue_name_hash(x['package']): x['package'] for x in reg.assets}
    game_imports, game_importers = collections.defaultdict(set), collections.defaultdict(set)
    for q, v in store.items():
        qn = by_id.get(q)
        if qn is None:
            continue
        for i in v['imports']:
            iname = by_id.get(i)
            if iname is not None:
                game_imports[qn].add(iname)
                game_importers[iname].add(qn)

    def cls_of(name):
        x = reg.asset(name)
        if x:
            return x['class'].rsplit('.', 1)[-1]
        p = pk.get(name)
        if p is None:
            return None
        tops = [e for e in p.exports if e['outer'][0] == 'Null']
        if len(tops) != 1:
            return None
        d = p.describe_ref(tops[0]['class'], script)
        return ((d or {}).get('path') or '').rsplit('/', 1)[-1] or None

    cache = {}

    def decode(n):
        if n not in cache:
            try:
                cache[n] = (mesh_sections.decode(pk[n].data, label=n), None)
            except mesh_sections.MeshError as e:
                cache[n] = (None, str(e))
        return cache[n]

    br = json.load(open(a.base_report)) if a.base_report else None
    rep = build(plan, pk, cls_of, game_importers, game_imports, decode, br)
    c = rep['counts']
    print('attribution: %d mod meshes (%d decoded, %d slots), %d character Blueprints, %d accessory meshes, '
          '%d Blueprint-set materials' % (c['mod_meshes'], c['decoded'], c['slots'], c['character_blueprints'],
                                          c['accessory_meshes'], c['blueprint_materials']))
    if c['mod_meshes'] == 0 or c['slots'] == 0:
        print('attribution: FAIL (zero meshes or slots checked)')
        return 1
    st = collections.Counter(s['status'] for e in rep['mod_meshes'] if e['decoded'] for s in e['slots'])
    print('attribution: mod-mesh slot statuses: ' + ', '.join('%s %d' % kv for kv in sorted(st.items())))
    with open(a.out, 'w') as fh:
        json.dump(rep, fh, indent=1)
    if a.md:
        with open(a.md, 'w') as fh:
            fh.write(markdown(rep))
    return 0


if __name__ == '__main__':
    sys.exit(main())
