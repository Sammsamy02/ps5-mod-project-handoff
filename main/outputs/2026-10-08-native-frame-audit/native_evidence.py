#!/usr/bin/env python3
"""Bounded saved-image instruction census; no engine execution."""
from pathlib import Path
import json,hashlib,struct,capstone
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what');OUT=ROOT/'outputs/2026-10-08-native-frame-audit';IMG=ROOT/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
b=IMG.read_bytes();sha=hashlib.sha256(b).hexdigest();assert sha=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
phoff=struct.unpack_from('<Q',b,32)[0];pe,pn=struct.unpack_from('<HH',b,54);segs=[]
for i in range(pn):
 t,f,o,v,p,fs,ms,a=struct.unpack_from('<IIQQQQQQ',b,phoff+i*pe)
 if t==1:segs.append(dict(flags=f,file_offset=o,image_vaddr=v,file_size=fs,memory_size=ms))
def fo(a,n=1):
 s=next(s for s in segs if s['image_vaddr']<=a and a+n<=s['image_vaddr']+s['file_size']);return s['file_offset']+a-s['image_vaddr']
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
E={};ranges=[('UFunctionInvoke',0x326ac0,0x82),('FrameStep',0x4577a0,0x2a),('FrameStepExplicit',0x4577d0,0x5e),('OutputDeviceCtor',0x1d4680,0x16),('NativeProcessEvent',0x459830,0x510),('OutputDeviceDtorCandidate',0xb290,0x6),('NativeFrameDtor',0x459d40,0x5c),('NativePropertyOpcodeCandidate',0x459da0,0x70),('NativeFrameSerialize',0x457980,0x1a4),('NativeFrameStackWalk',0x457c10,0x250)]
for name,a,n in ranges:
 ins=list(md.disasm(b[fo(a,n):fo(a,n)+n],a)); rows=[]
 for x in ins:
  ev=dict(image_vaddr=hex(x.address),file_offset=hex(fo(x.address,x.size)),bytes=x.bytes.hex(),instruction=x.mnemonic+' '+x.op_str)
  rip=[]
  for op in x.operands:
   if op.type==capstone.x86.X86_OP_MEM and op.mem.base==capstone.x86.X86_REG_RIP:rip.append(hex(x.address+x.size+op.mem.disp))
  if rip:ev['rip_targets']=rip
  E[hex(x.address)]=ev;rows.append(ev)
 (OUT/(name+'.asm')).write_text('\n'.join(f"{r['image_vaddr']} file{r['file_offset']} {r['bytes']} {r['instruction']}"+(' rip->'+','.join(r['rip_targets']) if r.get('rip_targets') else '') for r in rows)+'\n')
# Native frame vptrs from ProcessEvent and its directly called base constructor.
# Exact RELA records for only these vtables; addends establish native virtual-body topology, not class sizes.
dyn={}
for i in range(pn):
 t,f,o,v,p,fs,ms,a=struct.unpack_from('<IIQQQQQQ',b,phoff+i*pe)
 if t==2:
  for off in range(o,o+fs,16):
   tag,val=struct.unpack_from('<QQ',b,off)
   if tag==0:break
   dyn[tag]=val
rv,rs,re=dyn[7],dyn[8],dyn[9];assert re==24;rf=fo(rv,rs);rel=[]
for off in range(rf,rf+rs,re):
 target,info,add=struct.unpack_from('<QQq',b,off)
 if any(base<=target<base+0x80 for base in [0x7ec8c80,0x7efef70]):rel.append(dict(file_offset=hex(off),target=hex(target),info=hex(info),addend=hex(add),bytes=b[off:off+24].hex()))
result=dict(image=dict(path=str(IMG),sha256=sha,bytes=len(b),segments=segs),scope='saved-image-only; no runtime base/address/table readiness claim',instructions=E,relocations=rel,gnatives=dict(candidate_image_vaddr='0x93b8900',anchor='0x4577ab',domain='unsigned8 opcode:0..255',entry_access_bytes=8,entry_count='not independently established;256 reachable byte indices',runtime_entry_population='not validated'))
(OUT/'NATIVE-EVIDENCE.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'image':sha,'instruction_windows':len(E),'rela_records':len(rel),'gnatives':'0x93b8900'}))
