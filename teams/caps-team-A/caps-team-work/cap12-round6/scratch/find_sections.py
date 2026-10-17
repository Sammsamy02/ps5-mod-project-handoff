import sys,os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import struct
from pk import pkg
n=sys.argv[1]; nmat=int(sys.argv[2])
z=pkg(n)
e=[e for e in z.exports if e['outer'][0]=='Null'][0]
d=z.export_bytes(e)
L=len(d)
# candidate section starts: u16 mat < nmat, u32 base (base%3==0), u32 ntri>0
cands={}
for off in range(0, L-10):
    m=d[off]|(d[off+1]<<8)
    if m>=nmat: continue
    b,t=struct.unpack_from('<II',d,off+2)
    if b==0 and 0<t<2_000_000:
        cands[off]=(m,b,t)
# for each base==0 candidate, try strides 30..400 to find chain
out=[]
for off,(m,b,t) in cands.items():
    for stride in range(30,600):
        o2=off+stride
        if o2+10>L: break
        m2=d[o2]|(d[o2+1]<<8)
        b2,t2=struct.unpack_from('<II',d,o2+2)
        if m2<nmat and b2==3*t and 0<t2<2_000_000:
            out.append((off,stride,m,t,m2,t2))
print(len(cands), out[:40])
