import sys, os, struct, json
sys.dont_write_bytecode=True
T='/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-fulllook/PATCH/tools'
sys.path.insert(0,T)
import zen, cityhash
EV='/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/evidence/'
dirs=[EV+'cap12-fetched-20261013/headers-r%d'%i for i in (1,2,3)]
for mesh in ('/Game/MetaHumans/Simon_Head_5_6_Optimised/Face/SKM_Simon_Head_FaceMesh','/Game/MetaHumans/Zoe_Head_5_6_Optimised/Face/SKM_Zoe_Head_FaceMesh'):
    fn='%016x.hdr.bin'%cityhash.ue_name_hash(mesh)
    p=next((d+'/'+fn for d in dirs if os.path.exists(d+'/'+fn)),None)
    d=open(p,'rb').read(); z=zen.ZenPackage(d)
    print('==',mesh, len(d), 'exports', [(e['name'],e['serial_size']) for e in z.exports][:6])
    print(' imported packages', z.imported_packages)
    # import map: find the import that refers to M_Hide's public export hash
    hide_pkg='/Game/MetaHumans/Common/Materials/M_Hide'
    hp=z.imported_packages.index(hide_pkg)
    print(' import map size', len(z.import_map) if hasattr(z,'import_map') else '?')
    print(' attrs', [a for a in dir(z) if not a.startswith('_')])
    break
print(' names', z.names)
print(' imports (first 30)', [z.describe_ref(i) if callable(z.describe_ref) else i for i in z.imports[:40]])
