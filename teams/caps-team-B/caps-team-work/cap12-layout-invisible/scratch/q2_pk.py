import sys, os, struct
sys.dont_write_bytecode=True
sys.path.insert(0,'/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/pak-intake')
import ue_formats as uf
F='/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/evidence/cap12-fetched-20261013/'
want=sys.argv[1:]
for dd in ('headers-r1','headers-r2','headers-r3'):
    for f in sorted(os.listdir(F+dd)):
        d=open(F+dd+'/'+f,'rb').read()
        try: z=uf.parse_zen_package(d)
        except Exception as ex: continue
        if any(z.name.endswith(w) for w in want):
            print('==',dd,f,z.name,'len',len(d),'hdr',z.header_size)
            print(' names', z.names)
            print(' imported pkgs', z.imported_package_names)
            for e in z.exports: print(' export', e['name'], 'size', e['serial_size'], 'at', e.get('chunk_offset'))
L='/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/'
t=uf.parse_toc(open(L+'zzz_Leon_Townfall_2K_P.utoc','rb').read()); rd=uf.ChunkReader(t,L+'zzz_Leon_Townfall_2K_P.ucas')
for row,cid in enumerate(t.chunk_ids):
    if cid[11]!=1: continue
    d=rd.read_row(row); z=uf.parse_zen_package(d)
    if any(z.name.endswith(w) for w in want):
        print('== MOD',z.name,'len',len(d))
        print(' names', z.names[:80], '...' if len(z.names)>80 else '')
        print(' imported pkgs', z.imported_package_names)
        for e in z.exports: print(' export', e['name'], 'size', e['serial_size'])
        for tok in (b'OpaqueBasePass', b'TranslucentBasePass', b'PCD3D_SM5', b'PCD3D_SM6', b'DXBC', b'DXIL', b'discard', b'clip'):
            print('   ', tok, d.count(tok))
