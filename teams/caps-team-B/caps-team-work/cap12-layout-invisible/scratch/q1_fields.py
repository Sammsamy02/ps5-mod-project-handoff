import sys, struct
sys.dont_write_bytecode=True
sys.path.insert(0,'/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/pak-intake')
import ue_formats as uf
def load(utoc, ucas, want):
    t=uf.parse_toc(open(utoc,'rb').read()); rd=uf.ChunkReader(t,ucas)
    for row,cid in enumerate(t.chunk_ids):
        if cid[11]!=1: continue
        d=rd.read_row(row)
        try: z=uf.parse_zen_package(d)
        except Exception: continue
        if z.name.endswith(want): return z,d
def show(z,d,label):
    e=z.exports[0]; x=uf.export_data(d,z,e)
    pf=[n for n in z.names if n.startswith('PF_')][0]
    needle=struct.pack('<i',len(pf)+1)+pf.encode()+b'\0'; at=x.find(needle)
    print('==',label,z.name,'export',len(x),'bytes; PF string at',at)
    print('names:', z.names)
    print('bulk map:', z.bulk_data)
    pre=x[:at]
    print('bytes 0..PF:', pre.hex(' '))
    q=at+len(needle)
    print('after PF (64B):', x[q:q+64].hex(' '))
    print('tail of export (48B):', x[-48:].hex(' '))
M='/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/pak-mirror-townfall-20261013/'
L='/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/'
z,d=load(M+'pakchunk10-ps5.utoc',M+'pakchunk10-ps5.ucas','Deluxe_Deluxe_N'); show(z,d,'GAME')
z,d=load(L+'zzz_Leon_Townfall_2K_P.utoc',L+'zzz_Leon_Townfall_2K_P.ucas','T_Leon_UpperBody_N'); show(z,d,'MOD')
z,d=load(L+'zzz_Leon_Townfall_2K_P.utoc',L+'zzz_Leon_Townfall_2K_P.ucas','T_Leon_UpperBody_D'); show(z,d,'MOD')
