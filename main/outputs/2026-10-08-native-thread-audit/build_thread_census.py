from pathlib import Path
import json,subprocess,os,hashlib,struct
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what');OUT=Path(__file__).resolve().parent
def pin(p):
 p=Path(p);return dict(path=str(p),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
rp=ROOT/'outputs/2026-10-07-l19-r4-full-build/parent-review/AFFECTED-RECIPES.json';r=json.loads(rp.read_text())['recipes']['unreal-flat-fframe'];a=list(r['pass1_argv'])
items=[('pointer.size','sizeof(void*)'),('pthread_t.size','sizeof(pthread_t)'),('pthread_t.align','alignof(pthread_t)'),('pthread_t.is_pointer','std::is_pointer_v<pthread_t>'),('std.thread_id.size','sizeof(std::thread::id)'),('std.thread_id.align','alignof(std::thread::id)'),('pthread_self.same_pthread_t','std::is_same_v<decltype(pthread_self()),pthread_t>'),('engine.consumed_id32.size','sizeof(std::uint32_t)'),('int.size','sizeof(int)'),('id32.getter_pointer.size','sizeof(std::uint32_t(*)())')]
src=OUT/'target-thread-census.cpp'
src.write_text('#include <thread>\n#include <pthread.h>\n#include <type_traits>\n#include <cstdint>\n#include "'+str(ROOT/'.superpowers/native-dependency-spike/ue4ss-native-game-thread-1/include/Unreal/Platform/NativeGameThreadIdentity.hpp')+'"\nextern "C" { extern const unsigned long long ThreadFacts[] = {\n'+',\n'.join(e for _,e in items)+'\n}; }\nstd::thread::id CurrentStdThreadIdentity() { return std::this_thread::get_id(); }\nbool CurrentStdThreadAffinity(RC::Unreal::NativeThreadDetail::Identity& state) { return state.in_current_raw(); }\n')
for flag,val in [('-c',str(src)),('-o',str(OUT/'target-thread-census.o')),('-MF',str(OUT/'target-thread-census.d'))]:a[a.index(flag)+1]=val
a+=['-Xclang','-fdump-record-layouts'];env=os.environ.copy();env['LLVM_CONFIG']=r['LLVM_CONFIG'];z=subprocess.run(a,cwd=ROOT,env=env,text=True,capture_output=True)
(OUT/'target-thread-census.stdout').write_text(z.stdout);(OUT/'target-thread-census.stderr').write_text(z.stderr)
(OUT/'target-thread-census.command.json').write_text(json.dumps(dict(argv=a,LLVM_CONFIG=r['LLVM_CONFIG'],exit_code=z.returncode,source_recipe=pin(rp),scope='actual SDK target compile only, no engine or target execution'),indent=2)+'\n');assert z.returncode==0,z.stderr[-3000:]
b=(OUT/'target-thread-census.o').read_bytes();shoff=struct.unpack_from('<Q',b,40)[0];se,sn,st=struct.unpack_from('<HHH',b,58);ss=[struct.unpack_from('<IIQQQQIIQQ',b,shoff+i*se) for i in range(sn)];symrows=[];found=None
for s in ss:
 if s[1]!=2:continue
 names=b[ss[s[6]][4]:ss[s[6]][4]+ss[s[6]][5]]
 for o in range(s[4],s[4]+s[5],s[9]):
  n,info,other,idx,val,size=struct.unpack_from('<IBBHQQ',b,o);nm=names[n:names.index(0,n)].decode()
  if nm=='ThreadFacts':found=(ss[idx][4]+val,size)
  if nm and (idx==0 or 'CurrentStdThread' in nm):symrows.append(dict(name=nm,section_index=idx,value=hex(val),size=size))
assert found[1]==len(items)*8;values=struct.unpack_from('<'+'Q'*len(items),b,found[0]);facts=[dict(key=k,value=hex(v),expression=e,status='target-compiler representation; engine-ID semantics not implied') for (k,e),v in zip(items,values)]
sdk=ROOT/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk';extra=[sdk/'target/include/c++/v1/__threading_support',sdk/'target/include/c++/v1/__thread/id.h',sdk/'target/include/sys/_pthreadtypes.h',sdk/'target/include/pthread.h',sdk/'target/include/pthread_np.h',sdk/'target/lib/libkernel.so',sdk/'bin/prospero-nid',Path(a[0]),Path(r['LLVM_CONFIG']),src,ROOT/'.superpowers/native-dependency-spike/ue4ss-native-game-thread-1/include/Unreal/Platform/NativeGameThreadIdentity.hpp']
(OUT/'TARGET-THREAD-CENSUS.json').write_text(json.dumps(dict(facts=facts,symbols=symrows,inputs=[pin(p) for p in extra],command=pin(OUT/'target-thread-census.command.json'),object=pin(OUT/'target-thread-census.o'),scope='compile-only actual selected Identity and SDK libc++ pthread declarations'),indent=2)+'\n')
print(json.dumps(dict(exit_code=z.returncode,facts={x['key']:x['value'] for x in facts},pthread_self_undefined=any(x['name']=='pthread_self' and x['section_index']==0 for x in symrows))))
