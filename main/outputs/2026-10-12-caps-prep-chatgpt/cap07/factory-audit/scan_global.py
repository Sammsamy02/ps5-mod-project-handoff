#!/usr/bin/env python3
import sys
sys.dont_write_bytecode=True
import json,mmap,struct,hashlib
from pathlib import Path
H=Path(__file__).resolve().parent;R=H.parents[3];P=R/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin';SHA='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
with P.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as b:
 assert hashlib.sha256(b).hexdigest()==SHA
 phoff=struct.unpack_from('<Q',b,32)[0];stride,count=struct.unpack_from('<HH',b,54);segs=[struct.unpack_from('<IIQQQQQQ',b,phoff+i*stride) for i in range(count)];hits=[]
 for p in segs:
  if p[0]!=1 or not p[1]&1:continue
  start,end=p[2],p[2]+p[5]
  for op in (0x89,0x8b,0x8d,0xc7):
   for rex in range(0x48,0x50):
    prefix=bytes((rex,op));i=start
    while True:
     i=b.find(prefix,i,end)
     if i<0:break
     if i+7<=end and b[i+2]&0xc7==5:
      va=p[3]+i-p[2];target=va+(11 if op==0xc7 else 7)+struct.unpack_from('<i',b,i+3)[0]
      if target==0x930c530:hits.append({'site':hex(va),'opcode':hex(op),'bytes':b[i:i+11 if op==0xc7 else i+7].hex(),'candidate_only':True})
     i+=1
 r={'label':'Mac-only raw RIP-reference candidates; decode enclosing entry before claim','image_sha256':SHA,'image_path':str(P.relative_to(R)),'target':'0x930c530','hits':sorted(hits,key=lambda x:int(x['site'],16))};(H/'global-references.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r['hits'],indent=2))
