#!/usr/bin/env python3
"""Join actual Epic source contract, native topology, and PS5 declaration controls."""
import hashlib,json,struct
from pathlib import Path
import capstone
ROOT=Path(__file__).resolve().parents[3];OUT=Path(__file__).resolve().parent
ENGINE=ROOT/'outputs/2026-10-07-l19-r4-official-engine-source';DEP=ENGINE/'dependencies'
IMAGE=ROOT/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def pin(p):return {'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size}
image=IMAGE.read_bytes();image_sha=hashlib.sha256(image).hexdigest()
assert image_sha=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
prior=OUT.parent/'NATIVE-PREFIX-CONSUMER-FACTS.json';old=json.loads(prior.read_text())
evidence=dict(old['instruction_evidence']);loads=old['PT_LOAD']
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
def offset(a):
    found=[p['file_offset']+a-p['image_vaddr'] for p in loads if p['image_vaddr']<=a<p['image_vaddr']+p['file_size']]
    assert len(found)==1;return found[0]
def decode_block(lo,hi):
    f=offset(lo)
    for ins in md.disasm(image[f:f+hi-lo],lo):
        evidence[hex(ins.address)]={'elf_image_vaddr':hex(ins.address),'file_byte_offset':hex(offset(ins.address)),
                                   'instruction_bytes':ins.bytes.hex(),'instruction':f'{ins.mnemonic} {ins.op_str}'}
for lo,hi in [(0xb290,0xb296),(0x26030,0x2603b),(0x364700,0x36470c),(0x364710,0x36477e),(0x3639ef,0x3639f2)]:decode_block(lo,hi)
for a in [0x364b8a,0x364b94,0x364ba6,0x364baa,0x364bc7]:
    f=offset(a);ins=next(md.disasm(image[f:f+15],a,count=1))
    evidence[hex(a)]={'elf_image_vaddr':hex(a),'file_byte_offset':hex(f),'instruction_bytes':ins.bytes.hex(),'instruction':f'{ins.mnemonic} {ins.op_str}'}
root=0x7edc650
slots=[(0,'GetFieldSize'),(8,'complete destructor'),(0x10,'deleting destructor'),(0x18,'Serialize'),(0x20,'PostLoad'),
       (0x28,'GetPreloadDependencies'),(0x30,'BeginDestroy'),(0x38,'AddReferencedObjects'),(0x40,'AddCppProperty'),
       (0x48,'Bind'),(0x50,'PostDuplicate'),(0x58,'GetInnerFieldByName'),(0x60,'GetInnerFields')]
found={slot:[] for slot,name in slots}
for f in range(0x932f430,0x932f430+39878376,24):
    target,info,addend=struct.unpack_from('<QQq',image,f)
    if target-root in found:
        found[target-root].append({'slot_byte_offset':hex(target-root),'r_offset':hex(target),'r_info':hex(info),'r_addend':hex(addend),
                                  'file_byte_offset':hex(f),'entry_bytes':image[f:f+24].hex()})
table=[]
for slot,name in slots:
    assert len(found[slot])==1 and found[slot][0]['r_info']=='0x8'
    record=found[slot][0];record['official_method_identity']=name
    record['identity_kind']='source-order correlation; native relocation/function independently checked'
    table.append(record)
family=ROOT/'outputs/2026-10-07-l19-remaining-total-evidence/SIZE-QUERY-FAMILY.json'
family_data=json.loads(family.read_text())
for rec in family_data['records']:
    for ins in rec['function_instructions']:
        a=int(ins['vaddr'],16)
        evidence[hex(a)]={'elf_image_vaddr':hex(a),'file_byte_offset':hex(offset(a)),
                          'instruction_bytes':ins['bytes'],'instruction':ins['instruction']}
    f=int(rec['relocation_file_offset'],16);assert image[f:f+24].hex()==rec['entry_bytes']
    matches=0;target=int(rec['vptr'],16)
    for loc in range(0x932f430,0x932f430+39878376,24):
        t,info,addend=struct.unpack_from('<QQq',image,loc)
        if t==target:matches+=1
    assert matches==1

native={
 'sizeof_FField':0x30,'sizeof_FName':8,'sizeof_FFieldVariant':8,'sizeof_SIZE_T':8,'sizeof_EClassFlags':4,'sizeof_EObjectFlags':4,
 'FField_ClassPrivate':8,'FField_Owner':0x10,'FField_Next':0x18,'FField_NamePrivate':0x20,'FField_FlagsPrivate':0x28,
 'FFieldClass_Name':0,'FFieldClass_Id':8,'FFieldClass_CastFlags':0x10,'FFieldClass_ClassFlags':0x18,
 'FFieldClass_SuperClass':0x20,'FFieldClass_DefaultObject':0x28,'FFieldClass_ConstructFn':0x30,'FFieldClass_Counter':0x38}
controls=json.loads((OUT/'COMPILED-CONTROLS.json').read_text())
matches=[];decisions=[]
for control in controls['controls']:
    mismatches={k:{'native':v,'compiled':control['values'][k]} for k,v in native.items() if control['values'][k]!=v}
    if not mismatches:matches.append(control['name'])
    decisions.append({'control':control['name'],'matches_all_checked_native_constraints':not mismatches,'mismatches':mismatches,
                      'compiled_FField_total':hex(control['values']['sizeof_FField']),
                      'compiled_FFieldClass_total':hex(control['values']['sizeof_FFieldClass'])})
assert matches==['natural','case1_outline1']
assert all(c['values']['sizeof_FFieldClass']==0x40 and c['values']['sizeof_FField']==0x30 for c in controls['controls'] if c['name'] in matches)
source_receipts=[ENGINE/'SOURCE-RECEIPT.json',DEP/'SOURCE-RECEIPT.json',DEP/'MACRO-SOURCE-RECEIPT.json']
licensed=[]
for path in source_receipts:
    for s in json.loads(path.read_text())['files']:
        p=Path(s['local_path']);assert sha(p)==s['sha256']
        assert hashlib.sha1(b'blob '+str(p.stat().st_size).encode()+b'\0'+p.read_bytes()).hexdigest()==s['blob_sha1']
        licensed.append(s)
source_files=[prior,OUT.parent/'SOURCE-HASHES.json',family,
              ROOT/'outputs/2026-10-07-l19-class-size-evidence/CLASS-SIZE-FACTS.json',
              ROOT/'outputs/2026-10-07-l19-property-subtype-evidence/SUBTYPE-FACTS.json',
              ROOT/'outputs/2026-10-07-l19-unresolved-evidence/FIELDC-FACTS.json',
              ROOT/'outputs/2026-10-07-l19-static-layout-check/REFLECTION-FACTS.json',
              OUT/'EXTRACTION.json',OUT/'literal_layout_control.cpp',OUT/'COMPILE-COMMANDS-FINAL.json',OUT/'COMPILED-CONTROLS.json',OUT/'TARGET-IDENTITY.json',OUT/'COMPLETE-DECLARATION-AUDIT.json']+source_receipts
facts=[
 {'key':'FField.UEP_TotalSize','value':'0x30','width_bytes':None,'kind':'exact native class total',
  'status':'validated-static','live_validation':False,
  'confidence':'high-confidence validated-static source-contract-correlated inference',
  'evidence_keys':['0x3645c4','0x3645cf','0x3701a4','0x36471d','0x364731','0x36474c','0x364707'],
  'inference_chain':['Named FField registration and constructors identify root native vptr7edc650.',
                     'Unique actual DT_RELA slot0 selects3701a0;3701a4 returns zero-extended30.',
                     'Licensed pinned5.6.1 header declares GetFieldSize first, with return sizeof(FField); property declaration macro overrides with sizeof(TClass).',
                     'Independent destructor/Serialize/PostLoad->Bind native slots corroborate the full official root virtual order, not only a free-standing constant.',
                     'Five named property subtype slot0 constants independently equal their proved physical allocations.',
                     'Exact actual-source declaration/dependency projection compiled for x86_64-sie-ps5: both physically matching configurations yield sizeofFField30; metadata1/pack4/name controls contradict native data or query.'],
  'source_body_span':'Field.h:468-471', 'measurement_kind':'native return constant interpreted by independently corroborated official source contract; not a new allocation/live sizeof observation'},
 {'key':'FFieldClass.UEP_TotalSize','value':'0x40','width_bytes':None,'kind':'exact native class total',
  'status':'validated-static','live_validation':False,
  'confidence':'high-confidence validated-static source-contract-correlated inference',
  'evidence_keys':['0x3639ef','0x3639b7','0x3639bb','0x3639bf','0x3639c6','0x3639ca','0x3639d2','0x3639d6'],
  'inference_chain':['Pinned complete official FFieldClass declaration has no base/virtual destructor/conditional data/alignas and exactly eight fields; actual UE_NONCOPYABLE macro declares only four deleted methods.',
                     'Every field of that complete declaration independently matches native constructor offset/width: Name0/8,Id8/8,Cast10/8,ClassFlags18/4,Super20/8,Default28/8,Construct30/8,Counter38/4.',
                     'Literal source-defined FName/Id/counter/flags/pointer dependencies compiled with PS5 compiler; all tested configurations matching native scalar sizes/member offsets/root size-query produce FFieldClass40.',
                     'Packing4 produces Super1C/Default24/Construct2C/Counter34 and total38, contradicting measured target offsets; case-preserving inline Name12 also contradicts target. Natural and case-preserving+outlined Name8 both yield exact40.',
                     'This uses the source complete-class contract plus target constructor geometry and target-compiler layout, not rounding a raw member extent or the guard gap alone. Guard placement40 is only corroboration.'],
  'source_declaration_span':'Field.h:64-184; dependency data and macro spans in EXTRACTION.json',
  'measurement_kind':'complete source declaration/target geometry/target compiler inference; no native heap allocation operand or direct live sizeof measurement'}]
result={'format':'l19-r4-official-source-native-totals-v1','status':'review_pending',
        'scope':'Two exact native totals proposed for promotion after independent semantic review. Static saved-image + pinned official source contract + actual PS5 compile controls. No runtime integration or console observation.',
        'image':{'path':str(IMAGE),'bytes':len(image),'sha256':image_sha},'PT_LOAD':loads,
        'official_commit':'6978b63c8951e57d97048d8424a0bebd637dde1d','official_release':'5.6.1-release',
        'facts':facts,'instruction_evidence':evidence,'data_evidence':old['data_evidence'],
        'root_vtable_relocation_evidence':table,'same_slot_size_family':family_data['records'],
        'licensed_sources':licensed,'sources':[pin(p) for p in source_files],
        'native_constraints':native,'compiled_config_decisions':decisions,'physically_matching_controls':matches,
        'configuration_conclusions':{'WITH_METADATA':'false under correlated official source contract; native complete destructor no-op and Serialize lacks metadata branch; size-query30 rejects metadata1 size38.',
                                     'WITH_EDITORONLY_DATA':'not independently resolved by this proof; declaration affects methods/delegates, not the projected physical data members. Do not substitute it for WITH_METADATA.',
                                     'FName_flag_uniqueness':'not claimed from these layouts: natural noncase-inline and case-preserving-outlined both produce physical Name8 and totals30/40. Existing independent FName profile evidence remains separate.'},
        'assumptions_and_limits':['Exact game engine source commit is not proved identical to official5.6.1-release. Mapping is a high-confidence contract correlation backed by named registration, every data field, source constructor/factory behavior, root virtual topology and five independent subtype allocations.',
                                  'Projection omits non-layout methods, friends, static members and inaccessible full engine dependencies; source data declarations/order/conditional member, first virtual size body, destructor and actual noncopyable macro are preserved literally. No hand-authored native-size mirror is used.',
                                  'Full proprietary PS5 platform compiler headers/config were not reproduced. Compiler targetx86_64-sie-ps5 and physical scalar/layout constraints are independently checked; explicit nonstandard per-class packing, alignment overrides, or native added fields outside the correlated declaration would invalidate the source-contract inference.',
                                  'No assumption that qword accesses or an aligned address alone imply class alignment. Target actual data geometry is matched to complete source declaration under empirical compiler controls.',
                                  'Guard-object boundary is not the proof of size. No need to identify __cxa imports or infer nonoverlap for the primary complete-declaration proof.',
                                  'Totals do not approve current FFieldVariant wrapper behavior; native tagged8/public legacy16 ABI correction remains separately reviewed.',
                                  'Original frozen unknown/prefix receipt and R3 selection/evidence remain unchanged. New facts are pending independent audit; no source/profile/checkpoint mutation or runtime/menu readiness claim.']}
(OUT/'OFFICIAL-TOTAL-FACTS.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'facts':2,'values':{'FField':'0x30','FFieldClass':'0x40'},'status':result['status'],
                  'instruction_windows':len(evidence),'root_relocations':len(table),'family_records':len(family_data['records']),
                  'licensed_files_verified':len(licensed),'physically_matching_controls':matches}))
