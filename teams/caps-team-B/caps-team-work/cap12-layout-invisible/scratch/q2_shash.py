import sys, collections
sys.dont_write_bytecode=True
T='/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-fulllook/PATCH/tools'
sys.path.insert(0,T)
import container_header, cityhash, asset_registry as ar, shader_library as sl
F='/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/evidence/cap12-fetched-20261013/manifests/'
X='/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Townfall'
store={}
for c in ('0','1','10','11','12'):
    store.update(container_header.parse(open(F+'pakchunk%s-ps5.container-header.bin'%c,'rb').read())[1])
libs=[sl.Library(open(F+'Townfall_Chunk0.type8.bin','rb').read()), sl.Library(open(F+'Global.type8.bin','rb').read())]
r=ar.load(X+'/AssetRegistry.bin')
def shaders(pkg):
    e=store.get(cityhash.ue_name_hash(pkg))
    if not e or not e['shader_maps']: return None
    out=set()
    for mh in e['shader_maps']:
        for lib in libs:
            sm=lib.shader_map(mh)
            if sm is not None:
                for s in sm: out.add((lib.shader_hashes[s['shader_index']], s['frequency']))
                break
    return out
print('sample shader map entry keys:', libs[0].shader_map(store[cityhash.ue_name_hash('/Game/MetaHumans/Common/Materials/M_Hide')]['shader_maps'][0])[:2])
pop={}
for a in r.assets:
    if a['class'] in ('/Script/Engine.Material','/Script/Engine.MaterialInstanceConstant'):
        s=shaders(a['package'])
        if s: pop[a['package']]=s
print('materials/instances with PS5 shader maps:', len(pop))
cnt=collections.Counter(h for s in pop.values() for h in s)
H='/Game/MetaHumans/Common/Materials/M_Hide'
I='/Game/Townfall/Characters/CharacterProps/IntubationKits/Meshes/mat/M_InvisbleMesh'
N='/Engine/EngineMaterials/NaniteHiddenSectionMaterial'
for p in (H,I,N):
    s=pop.get(p); print(p.split('/')[-1], 'shaders', len(s) if s else None, 'freqs', collections.Counter(f for _,f in s) if s else None)
for a,b in ((H,I),(H,N),(I,N)):
    sh=pop[a]&pop[b]
    rows=[(f, cnt[(h,f)]) for h,f in sh]
    print(a.split('/')[-1],'&',b.split('/')[-1],'shared', len(sh), 'by freq', collections.Counter(f for f,_ in rows), 'how many materials share each:', sorted(rows))
# who else has M_Hide's pixel shaders?
for h,f in sorted(pop[H], key=lambda x:x[1]):
    others=[p.split('/')[-1] for p,s in pop.items() if (h,f) in s and p!=H]
    print('M_Hide shader freq',f, h.hex()[:12], 'also in', len(others), others[:6])
