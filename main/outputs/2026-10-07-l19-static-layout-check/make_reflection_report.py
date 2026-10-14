#!/usr/bin/env python3
"""Byte-check a bounded reflection supplement; only local saved-image reads."""
from pathlib import Path
import hashlib,json,re,struct
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OUT=ROOT/'outputs/2026-10-07-l19-static-layout-check'
IMG=ROOT/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
data=IMG.read_bytes()
digest=hashlib.sha256(data).hexdigest()
assert digest=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
phoff=struct.unpack_from('<Q',data,32)[0]
phentsize,phnum=struct.unpack_from('<HH',data,54)
textseg=next(struct.unpack_from('<IIQQQQQQ',data,phoff+n*phentsize) for n in range(phnum) if struct.unpack_from('<I',data,phoff+n*phentsize)[0]==1)
assert textseg[2]==0x4000 and textseg[3]==0
E={}
def ev(file,*addresses):
    rows={}
    for line in OUT.joinpath(file).read_text().splitlines():
        m=re.match(r'^0x[0-9a-f]+  \(f (0x[0-9a-f]+)\)  ([0-9a-f]+) +(.+)$',line)
        if m: rows[int(m[1],16)]=(m[2],m[3])
    keys=[]
    for a in addresses:
        a=int(a,16); b,insn=rows[a]
        assert data[a+0x4000:a+0x4000+len(bytes.fromhex(b))]==bytes.fromhex(b)
        key=hex(a)
        E[key]=dict(elf_image_vaddr=hex(a),file_byte_offset=hex(a+0x4000),instruction_bytes=b,instruction=insn,disassembly_artifact=file)
        keys.append(key)
    return keys
F=[]
def fact(k,v,w,es,why):
    F.append(dict(key=k,value=v,width_bytes=w,status='validated-static',confidence='high',evidence_keys=es,inference=why,live_validation=False))

fact('object_storage.native_chunked_65536',True,None,ev('AllocateUObjectIndex.asm','483b98','483b9f','483ba3','483ba6','483baa','483c92','483c8d')+ev('FreeUObjectIndex.asm','4840cd','4840d2','4840d6','4840d9','4840dd'),'Bothnativeindexroutines splitindex intohigh16 chunkindex andlow16 itemindex. Allocationprovides65536*24B perchunk andrecordsarrayheadercount65536. This establishesactualchunked65536 nativeaccesspath fortheauthenticatedimage; noglobalbuildmacro/freshlivesnapshot claim.')

fact('UFunction.FunctionFlags','0xb0',4,ev('UFunctionNextFunction.asm','326b81','326bc9')+ev('UFunctionBindCandidate.asm','326eee','326f05'),'Serialize selects self+B0 and writes a dword there; PostLoad independently reads/writes self+B0 asdword. This closes the prior byteB1-only limitation.')
fact('UFunction.NumParms','0xb4',1,ev('UFunctionNextFunction.asm','326c55','326cd0','326cd2'),'Loading/rebuilding function metadata clears a byte atB4 and increments/stores it onceperCPF_Parm property.')
fact('UFunction.ParmsSize','0xb6',2,ev('UFunctionNextFunction.asm','326cd8','326cdc','326ce1','326ce4','326ce7'),'Parameterextent is property.ElementSize*ArrayDim+Offset; that extent stores aswordB6. Independent ProcessEvent reads confirm its use.')
fact('UFunction.ReturnValueOffset','0xb8',2,ev('UFunctionNextFunction.asm','326cef','326cf7'),'CPF_ReturnParm bit0x400 selects propertyoffset and stores it aswordB8; independentProcessEvent usesffff sentinel or adds toparameterbase.')
fact('UFunction.FirstPropertyToInit','0xc0',8,ev('UFunctionNextFunction.asm','326c81','326cac','326cb1','326cb4','326cb9'),'Serialize/rebuild selects self+C0 as head storage and chains selectednonparameter properties throughproperty+60; ProcessEvent459bdb reads samehead and initializes propertyvalues.')

fact('UStruct.SuperStruct','0x40',8,ev('UStructSerialize.asm','31cc57','31ccb0','31cceb'),'Serialize reads self+40, follows repeated+40 parenthops, and rebuilds basechain metadata; uniqueparentlink inidentifiedUStruct baseSerialize calledbyUFunctionSerialize.')
fact('UStruct.Children','0x48',8,ev('UStructSerialize.asm','31cdb7','31cddb','31ce16','31cfc4'),'Serialize addresses/reads self+48, traverses childUFields via+28, and stores rebuilthead at+48.')
fact('UField.Next','0x28',8,ev('UStructSerialize.asm','31ce16','31cff1','31cffb'),'Children chain traverses +28 and reconstructs eachchildnext pointer at+28, null-terminating thelist.')
fact('UStruct.ChildProperties','0x50',8,ev('UStructSerializeProperties.asm','31d525','31d583','31d708','31d70f'),'PropertySerialize counts/iterates self+50 throughfield+18, then reconstructs exactlythat chain and storeshead backtoself+50;UFunctionSerialize independentlyreadsself+50.')
fact('UStruct.PropertiesSize','0x58',4,ev('ProcessEvent.asm','459948','459970','459978'),'ProcessEvent reads signed dwordself+58 as fullstruct storagesize; it comparesagainstwordParmsSizeB6 and allocates/zeros remainingstorage.')
fact('UStruct.MinAlignment','0x5c',2,ev('ProcessEvent.asm','459951','459956','4599af','4599b1','4599b9'),'ProcessEvent reads alignmentword5C, comparesagainst16 and uses it for storagealignment math.')
fact('UStruct.Script','0x60',None,ev('ProcessEvent.asm','459a2c','459a30','459876')+ev('UStructSerialize.asm','31cedc'),'Data pointer at60 becomes FFrame.Code; dword68 is bytecodeNum usedasemptyguard/serializedcount. Baseoffset60 isproved, completecontainerwidth/maxword not separatelyobserved here; widthnull isintentional.')
fact('UStruct.DestructorLink','0x80',8,ev('ProcessEvent.asm','459c5f','459c70','459cba'),'After function Invoke, ProcessEvent walks self+80 andfield+58, calls destructionvirtualslot140 on nonparameter values. This identifies destructorproperty chain bybehavior.')

fact('FField.ClassPrivate','0x8',8,ev('UFunctionNextFunction.asm','326c73','326c77','326d0b','326d0f'),'Childproperty walker reads field+8 asclass descriptor, testsdescriptorflags and selectsproperty processing; PropertySerialize31d5a0 loadsfield+8 andserializes classdescriptor name.')
fact('FField.Next','0x18',8,ev('UFunctionNextFunction.asm','326cfe')+ev('UStructSerializeProperties.asm','31d530','31d5c6','31d708'),'Both propertymetadata andSerialize traversefield+18; listreconstruction writes the sameposition.')
fact('FField.Owner','0x10',8,ev('FFieldNearby-364b80.asm','364b8a','364b94','364ba6','364baa','364bc7'),'Field helper reads owner atfield+10. It testsbit0; forfieldowner follows anotherfield+10; forobjectowner clearsbit0 and then usesobjectClass+10/Outer+20 forclass/owner traversal.')
fact('FField.NamePrivate','0x20',8,ev('FFieldNearby-364310.asm','364331','36434a','364354','364364','36436f','364377')+ev('FFieldNearby-364710.asm','36471d','364727'),'Variant-name helper usesfield+20 whenbit0clear andUObject+18 whenbit0set, passingthe8B FName into knownToString. FFieldSerialize independentlyserializesnameatself+20.')
fact('FField.FlagsPrivate','0x28',4,ev('FFieldNearby-364710.asm','364731','364738','36474c','364764'),'FFieldSerialize selects self+28 andserializes4 bytes; fastpath storesdword atthat address.')
fact('FField.owner_variant_encoding',dict(storage_bytes=8,tag_bit=0,tag_set_kind='UObject',tag_clear_kind='FField',untag_mask='0xfffffffffffffffe'),8,ev('UStructSerializeProperties.asm','31d632','31d636')+ev('FFieldNearby-364310.asm','364331','36434a','364364','36436f')+ev('FFieldNearby-364b80.asm','364b8a','364b94','364ba6','364bc7'),'Whenconstructing a propertyownedbyUStruct, serializer formsstruct_pointer|1. Variantnamehelperbit1routesafterclearingtag toUObjectName18;clear routesFFieldName20. Ownerwalkerfield+10 applies sametag/untag rule. This establisheslowbitobjecttagencoding forobserved path.')

fact('FProperty.ArrayDim','0x2c',4,ev('UFunctionNextFunction.asm','326cd8','326cdc')+ev('ProcessEvent.asm','459bf0','459bf4','459bfd'),'Twoindependentpaths multiplydword2C by dword30 forvalueextent. Since destructorloop loads2C ascount and30 aselementbytewidth, ArrayDim2C isuniquelyidentified. Windowscandidate30 isrefuted.')
fact('FProperty.ElementSize','0x30',4,ev('UFunctionNextFunction.asm','326cd8','326cdc')+ev('ProcessEvent.asm','459bf0','459bf4','459bfd'),'Valueextent multiplies ElementSize30 byArrayDim2C. Windowscandidate34 isrefuted. Derivedproperty canreuseFField base tailpadding; this doesnot proveFFieldsizeof2C.')
fact('FProperty.PropertyFlags','0x38',8,ev('UFunctionNextFunction.asm','326c90','326c94','326cef')+ev('ProcessEvent.asm','459c99','459ca5'),'Fullqwordflags38 areloaded andtested forCPF_Parm/ReturnParm andhighbit36 inProcessEvent;8B width isdirecttargetoperand evidence.')
fact('FProperty.Offset_Internal','0x44',4,ev('UFunctionNextFunction.asm','326ce1')+ev('PropertyThunkHelper.asm','4577de','4577e2'),'Propertyoffsetdword44 isaddedtoelementextent and toFrameLocals base. Exactexistinggap-l2a observation isindependently rechecked.')
fact('FProperty.DestructorLinkNext','0x58',8,ev('ProcessEvent.asm','459c70','459cc0'),'ProcessEvent destroysselectedpropertyvalues andadvancesdestructorchain throughproperty+58.')
fact('FProperty.PostConstructLinkNext','0x60',8,ev('UFunctionNextFunction.asm','326cb4','326cb9','326cbf')+ev('ProcessEvent.asm','459c06','459c2e'),'Functionmetadata build chainsnonparameter propertiesfromFirstPropertyToInitC0 via+60; ProcessEvent initializesvalues whilefollowingexactlythatlink.')

unknown=[
 dict(key='UStruct.UEP_TotalSize',status='unresolved',candidate='0xb0',reason='Super/children/property/script members recovered, butnoexactsizeof/classregistrationtotal uniquelyidentified.'),
 dict(key='FField.UEP_TotalSize',status='unresolved',candidate='0x30',member_extent_lower_bound='0x2c',reason='Flags28+4 ends2C;8Balignmentroundsup30, butsizeofnotdirectlymeasured. FProperty.ArrayDim2C canreusetailpadding; donotclaimFFieldsize2C.'),
 dict(key='FProperty.UEP_TotalSize',status='unresolved',candidate='0x70',member_extent_lower_bound='0x68',reason='PostConstructLinkNext60+8 ends68. RepNotifyFunc68 istemplateonly inthispass; exactsize/registrationnot recovered.'),
 dict(key='FProperty.PropertyLinkNext',status='unresolved',candidate='0x48',reason='Noindependentlyidentified linkoperation at+48 inthisboundedpass.'),
 dict(key='FProperty.NextRef',status='unresolved',candidate='0x50',reason='Noindependentlyidentified referencechain operation at+50 inthisboundedpass.'),
 dict(key='FProperty.RepIndex',status='unresolved',candidate='0x40',reason='Noidentifying operand encountered inthispass.'),
 dict(key='FProperty.RepNotifyFunc',status='unresolved',candidate='0x68',reason='Noidentifying operand encountered inthispass.'),
 dict(key='UStruct.PropertyLink',status='unresolved',candidate='0x70',reason='Noindependentlyidentified head readat70 inthispass.'),
 dict(key='UStruct.RefLink',status='unresolved',candidate='0x78',reason='Noindependentlyidentified head readat78 inthispass.'),
 dict(key='UStruct.PostConstructLink',status='unresolved',candidate='0x88',reason='FirstPropertyToInitC0 initializationchain isverified; generalstructhead88 notindependentlyidentified.'),
]
sources=[OUT/p for p in sorted({e['disassembly_artifact'] for e in E.values()})]
sources += [OUT/'make_reflection_report.py', OUT/'REPORT-SUPPLEMENT.md', IMG]
hashes=[dict(path=str(p),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sources]
result=dict(format=1,scope='local-static-only',image=dict(path=str(IMG),bytes=len(data),sha256=digest),address_model='ELFimagevaddr; fileoffset=vaddr+4000 perPT_LOAD0; nofreshliveevidence',facts=F,unresolved=unknown,evidence=E,sources=hashes,validation=dict(instruction_windows_checked=len(E),all_instruction_bytes_exact=True,source_mutations=0,console_network_account_actions=0,runtime_tests=0),supersedes_base_partial_fields=['UFunction.FunctionFlags','UFunction.NumParms'],scope_limits=['Noexactcompleteclasssize provedbeyondFUObjectItemarraystride18.','ExistingcandidateFPropertyArrayDim30/ElementSize34 contradictedbytargetinstructions.','Exploratoryfunctionsstartingmidinstructionnotcited.','NoGhidraprojectmutations/headlessrunsneeded.'])
OUT.joinpath('REFLECTION-FACTS.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(dict(facts=len(F),instruction_windows_checked=len(E),reflection_facts_sha256=hashlib.sha256(OUT.joinpath('REFLECTION-FACTS.json').read_bytes()).hexdigest())))
