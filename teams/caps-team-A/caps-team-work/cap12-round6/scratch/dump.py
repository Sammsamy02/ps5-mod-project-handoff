import sys,os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import struct
from pk import pkg
n=sys.argv[1]; a=int(sys.argv[2]); b=int(sys.argv[3])
z=pkg(n)
e=[e for e in z.exports if e['outer'][0]=='Null'][0]
d=z.export_bytes(e)
for o in range(a,b,4):
    v=struct.unpack_from('<i',d,o)[0]; f=struct.unpack_from('<f',d,o)[0]
    nm=z.names[v] if 0<=v<len(z.names) else ''
    print(o, d[o:o+4].hex(), v, '%.4g'%f, nm)
