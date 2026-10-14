#!/usr/bin/env python3
"""Read-only evidence validator and proposal arithmetic controls, not a runtime test."""
import hashlib
import json
import struct
from pathlib import Path
import capstone

OUT = Path(__file__).resolve().parent
def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

data=json.loads((OUT/'NATIVE-PREFIX-CONSUMER-FACTS.json').read_text())
census=json.loads((OUT/'SOURCE-CONSUMER-CENSUS.json').read_text())
assert sha(data['image']['path'])==data['image']['sha256']
image=Path(data['image']['path']).read_bytes()
assert len(image)==data['image']['bytes']
phoff=struct.unpack_from('<Q',image,32)[0]
phsize,phnum=struct.unpack_from('<HH',image,54)
loads=[]
for i in range(phnum):
    p=struct.unpack_from('<IIQQQQQQ',image,phoff+i*phsize)
    if p[0]==1:loads.append({'file_offset':p[2],'image_vaddr':p[3],'file_size':p[5],'memory_size':p[6],'flags':p[1]})
assert loads==data['PT_LOAD']
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
md.detail=True
def valid_window(record):
    address=int(record['elf_image_vaddr'],16)
    raw=bytes.fromhex(record['instruction_bytes'])
    candidates=[p['file_offset']+address-p['image_vaddr'] for p in loads
                if p['image_vaddr']<=address and address+len(raw)<=p['image_vaddr']+p['file_size']]
    if len(candidates)!=1 or candidates[0]!=int(record['file_byte_offset'],16):return False
    if image[candidates[0]:candidates[0]+len(raw)]!=raw:return False
    ins=list(md.disasm(raw,address))
    return len(ins)==1 and ins[0].size==len(raw) and f'{ins[0].mnemonic} {ins[0].op_str}'.strip()==record['instruction'].strip()
assert all(valid_window(r) for r in data['instruction_evidence'].values())
def instruction(key):
    r=data['instruction_evidence'][key]
    return next(md.disasm(bytes.fromhex(r['instruction_bytes']),int(r['elf_image_vaddr'],16)))
from capstone.x86 import X86_OP_MEM,X86_OP_REG,X86_OP_IMM,X86_REG_RDI,X86_REG_RBX,X86_REG_RAX,X86_REG_RIP
def written_end(key,base,disp,size):
    operand=instruction(key).operands[0]
    assert operand.type==X86_OP_MEM and operand.mem.base==base and operand.mem.disp==disp and operand.size==size
    return disp+size
field_end=written_end('0x3645db',X86_REG_RDI,0x1c,16)
assert field_end==written_end('0x364651',X86_REG_RDI,0x28,4)==0x2c
fieldclass_end=written_end('0x3639d6',X86_REG_RDI,0x38,4)
assert fieldclass_end==0x3c
assert {r['class']:r['minimum_occupied_bytes'] for r in data['verified_prefixes']}=={'FField':field_end,'FFieldClass':fieldclass_end}
def lea_target(key,dst):
    ins=instruction(key);a,b=ins.operands
    assert ins.mnemonic=='lea' and a.type==X86_OP_REG and a.reg==dst and b.type==X86_OP_MEM and b.mem.base==X86_REG_RIP
    return ins.address+ins.size+b.mem.disp
assert lea_target('0x3645c4',X86_REG_RAX)==0x7edc650
assert lea_target('0x364559',X86_REG_RBX)==0x939ec98
this_move=instruction('0x36457b')
assert this_move.mnemonic=='mov' and this_move.operands[0].reg==X86_REG_RDI and this_move.operands[1].reg==X86_REG_RBX
ctor_call=instruction('0x36457e')
assert ctor_call.mnemonic=='call' and ctor_call.operands[0].type==X86_OP_IMM and ctor_call.operands[0].imm==0x363990
for r in data['data_evidence']:
    address=int(r['elf_image_vaddr'],16);f=int(r['file_byte_offset'],16);raw=bytes.fromhex(r['bytes'])
    assert any(f==p['file_offset']+address-p['image_vaddr'] and p['image_vaddr']<=address and address+len(raw)<=p['image_vaddr']+p['file_size'] for p in loads)
    assert image[f:f+len(raw)]==raw and raw.decode('utf-16le')==r['expected_utf16le']
for r in data['file_data_evidence']:
    f=r['file_byte_offset'];raw=bytes.fromhex(r['bytes'])
    assert image[f:f+len(raw)]==raw
    assert struct.unpack_from('<Q',raw,40)[0]==r['section_header_offset']
    assert struct.unpack_from('<H',raw,58)[0]==r['section_header_entry_size']
    assert struct.unpack_from('<H',raw,60)[0]==r['section_header_count']
    assert r['section_header_offset']+r['section_header_entry_size']*r['section_header_count']>len(image)
for s in census['source_files']+data['sources']:assert sha(s['path'])==s['sha256']
selection=json.loads(Path(census['selected_R3_source_manifest']['path']).read_text())
assert sha(census['selected_R3_source_manifest']['path'])==census['selected_R3_source_manifest']['sha256']
r3=Path(census['selected_R3_source_manifest']['path']).parents[1]
assert all(sha(r3/p)==h for p,h in selection['source_files'].items())
assert len(selection['source_files'])==census['selected_R3_source_pins_verified']==75

# Re-run lexical census against every pinned file, not merely its saved hit count.
import re
patterns={k:re.compile(v) for k,v in census['patterns'].items()}
replayed=[]
for src in census['source_files']:
    for line,text in enumerate(Path(src['path']).read_text(errors='replace').splitlines(),1):
        names=[k for k,p in patterns.items() if p.search(text)]
        if names:replayed.append({'path':src['path'],'line':line,'patterns':names,'text':text})
assert replayed==census['hits']
assert len(census['generated_total_getter_definitions'])==41
for r in census['generated_total_getter_definitions']:
    assert r['returns_cached_reference'] and r['missing_key_returns_negative_one'] and not r['checked_throw_in_body']
assert data['exact_total_facts']==[]
assert {r['key']:r['status'] for r in data['unresolved']}=={'FField.UEP_TotalSize':'unknown','FFieldClass.UEP_TotalSize':'unknown'}

# Independently verify the saved target object's compiled accessor widths.
width_source=next(s for s in data['sources'] if s['path'].endswith('/PUBLIC-WIDTHS.json'))
widths=json.loads(Path(width_source['path']).read_text())
obj=Path(widths['object_path']).read_bytes()
assert hashlib.sha256(obj).hexdigest()==widths['object_sha256']
raw=bytes.fromhex(widths['exact_array_bytes'])
assert len(raw)==widths['symbol_size_bytes']==216
assert obj[widths['file_byte_offset']:widths['file_byte_offset']+len(raw)]==raw
values=struct.unpack('<54I',raw)
assert values==tuple(r['selected_public_width_bytes'] for r in widths['rows'])
width_by_name={r['name']:r['selected_public_width_bytes'] for r in widths['rows']}
def within_prefix(offset,width,bound):
    return isinstance(offset,int) and isinstance(width,int) and offset>=0 and width>0 and offset<bound and width<=bound-offset
spans=data['required_R3_accessor_span_checks']
for r in spans:
    assert r['R3_selected_public_accessor_width_bytes']==width_by_name[r['name']]
    assert r['access_end_exclusive']==r['offset']+r['R3_selected_public_accessor_width_bytes']
    assert within_prefix(r['offset'],r['R3_selected_public_accessor_width_bytes'],r['constructor_occupied_prefix_minimum_bytes'])
assert len(spans)==7
assert next(r for r in spans if r['name']=='FField::Owner')['native_field_width_bytes']==8
assert next(r for r in spans if r['name']=='FField::Owner')['R3_selected_public_accessor_width_bytes']==16

negative_arithmetic=0
for r in spans:
    off,width,bound=r['offset'],r['R3_selected_public_accessor_width_bytes'],r['constructor_occupied_prefix_minimum_bytes']
    for bad_off,bad_width in [(off,bound-off+1),(bound,width),(-1,width),(off,0)]:
        assert not within_prefix(bad_off,bad_width,bound);negative_arithmetic+=1
# Proposal checked exact-total query: unknown total cannot become unsigned allocation size.
# This is a model control only; current generated getters still return cached-1.
def checked_exact_total(name,exact_totals):
    value=exact_totals.get(name)
    if value is None or not isinstance(value,int) or value<=0:raise ValueError('Exact native total unavailable')
    return value
exact={'FProperty':0x70}
assert checked_exact_total('FProperty',exact)==0x70
negative_total_queries=0
for cls in ('FField','FFieldClass'):
    try:checked_exact_total(cls,exact)
    except ValueError:negative_total_queries+=1
    else:raise AssertionError('Unknown total accepted')
for value in (None,-1,0):
    try:checked_exact_total('Control',{'Control':value})
    except ValueError:negative_total_queries+=1
    else:raise AssertionError('Invalid total accepted')
sample=dict(data['instruction_evidence']['0x364651'])
bad=dict(sample,instruction_bytes='00'+sample['instruction_bytes'][2:])
assert not valid_window(bad)
bad=dict(sample,elf_image_vaddr=hex(int(sample['elf_image_vaddr'],16)+1))
assert not valid_window(bad)
result={'scope':'Offline evidence validation plus proposal-only arithmetic/checked-query model controls. No engine method execution, runtime binding change, map publication, native probe, or menu/spawn proof.',
        'image_sha256_verified':data['image']['sha256'],'PT_LOAD_tables_verified':1,
        'single_instruction_exact_byte_windows':len(data['instruction_evidence']),
        'native_constructor_prefix_derivations_checked':2,'named_UTF16_data_windows_verified':len(data['data_evidence']),
        'ELF_header_file_windows_verified':len(data['file_data_evidence']),
        'source_census_files_hashed_and_replayed':len(census['source_files']),
        'R3_selected_source_pins_verified':len(selection['source_files']),
        'generated_total_getters_checked':41,'target_compiled_widths_verified':len(values),
        'positive_required_accessor_span_controls':len(spans),'negative_prefix_arithmetic_controls':negative_arithmetic,
        'positive_known_exact_total_model_controls':1,'negative_unknown_or_invalid_total_model_controls':negative_total_queries,
        'negative_evidence_byte_or_address_controls':2,'failures':0,'promoted_exact_totals':0,
        'current_generated_unknown_total_getter_throws':False,
        'proposed_prefix_contract_implemented':False,
        'validator_source_sha256':sha(__file__),
        'input_receipt_sha256':sha(OUT/'NATIVE-PREFIX-CONSUMER-FACTS.json'),
        'input_census_sha256':sha(OUT/'SOURCE-CONSUMER-CENSUS.json')}
(OUT/'VERIFICATION.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result))
