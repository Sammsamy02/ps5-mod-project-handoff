from pathlib import Path
import hashlib,json,os,subprocess
HERE=Path(__file__).resolve().parent
ROOT=HERE.parent.parent
ENTRY=ROOT/'outputs/2026-10-07-l19-r4-full-build/driver/staging/lineage-overlay-2/lifecycle/runtime_module_entry.c'
LUA=ROOT/'outputs/2026-10-04-zcode-team/worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def function_body(text, signature):
    start=text.index(signature); opening=text.index('{',start); level=1;end=opening+1
    while level:
        if text[end]=='{':level+=1
        elif text[end]=='}':level-=1
        end+=1
    return text[start:end]
body=function_body(ENTRY.read_text(),'int module_stop(size_t argc, const void *args)')
unit=HERE/'actual-module-stop.cpp'
unit.write_text(r'''
#include <cstddef>
#include <cstdint>
#include <iostream>
enum { PROBE_ACTIVE, PROBE_FINISHED };
struct Life { int state=PROBE_ACTIVE; } lifecycle;
int ops, finalized, released_input, closed_channel, errors, runtime_stop_calls;
unsigned char entry_guard, runtime_active=1;
void diagnostic_event(const char*,uint32_t) {}
void runtime_failure(const char*,int,const char*) { ++errors; }
int modbridge_runtime_stop() { ++runtime_stop_calls; return -37; }
int probe_lifecycle_stop(Life* p,int*) { ++finalized; p->state=PROBE_FINISHED; return 0; }
void probe_runtime_release_input() { ++released_input; }
int diagnostic_close_channel() { ++closed_channel; return 0; }
int leave_entry(int result) { __atomic_clear(&entry_guard,__ATOMIC_RELEASE);return result; }
'''+body+r'''
int main() {
    int result=module_stop(0,nullptr);
    if(result!=0 || finalized!=1 || runtime_active || errors!=1 || runtime_stop_calls!=1 || released_input!=1 || closed_channel!=1)return 1;
    std::cout<<"OBSERVED runtime stop refusal still clears runtime_active and finalizes DSO\n";
}
''')
prior=ROOT/'outputs/2026-10-08-thunk-independent-review/policy-actual.cpp'
prefix=prior.read_text().split('#include <memory>')[0]
lua_text=LUA.read_text();callback=lua_text[lua_text.index('    static auto lua_unreal_script_function_hook_pre'):]
opening=callback.index('auto& lua_data = *static_cast<LuaUnrealScriptFunctionData*>(custom_data);')
finish=callback.index('if (lua_data.scheduled_for_removal) return;')+len('if (lua_data.scheduled_for_removal) return;')
fragment=callback[opening:finish]
uaf=HERE/'actual-admitted-callback.cpp'
uaf.write_text(prefix+r'''
#include <thread>
#include <iostream>
using namespace RC::Unreal;
struct LuaUnrealScriptFunctionData { std::atomic<bool> scheduled_for_removal{false}; };
std::atomic<bool> admitted{},resume{};
void original(UObject*,FFrame&,void*) {}
void selected_lua_prefix(UnrealScriptFunctionCallableContext,void* custom_data) {
    // Explicit HOST scheduling seam: engine preempts immediately before the
    // selected Lua callback's first userdata access. No production lock or
    // lifetime token is inserted by this seam.
    admitted.store(true);
    while(!resume.load()) {}
'''+fragment+r'''
}
int main() {
    UFunction function;function.func=original;
    auto* raw=new LuaUnrealScriptFunctionData;
    auto id=function.RegisterPreHook(selected_lua_prefix,raw);
    auto loaded=function.func;
    UObject context;FFrame frame;
    std::thread engine([&]{loaded(&context,frame,nullptr);});
    while(!admitted.load()) {}
    // Selected LuaMod uninstall marks and removes callback IDs, then closes
    // Lua and erases owning unique_ptr userdata. The HOST allocation stands
    // in for that final erase, with the real prototype removal body retained.
    raw->scheduled_for_removal.store(true);
    function.GetFunctionHookData().RemoveCallback(id);
    delete raw;
    resume.store(true);
    engine.join();
}
''')
rows=[]
for name,source,args,expected in [('module-stop',unit,[],'observed-finalization-after-stop-refusal'),('admitted-lua-userdata',uaf,['-pthread'],'expected-ASan-use-after-free')]:
    binary=HERE/(name+'-test');command=['/usr/bin/clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',*args,str(source),'-o',str(binary)]
    build=subprocess.run(command,capture_output=True,text=True,timeout=60)
    (HERE/(name+'-compile.stderr')).write_text(build.stderr)
    assert build.returncode==0,build.stderr
    run=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='halt_on_error=1:abort_on_error=1:detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
    (HERE/(name+'.stdout')).write_text(run.stdout);(HERE/(name+'.stderr')).write_text(run.stderr)
    passed=run.returncode==0 if name=='module-stop' else 'AddressSanitizer: heap-use-after-free' in run.stderr
    rows.append({'name':name,'pass':passed,'expected_observation':expected,'scope':'Exact selected module_stop body or exact prototype callback bodies plus selected Lua pre userdata fragment; explicit HOST loader/Lua storage/scheduling seams','compile_command':command,'compile_returncode':build.returncode,'returncode':run.returncode,'source_sha256':sha(source),'binary_sha256':sha(binary),'stdout_sha256':sha(HERE/(name+'.stdout')),'stderr_sha256':sha(HERE/(name+'.stderr'))})
receipt={'pass':all(x['pass'] for x in rows),'source_pins':{'module_entry':{'path':str(ENTRY),'sha256':sha(ENTRY)},'LuaMod':{'path':str(LUA),'sha256':sha(LUA)},'exact_prototype_prefix_source':{'path':str(prior),'sha256':sha(prior)}},'rows':rows,'scope':'Bounded negative/behavior observations; not native or integrated runtime tests, no shared/selected edits'}
(HERE/'CONTROLS.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({'observations_pass':receipt['pass'],'controls':len(rows),'results':[{k:x[k] for k in ['name','returncode','pass']}for x in rows]}))
assert receipt['pass']
