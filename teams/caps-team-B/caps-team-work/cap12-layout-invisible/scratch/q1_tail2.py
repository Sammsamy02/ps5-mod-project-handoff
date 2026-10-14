import sys
exec(open(sys.argv[1]+'/q1_test.py').read())
def tail_coords(elog2, first, nmips, blklog2=16):
    """public addrlib Gfx10 HwlComputeSurfaceInfoMacroTiled tail placement (thin 2D)."""
    maxin = blklog2-4 if blklog2>11 else 1+(1<<(blklog2-9))
    B256={0:(16,16),1:(16,8),2:(8,8),3:(8,4),4:(4,4)}[elog2]
    out={}
    for i in range(first,nmips):
        m=maxin-1-(i-first)
        off=(16<<m) if m>6 else (m<<8)
        mx=sum(((off>>(9+2*k))&1)<<k for k in range(6)); my=sum(((off>>(8+2*k))&1)<<k for k in range(6))
        out[i]=(off, mx*B256[0], my*B256[1])
    return out
name=sys.argv[3]
z,d,bulk=tex(name)
x=uf.export_data(d,z,z.exports[0])
so,_,ss,fl=z.bulk_data[5]
tail=x[so:so+ss]; assert len(tail)==65536
info,c=swz.find(pats,'GFX10_SW_64K_S_PATINFO',r'^1 pipes 16 bpe')[0]
peq=parse_eq(swz.equation(tabs,info),4,16)
tc=tail_coords(4,5,13)
print('tail placement (mip: byte offset, x0, y0 in blocks):', tc)
# mip4 full (bulk) deswizzled
V4=to_vals(grid_fwd(mip_bytes(z,bulk,4),64,64,16,peq,16,4),val_bc5_mean)
prev=V4
res={}
for mip in range(5,9):
    bw=max(1,(4096>>mip)//4)
    off,x0,y0=tc[mip]
    g=[[None]*bw for _ in range(bw)]
    for yy in range(bw):
        for xx in range(bw):
            e=addr_of(peq,xx+x0,yy+y0,64,64,1,16,4)
            g[yy][xx]=tail[e*16:(e+1)*16]
    V=to_vals(g,val_bc5_mean)
    # wrong-placement control: same mip read at origin (0,0)
    g0=[[tail[addr_of(peq,xx,yy,64,64,1,16,4)*16:addr_of(peq,xx,yy,64,64,1,16,4)*16+16] for xx in range(bw)] for yy in range(bw)]
    V0=to_vals(g0,val_bc5_mean)
    if bw>=2:
        res[mip]=(round(corr(downsample(prev),V),4), round(corr(downsample(prev),V0),4))
    prev=V
print('mip: corr(downsample(previous mip), this mip) [addrlib placement, control at origin]', res)
