#!/usr/bin/env python3
"""Fresh Mac-only saved-image verification; no runtime or shipping ABI claims."""
import sys
sys.dont_write_bytecode=True
import capstone,hashlib,json,mmap,struct
from pathlib import Path
H=Path(__file__).resolve().parent;r=json.loads((H/'receipt.json').read_text());P=H.parents[3]/r['image_path']
checks={
0x1754873:('mov','r12, rax'),0x17548ce:('vmovups','ymmword ptr [r12 + 0x1d0], ymm1'),0x1754c50:('lea','rcx, [r12 + 0x1d0]'),0x1754c6c:('call','0x25e20'),0x25e96:('call','0x62fc0'),0x62ffe:('mov','qword ptr [rbx], rax'),0x1754cce:('mov','rbx, qword ptr [r12 + 0x1d0]'),0x1754cd6:('shl','r14, 4'),0x1754ef9:('lea','r13, [rbx + r14]'),0x1754f36:('mov','rax, qword ptr [rbp - 0x50]'),0x1754f3a:('mov','qword ptr [r13 + 0x18], rax'),0x1756ab9:('mov','rbx, qword ptr [r8 + 0x1d0]'),
0x1f89a:('lea','rbx, [rax + rcx + 8]'),0x1f970:('mov','byte ptr [rcx], 1'),0x1fa99:('mov','rax, r14'),0x203bd:('call','0x1f6c0'),0x203c2:('mov','r14, rax'),0x20418:('mov','rdx, qword ptr [rsi]'),0x2041f:('mov','qword ptr [r14 + 0x68], rdx'),0x20423:('lea','rdx, [rbx + 0x50]'),0x20450:('cmove','rdx, rcx'),0x2045f:('mov','qword ptr [rdx], r14'),0x206cb:('lea','rsi, [rbp - 0x40]'),0x206cf:('lea','rdi, [rbx + 0x130]'),0x206d6:('call','0x1dc40'),
0x1dc51:('mov','r12, qword ptr [rsi]'),0x1dd0f:('cmp','byte ptr [r14 + 0x108], 1'),0x1ddfe:('mov','rcx, qword ptr [r14 + 8]'),0x1de02:('mov','qword ptr [rcx + rdx*8], r12'),0x1d250:('mov','rdi, qword ptr [rbx + 8]'),0x1d258:('mov','rax, qword ptr [rdi]'),0x1d269:('mov','r15, rax'),0x1d37a:('mov','rax, r15'),0x1d3c2:('mov','byte ptr [rax + 0xa5], 2'),0x1cba1:('lea','r13, [rbx + 0x130]'),0x1cbf2:('mov','r14, rax'),0x1cc4a:('mov','rax, qword ptr [rbx + 0x138]'),0x1cc51:('mov','rax, qword ptr [rax]'),0x1cdf3:('mov','r15, qword ptr [r14 + 0x68]'),0x1ce06:('mov','rdi, qword ptr [r15]'),0x1ce24:('call','qword ptr [rax + 0x28]')}
needed={0x7ebcb90:0x20760,0x7ebcba0:0x1d0e0}
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
 out={'label':'Fresh Mac-only verification; concrete image routes only','image_sha256':r['image_sha256'],'windows':len(r['windows']),'critical_instructions':found,'dynamic_rela':{hex(t):hex(tags[t]) for t in (7,8,9)},'critical_relocations':relocs}
 (H/'verification.json').write_text(json.dumps(out,indent=2)+'\n');print('PASS:',len(r['windows']),'window hashes/decodes,',len(found),'critical instructions,',len(relocs),'relative relocations')
