#!/usr/bin/env python3
"""Real-data dry-run checks for round 6 (Mac-only). Reads the outputs of tests/real_leon_r6.sh and checks
them against the expected Leon result and the staged evidence. Prints every count; exits 1 on any mismatch
or on a zero count."""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
EVID = os.path.join(HERE, '..', 'evidence')
fails, checks = [], [0]


def check(ok, what):
    checks[0] += 1
    print(('  ok   ' if ok else '  FAIL ') + what)
    if not ok:
        fails.append(what)


def short(p):
    return (p or '').rsplit('/', 1)[-1]


def main(out):
    plan = json.load(open(os.path.join(out, 'reparent-plan-r6.json')))
    rows = {r['mod_package']: r for r in plan['rows']}
    acts = {}
    for r in plan['rows']:
        acts[r.get('action')] = acts.get(r.get('action'), 0) + 1
    print('plan actions:', acts)
    check(acts == {'reparent': 20, 'reparent-partial': 4, 'known-checker': 43}, 'plan actions 20 / 4 / 43')
    inv = [r for r in plan['rows'] if r.get('route') == 'invisible']
    check(len(inv) == 14, '14 invisible-route rows (%d)' % len(inv))
    check(all(r['new_parent'].endswith('/M_Hide') for r in inv), 'every invisible row -> M_Hide')
    part = sorted(short(r['mod_package']) for r in plan['rows'] if r.get('action') == 'reparent-partial')
    check(part == ['MI_Simon_Accessories', 'MI_Simon_Accessories_Glass', 'MI_Simon_IVBagExterior_Back',
                   'M_Medical_Liquid'], 'partial rows: glasses frame + lens, IV-bag back, enemy cannula (%s)' % part)
    for want in ('MI_Simon_IVBagExterior', 'MI_Face_Skin_Baked_LOD1'):
        r = [x for x in inv if short(x['mod_package']) == want]
        check(len(r) == 1 and r[0]['action'] == 'reparent' and r[0]['users_proven_by'] == 'original-master-paths',
              '%s -> full invisible row through its game original' % want)
    check('base_material_route' not in plan['invisible_route'], 'the report-only base route is not in the plan')
    for keep in ('MI_Leon_Hair', 'MI_Liquid_Oil_01', 'MI_Simon_BodywarmerBeanieTag_Deluxe', 'HairDefaultMaterial',
                 'MI_Simon_BodywarmerBeanieTag'):
        r = [x for x in plan['rows'] if short(x['mod_package']) == keep]
        check(len(r) == 1 and r[0]['action'] == 'known-checker', '%s stays known-checker' % keep)
    leon = [r for r in plan['rows'] if r.get('action') == 'reparent' and r.get('route') != 'invisible']
    check(len(leon) == 10 and all(r['new_parent'].endswith('/M_MRO_Cloth_NonVT') for r in leon),
          'the 10 round-5 Leon rows are unchanged (M_MRO_Cloth_NonVT)')
    base_rows = {short(r['mod_package']): r for r in json.load(open(os.path.join(
        '/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/evidence/'
        'cap12-launch-prep-20261013/reparent-plan-r5-textures.json')))['rows']}
    same = all(json.dumps(base_rows[short(r['mod_package'])], sort_keys=True) == json.dumps(r, sort_keys=True)
               for r in leon)
    check(same, 'the 10 Leon rows are byte-equal (as JSON) to the round-5 console plan rows')
    bm = json.load(open(os.path.join(out, 'base-material-report.json')))
    res = {}
    for m in bm['materials']:
        res[m['result']] = res.get(m['result'], 0) + 1
    print('base-material route:', res)
    check(res == {'parent-only': 5, 'placeholder': 8} and bm['shipped'] == 0, 'base route: 8 placeholder, 5 parent-only, 0 shipped')
    vf = plan['invisible_route']['vertex_factory_tokens']
    check(vf['static']['token'] == 'FLocalVertexFactoryShaderParameters' and vf['static']['trained'] == 82,
          'static token learned from 82 game materials')
    # manifest
    man = json.load(open(os.path.join(out, 'manifest-r6', 'manifest.json')))
    check(len(man['rows']) == 24, 'manifest rows 24 (%d)' % len(man['rows']))
    check(len(man.get('textures') or []) == 30, 'manifest derived textures 30')
    staged = json.load(open(os.path.join(EVID, 'manifest-r6', 'manifest.json')))
    check(man['manifest_sha256'] == staged['manifest_sha256'], 'manifest sha equals the staged evidence (%s)'
          % man['manifest_sha256'][:16])
    # paktx dry run
    pj = json.load(open(os.path.join(out, 'leon12r6', 'plan.json')))
    sub = pj.get('substitution') or {}
    post = (sub.get('post_build') or [{}])[0]
    check(post.get('substituted_chunks_verified') == 84, 'paktx: substituted chunks verified == 84 (%s)'
          % post.get('substituted_chunks_verified'))
    check(len(sub.get('textures') or []) == 30, 'paktx: 30 derived textures')
    check(sorted(sub.get('user_decisions') or []) == ['accept-unproven-opaque', 'allow-partial'],
          'paktx: user decisions accept-unproven-opaque + allow-partial (%s)' % sub.get('user_decisions'))
    check(len(sub.get('rows') or []) == 24 and not sub.get('plan_rows_not_shipped'),
          'paktx: 24 substitute rows, no plan row left unshipped')
    check(sub.get('reparent_plan_sha256') == '889c986a5a281b487f972dbcd6e51841215e486958c745432443f34d9e60a093',
          'paktx: bound to the round-6 plan sha')
    # attribution
    at = json.load(open(os.path.join(out, 'attribution.json')))
    c = at['counts']
    check(c['mod_meshes'] == 25 and c['decoded'] == 25 and c['slots'] > 0, 'attribution: 25 mod meshes decoded')
    lens = [m for a in at['accessory_meshes'] if short(a['mesh']) in ('SM_Glasses_Lenses', 'SK_SimonGlasses')
            for m in a['materials'] if short(m['material']) == 'MI_Simon_Accessories_Glass']
    check(len(lens) == 2 and all(m['status'] == 'invisible-partial' for m in lens),
          'glasses lens: invisible (partial) on both meshes')
    ph = [s['status'] for e in at['mod_meshes'] if e['decoded'] for s in e['slots']
          if (s['material'] or '').rsplit('/', 1)[-1].startswith('Simon_Slot')]
    check(len(ph) == 88 and set(ph) == {'placeholder'}, 'Simon_Slot slots: 88, all placeholder')
    print('check_real_r6: %d checks, %d failed' % (checks[0], len(fails)))
    return 1 if fails or checks[0] == 0 else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
