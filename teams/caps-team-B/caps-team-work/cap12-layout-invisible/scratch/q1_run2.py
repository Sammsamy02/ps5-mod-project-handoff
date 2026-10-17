import sys
exec(open(sys.argv[1]+'/q1_test.py').read())
z,d,bulk=tex('T_BodywarmerBeanieTag_Deluxe_Deluxe_N')
cands={}
cands['LINEAR']=None
for label,table,blk in [('SW_64K_S','GFX10_SW_64K_S_PATINFO',16),('SW_64K_D','GFX10_SW_64K_D_PATINFO',16),('SW_4K_S','GFX10_SW_4K_S_PATINFO',12),('SW_256_S','GFX10_SW_256_S_PATINFO',8)]:
    info,c=swz.find(pats,table,r'^1 pipes 16 bpe')[0]
    cands[label]=(parse_eq(swz.equation(tabs,info),4,blk),blk)
# transposed SW_64K_S (swap X/Y) as a control
p,b=cands['SW_64K_S']; cands['SW_64K_S_transposed']=([[('Y' if a=='X' else 'X',k) for a,k in t] for t in p],b)
print('SW_64K_S 16bpe address bits 4..15:', [t[0][0]+str(t[0][1]) for t in cands['SW_64K_S'][0]])
out={}
for lab,c in cands.items():
    Vs={}
    for m in (2,3,4):
        data=mip_bytes(z,bulk,m); bw=(4096>>m)//4
        g=linear(data,bw,bw,16) if c is None else grid_fwd(data,bw,bw,16,c[0],c[1],4)
        Vs[m]=to_vals(g,val_bc5_mean)
    ns=nbr_scores(Vs[3])
    c23=corr(downsample(Vs[2]),Vs[3]); c34=corr(downsample(Vs[3]),Vs[4])
    out[lab]=dict(neighbors_mip3=ns, mipcorr_2to3=round(c23,4), mipcorr_3to4=round(c34,4))
    print(lab, out[lab])
import json; json.dump(out, open(sys.argv[3],'w'), indent=1)
