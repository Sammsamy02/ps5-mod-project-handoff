import sys,os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import struct
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)),'sec2.py')).read().split('found=[]')[0])
o=249
n=i32(o); o+=4
for k in range(n):
    pi=i32(o); ni,nn=struct.unpack_from('<II',d,o+4); b=u32(o+12); ui=u32(o+16); uo=u32(o+20); dens=struct.unpack_from('<4f',d,o+24); tail=d[o+40:o+44].hex()
    imp=z.imports[-pi-1]
    pk_=z.imported_packages[imp[1][0]] if imp[0]=='PackageImport' else imp
    print(k, pk_.split('/')[-1], z.names[ni], b, ui, uo, dens, tail)
    o+=44
print('end',o)
nb=i32(o); print('bones',nb)
bones=[]
for k in range(nb):
    ni,nn,par=struct.unpack_from('<IIi',d,o+4+12*k); bones.append((z.names[ni]+('_%d'%(nn-1) if nn else ''),par))
print(bones[:5], bones[-3:])
o2=o+4+12*nb
print('after boneinfo', o2, d[o2:o2+16].hex(), i32(o2))
