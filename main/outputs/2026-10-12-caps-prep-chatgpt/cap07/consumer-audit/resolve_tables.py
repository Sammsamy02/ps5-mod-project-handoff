#!/usr/bin/env python3
import sys
sys.dont_write_bytecode=True
import hashlib,json,mmap,struct
from pathlib import Path
H=Path(__file__).resolve().parent;R=H.parents[3];P=R/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin';SHA='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
with P.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as b:
 assert hashlib.sha256(b).hexdigest()==SHA
 phoff=struct.unpack_from('<Q',b,32)[0];stride,count=struct.unpack_from('<HH',b,54);segs=[struct.unpack_from('<IIQQQQQQ',b,phoff+i*stride) for i in range(count)]
 def off(v,n):
  for p in segs:
   if p[0]==1 and p[3]<=v and v+n<=p[3]+p[5]:return p[2]+v-p[3]
  raise ValueError(hex(v))
 dyn=next(p for p in segs if p[0]==2);tags={}
 for o in range(dyn[2],dyn[2]+dyn[5],16):
  t,v=struct.unpack_from('<qQ',b,o)
  if t==0:break
  tags[t]=v
 assert tags[9]==struct.calcsize('<QQq');rows=[]
 for o in range(off(tags[7],tags[8]),off(tags[7],tags[8])+tags[8],tags[9]):
  dst,info,add=struct.unpack_from('<QQq',b,o)
  for ap,lo,hi in ((0x7ebccf0,0x7ebccf0,0x7ebccf0+0x30),(0x7ebc630,0x7ebc630,0x7ebc630+0x40)):
   if lo<=dst<hi:rows.append({'address_point':hex(ap),'vaddr':hex(dst),'relative_to_address_point':hex(dst-ap),'type':info&0xffffffff,'target':hex(add)})
 r={'label':'Mac-only actual PT_DYNAMIC relocation resolution; bounded table, not full table-size claim','image_sha256':SHA,'address_points':['0x7ebccf0','0x7ebc630'],'dynamic_rela':{hex(t):hex(tags[t]) for t in (7,8,9)},'rows':rows};(H/'pool-vtables.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(rows,indent=2))
