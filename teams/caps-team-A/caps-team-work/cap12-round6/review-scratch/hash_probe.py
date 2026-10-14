"""Reviewer probe: look for FHashedName-style 64-bit hashes of vertex-factory TYPE names in material exports."""
import json, struct, sys, collections
sys.dont_write_bytecode = True
exec(open(sys.argv[1]).read().split("ANY_VF =")[0])
import cityhash
VFS = ['FLocalVertexFactory', 'FGPUSkinPassthroughVertexFactory', 'TGPUSkinVertexFactoryDefault', 'TGPUSkinVertexFactoryUnlimited',
       'FGPUBaseSkinVertexFactory', 'TGPUSkinAPEXClothVertexFactoryDefault', 'TGPUSkinAPEXClothVertexFactoryUnlimited',
       'FInstancedStaticMeshVertexFactory', 'FSplineMeshVertexFactory', 'FNaniteVertexFactory', 'FGeometryCollectionVertexFactory',
       'FNiagaraMeshVertexFactory', 'FNiagaraSpriteVertexFactory', 'FGPUSkinVertexFactory', 'TGPUSkinVertexFactory']
def variants(n):
    for nm in (n, n.upper(), n.lower()):
        for enc in ('utf-16-le', 'utf-8'):
            yield (nm == n and 'asis' or nm == n.upper() and 'upper' or 'lower') + '/' + enc, cityhash.cityhash64(nm.encode(enc))
mats = {}
for pkg, z in sorted(whole.items()):
    a_ = registry.asset(pkg)
    if not a_ or a_['class'] != mc.MATERIAL_AR_CLASS:
        continue
    tops = [b for e, b in mesh_sections.export_slices(z) if e['outer'][0] == 'Null']
    mats[pkg] = tops[0]
hits = collections.defaultdict(collections.Counter)
for n in VFS:
    for tag, h in variants(n):
        pat = struct.pack('<Q', h)
        c = sum(1 for b in mats.values() if pat in b)
        if c:
            hits[n][tag] = c
print(dict(hits))
mh = [k for k in mats if k.endswith('/M_Hide')][0]
for n in VFS:
    for tag, h in variants(n):
        if struct.pack('<Q', h) in mats[mh]:
            print('M_Hide has', n, tag)
for k in mats:
    if k.rsplit('/',1)[-1] in ('M_skin_unified_baked','M_MRO_TranslucentGlass_OneSided_ThinTranslusent','M_Head_Baked','M_SkinBasic'):
        print(k.rsplit('/',1)[-1], [n for n in VFS for tag, h in variants(n) if struct.pack('<Q', h) in mats[k]])
