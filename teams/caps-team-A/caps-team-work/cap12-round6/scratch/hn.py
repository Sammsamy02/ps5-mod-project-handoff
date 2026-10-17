import sys,struct,zlib
sys.dont_write_bytecode=True
sys.path.insert(0,'/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-textures/PATCH/tools')
import cityhash, material_class as mc
X='/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Townfall/Content/'
st=mc.load_stinfo(open(X+'ShaderTypeInfo-Townfall_Chunk0-SF_PS5-SP_PS5.stinfo','rb').read())
st2=mc.load_stinfo(open(X+'ShaderTypeInfo-Global-SF_PS5-SP_PS5.stinfo','rb').read())
uni=set(t for v in st+st2 for t in v)
names='''FHitProxyPS FHitProxyVS FVelocityPS FVelocityVS TBasePassPSFNoLightMapPolicy TBasePassVSFNoLightMapPolicy FDepthOnlyPS FDepthOnlyVS TDepthOnlyVS<false> TDepthOnlyVS<true> TDepthOnlyPS<false> TDepthOnlyPS<true> FDepthOnlyPS<false> FDepthOnlyPS<true> FPositionOnlyDepthVS FShadowDepthBasePS TShadowDepthVSVertexShadowDepth_PerspectiveCorrect FLumenCardPS FLumenCardVS FMeshDecalsPS FAnisotropyPS FDebugViewModePS FClearUAVUIntCS FCopyTextureCS FScreenVS FScreenPS FPostProcessVS FFillTextureCS TBasePassPSFCachedPointIndirectLightingPolicy FLumenCardPS<false> FLumenCardPS<true> FNaniteMultiViewMaterialVS FVisualizeTexturePS FHairMaterialPS FHairMaterialVS'''.split()
schemes={
 'u8':lambda s:s.encode(),'U8':lambda s:s.upper().encode(),'l8':lambda s:s.lower().encode(),
 'u16':lambda s:s.encode('utf-16-le'),'U16':lambda s:s.upper().encode('utf-16-le'),'l16':lambda s:s.lower().encode('utf-16-le'),
 'U16z':lambda s:(s.upper()+'\0').encode('utf-16-le'),'U8z':lambda s:(s.upper()+'\0').encode(),
}
for nm in names:
    for k,f in schemes.items():
        h=cityhash.cityhash64(f(nm))
        if h in uni: print('HIT',nm,k)
print('done', len(uni))
