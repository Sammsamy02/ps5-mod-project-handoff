import hashlib
import json
import os
import re
import subprocess
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OUT = ROOT / 'outputs/2026-10-07-l15-fname-adapter/audit/engine-calls'
receipt = json.loads((ROOT/'outputs/2026-10-07-l14-full-build/runs/run-l14-2/receipt.json').read_text())
manifest = json.loads((OUT/'source-manifest.json').read_text())
def split(s):
    s=s.replace('operator<<','operator_shift_left')
    depth = 0
    out = []
    a = 0
    for i,c in enumerate(s):
        if c in '(<[': depth += 1
        if c in ')>]': depth -= 1
        if c == ',' and depth == 0:
            out.append(s[a:i].strip())
            a = i+1
    out.append(s[a:].strip())
    return [v.replace('operator_shift_left','operator<<') for v in out]
wrappers = []
includes = {'#include <Unreal/NameTypes.hpp>', '#include <Unreal/UObjectGlobals.hpp>', '#include <Unreal/FText.hpp>', '#include <Unreal/FURL.hpp>'}
for f in manifest['local_files']:
    p = f['path']
    if '/port/unreal/src/' not in p: continue
    text = Path(p).read_text()
    found = False
    for m in re.finditer(r'(?m)^\s*(IMPLEMENT_(?:UNREAL|FFIELD)_VIRTUAL_WRAPPER\w*)\s*\(', text):
        a = m.end(); b = a; depth = 1
        while depth:
            depth += (text[b]=='(')-(text[b]==')'); b += 1
        parts = split(text[a:b-1])
        if parts[0] == 'class_name': continue
        ret = parts[2]
        args = [] if 'NO_PARAMS' in m.group(1) else split(parts[3][parts[3].index('(')+1:-1])
        if args == ['']: args=[]
        wrappers.append({'id':'virtual_%03d'%len(wrappers), 'family':'vtable_wrapper','class':parts[0],'method':parts[1], 'return_type':ret, 'parameter_types':['const '+parts[0]+'*']+args, 'native_parameter_types':['const void*']+args, 'source':p, 'line':text.count('\n',0,m.start(1))+1, 'macro':m.group(1), 'macro_arguments':parts, 'game_abi_status':'inferred_from_selected_declaration; native game signature/slot not validated by this audit'})
        found = True
    if found:
        for inc in re.findall(r'(?m)^\s*(#include[^\n]+)', text.split('namespace RC::Unreal')[0]):
            if 'MemberVariable' not in inc and 'Windows.h' not in inc: includes.add(inc.strip())
functions = [
 ('FName::ConstructorInternal','FName',['const CharType*','EFindName'],'NameTypes.hpp',228),
 ('FName::ToStringInternal','void',['const FName*','FStringOut&'],'NameTypes.hpp',225),
 ('UObject::ProcessEventInternal','void',['UObject*','UFunction*','void*'],'UObject.hpp',126),
 ('UObject::ProcessConsoleExecInternal','bool',['UObject*','const TCHAR*','FOutputDevice&','UObject*'],'UObject.hpp',129),
 ('UObject::ProcessInternalInternal','void',['UObject*','FFrame&','void*'],'UObject.hpp',132),
 ('UObject::ProcessLocalScriptFunctionInternal','void',['UObject*','FFrame&','void*'],'UObject.hpp',135),
 ('UObject::CallFunctionByNameWithArgumentsInternal','bool',['UObject*','const TCHAR*','FOutputDevice&','UObject*','bool'],'UObject.hpp',138),
 ('AActor::BeginPlayInternal','void',['AActor*'],'AActor.hpp',43),
 ('AActor::EndPlayInternal','void',['AActor*','EEndPlayReason'],'AActor.hpp',45),
 ('AActor::TickInternal','void',['AActor*','float'],'AActor.hpp',47),
 ('AGameModeBase::InitGameStateInternal','void',['AGameModeBase*'],'AGameModeBase.hpp',25),
 ('UEngine::LoadMapInternal','bool',['UEngine*','FWorldContext&','FURL','UPendingNetGame*','FString&'],'UEngine.hpp',16),
 ('UEngine::TickInternal','void',['UEngine*','float','bool'],'UEngine.hpp',18),
 ('UGameViewportClient::TickInternal','void',['UGameViewportClient*','float'],'UGameViewportClient.hpp',21),
 ('ULocalPlayer::ExecInternal','bool',['ULocalPlayer*','UWorld*','const TCHAR*','FOutputDevice&'],'ULocalPlayer.hpp',30),
 ('UStruct::LinkInternal','void',['UStruct*','FArchive&','bool'],'CoreUObject/UObject/Class.hpp',137),
 ('GlobalState::StaticConstructObjectInternalBase','UObject*',['UClass*','UObject*','FName','EObjectFlags','UObject*','bool','FObjectInstancingGraph*'],'UObjectGlobals.hpp',67),
 ('GlobalState::StaticConstructObjectInternal411','UObject*',['const UClass*','UObject*','FName','EObjectFlags','EInternalObjectFlags','UObject*','bool','FObjectInstancingGraph*','bool','void*'],'UObjectGlobals.hpp',77),
 ('GlobalState::StaticConstructObjectInternal426','UObject*',['const FStaticConstructObjectParameters&'],'UObjectGlobals.hpp',140),
 ('FFieldClass::ConstructFn','FField*',['const FFieldVariant&','const FName&','EObjectFlags'],'FField.hpp',478),
 ('FNativeFuncPtr/UnrealScriptFunction','void',['UObject*','FFrame&','void*'],'UFunctionStructs.hpp',13),
 ('FInitGameStateDetour::NewStaticHookFn/trampoline','void',['AGameModeBase*','void*','void*','void*'],'Hooks/Internal/DetourSubclasses.hpp',262),
]
base = ROOT/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal'
for name, ret, args, header, line in functions:
    wrappers.append({'id':'function_%03d'%len(wrappers),'family':'Function_wrapper' if 'Internal' in name and not name.startswith('FNative') else 'direct_function_pointer', 'name':name,'return_type':ret,'parameter_types':args,'native_parameter_types':args,'source':str(base/header),'line':line,'game_abi_status':'confirmed_constructor_mismatch_from_L14_disassembly' if name=='FName::ConstructorInternal' else 'inferred_from_selected_declaration; native game ABI not validated by this audit'})
for h in ['AActor.hpp','AGameModeBase.hpp','UEngine.hpp','UGameViewportClient.hpp','ULocalPlayer.hpp','UFunctionStructs.hpp']:
    includes.add('#include <Unreal/'+h+'>')
code = '\n'.join(sorted(includes))+'\n#include <type_traits>\nusing namespace RC;\nusing namespace RC::Unreal;\n'
code += 'static_assert(sizeof(CharType)==2 && sizeof(TCHAR)==2 && sizeof(wchar_t)==2 && sizeof(char16_t)==2);\nstatic_assert(sizeof(FName)==8 && std::is_trivially_copyable_v<FName> && std::is_trivially_destructible_v<FName>);\n'
for w in wrappers:
    args = w['native_parameter_types']
    decl = ', '.join(t+' '+('self' if i==0 and w['family']=='vtable_wrapper' else 'a'+str(i)) for i,t in enumerate(args))
    call = ', '.join('self' if i==0 and w['family']=='vtable_wrapper' else 'a'+str(i) for i in range(len(args)))
    if w['family']=='vtable_wrapper':
        klass='ABIProbe_'+w['id']
        code += 'struct '+klass+' : '+w['class']+' { static '+w['return_type']+' abi_native_'+w['id']+'('+decl+');\n'
        code += 'static '+w['return_type']+' abi_probe_'+w['id']+'('+decl+') { return abi_native_'+w['id']+'('+call+'); }};\n'
        code += 'auto force_emit_'+w['id']+' = &'+klass+'::abi_probe_'+w['id']+';\n'
    else:
        code += 'extern "C" '+w['return_type']+' abi_native_'+w['id']+'('+decl+');\n'
        code += 'extern "C" '+w['return_type']+' abi_probe_'+w['id']+'('+decl+') { return abi_native_'+w['id']+'('+call+'); }\n'
source = OUT/'declared-wrapper-abi-probe.cpp'
source.write_text(code)
orig = receipt['selected_runtime_units']['unreal-nametypes']['argv']
args=[]; i=0
while i<len(orig):
    a=orig[i]
    if a in ['-o','-MF','-MT','-MQ']:
        i+=2; continue
    if a in ['-c','-MD','-MMD'] or a==receipt['selected_runtime_units']['unreal-nametypes']['source']:
        i+=1; continue
    args.append(a); i+=1
args += ['-S','-emit-llvm','-fno-discard-value-names','-Wno-return-type-c-linkage',str(source),'-o',str(OUT/'declared-wrapper-abi-probe.ll')]
env=dict(os.environ)
env['LLVM_CONFIG']=receipt['selected_runtime_units']['unreal-nametypes']['LLVM_CONFIG']
result=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,env=env)
(OUT/'declared-wrapper-abi-probe.stdout').write_text(result.stdout)
(OUT/'declared-wrapper-abi-probe.stderr').write_text(result.stderr)
(OUT/'declared-wrapper-abi-probe.receipt.json').write_text(json.dumps({'argv':args,'LLVM_CONFIG':env['LLVM_CONFIG'],'exit':result.returncode,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'purpose':'read-only compiler lowering of selected runtime declared wrappers; not a game ABI validation'},indent=2)+'\n')
(OUT/'declared-wrappers.json').write_text(json.dumps(wrappers,indent=2)+'\n')
print('probes',len(wrappers),'exit',result.returncode)
print(result.stderr[:8000])
