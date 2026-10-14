"""Reviewer probe: instance nodes visited by the user walk of each invisible row (read-only)."""
import json, sys, collections
sys.dont_write_bytecode = True
exec(open(sys.argv[1]).read().split("ANY_VF =")[0])
plan = json.load(open(sys.argv[2]))
mod_imports = {n: set(p.imported_packages) for n, p in pk.items()}
overridden = {n for n in pk if registry.asset(n)}
for r in plan['rows']:
    if r.get('route') != 'invisible':
        continue
    pkg = r['mod_package']
    seen, front, inst = {pkg}, [pkg], []
    for _ in range(8):
        nxt = []
        for p in front:
            imp = {q for q in game_importers.get(p, ()) if q not in overridden}
            imp |= {q for q, ims in mod_imports.items() if p in ims}
            for q in sorted(imp):
                if q in seen: continue
                seen.add(q)
                if cls_of(q) == 'MaterialInstanceConstant':
                    nxt.append(q); inst.append((q.rsplit('/',1)[-1], 'overridden' if q in overridden else ('mod-only' if q in pk else 'game')))
        front = nxt
    gi = sorted(q.rsplit('/',1)[-1] for q in game_importers.get(pkg, ()) if q in overridden)
    print(r['action'], pkg.rsplit('/',1)[-1], 'instance nodes:', inst, '| game importers replaced by mod:', gi)
