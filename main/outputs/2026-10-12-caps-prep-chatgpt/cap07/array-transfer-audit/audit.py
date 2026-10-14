"""Bounded read-only call-site and import-identity replay; no API-name guessing."""
import sys
sys.dont_write_bytecode=True
import capstone,hashlib,json,mmap,struct
from pathlib import Path
H=Path(__file__).resolve().parent;R=H.parents[3];P=R/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin';SHA='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc';ALPH='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-'
sources=['CLAUDE.md','outputs/2026-10-12-caps-prep-chatgpt/cap07/STATUS.md','outputs/2026-10-12-caps-prep-chatgpt/cap07/import-audit/audit_imports.py','outputs/2026-10-12-caps-prep-chatgpt/cap07/sorted-multi-entry-audit/FINDING.md','outputs/2026-10-12-caps-prep-chatgpt/cap07/sorted-handoff-audit/critical.json']
def hashes():return {p:hashlib.sha256((R/p).read_bytes()).hexdigest() for p in sources}
def ident(s):
 n=0
 for c in s:n=n*64+ALPH.index(c)
 return n
before=hashes()
with P.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as b:
 assert hashlib.sha256(b).hexdigest()==SHA and b[:6]==b'\x7fELF\x02\x01'
 po=struct.unpack_from('<Q',b,32)[0];pe,pn=struct.unpack_from('<HH',b,54);ps=[struct.unpack_from('<IIQQQQQQ',b,po+i*pe) for i in range(pn)]
 def off(v,n):
  for p in ps:
   if p[0]==1 and p[3]<=v and v+n<=p[3]+p[5]:return p[2]+v-p[3]
  raise ValueError(hex(v))
 def rec(v,n):
  o=off(v,n);raw=b[o:o+n];return {'vaddr':hex(v),'file_offset':hex(o),'bytes':raw.hex(),'sha256':hashlib.sha256(raw).hexdigest()}
 d=next(p for p in ps if p[0]==2);tags={};dy=[]
 for o in range(d[2],d[2]+d[5],16):
  t,v=struct.unpack_from('<qQ',b,o)
  if not t:break
  tags[t]=v;dy.append((t,v,o))
 assert tags[9]==tags[11]==24 and tags[20]==7 and 0<tags[2]<1024*1024 and tags[2]%24==0
 def string(no):
  assert 0<=no<tags[10];o=off(tags[5]+no,1);e=b.find(b'\0',o,o+tags[10]-no);assert e>=o
  return {'value':b[o:e].decode('ascii'),'file_offset':hex(o),'bytes':b[o:e+1].hex(),'string_offset':hex(no)}
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);wins=[];critical=[];calls=[]
 expectations={0x1dcad:'e8de414507',0x1e9d2:'e8b9344507'}
 for v,n in ((0x1dc90,0x2a),(0x1e9b4,0x2a),(0x7471e90,0x10)):
  rr=rec(v,n);ins=list(md.disasm(bytes.fromhex(rr['bytes']),v));assert ins and sum(i.size for i in ins)==n;lines=[]
  for j,i in enumerate(ins,1):
   lines.append(f'{i.address:#010x} {i.bytes.hex():<30} {i.mnemonic:8} {i.op_str}');critical.append({'address':hex(i.address),'bytes':i.bytes.hex(),'instruction':i.mnemonic+' '+i.op_str,'window':f'window-{v:x}.txt','line':j})
   if i.address in expectations:
    assert i.mnemonic=='call' and i.bytes.hex()==expectations[i.address];target=i.address+i.size+struct.unpack_from('<i',i.bytes,1)[0];assert target==0x7471e90 and i.op_str==hex(target);calls.append({'site':hex(i.address),'bytes':i.bytes.hex(),'target':hex(target)})
  (H/f'window-{v:x}.txt').write_text('\n'.join(lines)+'\n');rr.update(file=f'window-{v:x}.txt',instructions=len(ins));wins.append(rr)
 stub=bytes.fromhex(wins[-1]['bytes']);assert stub[:2]==b'\xff\x25';got=0x7471e96+struct.unpack_from('<i',stub,2)[0]
 matches=[]
 for k in range(tags[2]//24):
  v=tags[23]+24*k;o=off(v,24);dst,info,add=struct.unpack_from('<QQq',b,o)
  if dst==got:matches.append((k,v,info,add))
 assert len(matches)==1;k,v,info,add=matches[0];assert info&0xffffffff==7;si=info>>32;sva=tags[6]+si*24;so=off(sva,24);no,inf,oth,section,val,size=struct.unpack_from('<IBBHQQ',b,so);name=string(no);parts=name['value'].split('#');assert len(parts)==3 and section==0
 providers={}
 for t,pv,o in dy:
  if t in (0x61000045,0x61000049):
   kind='module' if t==0x61000045 else 'library';which=ident(parts[2] if kind=='module' else parts[1])
   if pv>>48==which:providers[kind]={'dynamic_file_offset':hex(o),'dynamic_bytes':b[o:o+16].hex(),'encoded_numeric_id':which,'name':string(pv&0xffffffff),'version_bits':hex(pv>>32&0xffff)}
 assert len(providers)==2 and len(calls)==2
 after=hashes();assert before==after
 out={'label':'Fresh Mac-only exact import and call registers; no transfer semantic or API-name admission','image_path':str(P.relative_to(R)),'image_sha256':SHA,'image_bytes':len(b),'capstone':capstone.__version__,'load_segments':[{'flags':p[1],'offset':hex(p[2]),'vaddr':hex(p[3]),'filesz':hex(p[5])} for p in ps if p[0]==1],'windows':wins,'critical_instructions':critical,'direct_calls':calls,'import':{'plt':'0x7471e90','got':hex(got),'jmprel_index':k,'relocation':rec(v,24),'relocation_type':7,'symbol_index':si,'symbol_record':rec(sva,24),'symbol_section':section,'symbol_value':hex(val),'name_record':name,'providers':providers},'dynamic_bounds':{'offset':hex(d[2]),'bytes':d[5]},'dynamic_table_records':[{'tag':hex(t),'value':hex(pv),'file_offset':hex(o),'bytes':b[o:o+16].hex()} for t,pv,o in dy if t in (2,5,6,9,10,11,20,23)],'source_hashes_before':before,'source_hashes_after':after,'full_image_sha_passes':1,'same_image_implementation_established':False,'plaintext_api_name_established':False}
 # On later runs compare immutable exact-image evidence; source context hashes are recorded per replay.
 path=H/'receipt.json'
 if path.exists():
  old=json.loads(path.read_text());keys=[x for x in out if x not in ('source_hashes_before','source_hashes_after')];assert all(old[x]==out[x] for x in keys),'image receipt drift'
 path.write_text(json.dumps(out,indent=2)+'\n')
 (H/'verification.json').write_text(json.dumps({'label':'Fresh Mac-only replay PASS','image_sha256':SHA,'windows':len(wins),'instructions':len(critical),'direct_calls':len(calls),'plt_got_relocation_symbol_provider_chains':1,'unchanged_context_hashes':len(before),'plaintext_api_names':0,'full_image_sha_passes':1},indent=2)+'\n')
 print('PASS:',len(wins),'windows,',len(critical),'instructions,2 relative calls,1 PLT/GOT/RELA/symbol/provider chain,',len(before),'unchanged context hashes; encoded import',name['value'],'library',providers['library']['name']['value'],'module',providers['module']['name']['value'])
