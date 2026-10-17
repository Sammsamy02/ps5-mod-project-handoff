#!/usr/bin/env python3
"""Revalidate sealed native/source/target-object proof; no engine execution."""
import hashlib,json,re,struct
from pathlib import Path
import capstone
OUT=Path('/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l19-r4-total-evidence/official-engine-evidence');OWN=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
d=json.loads((OUT/'OFFICIAL-TOTAL-FACTS.json').read_text())
image=Path(d['image']['path']).read_bytes()
assert hashlib.sha256(image).hexdigest()==d['image']['sha256'] and len(image)==d['image']['bytes']
phoff=struct.unpack_from('<Q',image,32)[0];phsize,num=struct.unpack_from('<HH',image,54)
loads=[]
for i in range(num):
    p=struct.unpack_from('<IIQQQQQQ',image,phoff+i*phsize)
    if p[0]==1:loads.append({'file_offset':p[2],'image_vaddr':p[3],'file_size':p[5],'memory_size':p[6],'flags':p[1]})
assert loads==d['PT_LOAD']
def offset(a,size):
    matches=[p['file_offset']+a-p['image_vaddr'] for p in loads if p['image_vaddr']<=a and a+size<=p['image_vaddr']+p['file_size']]
    assert len(matches)==1;return matches[0]
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
def valid_window(r):
    a=int(r['elf_image_vaddr'],16);raw=bytes.fromhex(r['instruction_bytes']);f=offset(a,len(raw))
    if f!=int(r['file_byte_offset'],16) or image[f:f+len(raw)]!=raw:return False
    ins=list(md.disasm(raw,a))
    return len(ins)==1 and ins[0].size==len(raw) and f'{ins[0].mnemonic} {ins[0].op_str}'.strip()==r['instruction'].strip()
assert all(valid_window(r) for r in d['instruction_evidence'].values())
def ins(key):
    r=d['instruction_evidence'][key]
    return next(md.disasm(bytes.fromhex(r['instruction_bytes']),int(r['elf_image_vaddr'],16)))
from capstone.x86 import X86_OP_MEM,X86_OP_REG,X86_OP_IMM,X86_REG_RDI,X86_REG_RBX,X86_REG_EAX
stores={'FFieldClass_Name':('0x3639ef',X86_REG_RBX,0,8),
        'FFieldClass_Id':('0x3639b7',X86_REG_RDI,8,8),
        'FFieldClass_CastFlags':('0x3639bb',X86_REG_RDI,0x10,8),
        'FFieldClass_ClassFlags':('0x3639bf',X86_REG_RDI,0x18,4),
        'FFieldClass_SuperClass':('0x3639c6',X86_REG_RDI,0x20,8),
        'FFieldClass_DefaultObject':('0x3639ca',X86_REG_RDI,0x28,8),
        'FFieldClass_ConstructFn':('0x3639d2',X86_REG_RDI,0x30,8),
        'FFieldClass_Counter':('0x3639d6',X86_REG_RDI,0x38,4),
        'FField_ClassPrivate':('0x3645d2',X86_REG_RDI,8,8),
        'FField_Next':('0x364642',X86_REG_RDI,0x18,8),
        'FField_NamePrivate':('0x36464d',X86_REG_RDI,0x20,8),
        'FField_FlagsPrivate':('0x364651',X86_REG_RDI,0x28,4)}
for name,(key,base,disp,size) in stores.items():
    op=ins(key).operands[0]
    assert op.type==X86_OP_MEM and op.mem.base==base and op.mem.disp==disp and op.size==size
    assert d['native_constraints'][name]==disp
owner=ins('0x364b8a')
assert any(op.type==X86_OP_MEM and op.mem.disp==0x10 and op.size==8 for op in owner.operands)
query=ins('0x3701a4')
assert query.mnemonic=='mov' and query.operands[0].reg==X86_REG_EAX and query.operands[1].imm==0x30
assert ins('0x364707').operands[0].mem.disp==0x48
assert ins('0x36471d').operands[1].mem.disp==0x20
assert ins('0x364731').operands[1].imm==0x28 and ins('0x36474c').operands[1].imm==4

targets=set(int(r['r_offset'],16) for r in d['root_vtable_relocation_evidence'])
targets.update(int(r['vptr'],16) for r in d['same_slot_size_family'])
counts={t:0 for t in targets}
for f in range(0x932f430,0x932f430+39878376,24):
    target,info,addend=struct.unpack_from('<QQq',image,f)
    if target in counts:counts[target]+=1
assert all(v==1 for v in counts.values())
for r in d['root_vtable_relocation_evidence']:
    f=int(r['file_byte_offset'],16)
    assert image[f:f+24].hex()==r['entry_bytes']
    target,info,addend=struct.unpack_from('<QQq',image,f)
    assert target==int(r['r_offset'],16)==0x7edc650+int(r['slot_byte_offset'],16) and info==8 and addend==int(r['r_addend'],16)
for r in d['same_slot_size_family']:
    f=int(r['relocation_file_offset'],16)
    assert image[f:f+24].hex()==r['entry_bytes']
    t,info,a=struct.unpack_from('<QQq',image,f)
    assert t==int(r['vptr'],16) and info==8 and a==int(r['function'],16)

for p in d['sources']:assert sha(p['path'])==p['sha256']
for p in d['licensed_sources']:
    raw=Path(p['local_path']).read_bytes()
    assert hashlib.sha256(raw).hexdigest()==p['sha256']
    assert hashlib.sha1(b'blob '+str(len(raw)).encode()+b'\0'+raw).hexdigest()==p['blob_sha1']
    assert p['commit']==d['official_commit']
extract=json.loads((OUT/'EXTRACTION.json').read_text())
assert sha(OUT/'literal_layout_control.cpp')==extract['output_cpp_sha256']
for r in extract['segments']:
    source=Path(r['source_path']).read_bytes()
    assert hashlib.sha256(source).hexdigest()==r['source_sha256']
    raw=source[r['start_byte']:r['end_byte_exclusive']]
    assert raw.decode()==r['exact_text'] and hashlib.sha256(raw).hexdigest()==r['text_sha256']
header=Path(next(p['local_path'] for p in d['licensed_sources'] if p['path'].endswith('/Field.h'))).read_text()
cpp=Path(next(p['local_path'] for p in d['licensed_sources'] if p['path'].endswith('/Field.cpp'))).read_text()
assert 'virtual SIZE_T GetFieldSize() const\n\t{\n\t\treturn sizeof(FField);\n\t}' in header
assert 'return sizeof(TClass);' in header and 'virtual SIZE_T GetFieldSize() const override' in header
field=header[header.index('class FField\n{'):]
assert field.index('virtual SIZE_T GetFieldSize()')<field.index('virtual ~FField()')<field.index('virtual void Serialize(')<field.index('virtual void PostLoad(')<field.index('virtual void Bind(')
assert 'void FField::PostLoad()\n{\n\tBind();\n}' in cpp
assert 'Ar << NamePrivate;\n\tAr << (uint32&)FlagsPrivate;' in cpp
assert 'static FFieldClass StaticFieldClass(TEXT("FField")' in cpp
complete=json.loads((OUT/'COMPLETE-DECLARATION-AUDIT.json').read_text())
assert complete['whole_Field_h_contains_no_pack_pragma']
assert [r['name'] for r in complete['classes']]==['FFieldClass','FThreadSafeCounter','FName','FNameEntryId']
for r in complete['classes']:
    assert r['candidate_data_statements']==complete['expected_data_statements'][r['name']]
    assert not (r['contains_virtual_keyword'] or r['contains_layout_attribute'] or r['contains_pragma_pack']) and r['head_has_no_base_clause']

# Re-read conventional target objects independently, checking exact symbol bytes.
compiled=json.loads((OUT/'COMPILED-CONTROLS.json').read_text())
labels=extract['array_labels'];verified_objects=0
for r in compiled['controls']:
    raw=Path(r['object_path']).read_bytes()
    assert hashlib.sha256(raw).hexdigest()==r['object_sha256'] and raw[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<H',raw,18)[0]==62
    shoff=struct.unpack_from('<Q',raw,40)[0];size,num=struct.unpack_from('<HH',raw,58)
    sections=[struct.unpack_from('<IIQQQQIIQQ',raw,shoff+i*size) for i in range(num)]
    symbols={}
    for s in sections:
        if s[1]!=2:continue
        strings=sections[s[6]];names=raw[strings[4]:strings[4]+strings[5]]
        for f in range(s[4],s[4]+s[5],s[9]):
            n,info,other,index,value,nbytes=struct.unpack_from('<IBBHQQ',raw,f)
            name=names[n:names.index(0,n)].decode()
            if index==0 or index>=len(sections):continue
            section=sections[index];off=section[4]+value-section[3]
            symbols[name]=(off,nbytes,raw[off:off+nbytes])
    for key in ('array_symbol','direct_size_query_body_symbol'):
        record=r[key];off,nbytes,b=symbols[record['name']]
        assert off==record['file_byte_offset'] and nbytes==record['size'] and b.hex()==record['bytes']
    off,nbytes,b=symbols[r['array_symbol']['name']]
    values=dict(zip(labels,struct.unpack('<'+str(len(labels))+'I',b)))
    assert values==r['values'] and r['compile_source_sha256']==extract['output_cpp_sha256']
    body=list(md.disasm(bytes.fromhex(r['direct_size_query_body_symbol']['bytes']),0))
    assert len(body)==2 and body[0].mnemonic=='mov' and body[0].operands[0].reg==X86_REG_EAX and body[0].operands[1].imm==values['sizeof_FField'] and body[1].mnemonic=='ret'
    verified_objects+=1
matched=[r['name'] for r in compiled['controls'] if all(r['values'][k]==v for k,v in d['native_constraints'].items())]
assert matched==d['physically_matching_controls']==['natural','case1_outline1']
assert all(r['values']['sizeof_FField']==0x30 and r['values']['sizeof_FFieldClass']==0x40 for r in compiled['controls'] if r['name'] in matched)
assert sum(not r['matches_all_checked_native_constraints'] for r in d['compiled_config_decisions'])==4
target=json.loads((OUT/'TARGET-IDENTITY.json').read_text())
assert target['exit']==0 and target['output'].strip()=='x86_64-sie-ps5'
assert {r['key']:r['value'] for r in d['facts']}=={'FField.UEP_TotalSize':'0x30','FFieldClass.UEP_TotalSize':'0x40'}
assert all(r['width_bytes'] is None for r in d['facts'])
# The earlier bounded-pass freeze must remain intact.
prior=json.loads((OUT.parent/'SOURCE-HASHES.json').read_text())
assert all(sha(OUT.parent/name)==pin['sha256'] for name,pin in prior['files'].items())
negative=0
sample=d['instruction_evidence']['0x3701a4']
assert not valid_window(dict(sample,instruction_bytes='b831000000'));negative+=1
assert not valid_window(dict(sample,elf_image_vaddr='0x3701a5'));negative+=1
pack=next(r['values'] for r in compiled['controls'] if r['name']=='pack4')
assert [pack[k] for k in ('FFieldClass_SuperClass','FFieldClass_DefaultObject','FFieldClass_ConstructFn','FFieldClass_Counter')]==[0x1c,0x24,0x2c,0x34]
assert pack['FFieldClass_Counter']!=d['native_constraints']['FFieldClass_Counter'];negative+=1
result={'scope':'Offline static source/native/PS5 declaration controls; no engine execution, runtime integration, live sizeof measurement or menu claim.',
        'image_sha256_verified':d['image']['sha256'],'instruction_windows_verified':len(d['instruction_evidence']),
        'root_vtable_unique_actual_RELA_records':13,'same_slot_family_records':6,'distinct_unique_relocation_targets':len(targets),
        'licensed_source_files_sha256_and_Gitblob_verified':len(d['licensed_sources']),
        'literal_source_segments_verified':len(extract['segments']),'complete_class_declarations_audited':4,
        'native_member_geometry_offsets_and_widths_derived':len(stores),'compiled_PS5_objects_verified':verified_objects,
        'physical_configurations_matching_all_constraints':matched,'matching_exact_totals':{'FField':'0x30','FFieldClass':'0x40'},
        'contradictory_config_controls_rejected':4,'negative_native_or_packing_controls':negative,
        'earlier_bounded_R4_receipt_frozen_files_unchanged':True,'failures':0,
        'facts_confidence':'high-confidence validated-static source-contract-correlated inference',
        'facts_status':d['status'],'receipt_sha256':sha(OUT/'OFFICIAL-TOTAL-FACTS.json'),'validator_sha256':sha(__file__)}
(OWN/'REPLAY-VERIFICATION.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result))
