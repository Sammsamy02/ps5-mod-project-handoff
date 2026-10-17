import sys, json, os
sys.dont_write_bytecode=True
T='/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-fulllook/PATCH/tools'
sys.path.insert(0,T)
import reparent_plan as rp, material_class as mc, material_props as mprops, mod_material_audit as mma, asset_registry, container_header
WHAT='/WORKSPACE/.codex/worktrees/translation-core/what'; EV=WHAT+'/outputs/2026-10-08-l25-prep/evidence'; F=EV+'/cap12-fetched-20261013'
X=WHAT+'/.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Townfall'
L=WHAT+'/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/'
store={}
for c in ('0','1','10','11','12'): store.update(container_header.parse(open(F+'/manifests/pakchunk%s-ps5.container-header.bin'%c,'rb').read())[1])
checked=[r for p in ('round1','round2','round3') for r in json.load(open(EV+'/cap12-20261012/%s-check.json'%p))['checked'] if 'verified' in r]
headers={r['package']:r for r in checked}
reg=asset_registry.load(X+'/AssetRegistry.bin')
ti=rp.load_type_index([F+'/manifests/Townfall_Chunk0.type8.bin',F+'/manifests/Global.type8.bin'],[X+'/Content/ShaderTypeInfo-Townfall_Chunk0-SF_PS5-SP_PS5.stinfo',X+'/Content/ShaderTypeInfo-Global-SF_PS5-SP_PS5.stinfo'],store)
peers=mc.Peers(reg,ti)
whole=rp.fetched_packages([F+'/headers-r1',F+'/headers-r2',F+'/headers-r3'],checked)
rc=mprops.ReaderC(mprops.training_set(whole,reg,mc.MATERIAL_AR_CLASS),peers.bits)
print('readerC', rc.full.ok, rc.full.found, rc.full.n_train)
for g in ('/Game/MetaHumans/Common/Materials/M_Hide','/Game/Townfall/Characters/CharacterProps/IntubationKits/Meshes/mat/M_InvisbleMesh'):
    bp=mc.base_props(g,reg,ti,peers,headers)
    c=rc.base(g, mprops.top_export_bytes(whole[g]), {g})
    print(g.split('/')[-1], 'base_props', {k:bp.get(k) for k in ('readable','blend','blend_group','lit','models','why','sources')}); print('   readerC LOO', c)
pk,_=mma.load_mod(L+'zzz_Leon_Townfall_2K_P.utoc',L+'zzz_Leon_Townfall_2K_P.ucas')
for m in ('/Game/MetaHumans/Simon_Head_5_6_Optimised/Face/Materials/M_Invisible','/Game/Townfall/Characters/Simon/Materials/M_Invisible','/Game/Townfall/Characters/Simon/Meshes/Simon_Slot0','/Game/Leon/M_Leon_Body'):
    b=mprops.top_export_bytes(pk[m])
    print(m.split('/')[-1], m.split('/')[2], 'readerC', rc.mod(b)); mp_=mc.mod_props(b,peers); print('    readers A/B', {k:mp_.get(k) for k in ('readable','blend_group','why','reader_b','reader_a_classes','unproven_opaque')})
