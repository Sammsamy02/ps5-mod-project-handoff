import sys, struct, random, itertools
sys.dont_write_bytecode=True
sys.path.insert(0,'/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/pak-intake')
sys.path.insert(0, sys.argv[1])
import ue_formats as uf, swz
AL=sys.argv[2]
tabs,pats=swz.load(AL+'/gfx10SwizzlePattern.h')
M='/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/pak-mirror-townfall-20261013/'
t=uf.parse_toc(open(M+'pakchunk10-ps5.utoc','rb').read()); rd=uf.ChunkReader(t,M+'pakchunk10-ps5.ucas')
pk={}
for row,cid in enumerate(t.chunk_ids):
    pk.setdefault(cid[:8],{})[cid[11]]=row
def tex(name_part):
    for k,v in pk.items():
        if 1 in v:
            d=rd.read_row(v[1]); z=uf.parse_zen_package(d)
            if z.name.endswith('/'+name_part) or z.name==name_part: return z, d, rd.read_row(v[2]) if 2 in v else None
def mip_bytes(z, bulk, m):
    so,dso,ss,fl=z.bulk_data[m]
    return bulk[so:so+ss]
def eqmap(eq, elog2, tilebits):
    """element index within tile -> (x,y); returns None if XOR terms present (handled separately)."""
    bits=eq[elog2:tilebits]
    xs=[];ys=[]
    n=1<<(tilebits-elog2)
    m=[]
    for i in range(n):
        x=y=0
        for k,sym in enumerate(bits):
            if not (i>>k)&1: continue
            for term in sym.split('^'):
                term=term.strip()
                if term[0]=='X': x^=1<<int(term[1:])
                elif term[0]=='Y': y^=1<<int(term[1:])
                elif term=='0': pass
                else: raise ValueError(term)
        m.append((x,y))
    return m
def deswizzle(data, bw, bh, ebytes, emap, tw, th):
    """returns grid[y][x] of element bytes. tiles row-major."""
    tile_elems=tw*th
    tpr=(bw+tw-1)//tw
    g=[[None]*bw for _ in range(bh)]
    ne=len(data)//ebytes
    for e in range(ne):
        tile, w = divmod(e, tile_elems)
        ty, tx = divmod(tile, tpr)
        x,y=emap[w]; x+=tx*tw; y+=ty*th
        if x<bw and y<bh: g[y][x]=data[e*ebytes:(e+1)*ebytes]
    return g
def linear(data,bw,bh,eb):
    return [[data[(y*bw+x)*eb:(y*bw+x+1)*eb] for x in range(bw)] for y in range(bh)]
def val_bc5(b): return (b[0]+b[1], b[8]+b[9])
def score(g, valf, seed=1):
    h=len(g); w=len(g[0])
    dsum=0; n=0
    def d(a,b):
        va,vb=valf(a),valf(b); return sum(abs(p-q) for p,q in zip(va,vb))
    for y in range(h):
        for x in range(w):
            a=g[y][x]
            if x+1<w: dsum+=d(a,g[y][x+1]); n+=1
            if y+1<h: dsum+=d(a,g[y+1][x]); n+=1
    rnd=random.Random(seed); r=0
    for _ in range(20000):
        r+=d(g[rnd.randrange(h)][rnd.randrange(w)], g[rnd.randrange(h)][rnd.randrange(w)])
    return (dsum/n)/(r/20000)
def hamming(a,b): return bin(int.from_bytes(a,'little')^int.from_bytes(b,'little')).count('1')
def score_h(g, nbytes=8, seed=1):
    h=len(g); w=len(g[0]); s=0; n=0
    for y in range(h):
        for x in range(w):
            a=g[y][x][:nbytes]
            if x+1<w: s+=hamming(a,g[y][x+1][:nbytes]); n+=1
            if y+1<h: s+=hamming(a,g[y+1][x][:nbytes]); n+=1
    rnd=random.Random(seed); r=0
    for _ in range(20000):
        r+=hamming(g[rnd.randrange(h)][rnd.randrange(w)][:nbytes], g[rnd.randrange(h)][rnd.randrange(w)][:nbytes])
    return (s/n)/(r/20000)
def parse_eq(eq, elog2, blkbits):
    """-> list over address bits [elog2, blkbits) of list of (axis, bit)."""
    out=[]
    for sym in eq[elog2:blkbits]:
        terms=[]
        for term in sym.split('^'):
            term=term.strip()
            if term=='0': continue
            if term[0] in 'XY': terms.append((term[0], int(term[1:])))
            else: raise ValueError(term)
        out.append(terms)
    return out
def block_dims(peq):
    """Tile dims from the non-XOR (first) term of each bit."""
    xb=sum(1 for t in peq if t and t[0][0]=='X'); yb=sum(1 for t in peq if t and t[0][0]=='Y')
    return 1<<xb, 1<<yb
def addr_of(peq, x, y, tw, th, tpr, blkbits, elog2):
    a=0
    for k,terms in enumerate(peq):
        v=0
        for ax,b in terms:
            v^=((x if ax=='X' else y)>>b)&1
        a|=v<<k
    tile=(y//th)*tpr + (x//tw)
    return (tile<<(blkbits-elog2)) + a
def grid_fwd(data, bw, bh, eb, peq, blkbits, elog2):
    tw,th=block_dims(peq); tpr=(bw+tw-1)//tw
    g=[[None]*bw for _ in range(bh)]
    for y in range(bh):
        for x in range(bw):
            e=addr_of(peq,x,y,tw,th,tpr,blkbits,elog2)
            g[y][x]=data[e*eb:(e+1)*eb]
    return g
def bc4_mean(b):
    a0,a1=b[0],b[1]
    pal=[a0,a1]
    if a0>a1: pal+=[((7-i)*a0+i*a1)/7 for i in range(1,7)]
    else: pal+=[((5-i)*a0+i*a1)/5 for i in range(1,5)]+[0,255]
    bits=int.from_bytes(b[2:8],'little')
    return sum(pal[(bits>>(3*k))&7] for k in range(16))/16
def val_bc5_mean(b): return (bc4_mean(b[:8]), bc4_mean(b[8:16]))
def to_vals(g, valf): return [[valf(c) for c in row] for row in g]
def nbr_scores(V, period_list=(4,16,64)):
    """mean |dv| for neighbor pairs: interior vs pairs crossing a boundary at multiples of p."""
    h=len(V); w=len(V[0])
    def d(a,b): return sum(abs(p-q) for p,q in zip(a,b))
    acc={'all':[0,0]}
    for p in period_list: acc['cross%d'%p]=[0,0]
    acc['inner4']=[0,0]
    for y in range(h):
        for x in range(w):
            for (nx,ny) in ((x+1,y),(x,y+1)):
                if nx>=w or ny>=h: continue
                dv=d(V[y][x],V[ny][nx])
                acc['all'][0]+=dv; acc['all'][1]+=1
                coord = x if nx!=x else y
                crossed=False
                for p in period_list:
                    if coord%p==p-1:
                        k='cross%d'%p; acc[k][0]+=dv; acc[k][1]+=1; crossed=True
                if coord%4!=3: acc['inner4'][0]+=dv; acc['inner4'][1]+=1
    return {k:(round(v[0]/v[1],3) if v[1] else None) for k,v in acc.items()}
def downsample(V):
    h=len(V)//2; w=len(V[0])//2
    return [[tuple((V[2*y][2*x][c]+V[2*y][2*x+1][c]+V[2*y+1][2*x][c]+V[2*y+1][2*x+1][c])/4 for c in range(len(V[0][0]))) for x in range(w)] for y in range(h)]
def corr(A,B):
    xs=[a for row in A for v in row for a in v]; ys=[b for row in B for v in row for b in v]
    n=len(xs); mx=sum(xs)/n; my=sum(ys)/n
    sxy=sum((a-mx)*(b-my) for a,b in zip(xs,ys)); sxx=sum((a-mx)**2 for a in xs); syy=sum((b-my)**2 for b in ys)
    return sxy/((sxx*syy)**0.5) if sxx and syy else 0.0
BC7M=[(3,4,0,0,4,0),(2,6,0,0,6,0),(3,6,0,0,5,0),(2,6,0,0,7,0),(1,0,2,1,5,6),(1,0,2,0,7,8),(1,0,0,0,7,7),(2,6,0,0,5,5)]
def val_bc7_mean(b):
    v=int.from_bytes(b,'little')
    mode=0
    while mode<8 and not (v>>mode)&1: mode+=1
    if mode==8: return (0,0,0)
    ns,pb,rb,isb,cb,ab=BC7M[mode]
    p=mode+1+pb+rb+isb
    ch=[]
    for c in range(3):
        s=0
        for k in range(2*ns):
            s+=((v>>p)&((1<<cb)-1))<<(8-cb); p+=cb
        ch.append(s/(2*ns))
    return tuple(ch)
