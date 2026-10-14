import sys,os,struct,math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pk import pkg, bypath
def run(n):
    z=pkg(n)
    tops=[e for e in z.exports if e['outer'][0]=='Null']
    e=tops[0] if len(tops)==1 else max(z.exports,key=lambda e:e["serial_size"])
    d=z.export_bytes(e); L=len(d)
    u32=lambda o:struct.unpack_from('<I',d,o)[0]; i32=lambda o:struct.unpack_from('<i',d,o)[0]
    nimp=len(z.imports)
    # materials
    mats=None
    for o in range(0,min(L-8,20000)):
        c=i32(o)
        if not 1<=c<=128: continue
        ok=True; ents=[]
        for k in range(c):
            p=o+4+44*k
            if p+44>L: ok=False;break
            pi=i32(p); ni=u32(p+4); nn=u32(p+8); b=u32(p+12)
            if not(-nimp<=pi<=-1 and ni<len(z.names) and b in (0,) and u32(p+16) in (0,1) and u32(p+20) in (0,1) and u32(p+40)==0): ok=False;break
            imp=z.imports[-pi-1]
            if imp[0]!='PackageImport': ok=False;break
            ents.append((z.imported_packages[imp[1][0]].split('/')[-1], z.names[ni]))
        if ok:
            mats=(o,ents); break
    nmat=len(mats[1]) if mats else 0
    def parse(o):
        s={}
        if o+40>L: return None
        s['mat']=d[o+2]|(d[o+3]<<8); o+=4
        s['base']=u32(o); s['ntri']=u32(o+4); o+=8
        if u32(o) not in (0,1): return None
        o+=4; o+=1
        if u32(o) not in (0,1) or u32(o+4) not in (0,1): return None
        o+=8
        s['basev']=u32(o); o+=4
        c=i32(o); o+=4
        if not (0<=c<16): return None
        for _ in range(c):
            k=i32(o); o+=4
            if k!=0: return None
        bm=i32(o); o+=4
        if not (0<bm<2000) or o+2*bm>L: return None
        s['bonemap']=struct.unpack_from('<%dH'%bm,d,o); o+=2*bm
        s['nverts']=u32(o); o+=4
        s['maxinf']=i32(o); o+=4
        o+=2+16+4
        s['disabled']=u32(o); o+=4
        s['end']=o
        return s
    tables=[]
    o=0
    while o<L-60:
        c=i32(o)
        if 1<=c<=64 and (d[o+6]|(d[o+7]<<8))<max(nmat,1) and u32(o+8)==0 and u32(o+4+20+5)==0:
            p=o+4; secs=[]
            for k in range(c):
                t=parse(p)
                if not t or t['mat']>=nmat: secs=None;break
                p=t['end']; secs.append(t)
            if secs and secs[0]['basev']==0 and all(1<=t['maxinf']<=16 for t in secs) and all(secs[k+1]['base']==secs[k]['base']+3*secs[k]['ntri'] for k in range(len(secs)-1)):
                tables.append((o,secs)); o=p; continue
        o+=1
    print('==',n.split('/')[-1],'export',e['name'],e['serial_size'],'mats',nmat,'tables',len(tables))
    for o,secs in tables:
        print('  LOD@',o,[(mats[1][t['mat']][0],t['ntri']) for t in secs])
for n in sorted(bypath):
    if '/SK' in n or 'FaceMesh' in n:
        run(n)
