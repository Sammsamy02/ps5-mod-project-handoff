import json, sys
sys.dont_write_bytecode = True
exec(open(sys.argv[1]).read().split("ANY_VF =")[0])
plan = json.load(open(sys.argv[2]))
vf = {r['package']: r for r in json.load(open(sys.argv[3]))['rows']}
game_imports = collections.defaultdict(set)
for q, ims in game_importers.items():
    for i in ims:
        game_imports[i].add(q)
for r in plan['rows']:
    if r.get('route') != 'invisible' or r['users_proven_by'] != 'user-classes':
        continue
    cur, chain = r['mod_package'], []
    for _ in range(8):
        mats = sorted(q for q in game_imports.get(cur, ()) if cls_of(q) in ('MaterialInstanceConstant', 'Material'))
        if len(mats) != 1:
            chain.append('%d imports' % len(mats)); break
        cur = mats[0]; chain.append(cur.rsplit('/', 1)[-1])
        if cls_of(cur) == 'Material':
            break
    m = vf.get(cur)
    print(r['action'], r['mod_package'].rsplit('/', 1)[-1], '->', chain, '| master VF strings:', m['any_vf_strings'] if m else 'not fetched')
