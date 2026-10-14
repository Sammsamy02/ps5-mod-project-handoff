from pathlib import Path
import struct,hashlib,re
from capstone import Cs,CS_ARCH_X86,CS_MODE_64
ROOT=Path(__file__).resolve().parents[4]
IMAGE=ROOT/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
B=IMAGE.read_bytes();SHA=hashlib.sha256(B).hexdigest()
assert SHA=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
po=struct.unpack_from('<Q',B,32)[0];pe,pn=struct.unpack_from('<HH',B,54)
P=[struct.unpack_from('<IIQQQQQQ',B,po+i*pe) for i in range(pn)]
def off(a,n=1):
 hits=[p[2]+a-p[3] for p in P if p[0]==1 and p[3]<=a and a+n<=p[3]+p[5]]
 assert len(hits)==1,(hex(a),n,hits)
 return hits[0]
def read(a,n):return B[off(a,n):off(a,n)+n]
def va(o):return next(p[3]+o-p[2] for p in P if p[0]==1 and p[2]<=o<p[2]+p[5])
M=Cs(CS_ARCH_X86,CS_MODE_64);M.detail=True
def dis(a,n):return list(M.disasm(read(a,n),a))
def dump(a,n):return '\n'.join(f'{i.address:#x} {i.bytes.hex()} {i.mnemonic} {i.op_str}' for i in dis(a,n))
def refs(targets):
 out=[]
 for p in P:
  if p[0]!=1 or not p[1]&1:continue
  b=B[p[2]:p[2]+p[5]]
  for m in re.finditer(rb'[\x48-\x4f][\x8b\x89\x8d][\x05\x0d\x15\x1d\x25\x2d\x35\x3d]',b):
   a=p[3]+m.start();t=a+7+struct.unpack_from('<i',b,m.start()+3)[0]
   if t in targets:out.append((a,t))
 return out
D={}
for p in P:
 if p[0]==2:
  for o in range(p[2],p[2]+p[5],16):
   t,v=struct.unpack_from('<QQ',B,o)
   if not t:break
   D[t]=v
R={}
for o in range(off(D[7]),off(D[7])+D[8],D[9]):
 a,i,t=struct.unpack_from('<QQq',B,o)
 if i==8:R[a]=(t,o)
