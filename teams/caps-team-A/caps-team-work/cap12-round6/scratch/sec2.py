import sys,os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import struct
from pk import pkg
n=sys.argv[1]; nmat=int(sys.argv[2])
z=pkg(n)
e=[e for e in z.exports if e['outer'][0]=='Null'][0]
d=z.export_bytes(e)
L=len(d)
def u32(o): return struct.unpack_from('<I',d,o)[0]
def i32(o): return struct.unpack_from('<i',d,o)[0]
def parse(o):
    s={}
    s['strip']=d[o:o+2].hex(); o+=2
    s['mat']=d[o]|(d[o+1]<<8); o+=2
    s['base']=u32(o); s['ntri']=u32(o+4); o+=8
    s['recomp']=u32(o); o+=4
    s['mask']=d[o]; o+=1
    s['cast']=u32(o); o+=4
    s['vrt']=u32(o); o+=4
    s['basev']=u32(o); o+=4
    c=i32(o); o+=4
    if not (0<=c<16): return None
    s['cloth_lods']=c
    for _ in range(c):
        k=i32(o); o+=4
        if k!=0: return None
    bm=i32(o); o+=4
    if not (0<bm<2000): return None
    s['bonemap']=list(struct.unpack_from('<%dH'%bm,d,o)); o+=2*bm
    s['nverts']=u32(o); o+=4
    s['maxinf']=i32(o); o+=4
    s['clothidx']=struct.unpack_from('<h',d,o)[0]; o+=2
    s['guid']=d[o:o+16].hex(); o+=16
    s['clothlod']=i32(o); o+=4
    s['end']=o
    return s
found=[]
for o in range(0,L-80):
    if d[o+2]|(d[o+3]<<8) >= nmat: continue
    if u32(o+4)!=0: continue
    t=u32(o+8)
    if not 0<t<5_000_000: continue
    if u32(o+12) not in (0,1): continue
    if d[o+16]>8: continue
    if u32(o+17) not in (0,1) or u32(o+21) not in (0,1): continue
    s=parse(o)
    if s and 1<=s['maxinf']<=16 and s['basev']==0:
        found.append((o,s))
for o,s in found[:10]:
    print(o, {k:(v if k!='bonemap' else (len(v),v[:8])) for k,v in s.items()})
    print('  after:', d[s['end']:s['end']+48].hex())
