import sys,os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import struct
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)),'sec2.py')).read().split('found=[]')[0])
o=43645
na=i32(o); o+=4; act=struct.unpack_from('<%dH'%na,d,o); o+=2*na
print('active bones',na)
print('next', o, d[o:o+80].hex())
bs=u32(o); o+=4
strip=d[o:o+2]; o+=2
dts=d[o]; o+=1
es=i32(o); n=i32(o+4); o+=8
print('bufsize',bs,'strip',strip.hex(),'dts',dts,es,n)
idx=struct.unpack_from('<%dI'%n,d,o); o+=4*n
print('pos hdr', d[o:o+24].hex())
stride=u32(o); nv=u32(o+4); o+=8
es2=i32(o); n2=i32(o+4); o+=8
print('stride',stride,'nv',nv,es2,n2)
pos=[struct.unpack_from('<3f',d,o+12*i) for i in range(n2)]
o+=12*n2
print('after pos', d[o:o+40].hex())
import math
for t in range(8):
    tri=idx[3*t:3*t+3]
    P=[pos[i] for i in tri]
    ax=[P[1][j]-P[0][j] for j in range(3)]; bx=[P[2][j]-P[0][j] for j in range(3)]
    cr=[ax[1]*bx[2]-ax[2]*bx[1], ax[2]*bx[0]-ax[0]*bx[2], ax[0]*bx[1]-ax[1]*bx[0]]
    area=0.5*math.sqrt(sum(c*c for c in cr))
    print('slot tri',t,tri,[tuple(round(c,3) for c in p) for p in P],'area cm2',area)
# overall bbox
xs=[p[0] for p in pos]; ys=[p[1] for p in pos]; zs=[p[2] for p in pos]
print('bbox', min(xs),max(xs),min(ys),max(ys),min(zs),max(zs))
