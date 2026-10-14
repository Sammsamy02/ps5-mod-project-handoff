import sys, json
exec(open(sys.argv[1]+'/q1_test.py').read())
L='/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/'
t=uf.parse_toc(open(L+'zzz_Leon_Townfall_2K_P.utoc','rb').read()); rd=uf.ChunkReader(t,L+'zzz_Leon_Townfall_2K_P.ucas')
pk={}
for row,cid in enumerate(t.chunk_ids): pk.setdefault(cid[:8],{})[cid[11]]=row
info,c=swz.find(pats,'GFX10_SW_64K_S_PATINFO',r'^1 pipes 16 bpe')[0]; S16=parse_eq(swz.equation(tabs,info),4,16)
info,c=swz.find(pats,'GFX10_SW_64K_S_PATINFO',r'^1 pipes 8 bpe')[0]; S8=parse_eq(swz.equation(tabs,info),3,16)
print('SW_64K_S 8bpe address bits 3..15:', [t_[0][0]+str(t_[0][1]) for t_ in S8])
def val_dxt1(b):
    c0,c1=struct.unpack_from('<HH',b,0)
    def rgb(c): return ((c>>11)&31)*8, ((c>>5)&63)*4, (c&31)*8
    a,bb=rgb(c0),rgb(c1); return tuple((p+q)/2 for p,q in zip(a,bb))
out={}
for k,v in sorted(pk.items()):
    if 1 not in v or 2 not in v: continue
    d=rd.read_row(v[1]); z=uf.parse_zen_package(d)
    pf=[n for n in z.names if n.startswith('PF_')]
    if not pf or not z.name.split('/')[-1] in ('T_Leon_UpperBody_N','T_Leon_UpperBody_D','T_Leon_LowerBody_D','T_Leon_Face_N'): continue
    bulk=rd.read_row(v[2]); pf=pf[0]
    eb = 8 if pf=='PF_DXT1' else 16; valf = val_dxt1 if pf=='PF_DXT1' else val_bc5_mean
    S = S8 if eb==8 else S16
    sx=struct.unpack_from('<i',uf.export_data(d,z,z.exports[0]), 0)  # unused
    W=None
    # size from first bulk size
    n0=z.bulk_data[0][2]; bw=int((n0//eb)**0.5)
    r={}
    for lab in ('LINEAR','SW_64K_S'):
        Vs={}
        for m in (1,2,3):
            so,_,ss,_=z.bulk_data[m]; data=bulk[so:so+ss]; w=bw>>m
            g=linear(data,w,w,eb) if lab=='LINEAR' else grid_fwd(data,w,w,eb,S,16,3 if eb==8 else 4)
            Vs[m]=to_vals(g,valf)
        r[lab]=dict(mip1to2=round(corr(downsample(Vs[1]),Vs[2]),4), mip2to3=round(corr(downsample(Vs[2]),Vs[3]),4))
    out[z.name]=dict(format=pf, blocks_wide_mip0=bw, **r); print(z.name.split('/')[-1], pf, r)
json.dump(out,open(sys.argv[3],'w'),indent=1)
