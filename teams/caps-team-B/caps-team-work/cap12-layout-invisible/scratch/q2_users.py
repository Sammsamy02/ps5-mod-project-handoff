import sys, collections
sys.dont_write_bytecode=True
T='/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-fulllook/PATCH/tools'
sys.path.insert(0,T)
import container_header, cityhash, asset_registry as ar
F='/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/evidence/cap12-fetched-20261013/manifests/'
store={}
for c in ('0','1','10','11','12'):
    store.update(container_header.parse(open(F+'pakchunk%s-ps5.container-header.bin'%c,'rb').read())[1])
X='/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Townfall'
r=ar.load(X+'/AssetRegistry.bin')
byid={cityhash.ue_name_hash(a['package']):a for a in r.assets}
k0=next(iter(store)); print('store entry keys', store[k0].keys())
for name in ('/Game/MetaHumans/Common/Materials/M_Hide','/Game/Townfall/Characters/CharacterProps/IntubationKits/Meshes/mat/M_InvisbleMesh','/Engine/EngineMaterials/NaniteHiddenSectionMaterial'):
    pid=cityhash.ue_name_hash(name)
    e=store.get(pid)
    print('==',name, 'in store' if e else 'NOT in store', 'shader maps', len(e['shader_maps']) if e else None)
    users=[q for q,v in store.items() if pid in v['imports']]
    cl=collections.Counter(byid[q]['class'] if q in byid else '?' for q in users)
    print(' importers', len(users), dict(cl))
    for q in users:
        if q in byid and 'Skeletal' in byid[q]['class']: print('   SK', byid[q]['package'])
    for q in users[:8]:
        if q in byid and 'Skeletal' not in byid[q]['class']: print('   ', byid[q]['class'], byid[q]['package'])
