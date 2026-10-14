#!/usr/bin/env python3
"""Fresh Mac-only saved-image verification; no runtime or shipping ABI claims."""
import sys
sys.dont_write_bytecode=True
import capstone,hashlib,json,mmap,struct
from pathlib import Path
H=Path(__file__).resolve().parent;r=json.loads((H/'receipt.json').read_text());P=H.parents[3]/r['image_path']
checks={0x579d:('mov','qword ptr [rdi + 0x38], rcx'),0x577f:('call','0x7472050'),0x3e163:('mov','qword ptr [rip + 0x92ca6ae], rax'),0x8a88:('call','qword ptr [rax + 0x10]'),0x59dd:('call','0x5240'),0x5284:('mov','qword ptr [rdi], rcx'),0x5559:('call','0x7472030'),0x5574:('call','0x7472040'),0x5582:('cmp','rax, r14'),0x20768:('call','0x20010'),0x20418:('mov','rdx, qword ptr [rsi]'),0x2041f:('mov','qword ptr [r14 + 0x68], rdx'),0x1cdf3:('mov','r15, qword ptr [r14 + 0x68]'),0x1ce06:('mov','rdi, qword ptr [r15]'),0x1ce0c:('call','qword ptr [rax + 0x18]'),0x1ce24:('call','qword ptr [rax + 0x28]'),0x1756f96:('call','qword ptr [rcx + 0x28]'),0x1756c17:('mov','qword ptr [rbp - 0x1d0], rax'),0x1756bf4:('mov','rax, qword ptr [rax]')}
needed={0x7ebcd00:0x58e0,0x7ebc658:0x5500,0x7ebc648:0x5770,0x7ebcb90:0x20760}
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
 # Check each RIP-LEA value actually feeding the two vptr writes.
 for va,target in ((0x3e12f,0x7ebccf0),(0x525e,0x7ebc630)):
  o=off(va,7);assert va+7+struct.unpack_from('<i',b,o+3)[0]==target
 out={'label':'Fresh Mac-only verification; concrete image routes only','image_sha256':r['image_sha256'],'windows':len(r['windows']),'critical_instructions':found,'dynamic_rela':{hex(t):hex(tags[t]) for t in (7,8,9)},'critical_relocations':relocs,'vptr_leas_verified':2}
 (H/'verification.json').write_text(json.dumps(out,indent=2)+'\n');print('PASS:',len(r['windows']),'window hashes/decodes,',len(found),'critical instructions,',len(relocs),'relative relocations,2 vptr LEAs')
