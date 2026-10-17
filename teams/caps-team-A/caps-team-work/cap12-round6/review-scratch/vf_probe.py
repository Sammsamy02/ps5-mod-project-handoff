"""Reviewer probe (read-only): vertex-factory tokens across the route's training set, token contexts,
zen.py bundle-order vs CookedSerialOffset slices. Writes only to argv[1] (an output dir)."""
import collections, json, os, re, struct, sys
sys.dont_write_bytecode = True
T = '/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-round6/work/tools'
sys.path.insert(0, T)
import asset_registry, cityhash, container_header, material_class as mc, material_props as mprops
import mod_material_audit as mma, reparent_plan as rp, zen, mesh_sections
import invisible_route as ir

WHAT = '/WORKSPACE/.codex/worktrees/translation-core/what'
OBS = WHAT + '/.superpowers/native-observations'
EV = WHAT + '/outputs/2026-10-08-l25-prep/evidence'
F = EV + '/cap12-fetched-20261013'
X = WHAT + '/.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Townfall'
out_dir = sys.argv[1]

store = {}
for c in (0, 1, 10, 11, 12):
    store.update(container_header.parse(open(F + '/manifests/pakchunk%d-ps5.container-header.bin' % c, 'rb').read())[1])
checked = [r for p in ('round1', 'round2', 'round3') for r in json.load(open(EV + '/cap12-20261012/%s-check.json' % p))['checked'] if 'verified' in r]
headers = {r['package']: r for r in checked}
registry = asset_registry.load(X + '/AssetRegistry.bin')
whole = rp.fetched_packages([F + '/headers-r1', F + '/headers-r2', F + '/headers-r3'], checked)
pk, _ = mma.load_mod(OBS + '/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc',
                     OBS + '/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.ucas')
script = mma.load_script_objects(EV + '/console-pulls-20261013-paks2/global.utoc', EV + '/console-pulls-20261013-paks2/global.ucas')
by_id = {cityhash.ue_name_hash(x['package']): x for x in registry.assets}
audit = json.load(open(EV + '/cap12-20261012/leon-material-audit-5hdr.json'))
audit_by = {m['package']: m for m in audit['materials']}


def cls_of(name):
    a_ = registry.asset(name)
    if a_:
        return ir._short(a_['class'])
    m = audit_by.get(name)
    if m:
        return m['class']
    p = pk.get(name)
    if p is None:
        return None
    tops = [e for e in p.exports if e['outer'][0] == 'Null']
    if len(tops) != 1:
        return None
    d = p.describe_ref(tops[0]['class'], script)
    return (d.get('path') or '').rsplit('/', 1)[-1] or None if d else None


game_importers = collections.defaultdict(set)
for q, v in store.items():
    qa = by_id.get(q)
    if qa is None:
        continue
    for i in v['imports']:
        ia = by_id.get(i)
        if ia is not None:
            game_importers[ia['package']].add(qa['package'])

ANY_VF = re.compile(rb'[A-Za-z0-9_]*VertexFactory[A-Za-z0-9_]*')
rows = []
stats = collections.Counter()
for pkg, z in sorted(whole.items()):
    a_ = registry.asset(pkg)
    if not a_ or a_['class'] != mc.MATERIAL_AR_CLASS:
        continue
    ob = None
    try:
        tops = [b for e, b in mesh_sections.export_slices(z) if e['outer'][0] == 'Null']
        ob = tops[0] if len(tops) == 1 else None
        stats['slices_ok'] += 1
    except mesh_sections.MeshError as e:
        stats['slices_fail:' + str(e)[:40]] += 1
    zb = mprops.top_export_bytes(z)
    stats['exports=%d' % len(z.exports)] += 1
    same = (ob == zb) if ob is not None else None
    stats['bundle==cso:%s' % same] += 1
    ucls = sorted(ir.effective_users(pkg, game_importers, {}, cls_of, set()))
    toks = sorted(ir.vf_tokens(ob)) if ob is not None else None
    anyvf = sorted({m.group().decode() for m in ANY_VF.finditer(ob or b'')})
    # where do the tokens sit: export bytes vs whole package bytes vs names
    in_names = sorted(n for n in z.names if 'VertexFactory' in n)
    whole_toks = sorted(ir.vf_tokens(z.data))
    ctx = []
    if ob is not None:
        for m in ir.VF_TOKEN_RE.finditer(ob):
            s = m.start()
            ctx.append((m.group().decode(), ob[max(0, s - 12):s].hex(), ob[m.end():m.end() + 8].hex()))
            if len(ctx) >= 4:
                break
    dom = (a_.get('tags') or {})
    rows.append({'package': pkg, 'users': ucls, 'tokens': toks, 'any_vf_strings': anyvf, 'names_with_vf': in_names,
                 'whole_pkg_tokens': whole_toks, 'n_exports': len(z.exports), 'bundle_equals_cso': same,
                 'ctx': ctx, 'tags': {k: v for k, v in dom.items() if k in ('BlendMode', 'MaterialDomain', 'ShadingModel', 'bUsedWithNanite', 'Usage')}})
json.dump({'stats': stats, 'rows': rows}, open(os.path.join(out_dir, 'vf_probe.json'), 'w'), indent=1, default=list)
tokc = collections.Counter(t for r in rows if r['tokens'] for t in r['tokens'])
anyc = collections.Counter(t for r in rows for t in r['any_vf_strings'])
print('stats', dict(stats))
print('token counts', dict(tokc))
print('any VertexFactory strings', dict(anyc))
print('no-token rows', [r['package'] for r in rows if r['tokens'] is not None and not r['tokens']][:20])
st = [r for r in rows if 'StaticMesh' in r['users'] and r['tokens'] is not None]
sk = [r for r in rows if 'SkeletalMesh' in r['users'] and r['tokens'] is not None]
print('static-used', len(st), 'skeletal-used', len(sk))
print('LocalVF missing in rows:', [(r['package'], r['users'], r['tags']) for r in rows if r['tokens'] is not None and 'FLocalVertexFactoryShaderParameters' not in r['tokens']])
print('skeletal-only rows:', [(r['package'].rsplit('/',1)[-1], r['tokens']) for r in rows if r['users'] == ['SkeletalMesh']])
print('names with VF', collections.Counter(n for r in rows for n in r['names_with_vf']))
print('whole-pkg extra tokens', [(r['package'], sorted(set(r['whole_pkg_tokens']) - set(r['tokens'] or []))) for r in rows if set(r['whole_pkg_tokens']) - set(r['tokens'] or [])][:10])
