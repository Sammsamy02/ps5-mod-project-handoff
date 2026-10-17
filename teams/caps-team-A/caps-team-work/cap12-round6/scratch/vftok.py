import sys,os,collections,json,struct,re
sys.dont_write_bytecode=True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from graph import *
import reparent_plan as rp, material_props as mprops, material_class as mc
EV=W+'/outputs/2026-10-08-l25-prep/evidence/cap12-20261012/'
checked=[r for p in ('round1-check.json','round2-check.json','round3-check.json') for r in json.load(open(EV+p))['checked'] if 'verified' in r]
whole=rp.fetched_packages([F+'/headers-r1',F+'/headers-r2',F+'/headers-r3'],checked)
def users(p,depth=0):
    c=set()
    for q in impby.get(p,()):
        k=cls(q)
        if k=='MaterialInstanceConstant' and depth<8: c|=users(q,depth+1)
        else: c.add(k)
    return c
toks=collections.Counter(); rows=[]
for pkg,z in sorted(whole.items()):
    a=reg.asset(pkg)
    if not a or a['class']!=mc.MATERIAL_AR_CLASS: continue
    b=z.data
    t=set(m.group().decode() for m in re.finditer(rb'F[A-Za-z0-9]*VertexFactory[A-Za-z0-9]*', b))
    u=users(pkg)
    rows.append((pkg,t,u))
    toks.update(t)
print('materials',len(rows)); print(toks)
for tok in toks:
    sk=[r for r in rows if 'SkeletalMesh' in r[2]]; sm=[r for r in rows if 'StaticMesh' in r[2]]; so=[r for r in rows if r[2]=={'StaticMesh'}]
    print(tok, 'skel-used with',sum(tok in r[1] for r in sk),'/',len(sk), ' static-used with',sum(tok in r[1] for r in sm),'/',len(sm), ' static-only with', sum(tok in r[1] for r in so),'/',len(so))
for r in rows:
    if 'StaticMesh' in r[2] and 'FLocalVertexFactoryShaderParameters' not in r[1]: print('STATIC USER WITHOUT LOCAL:', r[0], r[2], r[1])
    if r[0].endswith('M_Hide'): print('M_Hide', r[1], r[2])
print('no-token materials', [ (r[0].split('/')[-1], sorted(r[2])) for r in rows if not r[1]][:20])
