#!/usr/bin/env python3
"""Fresh Mac-only saved-image verification; no runtime or shipping ABI claims."""
import sys
sys.dont_write_bytecode=True
import capstone,hashlib,json,mmap,struct
from pathlib import Path
H=Path(__file__).resolve().parent;r=json.loads((H/'receipt.json').read_text());P=H.parents[3]/r['image_path']
checks={0x1d9ee:('lea','r15, [rax + 8]'),0x1db5d:('mov','qword ptr [rbx + 8], rcx'),0x1db74:('mov','qword ptr [rip + 0x92ee9b5], r15'),0x1dc34:('mov','rax, qword ptr [rip + 0x92ee8f5]'),0x1c6a6:('add','rsi, -8'),0x1c6ad:('call','0x1c470'),0x1c65e:('call','0x5b80'),0x5bb6:('mov','qword ptr [rbx], rax'),0x1c4e0:('call','qword ptr [rax + 0x88]'),0x1c52b:('call','qword ptr [rax + 0x100]'),0x25457:('call','qword ptr [rax + 0xf8]'),0x89fa:('mov','qword ptr [rax], rcx'),0x2a282:('call','0x6ecca60'),0x2a28f:('add','qword ptr [r15 + 0x18], rbx'),0x2a2cf:('call','0x7471e50')}
needed={0x7ebcb80:0x1c6a0,0x7ebc728:0x7810,0x7ebc7a0:0x25450,0x7ebc798:0x8970,0x7ebd000:0x2a180}
with P.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as b:
 assert hashlib.sha256(b).hexdigest()==r['image_sha256']
 phoff=struct.unpack_from('<Q',b,32)[0];stride,count=struct.unpack_from('<HH',b,54);ps=[struct.unpack_from('<IIQQQQQQ',b,phoff+i*stride) for i in range(count)]
 def off(v,n):
  for p in ps:
   if p[0]==1 and p[3]<=v and v+n<=p[3]+p[5]:return p[2]+v-p[3]
  raise ValueError(hex(v))
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);found={}
 for w in r['windows']:
  v=int(w['start'],16);n=w['length'];o=off(v,n);raw=b[o:o+n]
  assert hashlib.sha256(raw).hexdigest()==w['sha256'];lines=(H/w['file']).read_text().splitlines();ins=list(md.disasm(raw,v));assert len(ins)==w['instructions']==len(lines)
  for i in ins:
   if i.address in checks:
    assert (i.mnemonic,i.op_str)==checks[i.address],(hex(i.address),i.mnemonic,i.op_str)
    found[hex(i.address)]={'bytes':i.bytes.hex(),'instruction':i.mnemonic+' '+i.op_str,'window':w['file'],'line':next(j for j,s in enumerate(lines,1) if s.startswith(f'{i.address:#010x} '))}
 assert len(found)==len(checks)
 dyn=next(p for p in ps if p[0]==2);tags={}
 for o in range(dyn[2],dyn[2]+dyn[5],16):
  t,v=struct.unpack_from('<qQ',b,o)
  if t==0:break
  tags[t]=v
 assert tags[9]==struct.calcsize('<QQq');relocs={}
 for o in range(off(tags[7],tags[8]),off(tags[7],tags[8])+tags[8],tags[9]):
  dst,info,add=struct.unpack_from('<QQq',b,o)
  if dst in needed:
   assert info&0xffffffff==8 and add==needed[dst];relocs[hex(dst)]={'type':8,'target':hex(add)}
 assert len(relocs)==len(needed)
 # Check each RIP-LEA value actually feeding the three vptr writes.
 for va,target in ((0x1da2d,0x7ebcb68),(0x5bac,0x7ebc6a0),(0x89ef,0x7ebcfd8)):
  o=off(va,7);assert va+7+struct.unpack_from('<i',b,o+3)[0]==target
 out={'label':'Fresh Mac-only verification; concrete image routes only','image_sha256':r['image_sha256'],'windows':len(r['windows']),'critical_instructions':found,'dynamic_rela':{hex(t):hex(tags[t]) for t in (7,8,9)},'critical_relocations':relocs,'vptr_leas_verified':3}
 (H/'verification.json').write_text(json.dumps(out,indent=2)+'\n');print('PASS:',len(r['windows']),'window hashes/decodes,',len(found),'critical instructions,',len(relocs),'relative relocations,3 vptr LEAs')
