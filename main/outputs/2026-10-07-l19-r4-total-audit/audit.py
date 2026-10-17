#!/usr/bin/env python3
"""Independent source-contract/native geometry audit, not a native sizeof measurement."""
from pathlib import Path
import capstone,hashlib,json,re,struct,sys
root=Path.cwd();own=Path(__file__).resolve().parent;source=root/'outputs/2026-10-07-l19-r4-total-evidence/official-engine-evidence';p=Path(sys.argv[1]) if len(sys.argv)>1 else source/'OFFICIAL-TOTAL-FACTS.json';d=json.loads(p.read_text())
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert {(f['key'],f['value'],f['width_bytes']) for f in d['facts']}=={('FField.UEP_TotalSize','0x30',None),('FFieldClass.UEP_TotalSize','0x40',None)} and len(d['facts'])==2
assert all(f['status']=='validated-static' and f['live_validation'] is False and 'source-contract' in f['confidence'] for f in d['facts'])
b=Path(d['image']['path']).read_bytes();assert hashlib.sha256(b).hexdigest()==d['image']['sha256']=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc';assert len(b)==d['image']['bytes']==194241424
h=struct.unpack_from('<16sHHIQQQIHHHHHH',b);assert h[0][:6]==b'\x7fELF\x02\x01';ph=[struct.unpack_from('<IIQQQQQQ',b,h[5]+i*h[9]) for i in range(h[10])];loads=[x for x in ph if x[0]==1]
def offset(v,n):
 matches=[x[2]+v-x[3] for x in loads if x[3]<=v and v+n<=x[3]+x[5]];assert len(matches)==1,(hex(v),n);return matches[0]
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
ins={}
for key,r in d['instruction_evidence'].items():
 v=int(r['elf_image_vaddr'],0);raw=bytes.fromhex(r['instruction_bytes']);f=offset(v,len(raw));assert v==int(key,0) and f==int(r['file_byte_offset'],0) and b[f:f+len(raw)]==raw
 decoded=list(md.disasm(raw,v));assert len(decoded)==1 and decoded[0].size==len(raw);i=decoded[0];assert i.mnemonic+' '+i.op_str==r['instruction'];ins[v]=i
for r in d['data_evidence']:
 v=int(r['elf_image_vaddr'],0);raw=bytes.fromhex(r['bytes']);f=offset(v,len(raw));assert f==int(r['file_byte_offset'],0) and b[f:f+len(raw)]==raw
# Reparse actual DT_RELA membership rather than relying on a candidate's fixed range.
dyn={}
for x in ph:
 if x[0]!=2:continue
 for f in range(x[2],x[2]+x[5],16):
  tag,val=struct.unpack_from('<QQ',b,f)
  if tag==0:break
  dyn.setdefault(tag,[]).append(val)
for tag in (7,8,9):assert len(dyn[tag])==1
assert dyn[9][0]==24 and dyn[8][0]%24==0
wanted={int(r['r_offset'],0) for r in d['root_vtable_relocation_evidence']}|{int(r['vptr'],0) for r in d['same_slot_size_family']};rows={}
start=offset(dyn[7][0],dyn[8][0])
for f in range(start,start+dyn[8][0],24):
 v,info,addend=struct.unpack_from('<QQq',b,f)
 if v in wanted:rows.setdefault(v,[]).append((f,info,addend))
def rel(v):
 assert len(rows.get(v,[]))==1,(hex(v),rows.get(v));f,info,target=rows[v][0];assert info==8;return f,target
rootptr=0x7edc650
expected_targets=[0x3701a0,0xb290,0x26030,0x364710,0x364700,0xb290,0xb290,0x361350,0x3648d0,0xb290,0xb290,0xe0,0xb290]
assert len(d['root_vtable_relocation_evidence'])==13
for r,expected in zip(d['root_vtable_relocation_evidence'],expected_targets):
 slot=int(r['slot_byte_offset'],0);v=int(r['r_offset'],0);f,target=rel(v);assert v==rootptr+slot and target==expected==int(r['r_addend'],0) and f==int(r['file_byte_offset'],0) and b[f:f+24].hex()==r['entry_bytes']
family={}
for r in d['same_slot_size_family']:
 v=int(r['vptr'],0);f,target=rel(v);assert f==int(r['relocation_file_offset'],0) and target==int(r['function'],0) and b[f:f+24].hex()==r['entry_bytes']
 for w in r['function_instructions']:
  a=int(w['vaddr'],0);raw=bytes.fromhex(w['bytes']);assert b[offset(a,len(raw)):offset(a,len(raw))+len(raw)]==raw
  i=list(md.disasm(raw,a));assert len(i)==1 and i[0].size==len(raw) and i[0].mnemonic+' '+i[0].op_str==w['instruction']
 raw=b[offset(target,16):offset(target,16)+16];i=list(md.disasm(raw,target));assert [x.mnemonic for x in i[:5]]==['push','mov','mov','pop','ret'];assert i[2].reg_name(i[2].operands[0].reg)=='eax';family[r['named_class']]=i[2].operands[1].imm
assert family=={'FEnumProperty':0x80,'FField':0x30,'FProperty':0x70,'FArrayProperty':0x80,'FBoolProperty':0x78,'FStructProperty':0x78}
# Named root constructor and independent behavior anchors for source virtual order.
def text(v):i=ins[v];return i.mnemonic+' '+i.op_str
def rip(v):i=ins[v];return [i.address+i.size+x.mem.disp for x in i.operands if x.type==capstone.x86.X86_OP_MEM and x.mem.base==capstone.x86.X86_REG_RIP]
assert rootptr in rip(0x3645c4) and text(0x3645cf)=='mov qword ptr [rdi], rax'
assert text(0x3701a4)=='mov eax, 0x30' and text(0x364707)=='call qword ptr [rax + 0x48]'
assert text(0x36471d)=='lea rsi, [rdi + 0x20]' and text(0x364731)=='add rbx, 0x28' and text(0x36474c)=='mov edx, 4'
store_defs={
 'FFieldClass_Name':(0x3639ef,'rbx',0,8),'FFieldClass_Id':(0x3639b7,'rdi',8,8),'FFieldClass_CastFlags':(0x3639bb,'rdi',0x10,8),'FFieldClass_ClassFlags':(0x3639bf,'rdi',0x18,4),
 'FFieldClass_SuperClass':(0x3639c6,'rdi',0x20,8),'FFieldClass_DefaultObject':(0x3639ca,'rdi',0x28,8),'FFieldClass_ConstructFn':(0x3639d2,'rdi',0x30,8),'FFieldClass_Counter':(0x3639d6,'rdi',0x38,4),
 'FField_ClassPrivate':(0x3645d2,'rdi',8,8),'FField_Next':(0x364642,'rdi',0x18,8),'FField_NamePrivate':(0x36464d,'rdi',0x20,8),'FField_FlagsPrivate':(0x364651,'rdi',0x28,4)}
geometry={}
for name,(a,base,disp,width) in store_defs.items():
 op=ins[a].operands[0];assert op.type==capstone.x86.X86_OP_MEM and ins[a].reg_name(op.mem.base)==base and op.mem.disp==disp and op.size==width
 assert d['native_constraints'][name]==disp;geometry[name]={'offset':disp,'width':width,'instruction':hex(a)}
assert any(op.type==capstone.x86.X86_OP_MEM and op.mem.disp==0x10 and op.size==8 for op in ins[0x364b8a].operands)
for r in d['licensed_sources']:
 raw=Path(r['local_path']).read_bytes();assert hashlib.sha256(raw).hexdigest()==r['sha256'] and hashlib.sha1(b'blob '+str(len(raw)).encode()+b'\0'+raw).hexdigest()==r['blob_sha1'];assert r['commit']==d['official_commit']=='6978b63c8951e57d97048d8424a0bebd637dde1d'
for r in d['sources']:assert sha(r['path'])==r['sha256']
# Independently inspect complete source class boundaries, including omitted methods/macros.
def mask(s):return re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',lambda m:re.sub(r'[^\n]',' ',m[0]),s)
def classbody(path,name):
 s=Path(path).read_text();clean=mask(s);m=re.search(r'\b(?:class|struct)\s+'+name+r'\s*\{',clean);assert m
 start=clean.index('{',m.start());depth=1;end=start+1
 while depth:
  depth+=(clean[end]=='{')-(clean[end]=='}');end+=1
 assert ':' not in clean[m.start():start]
 return s[start+1:end-1],clean[start+1:end-1]
def declarations(body):
 depth=0;buf='';rows=[]
 for char in body:
  if depth==0:buf+=char
  if char=='{':depth+=1
  elif char=='}':
   depth-=1
   if depth==0:buf=''
  elif char==';' and depth==0:
   stmt=re.sub(r'^\s*#.*$', '',buf,flags=re.M);stmt=re.sub(r'\s+',' ',stmt).strip();stmt=re.sub(r'^(?:(?:public|private|protected):\s*)+','',stmt);buf=''
   if not stmt or stmt==';' or stmt.startswith(('using ','typedef ','friend ','static ','constexpr ')):continue
   if '(' in stmt and '(*ConstructFn)' not in stmt:continue
   rows.append(stmt)
 return rows
engine=root/'outputs/2026-10-07-l19-r4-official-engine-source';fieldpath=engine/'Field.h';fieldraw=fieldpath.read_text();cpp=(engine/'Field.cpp').read_text()
assert not re.search(r'#\s*pragma\s+pack',mask(fieldraw))
expected_fields={
 'FFieldClass':['FName Name;','uint64 Id;','uint64 CastFlags;','EClassFlags ClassFlags;','FFieldClass* SuperClass;','FField* DefaultObject;','FField* (*ConstructFn)(const FFieldVariant&, const FName&, EObjectFlags);','FThreadSafeCounter UnqiueNameIndexCounter;'],
 'FField':['FFieldClass* ClassPrivate;','FFieldVariant Owner;','FField* Next;','FName NamePrivate;','EObjectFlags FlagsPrivate;','TMap<FName, FString>* MetaDataMap;']}
classes={}
for name in ('FFieldClass','FField','FFieldVariant'):
 body,clean=classbody(fieldpath,name);assert not re.search(r'alignas|__attribute__|__declspec|#\s*pragma\s+pack',clean)
 classes[name]={'data_statements':declarations(clean),'virtuals':re.findall(r'\bvirtual\s+(?:[\w:<>,*& ]+\s+)?(~?\w+)\s*\(',clean)}
 if name in expected_fields:assert classes[name]['data_statements']==expected_fields[name],classes[name]
assert classes['FField']['virtuals']==['GetFieldSize','~FField','Serialize','PostLoad','GetPreloadDependencies','BeginDestroy','AddReferencedObjects','AddCppProperty','Bind','PostDuplicate','GetInnerFieldByName','GetInnerFields'],classes['FField']
assert classes['FFieldClass']['virtuals']==[] and classes['FFieldVariant']['virtuals']==[]
variant_body=classbody(fieldpath,'FFieldVariant')[1]
assert classes['FFieldVariant']['data_statements']==['Container;']
assert re.search(r'union FFieldObjectUnion\s*\{\s*FField\* Field;\s*UObject\* Object;\s*\} Container;',variant_body)
assert 'virtual SIZE_T GetFieldSize() const\n\t{\n\t\treturn sizeof(FField);\n\t}' in fieldraw and 'return sizeof(TClass);' in fieldraw
assert 'void FField::PostLoad()\n{\n\tBind();\n}' in cpp and 'Ar << NamePrivate;\n\tAr << (uint32&)FlagsPrivate;' in cpp
assert re.search(r'#if WITH_METADATA\s+private:\s*/\*\*[^*]*\*/\s*TMap<FName, FString>\* MetaDataMap;',fieldraw)
# Literal dependency spans and emitted configuration arrays are controls of the contract.
x=json.loads((source/'EXTRACTION.json').read_text());assert sha(source/'literal_layout_control.cpp')==x['output_cpp_sha256']
for r in x['segments']:
 raw=Path(r['source_path']).read_bytes();part=raw[r['start_byte']:r['end_byte_exclusive']];assert sha(r['source_path'])==r['source_sha256'] and part.decode()==r['exact_text'] and hashlib.sha256(part).hexdigest()==r['text_sha256']
compiled=json.loads((source/'COMPILED-CONTROLS.json').read_text());matched=[];object_controls=[]
def symbols(raw):
 assert raw[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<H',raw,18)[0]==62
 shoff=struct.unpack_from('<Q',raw,40)[0];size,num=struct.unpack_from('<HH',raw,58)
 sections=[struct.unpack_from('<IIQQQQIIQQ',raw,shoff+i*size) for i in range(num)];result={}
 for sec in sections:
  if sec[1]!=2:continue
  namesec=sections[sec[6]];names=raw[namesec[4]:namesec[4]+namesec[5]]
  for at in range(sec[4],sec[4]+sec[5],sec[9]):
   name,info,other,index,value,n=struct.unpack_from('<IBBHQQ',raw,at)
   if index==0 or index>=len(sections):continue
   name=names[name:names.index(0,name)].decode();data=sections[index];fileoff=data[4]+value-data[3];result[name]=raw[fileoff:fileoff+n]
 return result

for r in compiled['controls']:
 recompiled=own/'recompiled'/(r['name']+'.o');raw=recompiled.read_bytes();assert hashlib.sha256(raw).hexdigest()==r['object_sha256']
 syms=symbols(raw);array=syms[r['array_symbol']['name']];assert len(array)==4*len(x['array_labels'])
 values=dict(zip(x['array_labels'],struct.unpack('<'+str(len(x['array_labels']))+'I',array)));assert values==r['values']
 body=list(md.disasm(syms[r['direct_size_query_body_symbol']['name']],0));assert len(body)==2 and body[0].mnemonic=='mov' and body[0].reg_name(body[0].operands[0].reg)=='eax' and body[0].operands[1].imm==values['sizeof_FField'] and body[1].mnemonic=='ret'
 object_controls.append({'name':r['name'],'array_values':values,'object_sha256':hashlib.sha256(raw).hexdigest()})
 assert all(k in r['values'] for k in d['native_constraints'])
 if all(r['values'][k]==v for k,v in d['native_constraints'].items()):matched.append(r['name']);assert r['values']['sizeof_FField']==0x30 and r['values']['sizeof_FFieldClass']==0x40
assert matched==['natural','case1_outline1'] and matched==d['physically_matching_controls']
pack=next(r for r in compiled['controls'] if r['name']=='pack4')['values'];assert [pack[k] for k in ('FFieldClass_SuperClass','FFieldClass_DefaultObject','FFieldClass_ConstructFn','FFieldClass_Counter')]==[0x1c,0x24,0x2c,0x34]
result={'status':'OFFICIAL_TOTAL_STATIC_AUDIT_PASS','pass':True,'accepted_fact_source':{'path':str(p.resolve()),'sha256':sha(p)},'accepted_facts':d['facts'],'proof_kind':'high-confidence source-contract-correlated native inference; not direct native allocation/live sizeof measurement','verified':{'instruction_windows':len(ins),'data_windows':len(d['data_evidence']),'actual_unique_DT_RELA_targets':len(rows),'licensed_sources_sha256_and_gitblob':len(d['licensed_sources']),'literal_source_spans':len(x['segments']),'native_member_geometries':len(geometry)},'independently_decoded_recompiled_PS5_controls':object_controls,'source_complete_classes':classes,'geometry':geometry,'physically_matching_configurations':matched,'assumptions_and_limits':d['assumptions_and_limits'],'audit_script_sha256':sha(__file__)}
(own/'AUDIT.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'pass':True,'verified':result['verified'],'matching_controls':matched}))
