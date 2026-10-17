#!/usr/bin/env python3
"""Research helper: hash-gated mmap, PT_LOAD mapping, bounded disassembly.
Creates evidence only beside this script. Xrefs are byte-search candidates,
not instruction-boundary or class-identity proof. No RELA or SDK assumptions.
"""
import sys
sys.dont_write_bytecode=True
import argparse,capstone,hashlib,json,mmap,struct
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
IMAGE=ROOT/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
SHA='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('windows',nargs='*',help='image_vaddr:length in hex');p.add_argument('--xref',action='append',default=[]);a=p.parse_args()
with IMAGE.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as b:
 assert hashlib.sha256(b).hexdigest()==SHA
 assert b[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<H',b,18)[0]==62
 phoff=struct.unpack_from('<Q',b,32)[0];stride,count=struct.unpack_from('<HH',b,54);segs=[]
 for i in range(count):
  ph=struct.unpack_from('<IIQQQQQQ',b,phoff+i*stride)
  if ph[0]==1:segs.append(dict(flags=ph[1],offset=ph[2],vaddr=ph[3],filesz=ph[5]))
 def off(v,n):
  for ph in segs:
   if ph['vaddr']<=v and v+n<=ph['vaddr']+ph['filesz']:return ph['offset']+v-ph['vaddr']
  raise ValueError('Not wholly file-backed: '+hex(v))
 receipt=HERE/'receipt.json';r=json.loads(receipt.read_text()) if receipt.exists() else {}
 r.update(label='Mac-only bounded factory-write provenance audit',image_path=str(IMAGE.relative_to(ROOT)),image_sha256=SHA,image_bytes=len(b),capstone=capstone.__version__,segments=segs)
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
 for arg in a.windows:
  va,n=[int(x,16) for x in arg.split(':')];assert 0<n<=0x10000
  o=off(va,n);raw=b[o:o+n];lines=[]
  for ins in md.disasm(raw,va):
   notes=[]
   for op in ins.operands:
    if op.type==capstone.x86.X86_OP_MEM and op.mem.base==capstone.x86.X86_REG_RIP:notes.append('rip_target='+hex(ins.address+ins.size+op.mem.disp))
   lines.append(f'{ins.address:#010x} {ins.bytes.hex():<30} {ins.mnemonic:8} {ins.op_str}'+(' ; '+', '.join(notes) if notes else ''))
  name=f'window-{va:x}.txt';(HERE/name).write_text('\n'.join(lines)+'\n');r['windows']=[w for w in r.get('windows',[]) if w['start']!=hex(va)]+[dict(start=hex(va),length=n,file_offset=hex(o),sha256=hashlib.sha256(raw).hexdigest(),instructions=len(lines),file=name)]
  print(name,hex(n),len(lines),'instructions')
 if a.xref:
  lo,hi=0x1700000,0x1790000;raw=b[off(lo,hi-lo):off(lo,hi-lo)+hi-lo];targets={int(x,16) for x in a.xref};hits=[]
  for i in range(len(raw)-7):
   if raw[i]==0xe8:
    dst=lo+i+5+struct.unpack_from('<i',raw,i+1)[0]
    if dst in targets:hits.append(dict(kind='direct-call-candidate',site=hex(lo+i),target=hex(dst)))
   if raw[i] in (0x48,0x4c) and raw[i+1]==0x8d and raw[i+2]&0xc7==5:
    dst=lo+i+7+struct.unpack_from('<i',raw,i+3)[0]
    if dst in targets:hits.append(dict(kind='rip-lea-candidate',site=hex(lo+i),target=hex(dst)))
  r['xref_candidate_search']={'start':hex(lo),'end_exclusive':hex(hi),'targets':[hex(t) for t in sorted(targets)],'hits':hits};print(json.dumps(r['xref_candidate_search'],indent=2))
 receipt.write_text(json.dumps(r,indent=2)+'\n')
