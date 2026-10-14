import sys,os,collections,json,struct
sys.dont_write_bytecode=True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from graph import *
import reparent_plan as rp, material_props as mprops, material_class as mc
EV=W+'/outputs/2026-10-08-l25-prep/evidence/cap12-20261012/'
checked=[r for p in ('round1-check.json','round2-check.json','round3-check.json') for r in json.load(open(EV+p))['checked'] if 'verified' in r]
whole=rp.fetched_packages([F+'/headers-r1',F+'/headers-r2',F+'/headers-r3'],checked)
Xc=X+'/Content/'
st=mc.load_stinfo(open(Xc+'ShaderTypeInfo-Townfall_Chunk0-SF_PS5-SP_PS5.stinfo','rb').read())+mc.load_stinfo(open(Xc+'ShaderTypeInfo-Global-SF_PS5-SP_PS5.stinfo','rb').read())
uni=set(t for v in st for t in v)
def users(p):
    c=set()
    for q in impby.get(p,()):
        k=cls(q)
        if k=='MaterialInstanceConstant':
            c|=users(q)
        else: c.add(k)
    return c
mats=[]
for pkg,z in sorted(whole.items()):
    a=reg.asset(pkg)
    if not a or a['class']!=mc.MATERIAL_AR_CLASS: continue
    b=mprops.top_export_bytes(z)
    if not b: continue
    vals=set(struct.unpack_from('<Q',b,o)[0] for o in range(0,len(b)-7,4))
    vals={v for v in vals if v>>48 not in (0,0xffff) and v not in uni}
    mats.append((pkg,vals,users(pkg),len(b)))
print('materials',len(mats))
cnt=collections.Counter(v for _,vals,_,_ in mats for v in vals)
common=[(v,c) for v,c in cnt.most_common(40)]
for v,c in common[:40]:
    sk=sum(1 for _,vals,u,_ in mats if v in vals and 'SkeletalMesh' in u)
    stc=sum(1 for _,vals,u,_ in mats if v in vals and u=={'StaticMesh'})
    print(hex(v), c, 'skel',sk,'static-only',stc)
print('totals skel',sum(1 for m in mats if 'SkeletalMesh' in m[2]),'static-only',sum(1 for m in mats if m[2]=={'StaticMesh'}))
hide=[m for m in mats if m[0].endswith('/M_Hide')]
print('M_Hide', [hex(v) for v,c in common[:40] if hide and v in hide[0][1]])
