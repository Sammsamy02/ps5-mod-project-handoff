import sys
sys.dont_write_bytecode=True
import capstone,hashlib,json,mmap,struct
from pathlib import Path
H=Path(__file__).resolve().parent;R=H.parents[3];r=json.loads((H/'receipt.json').read_text());P=R/r['image_path'];dis=json.loads((H/'discovery.json').read_text())
sources=['CLAUDE.md','outputs/2026-10-12-caps-prep-chatgpt/cap07/factory-audit/FINDING.md','outputs/2026-10-12-caps-prep-chatgpt/cap07/factory-audit/factory-vtable.json','outputs/2026-10-12-caps-prep-chatgpt/cap07/handoff-audit/FINDING.md','outputs/2026-10-12-caps-prep-chatgpt/cap07/STATUS.md']
def hashes():return {p:hashlib.sha256((R/p).read_bytes()).hexdigest() for p in sources}
before=hashes();checks={0x1c72f:('mov','r12, rsi'),0x1c754:('mov','ebx, dword ptr [rip + 0x92bf922]'),0x1c7c8:('mov','dword ptr [rbp - 0x2c], ebx'),0x1c7cb:('mov','rbx, r12'),0x1c871:('mov','r12, qword ptr [rbp - 0x38]'),0x1c879:('mov','ebx, dword ptr [rbp - 0x2c]'),0x1c897:('test','ebx, ebx'),0x1c8a1:('setg','byte ptr [r12 + 0x238]'),0x1dad2:('mov','word ptr [rbx + 0x238], 0'),0x1db5a:('mov','qword ptr [rbx], rdx'),0x1cc1b:('cmp','byte ptr [rbx + 0x238], 1'),0x1cc22:('jne','0x1cc3d'),0x3e826:('lea','rsi, [rip + 0x78399b3]'),0x3e82d:('lea','rdx, [rip + 0x929d848]'),0x3e834:('lea','rcx, [rip + 0x79820d5]'),0x3e83e:('call','qword ptr [rax + 0x30]')}
with P.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as b:
 assert hashlib.sha256(b).hexdigest()==r['image_sha256']
 po=struct.unpack_from('<Q',b,32)[0];pe,pn=struct.unpack_from('<HH',b,54);ps=[struct.unpack_from('<IIQQQQQQ',b,po+i*pe) for i in range(pn)]
 def off(v,n):
  for p in ps:
   if p[0]==1 and p[3]<=v and v+n<=p[3]+p[5]:return p[2]+v-p[3]
  raise ValueError(hex(v))
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);found={}
 for w in r['windows']:
  v=int(w['start'],16);raw=b[off(v,w['length']):off(v,w['length'])+w['length']];assert hashlib.sha256(raw).hexdigest()==w['sha256'];ins=list(md.disasm(raw,v));assert len(ins)==w['instructions']
  for j,i in enumerate(ins,1):
   if i.address in checks:
    assert (i.mnemonic,i.op_str)==checks[i.address];found[hex(i.address)]={'bytes':i.bytes.hex(),'instruction':i.mnemonic+' '+i.op_str,'window':w['file'],'line':j}
 assert len(found)==len(checks)
 d=next(p for p in ps if p[0]==2);tags={}
 for o in range(d[2],d[2]+d[5],16):
  t,v=struct.unpack_from('<qQ',b,o)
  if not t:break
  tags[t]=v
 rel=next(x for x in dis['relocs'] if x['destination']=='0x7ebcb28');o=int(rel['offset'],16);assert off(tags[7],tags[8])<=o<off(tags[7],tags[8])+tags[8] and (o-off(tags[7],tags[8]))%tags[9]==0
 assert b[o:o+24].hex()==rel['bytes'];assert struct.unpack_from('<QQq',b,o)==(0x7ebcb28,8,0x1c710)
 assert 0x1da26+7+struct.unpack_from('<i',b,off(0x1da26,7)+3)[0]==0x7ebcab0
 assert struct.unpack_from('<i',b,off(0x92dc07c,4))[0]==1
 strings=[]
 for vs,expected in dis['strings'].items():
  v=int(vs,16);raw=expected.encode('utf-16-le')+b'\0\0';assert b[off(v,len(raw)):off(v,len(raw))+len(raw)]==raw;strings.append({'vaddr':vs,'file_offset':hex(off(v,len(raw))),'bytes':raw.hex(),'decoded':expected})
 after=hashes();assert before==after
 out={'label':'Fresh Mac-only replay PASS; static configuration only','image_sha256':r['image_sha256'],'windows':len(r['windows']),'critical_instructions':found,'setup_primary_vtable_slot':{'address_point':'0x7ebcab0','slot':'0x78','relocation':rel},'config_saved_initial_i32':1,'strings':strings,'source_sha256_before':before,'source_sha256_after':after,'live_selection_proved':False}
 (H/'verification.json').write_text(json.dumps(out,indent=2)+'\n');print('PASS:',len(r['windows']),'window hashes/decodes,',len(found),'critical instructions,1 relocation,2 UTF16 strings,',len(before),'unchanged source hashes')
