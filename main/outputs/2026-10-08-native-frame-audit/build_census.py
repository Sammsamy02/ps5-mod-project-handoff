#!/usr/bin/env python3
import json,pathlib,subprocess,os,hashlib,struct
ROOT=pathlib.Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OUT=ROOT/'outputs/2026-10-08-native-frame-audit'
def pin(p):
 p=pathlib.Path(p);return {'path':str(p),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
recipe_path=ROOT/'outputs/2026-10-07-l19-r4-full-build/parent-review/AFFECTED-RECIPES.json'
r=json.loads(recipe_path.read_text())['recipes']['unreal-flat-fframe']
fields=['Node','Object','Code','Locals','MostRecentProperty','MostRecentPropertyAddress','MostRecentPropertyContainer','FlowStack','PreviousFrame','OutParms','PropertyChainForCompiledIn','CurrentNativeFunction','PreviousTrackingFrame','bArrayContextFailed','bAbortingExecution']
oldfields=[x for x in fields if x not in ['MostRecentPropertyContainer','PreviousTrackingFrame','bAbortingExecution']]
items=[('pointer.size','sizeof(void*)'),('pointer.align','alignof(void*)'),('bool.size','sizeof(bool)'),('opcode.size','sizeof(EExprToken)'),('callback.result_pointer.size','sizeof(void*)'),('FNativeFuncPtr.size','sizeof(FNativeFuncPtr)'),('FNativeFuncPtr.align','alignof(FNativeFuncPtr)'),('UnrealScriptFunction.size','sizeof(UnrealScriptFunction)'),('UFunction.GetFunc419.footprint','sizeof(std::remove_reference_t<decltype(std::declval<UFunction>().GetFunc())>)'),('UObject.member_fn.size','sizeof(void (UObject::*)(FFrame&,void*))'),('FFrame.facade.size','sizeof(FFrame)'),('FOutputDevice.size','sizeof(FOutputDevice)'),('FOutputDevice.align','alignof(FOutputDevice)'),('FOutParmRec.size','sizeof(FOutParmRec)'),('FOutParmRec.align','alignof(FOutParmRec)')]
for f in ['Property','PropAddr','NextOutParm']:items.append(('FOutParmRec.'+f,'__builtin_offsetof(FOutParmRec,'+f+')'))
for c,fs in [('FFrame_50_AndBelow',oldfields),('FFrame_51_AndAbove',fields)]:
 items += [(c+'.size','sizeof('+c+')'),(c+'.align','alignof('+c+')')]
 items += [(c+'.'+f,'__builtin_offsetof('+c+','+f+')') for f in fs]
items += [('FlowStackType.size','sizeof(FlowStackType)'),('FNativeFuncPtr.is_member','std::is_member_function_pointer_v<FNativeFuncPtr>'),('UnrealScriptFunction.same_FNativeFuncPtr','std::is_same_v<UnrealScriptFunction,FNativeFuncPtr>')]
src=OUT/'target-census.cpp'
src.write_text('#include <Unreal/FFrame.hpp>\n#include <Unreal/Script.hpp>\n#include <Unreal/UFunctionStructs.hpp>\n#include <Unreal/CoreUObject/UObject/Class.hpp>\n#include <type_traits>\n#include <utility>\nusing namespace RC::Unreal;\nextern "C" { extern const unsigned long long FrameFacts[] = {\n'+',\n'.join(e for _,e in items)+'\n}; }\nextern "C" void CallRawNative(FNativeFuncPtr fn,UObject* ctx,FFrame& frame,void* result) {fn(ctx,frame,result);}\nextern "C" void CallMemberNative(void(UObject::*fn)(FFrame&,void*),UObject* ctx,FFrame& frame,void* result) {(ctx->*fn)(frame,result);}\n')
a=list(r['pass1_argv']);
for flag,val in [('-c',str(src)),('-o',str(OUT/'target-census.o')),('-MF',str(OUT/'target-census.d'))]:a[a.index(flag)+1]=val
# Preserve accepted R4 owner include-prefix and original target flags; only inputs/outputs changed.
a+=['-Wno-invalid-offset','-Xclang','-fdump-record-layouts']
env=os.environ.copy();env['LLVM_CONFIG']=r['LLVM_CONFIG']
z=subprocess.run(a,cwd=ROOT,env=env,text=True,capture_output=True)
(OUT/'target-census.stdout').write_text(z.stdout);(OUT/'target-census.stderr').write_text(z.stderr)
(OUT/'target-census.command.json').write_text(json.dumps({'argv':a,'LLVM_CONFIG':r['LLVM_CONFIG'],'exit_code':z.returncode,'source_recipe':pin(recipe_path),'recipe_key':r['name']},indent=2)+'\n')
assert z.returncode==0,z.stderr[-3000:]
b=(OUT/'target-census.o').read_bytes(); shoff=struct.unpack_from('<Q',b,40)[0];shent,shnum,shstr=struct.unpack_from('<HHH',b,58); sh=[struct.unpack_from('<IIQQQQIIQQ',b,shoff+i*shent) for i in range(shnum)]
st=sh[shstr];strings=b[st[4]:st[4]+st[5]]
def strz(d,o):return d[o:d.index(0,o)].decode()
found=None
for s in sh:
 if s[1]!=2:continue
 ss=sh[s[6]];names=b[ss[4]:ss[4]+ss[5]]
 for o in range(s[4],s[4]+s[5],s[9]):
  no,info,other,idx,val,size=struct.unpack_from('<IBBHQQ',b,o)
  if strz(names,no)=='FrameFacts':found=(sh[idx][4]+val,size)
assert found and found[1]==8*len(items)
values=struct.unpack_from('<'+'Q'*len(items),b,found[0]);facts=[{'key':k,'value':hex(v),'decimal':v,'expression':e,'status':'target-compiler-derived; native-match not implied'} for (k,e),v in zip(items,values)]
inputs=[src,pathlib.Path(a[0]),pathlib.Path(r['LLVM_CONFIG']),ROOT/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FFrame.hpp',ROOT/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FOutputDevice.hpp',ROOT/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Script.hpp',ROOT/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UFunctionStructs.hpp',ROOT/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/CoreUObject/UObject/Class.hpp']
(OUT/'TARGET-CENSUS.json').write_text(json.dumps({'scope':'compile-only: actual selected declarations, actual recipe flags, no target execution','facts':facts,'inputs':[pin(p) for p in inputs],'output':pin(OUT/'target-census.o'),'command':pin(OUT/'target-census.command.json')},indent=2)+'\n')
print(json.dumps({'exit_code':z.returncode,'census_facts':len(facts),'new_target_object_bytes':len(b),'sizes':{f['key']:f['value'] for f in facts if 'size' in f['key'] or 'footprint' in f['key']}}))
