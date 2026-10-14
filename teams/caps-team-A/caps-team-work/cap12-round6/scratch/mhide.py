import sys,struct
sys.dont_write_bytecode=True
sys.path.insert(0,'/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-textures/PATCH/tools')
import cityhash, zen
EV='/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/evidence/cap12-fetched-20261013/'
d=open(EV+'headers-r2/ad7b3742feeb3d06.hdr.bin','rb').read()
z=zen.ZenPackage(d)
print(z.names, z.imported_packages, [(e['name'],e['serial_size'],e.get('data_offset')) for e in z.exports], z.header_size, len(d))
e=z.exports[0]; b=z.export_bytes(e)
print(len(b))
names=['FLocalVertexFactory','TGPUSkinVertexFactoryDefault','FGPUSkinPassthroughVertexFactory','TGPUSkinVertexFactory','FGPUBaseSkinVertexFactory','FNaniteVertexFactory','FInstancedStaticMeshVertexFactory','FNiagaraSpriteVertexFactory','FNiagaraMeshVertexFactory','FHairCardsVertexFactory','TGPUSkinAPEXClothVertexFactoryDefault','FSplineMeshVertexFactory','FLandscapeVertexFactory','TGPUSkinVertexFactoryUnlimited','FGeometryCollectionVertexFactory','FNaniteVertexFactory','FDepthOnlyPS','TBasePassPSFNoLightMapPolicy']
for nm in names:
    for v in (nm,nm.upper(),nm.lower()):
      for enc in ('utf-8','utf-16-le'):
        h=cityhash.cityhash64(v.encode(enc))
        hb=struct.pack('<Q',h)
        if hb in d: print('HIT',nm,v,enc,d.count(hb), d.find(hb))
# print repeated u64 values
import collections
c=collections.Counter(struct.unpack_from('<Q',b,o)[0] for o in range(0,len(b)-8,8))
print(c.most_common(30))
import material_class as mc
X='/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Townfall/Content/'
st=mc.load_stinfo(open(X+'ShaderTypeInfo-Townfall_Chunk0-SF_PS5-SP_PS5.stinfo','rb').read())
uni=set(t for v in st for t in v)
found=collections.Counter()
for o in range(0,len(b)-8):
    v=struct.unpack_from('<Q',b,o)[0]
    if v in uni: found[v]+=1
print('stinfo hashes in M_Hide export:',len(found), found.most_common(10))
for v in (5917744173951221787,4576653922762622501,578328384472350733):
    print(v, v in uni, [ (o) for o in range(0,len(b)-8) if struct.unpack_from('<Q',b,o)[0]==v][:12])
