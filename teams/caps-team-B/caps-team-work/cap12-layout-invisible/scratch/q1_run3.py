import sys, json
exec(open(sys.argv[1]+'/q1_test.py').read())
info,c=swz.find(pats,'GFX10_SW_64K_S_PATINFO',r'^1 pipes 16 bpe')[0]
S=parse_eq(swz.equation(tabs,info),4,16)
out={}
for nm in ('T_Shirt_Deluxe_D','T_TrousersShoes_Deluxe_D','T_BodywarmerBeanieTag_Deluxe_D','T_BodywarmerBeanieTag_Deluxe_MRO','T_BodywarmerBeanieTag_Deluxe_Deluxe_N'):
    z,d,bulk=tex(nm); valf = val_bc5_mean if nm.endswith('_N') else val_bc7_mean
    r={}
    for lab in ('LINEAR','SW_64K_S'):
        Vs={}
        for m in (2,3,4):
            data=mip_bytes(z,bulk,m); bw=(4096>>m)//4
            g=linear(data,bw,bw,16) if lab=='LINEAR' else grid_fwd(data,bw,bw,16,S,16,4)
            Vs[m]=to_vals(g,valf)
        r[lab]=dict(mip2to3=round(corr(downsample(Vs[2]),Vs[3]),4), mip3to4=round(corr(downsample(Vs[3]),Vs[4]),4))
    out[z.name]=r; print(z.name.split('/')[-1], r)
json.dump(out,open(sys.argv[3],'w'),indent=1)
