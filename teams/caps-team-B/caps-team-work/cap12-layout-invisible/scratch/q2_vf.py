import sys, collections
sys.dont_write_bytecode=True
T='/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-fulllook/PATCH/tools'
sys.path.insert(0,T)
import container_header, cityhash, asset_registry as ar, material_class as mc, shader_library as sl
F='/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/evidence/cap12-fetched-20261013/manifests/'
X='/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Townfall'
store={}
for c in ('0','1','10','11','12'):
    store.update(container_header.parse(open(F+'pakchunk%s-ps5.container-header.bin'%c,'rb').read())[1])
pairs=[(sl.Library(open(F+'Townfall_Chunk0.type8.bin','rb').read()), mc.load_stinfo(open(X+'/Content/ShaderTypeInfo-Townfall_Chunk0-SF_PS5-SP_PS5.stinfo','rb').read())),
       (sl.Library(open(F+'Global.type8.bin','rb').read()), mc.load_stinfo(open(X+'/Content/ShaderTypeInfo-Global-SF_PS5-SP_PS5.stinfo','rb').read()))]
ti=mc.TypeIndex(pairs, store)
print('stinfo entries per shader (n hashes):', collections.Counter(len(v) for v in pairs[0][1]).most_common(6))
r=ar.load(X+'/AssetRegistry.bin')
cls={cityhash.ue_name_hash(a['package']):a['class'] for a in r.assets}
name={cityhash.ue_name_hash(a['package']):a['package'] for a in r.assets}
importers=collections.defaultdict(set)
for q,v in store.items():
    for i in v['imports']: importers[i].add(q)
MAT='/Script/Engine.Material'; MIC='/Script/Engine.MaterialInstanceConstant'
SK='/Script/Engine.SkeletalMesh'; SM='/Script/Engine.StaticMesh'
def mesh_users(pid, depth=8):
    """mesh classes reaching pid through MIC chains (store imports)."""
    out=set(); front=[pid]; seen=set()
    for _ in range(depth):
        nxt=[]
        for p in front:
            for u in importers.get(p,()):
                if u in seen: continue
                seen.add(u); c=cls.get(u)
                if c in (SK,SM): out.add(c)
                elif c==MIC: nxt.append(u)
        front=nxt
    return out
sk=[];st=[]
for pid,c in cls.items():
    if c!=MAT or pid not in store: continue
    sig,_=ti.signature(name[pid])
    if not sig: continue
    u=mesh_users(pid)
    if SK in u: sk.append((name[pid],sig))
    elif u=={SM}: st.append((name[pid],sig))
print('masters reached by skeletal meshes', len(sk), '; by static meshes only', len(st))
hide='/Game/MetaHumans/Common/Materials/M_Hide'
train_sk=[s for n,s in sk if n!=hide]
common=set.intersection(*[set(s) for s in train_sk])
static_any=set().union(*[set(s) for n,s in st])
marker=common-static_any
print('type hashes in EVERY skeletal-used master (M_Hide left out):', len(common), '; of those never in a static-only master:', len(marker))
for h in sorted(marker): print('  marker', hex(h[0]), 'freq', h[1])
hs,_=ti.signature(hide)
print('M_Hide signature size', len(hs), '; has all markers:', marker<=hs, '; markers present', len(marker&hs))
inv='/Game/Townfall/Characters/CharacterProps/IntubationKits/Meshes/mat/M_InvisbleMesh'
s2,_=ti.signature(inv); print('M_InvisbleMesh has markers:', len(marker&s2), 'of', len(marker))
# ---- usage flag from the material's own property block (learned) ----
import json, os, material_props as mp, zen
EV='/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/evidence/'
checked=[x for f in ('round1-check.json','round2-check.json','round3-check.json') for x in json.load(open(EV+'cap12-20261012/'+f))['checked'] if 'verified' in x]
dirs=[EV+'cap12-fetched-20261013/headers-r%d'%i for i in (1,2,3)]
whole={}
for x in checked:
    if x.get('verified')!='iohash': continue
    fn='%016x.hdr.bin'%cityhash.ue_name_hash(x['package'])
    for d in dirs:
        if os.path.exists(d+'/'+fn):
            try: whole[x['package']]=zen.ZenPackage(open(d+'/'+fn,'rb').read())
            except Exception: pass
            break
mats={p:z for p,z in whole.items() if cls.get(cityhash.ue_name_hash(p))==MAT}
print('whole fetched game Materials', len(mats))
pres={}
for p,z in mats.items():
    b=mp.top_export_bytes(z)
    if not b: continue
    ents,_=mp.parse_header(b)
    pres[p]={i for i,zero in ents if not zero}
users={p:mesh_users(cityhash.ue_name_hash(p)) for p in pres}
skp=[p for p in pres if SK in users[p]]; stp=[p for p in pres if users[p]=={SM}]
print('training: skeletal-used', len(skp), 'static-only', len(stp))
def learn(excl):
    a=[p for p in skp if p not in excl]; b=[p for p in stp if p not in excl]
    allk=set.intersection(*[pres[p] for p in a])
    return sorted(k for k in allk if sum(1 for p in b if k not in pres[p])>=3), {k: sum(1 for p in b if k in pres[p]) for k in allk}
ks, cnt = learn({hide})
print('indices present in every skeletal-used master and absent in >=3 static-only masters (M_Hide left out):', ks, {k:cnt[k] for k in ks})
print('M_Hide present indices:', sorted(pres.get(hide,())))
print('M_InvisbleMesh present indices:', sorted(pres.get(inv,())))
for p in sorted(skp): print('  SK', sorted(pres[p])[:30], p.split('/')[-1])
print('---- per-index registry blend correlation')
tags={a['package']:a['tags'] for a in r.assets}
for k in (12,64,108):
    rows=collections.Counter((tags[p].get('BlendMode'), tags[p].get('ShadingModel')) for p in pres if k in pres[p])
    print(k, dict(rows))
def hexdump(b,n=96): return b[:n].hex(' ')
print('---- M_Hide / M_InvisbleMesh / M_Cartilage export head')
for p in (hide, inv, '/Game/MetaHumans/Common/Face/Materials/Baked/M_Cartilage'):
    z=mats.get(p) or next((mats[q] for q in mats if q.endswith('/M_Cartilage')), None) if p.endswith('M_Cartilage') else mats.get(p)
    b=mp.top_export_bytes(z); print(p.split('/')[-1], len(b), hexdump(b,120))
# mod side
sys.path.insert(0,'/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/pak-intake')
import ue_formats as uf
L='/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/'
t=uf.parse_toc(open(L+'zzz_Leon_Townfall_2K_P.utoc','rb').read()); rd=uf.ChunkReader(t,L+'zzz_Leon_Townfall_2K_P.ucas')
for row,cid in enumerate(t.chunk_ids):
    if cid[11]!=1: continue
    d=rd.read_row(row); z=zen.ZenPackage(d)
    if z.name.endswith('/M_Invisible') or '/Simon_Slot' in z.name or z.name.endswith('/M_Leon_Body'):
        b=mp.top_export_bytes(z); ents,_=mp.parse_header(b)
        print('MOD', z.name.split('/')[-1], z.name.split('/')[2], sorted(i for i,zz in ents if not zz), 'zeroed', sorted(i for i,zz in ents if zz), hexdump(b,72))
