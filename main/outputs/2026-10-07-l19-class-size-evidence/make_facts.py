#!/usr/bin/env python3
"""Seal bounded named-registration/allocation evidence; no source/game writes."""
from pathlib import Path
import hashlib,json,re,struct
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OUT=ROOT/'outputs/2026-10-07-l19-class-size-evidence'
IMG=ROOT/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
D=IMG.read_bytes();H=hashlib.sha256(D).hexdigest()
assert H=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
phoff=struct.unpack_from('<Q',D,32)[0];ents,num=struct.unpack_from('<HH',D,54)
segs=[struct.unpack_from('<IIQQQQQQ',D,phoff+i*ents) for i in range(num)]
loads=[s for s in segs if s[0]==1]
def off(v):
 return next(s[2]+v-s[3] for s in loads if s[3]<=v<s[3]+s[5])
E={};DATA={};CALLS={}
def ev(file,*addresses):
 rows={}
 for line in (OUT/file).read_text().splitlines():
  m=re.match(r'^0x[0-9a-f]+  \(f (0x[0-9a-f]+)\)  ([0-9a-f]+) +(.+)$',line)
  if m:rows[int(m[1],16)]=(m[2],m[3])
 keys=[]
 for a in addresses:
  v=int(a,16);b,ins=rows[v];raw=bytes.fromhex(b)
  assert D[off(v):off(v)+len(raw)]==raw,(file,a)
  key=hex(v);record={'elf_image_vaddr':key,'file_byte_offset':hex(off(v)),
    'instruction_bytes':b,'instruction':ins,'disassembly_artifact':file}
  if key in E:assert E[key]['instruction_bytes']==b
  E[key]=record;keys.append(key)
  if raw[0]==0xe8:
   target=v+5+struct.unpack_from('<i',raw,1)[0]
   CALLS[key]={'elf_image_vaddr':key,'rel32_bytes':b,'callee_elf_image_vaddr':hex(target)}
 return keys
def string(key,address,text):
 v=int(address,16);raw=(text+'\0').encode('utf-16le')
 assert D[off(v):off(v)+len(raw)]==raw
 DATA[key]={'elf_image_vaddr':hex(v),'file_byte_offset':hex(off(v)),
            'bytes':raw.hex(),'encoding':'UTF16LE','value':text}
 return key
for key,address,text in [('name_Object','79ec802','Object'),('name_Field','77a9ef0','Field'),
 ('name_Struct','78ccca0','Struct'),('name_Class','76ae74e','Class'),
 ('name_Function','7755232','Function'),('name_FProperty','758e17e','FProperty'),
 ('name_FField','777d7c2','FField')]:string(key,address,text)

ALLOC=ev('StaticAllocateObjectInternal.asm','490fd2','491038','49112a','49112f','491141','491150','49116c')
ALLOC_RESULT=ev('StaticAllocateObjectInternal.asm','491171','49122e','49125e')
ALLOC_BASE=ev('UObjectAllocateBaseInit.asm','4916d0','4916e5','49174f','491752','491760')
ALLOC_BASE_CTOR=ev('UObjectAllocatedBaseCtor.asm','485445','485455','485458','48545b','48545e','485465','48548a','485492')
RAWALLOC=ev('UObjectAllocateRaw.asm','480d84','480d87','480d89')
MALLOC=ev('FMemoryMalloc-confirmed.asm','b7a31','b7a34','b7a4c','b7a4f','b7a52')
HELPER=ev('GetPrivateStaticClassBody.asm','30d87c','30d88b','30d88e','30d893','30d89d','30d8a0',
 '30d8aa','30d8b3','30d8be','30d904','30d909','30da63')
F=[]
def fact(key,value,width,keys,data,chain,kind='direct-named-registration',confidence='high'):
 F.append({'key':key,'value':value,'width_bytes':width,'status':'validated-static',
   'confidence':confidence,'derivation_kind':kind,'evidence_keys':keys,'data_evidence_keys':data,
   'inference':chain,'live_validation':False})

OBJECT=ev('UObjectPrivateClass.asm','3c5bfe','3c5c05','3c5c13','3c5c19','3c5c3b')
BASE=ev('UObjectBaseFlagsCtor.asm','485404','48540f','485412','485415','48541c','485421')
fact('UObjectBase.UEP_TotalSize','0x28',None,OBJECT+HELPER+ALLOC+ALLOC_RESULT+ALLOC_BASE+ALLOC_BASE_CTOR+BASE,['name_Object'],
 'Named Object registration calls common helper30d860 with R8D28 and alignmentR9D8. Helper savesR8D inR13D and writes it unchanged to the registered UClass PropertiesSize58; native object allocation reads58 and passes it as bytecount to imageb7a20. This independently establishes complete native UObject storage28. The malloc return RAX jumps491171 to49122e (RBX=RAX), then49125e to4916c5;4916d0 copies RBX unchanged toR12 and49174f passes it asRDI tofundamental-base constructor485430 at491760. That constructor installs the same fundamental-base vptr7eff620 as485400 and writes vptr0, flags8, indexC, class10, name18 and qwordOuter20. Thus the target fundamental base is explicitly contained at the complete-object allocation start; its occupied lowerbound28 and independently registered complete-object upperbound28 plus alignment8 uniquely give base physical total28. This is a two-sided bound with target this-flow, not direct registration under the name UObjectBase or a largest-member/template promotion.',
 'two-sided-native-object-registration-and-base-constructor')

UFIELD=ev('UFieldRegistration.asm','31a53b','31a54e','31a560','31a590','31a731','31a73b')
UFIELD_PARENT=ev('UStructPrivateClass.asm','320cfd','320d04','320d12','320d18','320d3a')
fact('UField.UEP_TotalSize','0x30',None,UFIELD+UFIELD_PARENT+HELPER+ALLOC,['name_Field'],
 'Named Field descriptor creation stores native size30 toClass.PropertiesSize58 at31a590. Independently, named Struct registration selects parentField at77a9ef0 and passesR8D30/R9D8 to commonhelper30d860; helper writes this exact size toPropertiesSize58. Actual StaticAllocateObjectInternal uses that field as the byte allocation request. Thus30 is the named native Field allocation/registration size, not an inferred child-link envelope.')

USTRUCT=ev('UStructPrivateClass.asm','320b38','320b4b','320b5a','320b98','320d9e','320da8')
USTRUCT_PARENT=ev('UFunctionPrivateClass.asm','3286ca','3286d1','3286df','3286e5','3286fd')
fact('UStruct.UEP_TotalSize','0xb0',None,USTRUCT+USTRUCT_PARENT+HELPER+ALLOC,['name_Struct'],
 'Named Struct descriptor storesB0 toClass.PropertiesSize58 at320b98. Named Function registration independently selects its parentStruct string78ccca0 and passes R8DB0/R9D8 tocommonhelper30d860; this becomesClass.PropertiesSize58 and controls native byte allocation. Exact named-registration sizeB0 is independently proved.')

UFUNCTION=ev('UFunctionPrivateClass.asm','3284fa','32850d','32851c','328553','32855a','328763','32876d')
fact('UFunction.UEP_TotalSize','0xe0',None,UFUNCTION+ALLOC,['name_Function'],
 'Private native Function registration constructs the named descriptor with Function7755232 (same string passed to finalnamednativeclass registration at328763/32876d), setsClass.PropertiesSize58=E0 andMinAlignment5C=8. StaticAllocateObjectInternal reads the registeredPropertiesSize58 as its exact native allocation bytecount. This closes prior E0 lowerbound ambiguity using registration metadata, not FuncD8+8 alone.')

UCLASS=ev('UClassPrivateClass.asm','3264f6','3264fb','326505','32650a','32651d','32656a','326571','326779','326783')
fact('UClass.UEP_TotalSize','0x200',None,UCLASS+RAWALLOC+MALLOC+ALLOC,['name_Class'],
 'Named native Class descriptor creation storesClass.PropertiesSize58=200 at32656a andalignment8. The verysame Class descriptor allocation independently requestsESI200 through480d70 at3264f6/326505; rawhelper transfersESI bytecount toRDI and calls imageb7a20. NamedClass registration and actualUClass allocation thus independently agree on200 exactnativebytes.')

FPROP=ev('FPropertyStaticClass.asm','3e1d05','3e1d0c','3e1d13','3e1d18','3e1d1d','3e1d23')
FIELDC_CONSTRUCTFN=ev('FFieldClassCtor.asm','3639a5','3639b7','3639d2','3639e6','3639ef')
FPROP_ALLOC=ev('FPropertyConstruct.asm','3e1b6e','3e1b7f','3e1b8b','3e1b96','3e1ba2','3e1ba7','3e1bae','3e1bb1','3e1c17','3e1c4b')
fact('FProperty.UEP_TotalSize','0x70',None,FPROP+FIELDC_CONSTRUCTFN+FPROP_ALLOC+MALLOC,['name_FProperty'],
 'The namedFProperty StaticClass getter passesFProperty758e17e andR9=ConstructFn3e1b50 intoFFieldClass constructor363990. Its registeredConstructFn requests70 bytes directly fromimageb7a20, keeps the returned unadjusted object pointer inRBX, initializesFField with thisnamedclass descriptor and sets finalFProperty vptr7eea438. It returns that same pointer. This is the exact allocation for the explicitly namednativeFProperty construction path, not a last-member-size estimate.',
 'named-registered-constructor-allocation')

COPY=ev('FPropertyCopyCandidate.asm','3e2470','3e2479','3e2482','3e2487','3e248f','3e2493','3e2496','3e2499','3e249c')
COPY_ARRAY=ev('FPropertyCopyComplete.asm','3e24b9','3e24bf','3e24c2','3e24c5','3e24cd','3e24d1','3e24db','3e24df','3e24e2')
fact('FProperty.CopyValuesInternal.vtable_byte_offset','0xf8',None,COPY+COPY_ARRAY,['name_FProperty'],
 'Nativeproperty copy wrappers receive selfRDI, DestRSI, SrcRDX. ForPOD (PropertyFlags38 bit30 testedasbyte3B&40), they memcpyElementSize30*Count. Otherwise sameDest/Src arepassedunchanged tovirtualF8 withCountECX=1 in3e2470 orArrayDim2C in3e24b0. This uniquely identifies the propertyvalue copyinternal virtualslotF8 by signature and paired PODbehavior, without a platformslotshift template.',
 'native-wrapper-signature-and-behavior')

DESTROY=ev('FPropertyDestroyValue.asm','3e2e60','3e2e6b','3e2e71','3e2e77','3e2e80','3e2e83')
fact('FProperty.DestroyValueInternal.vtable_byte_offset','0x140',None,DESTROY,['name_FProperty'],
 'Native destroy/free wrapper receives selfRDI andvalueRSI; whenCPF_NoDestructor (PropertyFlags38 bit36, byte3C&10) isclear it callsreceiver virtual140 onthevalue, then passesvalue toimageb7a90 free. This independently corroborates ProcessEvent destructor-chain call140. The conditional destructor-before-free behavior uniquely identifies DestroyValueInternal.',
 'native-destruction-before-free')

ALIGN=ev('FPropertyAllocateValue.asm','3e2e17','3e2e1b','3e2e22','3e2e26','3e2e29','3e2e2f','3e2e32','3e2e34')
ALLOC_ALIGNMENT=ev('MallocZeroed.asm','bf061','bf064','bf07c','bf07f','bf082')
CONSTALIGN=ev('FPropertyBaseAlignment.asm','5b4','5ba')
reloc=read_reloc=json.loads((OUT/'PROPERTY-VTABLE-RELA.json').read_text())
r=next(r for r in reloc['records'] if r['r_offset']=='0x7eea5a8')
assert r['addend']=='0x5b0' and r['r_info']=='0x8'
ro=int(r['relocation_file_offset'],16);assert D[ro:ro+24].hex()==r['entry_bytes']
DATA['FProperty_alignment_slot_RELA']={**r,'vptr_elf_image_vaddr':'0x7eea438','slot_byte_offset':'0x170'}
fact('FProperty.GetMinAlignment.vtable_byte_offset','0x170',None,ALIGN+ALLOC_ALIGNMENT+CONSTALIGN,
 ['name_FProperty','FProperty_alignment_slot_RELA'],
 'Native property allocation computesbytecount=ArrayDim2C*ElementSize30, callsreceiver virtual170, passesreturnedEAX asESI alongside bytecountRDI intoallocationhelperbf050. Thathelper forwards bytecountRSI/alignmentEDX toallocatorvirtual58. Independently the namedFProperty vptr7eea438 RELATIVE relocation at+170 points toimage5b0, which returns1 (the baseproperty minimumalignment). Callresult-as-allocation-alignment plus namedvtable binding uniquely establishes GetMinAlignment170.',
 'native-allocation-alignment-flow-and-named-vtable')

for a,target in [('3e1b7f','b7a20'),('480d89','b7a20'),('49116c','b7a20'),('3e2e34','bf050'),('491760','485430')]:
 assert CALLS[hex(int(a,16))]['callee_elf_image_vaddr']==hex(int(target,16))
assert len(F)==9
U=[{'key':'FField.UEP_TotalSize','status':'unresolved','candidate':'0x30',
     'reason':'Named FFieldClass registration uses ConstructFn=e0 empty stub and has no native sizeof argument. FField constructor fields end2C and alignment can suggest30, but no named FField direct allocation/size record recovered. Not promoted.'},
   {'key':'FFieldClass.UEP_TotalSize','status':'unresolved','candidate':'0x40',
     'reason':'Class-descriptor constructor writes through38/4 and named static instance guard is+40. These establish an extent/spacing candidate, not an independent exact sizeof argument or allocation. Descriptor +8 is cast-id (e.g.FProperty8000), not size. Not promoted.'},
   {'key':'UEngine.LoadMap.vtable_byte_offset','status':'unresolved',
     'reason':'Many Engine name references; no uniquely identified LoadMap native behavior and receiver vtable binding recovered in this bounded pass. No blanket UObject shift used.'},
   {'key':'UEngine.Tick.vtable_byte_offset','status':'unresolved',
     'reason':'No uniquely identified UEngine tick function/receiver vtable binding recovered in the bounded existing-report and named-registration pass.'},
   {'key':'AGameModeBase.InitGameState.vtable_byte_offset','status':'unresolved',
     'reason':'GameModeBase name references recorded but no uniquely identified InitGameState native behavior and exact receiver slot recovered in the bounded pass.'}]
J={'format':'townfall-class-size-and-property-vtable-static-v1',
 'scope':'LOCAL ONLY bounded saved-authenticated-image evidence. No console/network/contact/upload/game execution, source edits, frozen-input/report-summary edits.',
 'image':{'path':str(IMG),'sha256':H,'size_bytes':len(D)},
 'address_model':{'all_evidence_addresses':'ELF image virtual addresses; file mapping per PT_LOAD',
   'rdis_display_prefix':'instruction display and call operands add observed base400000; (f ...) and normalized rel32 call targets are authoritative',
   'rejected_capture':'FMemoryMalloc.asm was captured at mistakenly base-added4b7a20, starts misaligned and is diagnostic only; FMemoryMalloc-confirmed.asm correctly starts ELFb7a20',
   'footer_warning':'rdis calls footer double-adds base; ignored'},
 'facts':F,'unresolved':U,'evidence':E,'data_evidence':DATA,'direct_call_targets':CALLS,
 'common_helper_semantics':{'entry':'0x30d860','native_class_name_argument':'RSI UTF16 string',
   'size_argument':'R8D savedR13D ->Class.PropertiesSize58',
   'alignment_argument':'R9D savedR15D ->Class.MinAlignment5C',
   'allocation_proof':'StaticAllocateObjectInternal ELF49112A reads58 ->491141 RDIbytecount ->49116C callsELFb7a20'},
 'independent_related_evidence':{'UClass_classcast_CDO':str(ROOT/'outputs/2026-10-07-l19-unresolved-evidence/UCLASS-FACTS.json'),
   'FFieldClass_members':str(ROOT/'outputs/2026-10-07-l19-unresolved-evidence/FIELDC-FACTS.json')},
 'sources':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(OUT.glob('*.asm'))},
 'validation':{'all_instruction_windows_byte_match':True,'instruction_windows':len(E),
   'all_UTF16_name_windows_byte_match':True,'normalized_allocator_rel32_calls_match':True,
   'alignment_RELA_entry_byte_match':True,'positive_facts':len(F),'unknown_not_guessed':len(U)},
 'limits':['Exact totals describe identified native registration/construction paths; no fresh live object validation.',
   'UObjectBase28 uses explicit complete Object size plus occupied-base bound; it is separately labeled as a two-sided inference.',
   'FField/FFieldClass candidates are not promoted. No source typedef/template width or slot shift constitutes target proof.',
   'Prior static/summary/profile outputs remain immutable; integration requires a new profile revision.']}
(OUT/'CLASS-SIZE-FACTS.json').write_text(json.dumps(J,indent=2)+'\n')
print(json.dumps(J['validation'],indent=2))
print('CLASS-SIZE-FACTS.json sha256',hashlib.sha256((OUT/'CLASS-SIZE-FACTS.json').read_bytes()).hexdigest())
