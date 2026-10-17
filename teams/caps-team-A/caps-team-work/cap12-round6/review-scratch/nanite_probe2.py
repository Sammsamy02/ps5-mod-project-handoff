import json, sys, collections
sys.dont_write_bytecode = True
exec(open(sys.argv[1]).read().split("ANY_VF =")[0])
import invisible_route as ir
st = json.load(open(sys.argv[2]))
nanite = {x['package']: (x.get('tags') or {}).get('NaniteEnabled') for x in registry.assets if ir._short(x['class']) == 'StaticMesh'}
tab = collections.Counter()
for pkg, strs in st.items():
    names = {}
    ir.effective_users(pkg, game_importers, {}, cls_of, set(), names=names)
    sm = names.get('StaticMesh', [])
    has_n = any(nanite.get(q) == 'True' for q in sm)
    only_n = bool(sm) and all(nanite.get(q) == 'True' for q in sm)
    cs = any(s.startswith('TBasePassCS') for s in strs)
    tab[('nanite_user' if has_n else ('static_non_nanite_only' if sm else 'no_static_user'), 'CS' if cs else 'noCS')] += 1
print(sorted(tab.items()))
print('nanite-enabled static meshes in registry:', sum(1 for v in nanite.values() if v == 'True'), 'of', len(nanite))
for pkg, strs in st.items():
    names = {}
    ir.effective_users(pkg, game_importers, {}, cls_of, set(), names=names)
    sm = [q for q in names.get('StaticMesh', []) if nanite.get(q) == 'True']
    if sm and not any(s.startswith('TBasePassCS') for s in strs):
        a_ = registry.asset(pkg)
        print('EXCEPTION', pkg, (a_.get('tags') or {}).get('BlendMode'), sm[:3])
