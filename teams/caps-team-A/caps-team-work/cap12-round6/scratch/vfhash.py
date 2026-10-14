import sys,collections
sys.dont_write_bytecode=True
sys.path.insert(0,'/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-textures/PATCH/tools')
import cityhash, material_class as mc
X='/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Townfall/Content/'
st=mc.load_stinfo(open(X+'ShaderTypeInfo-Townfall_Chunk0-SF_PS5-SP_PS5.stinfo','rb').read())
cnt=collections.Counter(t for v in st for t in v)
pos=collections.Counter((i,t) for v in st for i,t in enumerate(v))
lens=collections.Counter(len(v) for v in st)
print('shaders',len(st),'universe',len(cnt),'lens',lens)
names=['FLocalVertexFactory','TGPUSkinVertexFactoryDefault','FGPUSkinPassthroughVertexFactory','TGPUSkinVertexFactory','FGPUBaseSkinVertexFactory','FGPUSkinVertexFactoryDefault','TBasePassPSFNoLightMapPolicy','FNaniteVertexFactory','TGPUSkinAPEXClothVertexFactoryDefault','FInstancedStaticMeshVertexFactory','FNiagaraSpriteVertexFactory','FNiagaraMeshVertexFactory','FDepthOnlyPS','TDepthOnlyVS<false>','FHairCardsVertexFactory','FGPUSkinVertexFactory']
for nm in names:
    for v in (nm, nm.upper(), nm.lower()):
        for enc in ('utf-8','utf-16-le'):
            h=cityhash.cityhash64(v.encode(enc))
            if h in cnt: print('HIT',nm,v==nm.upper() and 'UPPER' or v==nm.lower() and 'lower' or 'asis',enc,hex(h),cnt[h],[i for (i,t) in pos if t==h])
