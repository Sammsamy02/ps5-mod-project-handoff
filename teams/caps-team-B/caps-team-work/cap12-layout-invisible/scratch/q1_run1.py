import sys
exec(open(sys.argv[1]+'/q1_test.py').read())
VAL=val_bc5
name=sys.argv[3]; mips=[int(x) for x in sys.argv[4].split(',')]
z,d,bulk=tex(name)
print(z.name, len(bulk))
for m in mips:
    data=mip_bytes(z,bulk,m)
    tex_w=4096>>m; bw=tex_w//4
    print('mip',m,'texels',tex_w,'blocks',bw,'bytes',len(data))
    res=[('LINEAR', round(score(linear(data,bw,bw,16), VAL),4))]
    for table,cre,blk in [('GFX10_SW_64K_S_PATINFO',r'^1 pipes 16 bpe',16),('GFX10_SW_64K_D_PATINFO',r'^1 pipes 16 bpe',16),
                      ('GFX10_SW_4K_S_PATINFO',r'^1 pipes 16 bpe',12),('GFX10_SW_4K_D_PATINFO',r'^1 pipes 16 bpe',12),
                      ('GFX10_SW_64K_S_X_RBPLUS_PATINFO',r'16 bpe',16),('GFX10_SW_64K_D_X_RBPLUS_PATINFO',r'16 bpe',16),
                      ('GFX10_SW_64K_R_X_1xaa_RBPLUS_PATINFO',r'16 bpe',16),('GFX10_SW_64K_Z_X_1xaa_RBPLUS_PATINFO',r'16 bpe',16)]:
        seen=set()
        for info,c in swz.find(pats,table,cre):
            eq=tuple(swz.equation(tabs,info)[:blk])
            if eq in seen: continue
            seen.add(eq)
            try: peq=parse_eq(list(eq),4,blk)
            except ValueError as ex: res.append((table+' | '+c,'non-XY term '+str(ex))); continue
            tw,th=block_dims(peq)
            if tw*th*16 != 1<<blk: res.append((table+' | '+c,'dims')); continue
            res.append((table.replace('GFX10_SW_','').replace('_PATINFO','')+' | '+c, round(score(grid_fwd(data,bw,bw,16,peq,blk,4),VAL),4)))
    for r in sorted(res, key=lambda r: r[1] if isinstance(r[1],float) else 9): print('  ',r)
