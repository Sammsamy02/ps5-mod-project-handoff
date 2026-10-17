#!/usr/bin/env python3
"""Recheck saved-image provenance and critical route instructions; no execution proof."""
import sys
sys.dont_write_bytecode=True
import capstone,hashlib,json,mmap,struct
from pathlib import Path
H=Path(__file__).resolve().parent
r=json.loads((H/'receipt.json').read_text()); image=H.parents[3]/r['image_path']
checks={0x16fc6e4:('call','qword ptr [r8 + 0x130]'),0x171ba72:('call','qword ptr [rax + 0x88]'),0x176f3e9:('call','qword ptr [r10 + 0x70]'),0x175654d:('call','0x1755ab0'),0x1755b44:('call','0x1754560'),0x17547b4:('call','0x139d80'),0x139dc3:('call','0x139890'),0x1398f1:('call','qword ptr [r8 + 0xf0]'),0x84ce2:('call','qword ptr [r8 + 0xf8]'),0x1770ca3:('call','qword ptr [rax + 0xf8]'),0x1754df0:('call','qword ptr [r9 + 0x18]')}
with image.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as b:
 assert hashlib.sha256(b).hexdigest()==r['image_sha256']
 phoff=struct.unpack_from('<Q',b,32)[0];stride,count=struct.unpack_from('<HH',b,54)
 phs=[struct.unpack_from('<IIQQQQQQ',b,phoff+i*stride) for i in range(count)]
 def off(v,n):
  for p in phs:
   if p[0]==1 and p[3]<=v and v+n<=p[3]+p[5]:return p[2]+v-p[3]
  raise ValueError(hex(v))
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);found={}
 for w in r['windows']:
  v=int(w['start'],16);n=w['length'];o=off(v,n);raw=b[o:o+n]
  assert hashlib.sha256(raw).hexdigest()==w['sha256']
  lines=(H/w['file']).read_text().splitlines();decoded=list(md.disasm(raw,v)); assert len(decoded)==w['instructions']==len(lines)
  for i in decoded:
   if i.address in checks:
    assert (i.mnemonic,i.op_str)==checks[i.address],(hex(i.address),i.mnemonic,i.op_str)
    found[hex(i.address)]={'bytes':i.bytes.hex(),'instruction':i.mnemonic+' '+i.op_str,'window':w['file'],'line':next(j for j,s in enumerate(lines,1) if s.startswith(f'{i.address:#010x} '))}
 assert len(found)==len(checks),(len(found),len(checks))
 dyn=next(p for p in phs if p[0]==2);tags={}
 for o in range(dyn[2],dyn[2]+dyn[5],16):
  tag,val=struct.unpack_from('<qQ',b,o)
  if tag==0:break
  tags[tag]=val
 assert tags[9]==struct.calcsize('<QQq')
 needed={0x7fee9b0:0x84cb0,0x7fee9b8:0x1770bb0,0x7fee6f0:0x1756540,0x7fe72a0:0x171b9d0};relocs={}
 for o in range(off(tags[7],tags[8]),off(tags[7],tags[8])+tags[8],tags[9]):
  dst,info,add=struct.unpack_from('<QQq',b,o)
  if dst in needed:
   assert info&0xffffffff==8 and add==needed[dst]
   relocs[hex(dst)]={'type':8,'target':hex(add)}
 assert len(relocs)==len(needed)
 strings={}
 for v in (0x7974c24,0x78d1148,0x78d11c4):
  o=off(v,64);raw=b[o:o+64];end=next(i for i in range(0,64,2) if raw[i:i+2]==b'\0\0');strings[hex(v)]=raw[:end].decode('utf-16-le')
 out={'label':'Fresh Mac-only static evidence verification; no execution or ABI-name proof','image_sha256':r['image_sha256'],'window_count':len(r['windows']),'critical_instruction_count':len(found),'instructions':found,'dynamic_rela':{hex(t):hex(tags[t]) for t in (7,8,9)},'relative_relocations':relocs,'utf16_strings':strings}
 (H/'verification.json').write_text(json.dumps(out,indent=2)+'\n')
 print(json.dumps({'windows':len(r['windows']),'critical_instructions':len(found),'relocations':len(relocs),'strings':strings}))
