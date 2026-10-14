import sys,os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sys, struct
from pk import pkg
n=sys.argv[1]
z=pkg(n)
e=[e for e in z.exports if e['outer'][0]=='Null'][0]
d=z.export_bytes(e)
names=z.names
nimp=len(z.imports)
hits=[]
for off in range(0, min(len(d)-8, 400000)):
    cnt=struct.unpack_from('<i',d,off)[0]
    if 1<=cnt<=64:
        pi=struct.unpack_from('<i',d,off+4)[0]
        if -nimp<=pi<=-1:
            ni,nn=struct.unpack_from('<II',d,off+8)
            if ni<len(names) and nn<4:
                hits.append((off,cnt,pi,names[ni]))
print(len(d), hits[:20])
