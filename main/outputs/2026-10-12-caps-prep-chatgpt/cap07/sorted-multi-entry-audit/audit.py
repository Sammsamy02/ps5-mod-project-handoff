"""Read-only bounded saved-image replay: two-task, sorted append/affinity/pop path."""
import sys
sys.dont_write_bytecode=True
import capstone,hashlib,json,mmap,struct
from pathlib import Path
H=Path(__file__).resolve().parent;R=H.parents[3];S=H.parent/'sorted-handoff-audit';expected=json.loads((S/'receipt.json').read_text());P=R/expected['image_path']
ctx=['CLAUDE.md','outputs/2026-10-12-caps-prep-chatgpt/cap07/sorted-handoff-audit/FINDING.md','outputs/2026-10-12-caps-prep-chatgpt/cap07/sorted-handoff-audit/receipt.json','outputs/2026-10-12-caps-prep-chatgpt/cap07/sorted-handoff-audit/critical.json','outputs/2026-10-12-caps-prep-chatgpt/cap07/branch-selector-audit/FINDING.md','outputs/2026-10-12-caps-prep-chatgpt/cap07/branch-selector-audit/verification.json','outputs/2026-10-12-caps-prep-chatgpt/cap07/handoff-audit/FINDING.md']
def hashes():return {p:hashlib.sha256((R/p).read_bytes()).hexdigest() for p in ctx}
before=hashes();pins={int(c['address'],16):c for c in json.loads((S/'critical.json').read_text())}
extra='1dd3e 1dd43 1dd48 1dd4b 1dd4e 1dd51 1dd60 1dd64 1dd67 1dd6d 1dd73 1dd77 1dd7b 1dd7f 1dd83 1dd85 1dd89 1dd8c 1dd90 1dd92 1dd94 1dd97 1dd9b 1dd9e 1dda1 1dc93 1dc96 1dca0 1dca4 1dcb2 1dccd 1e98e 1e992 1e995 1e998 1e9a2 1e9a5 1e9b4 1e9b8 1e9bb 1e9be 1e9e4 1e9ed 1ea10 1ea13 1ea17 1ea1a 1ea1c 1ea1f 1ea21 1ea24 1ea2a 1ea2e 1ea32 1ea3a 1ea3e 1ea44 1ea48 1ea56 1ea5c 1ea5e 1eaca 1eacc 1eadc 1eadf 1eae5 1eaec 1eaee 1eaf4 1eafa 1eafe 1eb00 1eb06 1eb09 1eb0c 1eb0e 1eb11 1eb20 1eb24 1eb27 1eb29 1eb2b 1eb2f 1eb33 1eb37 1eb39 1eb3d 1eb40 1eb42 1eb50 1eb54 1eb5b 1eb5f 1eb62 1eb64 1eb67 1eb6b 1eb6e 1eb71 1eb73 1eb76 1eb78 1eb7b 1eb7f 1eb83 1ebf5 1ebf9 1ec07 1ec0e'.split();need={int(x,16) for x in extra}
for w in expected['windows']:
 for j,line in enumerate((S/w['file']).read_text().splitlines(),1):
  p=line.split(None,3);v=int(p[0],16)
  if v in need:pins[v]={'address':hex(v),'bytes':p[1],'mnemonic':p[2],'operands':p[3].split(' ; ')[0].strip() if len(p)>3 else '', 'window':w['file'],'line':j};need.remove(v)
assert not need,need
with P.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as b:
 image_sha=hashlib.sha256(b).hexdigest();assert image_sha==expected['image_sha256']
 po=struct.unpack_from('<Q',b,32)[0];pe,pn=struct.unpack_from('<HH',b,54);ps=[struct.unpack_from('<IIQQQQQQ',b,po+i*pe) for i in range(pn)]
 def off(v,n):
  for p in ps:
   if p[0]==1 and p[3]<=v and v+n<=p[3]+p[5]:return p[2]+v-p[3]
  raise ValueError(hex(v))
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True;decoded={};wins=[];branches=[]
 for w in expected['windows']:
  v=int(w['start'],16);n=w['length'];o=off(v,n);raw=b[o:o+n];digest=hashlib.sha256(raw).hexdigest();assert digest==w['sha256'];ins=list(md.disasm(raw,v));assert len(ins)==w['instructions'];lines=[]
  for j,i in enumerate(ins,1):
   notes=[]
   for op in i.operands:
    if op.type==capstone.x86.X86_OP_MEM and op.mem.base==capstone.x86.X86_REG_RIP:notes.append('rip_target='+hex(i.address+i.size+op.mem.disp))
   lines.append(f'{i.address:#010x} {i.bytes.hex():<30} {i.mnemonic:8} {i.op_str}'+(' ; '+', '.join(notes) if notes else ''));decoded[i.address]=(i,w['file'],j)
  assert '\n'.join(lines)+'\n'==(S/w['file']).read_text();(H/w['file']).write_text('\n'.join(lines)+'\n');wins.append(dict(w,file_offset=hex(o)))
 for v,c in sorted(pins.items()):
  i,w,j=decoded[v];assert (i.bytes.hex(),i.mnemonic,i.op_str,w,j)==(c['bytes'],c['mnemonic'],c['operands'],c['window'],c['line']);z=i.bytes;target=None
  if z[0] in (0xe8,0xe9):target=v+len(z)+struct.unpack_from('<i',z,1)[0]
  elif z[0]==0xeb or 0x70<=z[0]<=0x7f:target=v+len(z)+struct.unpack_from('<b',z,1)[0]
  elif z[0]==0x0f and 0x80<=z[1]<=0x8f:target=v+len(z)+struct.unpack_from('<i',z,2)[0]
  if target is not None:assert i.op_str==hex(target);branches.append({'site':hex(v),'target':hex(target),'bytes':z.hex()})
 after=hashes();assert before==after and len(wins)>0 and len(pins)>0
 (H/'critical.json').write_text(json.dumps(list(pins.values()),indent=2)+'\n')
 receipt={'label':'Fresh Mac-only bounded two-task sorted append and affinity selection; no live/concurrent proof','image_path':expected['image_path'],'image_sha256':image_sha,'image_bytes':len(b),'capstone':capstone.__version__,'segments':[{'flags':p[1],'offset':p[2],'vaddr':p[3],'filesz':p[5]} for p in ps if p[0]==1],'windows':wins}
 (H/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
 (H/'verification.json').write_text(json.dumps({'label':'Fresh Mac-only exact-image replay PASS; no semantic execution test','windows':len(wins),'critical_instructions':len(pins),'relative_branches_and_calls':branches,'source_hashes_before':before,'source_hashes_after':after,'image_sha256':image_sha,'critical_sha256':hashlib.sha256((H/'critical.json').read_bytes()).hexdigest(),'full_image_sha_passes':1},indent=2)+'\n')
 print('PASS:',len(wins),'bounded window hashes/decodes,',len(pins),'critical instructions,',len(branches),'relative branch/call displacements,',len(before),'unchanged context hashes;1 full image SHA pass')
