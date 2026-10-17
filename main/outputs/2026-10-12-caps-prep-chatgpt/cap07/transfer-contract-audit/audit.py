"""Exact captured libc export/three bounded count paths; no API-name/ABI inference."""
import sys
sys.dont_write_bytecode=True
import capstone,hashlib,json,mmap,os,struct
from pathlib import Path
H=Path(__file__).resolve().parent;R=H.parents[3]
P=R/'.superpowers/native-observations/user-return-20261003T140907Z-3bf558aa/libc.prx'
SHA='6951ddd7df5daabfbc9ac495ea0eee73e7de121cddb30a1e979fad005a7c6b04';ALPH='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-'
refs=[R/'outputs/2026-10-12-caps-prep-chatgpt/cap07/array-transfer-audit/receipt.json',R/'.superpowers/native-dependency-spike/ue4ss-original-libc-exports-1/strict-metadata.json',R/'.superpowers/native-dependency-spike/ue4ss-original-libc-exports-1/inputs.json',R/'.superpowers/native-dependency-spike/ue4ss-original-libc-exports-1/REPORT.md',R/'.superpowers/native-observations/user-return-20261003T140907Z-3bf558aa/receipt.json']
def digest(p):
 assert p.stat().st_size<=8*1024*1024
 return hashlib.sha256(p.read_bytes()).hexdigest()
before={str(p.relative_to(R)):digest(p) for p in refs};game=json.loads(refs[0].read_text())['import'];saved=json.loads(refs[1].read_text());inputs=json.loads(refs[2].read_text())
assert inputs['capture']['sha256']==SHA and inputs['capture']['bytes']==1911850
assert digest(refs[4])==inputs['capture_receipt']['sha256']
assert game['name_record']['value']=='+P6FRGH4LfA#6#4'
def ident(s):
 n=0
 for c in s:n=n*64+ALPH.index(c)
 return n
def identity(s):return (s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns)
with P.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as b:
 state=identity(os.fstat(f.fileno()));assert len(b)==1911850 and hashlib.sha256(b).hexdigest()==SHA
 assert b[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<H',b,16)[0]==0xfe18
 po=struct.unpack_from('<Q',b,32)[0];pe,pn=struct.unpack_from('<HH',b,54);assert pe==56 and 0<pn<=32
 ps=[struct.unpack_from('<IIQQQQQQ',b,po+i*pe) for i in range(pn)]
 def off(v,n):
  matches=[p[2]+v-p[3] for p in ps if p[0]==1 and p[3]<=v and v+n<=p[3]+p[5]];assert len(matches)==1;return matches[0]
 def record(v,n):
  o=off(v,n);raw=b[o:o+n];return {'vaddr':hex(v),'file_offset':hex(o),'bytes':raw.hex(),'sha256':hashlib.sha256(raw).hexdigest()}
 ds=[p for p in ps if p[0]==2];assert len(ds)==1;d=ds[0];assert d[5]<=4096 and d[5]%16==0
 dynamic=[];tags={}
 for o in range(d[2],d[2]+d[5],16):
  t,v=struct.unpack_from('<qQ',b,o)
  if not t:break
  dynamic.append((t,v,o));tags[t]=v
 assert tags[11]==24 and tags[10]<=1024*1024
 def string(no):
  assert 0<=no<tags[10];o=off(tags[5]+no,1);end=b.find(b'\0',o,min(o+tags[10]-no,o+256));assert end>=o
  return {'value':b[o:end].decode('ascii'),'file_offset':hex(o),'bytes':b[o:end+1].hex()}
 candidate=[c for c in saved['metadata']['Candidates'] if c.get('Nid')=='+P6FRGH4LfA'];assert len(candidate)==1;c=candidate[0];assert c['SymbolIndex']==445
 v=tags[6]+445*24;sym=record(v,24);no,info,other,section,value,size=struct.unpack('<IBBHQQ',bytes.fromhex(sym['bytes']));name=string(no)
 assert off(v,24)==c['SymbolByteOffset'] and (name['value'],value,size,section,info&15,info>>4,other&3)==(c['EncodedName'],c['Value'],c['Size'],c['SectionIndex'],2,1,3)
 assert (value,size,section,name['value'])==(0x3b90,54,3,'+P6FRGH4LfA#D#A')
 parts=name['value'].split('#');assert parts[0]==game['name_record']['value'].split('#')[0]
 provider={}
 for t,pv,o in dynamic:
  if t in (0x61000043,0x61000047):
   kind='module' if t==0x61000043 else 'library';which=ident(parts[2] if kind=='module' else parts[1])
   if pv>>48==which:
    assert kind not in provider
    provider[kind]={'numeric_id':which,'version':pv>>32&0xffff,'name':string(pv&0xffffffff),'dynamic_file_offset':hex(o),'dynamic_bytes':b[o:o+16].hex()}
 assert set(provider)=={'module','library'}
 for kind in provider:
  assert provider[kind]['name']['value']==game['providers'][kind]['name']['value']=='libc'
  assert provider[kind]['version']==int(game['providers'][kind]['version_bits'],16)
 assert provider['module']['version']==257 and provider['library']['version']==1
 soname=string(tags[14]);assert soname['value']=='libc.prx'
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);wins=[];edges=[]
 for start,n in [(0x3b90,54),(0x3c06,25),(0x41c7,2),(0x4215,8),(0x429d,10)]:
  rec=record(start,n);ins=list(md.disasm(bytes.fromhex(rec['bytes']),start));assert sum(i.size for i in ins)==n
  lines=[f'{i.address:#010x} {i.bytes.hex():<28} {i.mnemonic:8} {i.op_str}' for i in ins]
  rec.update(instructions=len(ins),file=f'window-{start:x}.txt');wins.append(rec)
  for i in ins:
   if i.mnemonic in ('jb','jae'):edges.append({'site':hex(i.address),'bytes':i.bytes.hex(),'target':i.op_str,'kind':i.mnemonic})
  (H/rec['file']).write_text('\n'.join(lines)+'\n')
 # RIP-relative table base independently computed from actual LEA bytes.
 raw=bytes.fromhex(record(0x3c0f,7)['bytes']);assert raw[:3]==b'\x48\x8d\x0d';table=0x3c16+struct.unpack_from('<i',raw,3)[0];assert table==0x15c1b4
 paths=[]
 for n,target in [(0,0x41c7),(8,0x4215),(16,0x429d)]:
  rec=record(table+4*n,4);delta=struct.unpack('<i',bytes.fromhex(rec['bytes']))[0];assert table+delta==target;paths.append({'input_rdx':n,'table_base':hex(table),'entry':rec,'signed_relative_value':delta,'computed_target':hex(target)})
 assert bytes.fromhex(wins[3]['bytes'])==bytes.fromhex('488b164889175dc3') and bytes.fromhex(wins[4]['bytes'])==bytes.fromhex('c5fa6f06c5fa7f075dc3')
 assert hashlib.sha256(b).hexdigest()==SHA and identity(os.fstat(f.fileno()))==state==identity(P.stat())
after={str(p.relative_to(R)):digest(p) for p in refs};assert before==after
out={'schema':1,'label':'Mac-only actual captured export and bounded register-path evidence','capture':{'path':str(P.relative_to(R)),'bytes':1911850,'sha256':SHA,'unchanged':True},'provider':provider,'soname':soname,'export':{'symbol_index':445,'symbol_record':sym,'name':name,'value':hex(value),'declared_size':size,'section':section,'type':2,'binding':1,'visibility':3},'game_import_receipt':str(refs[0].relative_to(R)),'provider_name_versions_match':True,'loaded_game_provider_identity_proven':False,'windows':wins,'conditional_edges':edges,'count_paths':paths,'selected_instruction_bytes':sum(len(bytes.fromhex(w['bytes'])) for w in wins),'selected_instructions':sum(w['instructions'] for w in wins),'capstone_version':capstone.__version__,'source_hashes_before':before,'source_hashes_after':after,'plaintext_api_name_proven':False,'general_transfer_or_overlap_contract_proven':False,'register_path_result':'With RDX=8 and entry guards passing, one qword load from RSI precedes one qword store to RDI, then RBP restoration/return; RAX contains entry RDI. RDX=16 analogously uses one unaligned16B load/store. No memory access on selected RDX=0 dispatch path. Pointer validity and nontrapping guard assumptions required.','limits':'Matching captured provider metadata is not proof this exact capture is the live game loader choice. No SDK, full-routine contract, arbitrary-count overlap, plaintext C prototype, target execution or runtime admission.'}
p=H/'receipt.json'
if p.exists():assert json.loads(p.read_text())==out,'replay receipt drift'
p.write_text(json.dumps(out,indent=2)+'\n');print('PASS exact provider/version/NID;',out['selected_instruction_bytes'],'instruction bytes;',out['selected_instructions'],'instructions; count paths0/8/16; original unchanged')
