import sys,os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import struct
from pk import pkg
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)),'sec2.py')).read().split('found=[]')[0].replace("n=sys.argv[1]; nmat=int(sys.argv[2])","n=sys.argv[1]; nmat=int(sys.argv[2])"))
o=int(sys.argv[3])
cnt=i32(o); o+=4
print('sections',cnt)
secs=[]
for k in range(cnt):
    s=parse(o)
    o=s['end']
    s['disabled']=u32(o); o+=4
    secs.append(s)
    print(k, s['mat'], 'base',s['base'],'ntri',s['ntri'],'basev',s['basev'],'nv',s['nverts'],'bones',len(s['bonemap']),'cast',s['cast'],'dis',s['disabled'], 'strip', s['strip'])
print('after sections', o, d[o:o+64].hex())
