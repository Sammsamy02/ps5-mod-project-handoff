import hashlib
import json
import re
from collections import Counter
from pathlib import Path

ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OUT=ROOT/'outputs/2026-10-07-l15-fname-adapter/audit/engine-calls'
manifest=json.loads((OUT/'source-manifest.json').read_text())
wrappers=json.loads((OUT/'declared-wrappers.json').read_text())
ir=(OUT/'declared-wrapper-abi-probe.ll').read_text().splitlines()
def split(s):
    depth=0; q=False; esc=False; start=0; out=[]
    for i,c in enumerate(s):
        if q:
            if esc: esc=False
            elif c=='\\': esc=True
            elif c=='"': q=False
            continue
        if c=='"': q=True;continue
        if c in '({[<': depth+=1
        elif c in ')}]>': depth-=1
        elif c==',' and depth==0:
            out.append(s[start:i].strip());start=i+1
    if s[start:].strip():out.append(s[start:].strip())
    return out
gprs=[('RDI','EDI','DI','DIL'),('RSI','ESI','SI','SIL'),('RDX','EDX','DX','DL'),('RCX','ECX','CX','CL'),('R8','R8D','R8W','R8B'),('R9','R9D','R9W','R9B')]
for w in wrappers:
    line=next(l for l in ir if l.startswith('define ') and 'abi_probe_'+w['id'] in l)
    w['compiler_lowered_definition']=line
    w['compiler_lowering_ref']={'path':str(OUT/'declared-wrapper-abi-probe.ll'),'line':ir.index(line)+1}
    symbol=line.split('@',1)[1].split('(',1)[0]
    signature=line[line.index('(',line.index('@'))+1:line.rindex(') #')]
    args=split(signature); ig=0; fp=0; st=0; mapped=[]
    for a in args:
        typ=a.split(' ',1)[0]; name=a.rsplit('%',1)[-1]; hidden='sret(' in a
        if typ in ['float','double'] or typ.startswith('<'):
            loc='XMM'+str(fp);fp+=1
        elif 'byval(' in a:
            loc='stack byval aggregate (see LLVM type/alignment)'
        elif ig<6:
            variant=0 if typ in ['ptr','i64'] else 1 if typ=='i32' else 2 if typ=='i16' else 3 if typ in ['i8','i1'] else 0
            loc=gprs[ig][variant];ig+=1
        else:
            loc='[RSP+%d] at callee entry'%(8+st*8);st+=1
        source_arg=None
        if name=='self': source_arg=w['parameter_types'][0]
        elif re.match(r'a\d+',name): source_arg=w['parameter_types'][int(re.match(r'a(\d+)',name).group(1))]
        note='nontrivial by-value C++ parameter lowered as an invisible-reference pointer' if typ=='ptr' and source_arg and '*' not in source_arg and '&' not in source_arg else None
        mapped.append({'lowered_argument':a,'source_type':source_arg,'sysv_location':loc,'hidden_sret_output':hidden,'passing_note':note})
    prefix=line[:line.index('@')].strip()
    if any(a['hidden_sret_output'] for a in mapped): ret='RAX = hidden result-storage pointer; constructed value in storage passed in RDI'
    elif re.search(r'\{\s*i64,\s*i64\s*\}$',prefix): ret='RAX = first 8-byte integer chunk; RDX = second 8-byte integer chunk'
    elif prefix.endswith('void'): ret='void'
    elif prefix.endswith(('double','float')): ret='XMM0'
    elif prefix.endswith('i1') or prefix.endswith('i8'): ret='AL (low byte)'
    elif prefix.endswith('i32'): ret='EAX'
    elif prefix.endswith('i16'): ret='AX'
    else: ret='RAX'
    w['declared_sysv_arguments']=mapped;w['declared_sysv_return']=ret
    w['caller_abi_evidence']='pinned Prospero compiler lowering plus SysV x86_64 register assignment; this measures the selected caller declaration only'
    w['string_argument_width_bytes']=2 if any(('TCHAR' in a or 'CharType' in a) and '*' in a for a in w['parameter_types']) else None
    w['fname_abi_note']=('Selected FName is an 8-byte trivial-for-calls aggregate; direct value args use one INTEGER register and direct FName returns use RAX. Adding a user-defined copy/move constructor or destructor can introduce invisible-reference arguments and hidden sret returns; re-audit all crossings before such a change.' if any('FName' in a for a in w['parameter_types']) or w['return_type']=='FName' else None)

files={f['path']:Path(f['path']).read_text(errors='replace') for f in manifest['local_files']}
project_files={p:t for p,t in files.items() if ('/source/port/unreal/' in p or '/source/port/ue4ss/' in p or '/source/port/first/' in p or '/units/' in p or p in {str(Path(u['source']) if Path(u['source']).is_absolute() else ROOT/u['source']) for u in manifest['selected_units']}) and '/source-dependencies/' not in p}
function_names=[w['name'].split('::')[-1] for w in wrappers if w['family']=='Function_wrapper']+['StaticConstructObjectInternal']
function_call_sites=[]; script_call_sites=[]; pointer_candidates=[]; literal_sites=[]; literal_boundary_sites=[]; reflection_sites=[]; trampoline_call_sites=[]
literal_re=re.compile(r'\bL(?:"(?:[^"\\]|\\.)*"|\x27(?:[^\x27\\]|\\.)*\x27)|\bSTR\s*\(')
boundary_re=re.compile(r'\b(FName(?:\s+[A-Za-z_]\w*)?|StaticFindObject(?:_Internal\w+)?(?:\s*<[^;\n]*?>)?|ConstructorInternal|ToStringInternal|ProcessConsoleExec|CallFunctionByNameWithArguments|DeferredRegister|PreSaveRoot|ImportCustomProperties|ImportText(?:_Internal)?|SetNumericPropertyValueFromString|Rename|Exec|FString(?:\s+[A-Za-z_]\w*)?|FText(?:\s+[A-Za-z_]\w*)?)\s*([({])')
def end_call(t,a,openchar):
    closechar=')' if openchar=='(' else '}';depth=1;q=None;esc=False;i=a
    while i<len(t) and depth:
        c=t[i]
        if q:
            if esc:esc=False
            elif c=='\\':esc=True
            elif c==q:q=None
        elif c in ['"',"'"]:q=c
        elif c==openchar:depth+=1
        elif c==closechar:depth-=1
        i+=1
    return i
for p,t in project_files.items():
    lines=t.splitlines()
    for i,l in enumerate(lines,1):
        for n in function_names:
            if re.search(r'\b'+re.escape(n)+r'\s*\(',l):function_call_sites.append({'path':p,'line':i,'family_name':n,'text':l.strip()})
        if re.search(r'GNatives_Internal\[.*\]\s*\(|GetFuncPtr\(\)\s*\(|GetOriginalFuncPtr\(\)\s*\(|\bProcessInternal\s*\(Context',l):script_call_sites.append({'path':p,'line':i,'text':l.strip(),'declared_sysv':'RDI=UObject Context; RSI=FFrame&; RDX=result buffer; void return'})
        if 'PLH::FnCast(Trampoline' in l:
            trampoline_call_sites.append({'path':p,'line':i,'text':l.strip(),'declared_abi':'same retained target Function signature except NewStaticHookFn: Context RDI, dummy1 RSI, dummy2 RDX, dummy3 RCX; void','game_abi_status':'inferred/unvalidated'})
        if re.search(r'(?:bit_cast|reinterpret_cast)\s*<.*(?:\(\*|FunctionSignature|UnrealScriptFunction|FNativeFuncPtr)|\(\*.*\)\s*\(',l):pointer_candidates.append({'path':p,'line':i,'text':l.strip()})
        if literal_re.search(l):literal_sites.append({'path':p,'line':i,'text':l.strip()})
        if re.search(r'UE_(?:BEGIN_NATIVE_FUNCTION_BODY|SET_STATIC_SELF|COPY_PROPERTY|CALL_STATIC_FUNCTION|CALL_FUNCTION|RETURN_PROPERTY)',l):reflection_sites.append({'path':p,'line':i,'text':l.strip(),'route':'local StaticFindObject/FName/property metadata -> UObject::ProcessEvent; payload strings copied into metadata-sized parameter frame'})
    for m in boundary_re.finditer(t):
        b=end_call(t,m.end(),m.group(2));call=t[m.start():b]
        if not literal_re.search(call):continue
        name=m.group(1).strip(); line=t.count('\n',0,m.start())+1
        if name.startswith('FName') or name=='ConstructorInternal':route='native FName constructor via retained ConstructorInternal (literal string path)'
        elif name.startswith('StaticFindObject'):route='local object lookup; may construct FName parts through native constructor'
        elif name.startswith(('FString','FText')):route='local payload construction; FString allocates through FMemory; FText conversion uses reflected ProcessEvent'
        else:route='declared native text-taking wrapper or wrapper caller; inspect call expression for overload/version'
        literal_boundary_sites.append({'path':p,'line':line,'callee_expression':name,'text':call,'route':route,'literal_forms':sorted(set(('L' if x.group(0).startswith('L') else 'STR') for x in literal_re.finditer(call))),'width_status':'Pinned Prospero L/STR storage is UTF-16 width (2 bytes); explicit char16_t/u boundary form requirement remains separate from width correctness'})

width_refs=[{'path':p,'line':i,'text':l.strip()} for p,t in files.items() for i,l in enumerate(t.splitlines(),1) if ('ProsperoPlatform.hpp' in p and 'static_assert' in l) or ('StringType.hpp' in p and any(s in l for s in ['using CharType', '#define STR', 'static_assert(sizeof'])) or ('/HAL/Platform.hpp' in p and 'static_assert(sizeof(TCHAR)' in l)]
macros=(OUT/'pinned-target-width-macros.txt').read_text().splitlines()
by_name={w.get('name',w.get('class','')+'::'+w.get('method','')):w for w in wrappers}
constructor=by_name['FName::ConstructorInternal']
constructor['confirmed_game_entry']={'entry_linked_vaddr':'0x23d590','required_arguments':[{'register':'RDI','meaning':'writable pointer to 8-byte FName output storage (native constructor this/output)'},{'register':'RSI','meaning':'UTF-16 input string; word loads and +2 stride confirmed'},{'register':'EDX','meaning':'32-bit find/add mode (third integer argument, preserved into inner helper)'}],'observed_store':'0x23d5f0: mov qword ptr [rbx], rax; RBX copied from RDI at 0x23d599','source_disassembly':str(ROOT/'outputs/2026-10-07-l14-full-build/console-test-20261007T161545Z-abde74a3/constructor-crash-disassembly.txt'),'game_fixture_sha256':'3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc','caller_disassembly':str(ROOT/'outputs/2026-10-07-l14-full-build/console-test-20261007T161545Z-abde74a3/constructor-callchain-disassembly.txt'),'mismatch':'Retained Function<FName(const CharType*, EFindName)> sends string in RDI and mode in ESI, receives packed return RAX; it supplies no writable output pointer and shifts mode/string roles.'}
drift=[]
for u in manifest['selected_units']:
    p=Path(u['source']);p=p if p.is_absolute() else ROOT/p
    actual=hashlib.sha256(p.read_bytes()).hexdigest()
    if u.get('source_sha256') and u['source_sha256']!=actual:drift.append({'unit':u['name'],'path':str(p),'receipt_sha256':u['source_sha256'],'current_sha256':actual})
counts={'selected_runtime_units':len(manifest['selected_units']),'local_source_header_dependency_files':len(manifest['local_files']),'missing_dependency_files':len(manifest['missing_dependency_files']),'missing_source_paths':len(manifest['missing_source_or_dependency_paths']),'virtual_boundary_macro_invocations':sum(w['family']=='vtable_wrapper' for w in wrappers),'distinct_virtual_class_method_signature_tuples':len({(w.get('class'),w.get('method'),w['return_type'],tuple(w['parameter_types'])) for w in wrappers if w['family']=='vtable_wrapper'}),'Function_wrapper_families':sum(w['family']=='Function_wrapper' for w in wrappers),'additional_direct_function_pointer_families':sum(w['family']=='direct_function_pointer' for w in wrappers),'total_compiler_lowered_entries':len(wrappers),'Function_call_site_matches':len(function_call_sites),'script_native_call_sites':len(script_call_sites),'L_STR_literal_lines_in_project_sources_and_headers':len(literal_sites),'L_STR_literal_boundary_expressions':len(literal_boundary_sites),'reflected_macro_lines':len(reflection_sites),'compiler_confirmed_FName_value_return_entries':sum(w['return_type']=='FName' for w in wrappers),'receipt_source_drift':len(drift)}
risks=[
 {'priority':'P0','title':'Confirmed FName native-constructor mismatch','refs':[constructor['source']+':'+str(constructor['line']),str(ROOT/'outputs/2026-10-07-l14-image-admission/units/UnrealInitializer.cpp')+':1014'],'finding':constructor['confirmed_game_entry']['mismatch'],'action':'Use local native constructor ABI thunk with explicit writable 8-byte output, const char16_t* UTF-16 input, int32 mode. Preserve retained Function<FName(const CharType*, EFindName)> API and symbol identity.'},
 {'priority':'P1','title':'Other game signatures are unvalidated','finding':'Compiler lowering proves caller expectations, not target ABI. Slot/profile signatures and real parameter layouts must agree before treating any family as game-ABI validated. This audit newly validates no game target other than the locally saved constructor evidence.'},
 {'priority':'P1','title':'FName trivialness and nontrivial return drift','refs':[str(ROOT/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/NameTypes.hpp')+':215'],'finding':'Selected FName is size8 and trivially copyable/destructible. Four compiled entries return FName in RAX (constructor wrapper, FProperty::GetID, UObject::GetExporterName, UStruct::FindPropertyNameFromGuid). Introducing nontrivial copy/move/destruction would change these to hidden-result/invisible-reference conventions and also affect by-value FName parameters and aggregate FPrimaryAssetId/FGuid pathways. Size alone is not enough to establish ABI.'},
 {'priority':'P1','title':'Placeholder structured-archive/delegate-sensitive declarations','refs':[str(ROOT/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UObject.hpp')+':165',str(ROOT/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/CoreUObject/UObject/UnrealType.hpp')+':215'],'finding':'UObject::FStructuredArchive::FRecord and FProperty::FStructuredArchive::FSlot are empty enums (lowered i32); FPropertyTag is void*. The game uses must be confirmed before invoking these wrappers. FScriptDelegate/FMulticastScriptDelegate and FURL value parameters lower as invisible-reference pointers due to selected nontrivial types; matching game trivialness is unconfirmed.'},
 {'priority':'P1','title':'Explicit UTF-16 boundary form still required','finding':'Current pinned target is width-correct: wchar_t=2, CharType=wchar_t, TCHAR=2, STR=L. This corrects a generic host wchar_t=4 assumption. Existing L/STR boundary expressions remain inventoried; literal width correctness does not make wchar_t* an explicit char16_t* API. Use per-boundary char16_t/u adaptation; do not globally FORCE_U16 because retained compiled symbol/API identities would drift.'},
 {'priority':'P2','title':'FName plain-string byte count bug outside direct-call ABI','refs':[str(ROOT/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/NameTypes.cpp')+':204'],'finding':'GetPlainNameString copies String.size()*sizeof(File::StringType::size_type) (8-byte size_type) into a TCHAR buffer (2-byte element) rather than character bytes. This is a separate existing memory-copy risk, not evidence of a 4-byte pointer crossing.'},
]
inventory={'scope':'Local-only read-only source/declared-caller ABI audit of L14 run-l14-2 selected runtime. All generated files are audit artifacts, not linked runtime modifications. No console/network contact.','receipt':manifest['receipt'],'counts':counts,'evidence_levels':{'confirmed_game_disassembly':'Saved local L14 constructor evidence for exact pinned game fixture','compiler_measured_caller':'Pinned Prospero compiler lowering of selected declarations with static width/trivialness assertions','inferred_game':'Declarations/engine version labels without target disassembly or execution confirmation'},'boundary_entries':wrappers,'function_wrapper_call_sites':function_call_sites,'script_native_call_sites':script_call_sites,'direct_pointer_cast_candidates':pointer_candidates,'literal_boundary_expressions':literal_boundary_sites,'all_project_L_STR_literal_lines':{'path':str(OUT/'ALL-L-STR-LITERAL-LINES.json'),'count':len(literal_sites),'scope_note':'Includes generated vtable/layout key literals; not all are game arguments'},'trampoline_call_sites':trampoline_call_sites,'reflected_macro_routes':reflection_sites,'width_assertion_refs':width_refs,'measured_macros':macros,'four_byte_character_pointer_crossings':{'confirmed_count':0,'finding':'No confirmed active 4-byte character-pointer leak into the inventoried engine entries. CharType/TCHAR/wchar_t are statically/compiler measured 2 bytes in the pinned target; UTF32 helpers/char32_t declarations are not evidence of native TCHAR leaks. Untyped void* reflected payloads still require metadata/frame layout checks.'},'receipt_source_drift':drift,'risks':risks,'non_engine_dispatch_notes':[{'family':'StaticFindObject','refs':[str(ROOT/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/UObjectGlobals.cpp')+':137'],'finding':'Selected implementation traverses UE4SS object arrays/names locally. No native StaticFindObject Function target exists here; FName constructors underneath are native crossings.'},{'family':'VirtualFunctionHelper/ObjectSearcherInternal','finding':'Project pseudo-virtual dispatch and local object-searcher function pointer are UE4SS-to-UE4SS calls; game vtable loads use UnrealVirtualBaseVC macro entries inventoried above.'},{'family':'UClass/ClassVTableHelper/FFieldClass versioned getters','finding':'Generated std::function-typed layout getters exist, but audit found no selected call through UClass ClassConstructor/ClassVTableHelper getters. FFieldClass direct ConstructFn call is inventoried separately; returning a getter is not invoking its target.'}]}
(OUT/'ALL-L-STR-LITERAL-LINES.json').write_text(json.dumps(literal_sites,indent=2)+'\n')
(OUT/'ENGINE-CALL-ABI-INVENTORY.json').write_text(json.dumps(inventory,indent=2)+'\n')
def source_link(p,line):return '['+Path(p).name+':'+str(line)+']('+p+':'+str(line)+')'
tables=['# Selected engine-call boundary sites','', 'Every row is a selected declaration/call boundary. Registers measure the caller ABI; native agreement is unconfirmed except the FName constructor mismatch. `self` is the original receiver; `aN` maps to parameter_types[N] in the JSON inventory.','', '| Entry | Source | Caller arguments | Caller return |','| --- | --- | --- | --- |']
for w in wrappers:
    name=w.get('name',w.get('class','')+'::'+w.get('method',''))
    regs='; '.join(a['lowered_argument'].rsplit('%',1)[-1]+'='+a['sysv_location'] for a in w['declared_sysv_arguments'])
    tables.append('| '+name+' | '+source_link(w['source'],w['line'])+' | '+regs+' | '+w['declared_sysv_return']+' |')
(OUT/'ENGINE-CALL-SITES.md').write_text('\n'.join(tables)+'\n')
report=['# Engine-call ABI audit — L14 selected runtime','', 'Local-only source audit. No console/network contact and no runtime edits. The L14 receipt selects **%d units** and **%d local source/header dependencies**; **%d virtual boundary invocations** (%d distinct class/method/signature tuples), **%d Function families**, and **%d additional direct pointer families** are inventoried. All **%d declared caller signatures** were successfully lowered with the pinned Prospero compiler.'%(counts['selected_runtime_units'],counts['local_source_header_dependency_files'],counts['virtual_boundary_macro_invocations'],counts['distinct_virtual_class_method_signature_tuples'],counts['Function_wrapper_families'],counts['additional_direct_function_pointer_families'],counts['total_compiler_lowered_entries']), '', 'The comprehensive [JSON inventory](./ENGINE-CALL-ABI-INVENTORY.json) includes every boundary, source reference, compiler LLVM declaration, SysV register map, call-site matches, literal expression, and evidence status. The [per-call table](./ENGINE-CALL-SITES.md) has all %d entries. The [source manifest](./source-manifest.json) records all selected sources/dependencies and hashes. Missing dependency files/paths: **0/0**. Selected source hash drift: **%d**.'%(len(wrappers),len(drift)), '', '## Confirmed constructor mismatch','', 'The pinned game constructor at linked `0x23d590` takes **RDI = writable 8-byte output**, **RSI = UTF-16 input**, and **EDX = mode**. Its word loads and `+2` stride establish UTF-16 width; `0x23d5f0` stores `RAX` through `RBX`, which was copied from RDI. The selected `Function<FName(const CharType*, EFindName)>` caller supplies **RDI = string, ESI = mode** and expects the 8-byte value in **RAX**. It never supplies output storage. Saved [game disassembly](../../../2026-10-07-l14-full-build/console-test-20261007T161545Z-abde74a3/constructor-crash-disassembly.txt) and [caller disassembly](../../../2026-10-07-l14-full-build/console-test-20261007T161545Z-abde74a3/constructor-callchain-disassembly.txt) establish that mismatch for game fixture SHA-256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`. Keep the retained Function API and adapt the native boundary with output storage and explicit `char16_t` input.', '', '## Width correction and literal sites','', 'The pinned target reports `__SIZEOF_WCHAR_T__=2`, `__WCHAR_WIDTH__=16`, `STR(str)=L##str`, and no `FORCE_U16`/`__linux__` macro. [Measured macros](./pinned-target-width-macros.txt) and successful probe assertions prove `sizeof(CharType)==sizeof(TCHAR)==sizeof(wchar_t)==sizeof(char16_t)==2`; ProsperoPlatform.hpp:12–13 and Platform.hpp:1185 enforce the same widths. A generic Mac `wchar_t==4` assumption does **not** describe this runtime.', '', 'There are **%d L/STR literal boundary expressions** and **%d L/STR source/header lines** in the selected project scope, The 181 boundary expressions comprise 131 FName literal expressions (including macro definitions) and 50 local StaticFindObject expressions. The large all-literal list includes generated vtable/layout lookup keys, which are not game string arguments; it is stored in ALL-L-STR-LITERAL-LINES.json. Only one raw L constructor expression occurs: NameTypes.cpp:92, L\"InName\". The remaining FName literal expressions use STR. **No active 4-byte character-pointer leak was confirmed.** Existing L/STR expressions are width-correct on this target but do not satisfy an explicit `char16_t`/`u` boundary API policy by themselves. Adapt each native boundary without globally changing FORCE_U16: retained wchar_t/CharType symbols must retain their compiled identity.'%(counts['L_STR_literal_boundary_expressions'],counts['L_STR_literal_lines_in_project_sources_and_headers']), '', '## Register expectations and validation limit','', '| Family | Declared SysV caller expectation | Evidence |','| --- | --- | --- |','| FName constructor | string RDI, mode ESI; packed value RAX | Compiler + saved caller; confirmed wrong for game ctor |','| FName ToString | FName pointer RDI, FStringOut pointer RSI; void | Compiler; game unvalidated |','| ProcessEvent (Function/vtable/script route) | UObject RDI, UFunction RSI, payload RDX; void | Compiler; game unvalidated |','| StaticConstructObject 4.26+ | parameter-struct pointer RDI; UObject pointer RAX | Compiler; game/layout unvalidated |','| StaticConstructObject pre-4.26 | old version scalar/FName argument lists; exact regs/stack in table | Compiler; version-dependent game ABI unvalidated |','| FMalloc Malloc/Realloc/Free | receiver RDI; Malloc size RSI/alignment EDX; Realloc old pointer RSI/size RDX/alignment ECX; Free pointer RSI | Compiler; slot/profile game agreement unvalidated |','| FString value-return virtuals | hidden result storage RDI, receiver RSI, subsequent args shift; result in storage | Compiler; game unvalidated |','| FName value-return virtuals | receiver RDI; packed 8-byte value RAX (Guid input RSI where present) | Compiler; game unvalidated |','| FPrimaryAssetId/FGuid value returns | two integer chunks RAX/RDX | Compiler; game unvalidated |','| FText conversions | reflected property frames -> ProcessEvent, then local copy/steal; property value ops use FProperty virtuals | Source route; game frame/ops/layout unvalidated |','| Script-native/GNatives pointers | Context RDI, FFrame pointer RSI, result pointer RDX; void | Compiler; game target unvalidated |','| FFieldClass ConstructFn | owner reference RDI, FName reference RSI, flags EDX; FField pointer RAX | Compiler; game layout/target unvalidated |', '', '`StaticFindObject` in this selected runtime is a local object traversal, not a native StaticFindObject target. The pseudo-virtual/helper dispatch map is also local code. No signature or vtable slot becomes game-ABI validated merely because its declaration compiled or a symbol/address was found.', '', '## Remaining risks','']
for risk in risks[1:]: report.append('- **'+risk['title']+'**: '+risk['finding'])
report += ['', 'FName requires special care: the selected type is **trivially copyable/destructible, size 8**. It has value/string constructors but no user-defined copy/move constructor or destructor. Its four direct return entries therefore lower to `i64` in RAX. A future nontrivial FName type would change return/argument handling; size assertions alone would miss that. FName-containing FPrimaryAssetId and the by-value FName sites are included in the per-call table.', '', 'The probe compile confirms caller lowering only. It ran no game code, linked no runtime, and establishes no console pass.']
report_text='\n'.join(report)+'\n'
report_text=re.sub(r'\]\(([^)]+)\)', lambda m: ']('+str((OUT/m.group(1)).resolve())+')' if not m.group(1).startswith('/') else m.group(0), report_text)
(OUT/'ENGINE-CALL-ABI-AUDIT.md').write_text(report_text)
print(json.dumps(counts,indent=2))
print('outputs',OUT/'ENGINE-CALL-ABI-AUDIT.md',OUT/'ENGINE-CALL-ABI-INVENTORY.json')
