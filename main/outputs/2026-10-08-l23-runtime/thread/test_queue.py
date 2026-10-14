from pathlib import Path
import json,subprocess
H=Path(__file__).resolve().parent;R=H.parents[2];B=H/'tests';D=R/'outputs/2026-10-08-l22-runtime/diagnostics/tests'
base=(D/'progress-candidate.cpp').read_text().split('int main(){',1)[0]
s=(H/'src/LuaMod.cpp').read_text();start=s.index('    namespace L23GameThreadProbe {');end=s.index('\n#endif',start);actual=s[start:end]
code='#include <chrono>\n#include "L23GameThreadProbe.hpp"\n'+base+r'''
static bool game_thread=false;static int32_t native_id=7;
extern "C" int32_t scePthreadGetthreadid(void){return native_id;}
namespace RC { namespace Unreal { bool IsInGameThreadRaw(){return game_thread;} }
'''+actual+r'''
}
static const LuaMadeSimple::Lua* active_lua;
static int queue_api(lua_State* L){lua_pushvalue(L,1);int ref=luaL_ref(L,LUA_REGISTRYINDEX);RC::enqueue(active_lua,ref,0,RC::GameThreadExecutionMethod::ProcessEvent);return 0;}
static int broken_api(lua_State* L){return luaL_error(L,"enqueue-sentinel");}
int main(int argc,char** argv){
 if(argc!=2)return 2;std::string mode=argv[1];auto* L=luaL_newstate();LuaMadeSimple::Lua lua{L};active_lua=&lua;
 if(mode!="missing"){lua_pushcfunction(L,mode=="error"?broken_api:queue_api);lua_setglobal(L,"ExecuteInGameThread");}
 lua_pushinteger(L,1234);int owner;RC::L23GameThreadProbe::Queue(&owner,L);
 check(lua_gettop(L)==1&&lua_tointeger(L,1)==1234,"protected call restores main Lua stack");
 if(mode=="error"||mode=="missing"){
  check(logged("enqueue failed"),"missing/broken API reports refusal");check(RC::LuaMod::m_game_thread_actions.empty(),"no false queue entry");check(!logged("self-test queued"),"no false queued marker");
 } else {
  check(logged("self-test queued"),"actual production API registration used");check(!logged("self-test ran"),"no update-thread fake drain");check(RC::LuaMod::m_game_thread_actions.size()==1,"exactly one callback queued");
  RC::L23GameThreadProbe::Queue(&owner,L);check(RC::LuaMod::m_game_thread_actions.size()==1,"repeat start cannot duplicate diagnostic");
  if(mode=="cancel")RC::L23GameThreadProbe::Cancel(&owner);
  game_thread=mode!="wrong";native_id=55;RC::process_simple_actions(RC::LuaMod::m_game_thread_actions);
  check(RC::LuaMod::m_game_thread_actions.empty(),"actual production dispatcher retires callback");
  check(logged("self-test ran thread=55")== (mode=="success"),"only genuine game thread records run");
  if(mode!="success")check(logged("callback refused"),"cancelled/wrong-thread callback refuses");
 }
 lua_close(L);std::cout<<mode<<": "<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
'''
cpp=B/'queue.cpp';cpp.write_text(code);lua=R/'.superpowers/native-dependency-spike/ue4ss-lua-vm-experiment-1/upstream/lua-official/lua-5.4.7/src'
rows=[]
for mode,flags in [('normal',[]),('asan',['-fsanitize=address,undefined'])]:
 exe=B/('queue-'+mode);cmd=['clang++','-std=c++20','-O0','-pthread',*flags,'-I'+str(lua),'-I'+str(D/'shims'),'-I'+str(H/'include'),str(cpp),*[str(p) for p in sorted((D/'lua').glob('*.o'))],'-o',str(exe)]
 p=subprocess.run(cmd,capture_output=True,text=True);(B/('queue-'+mode+'-compile.log')).write_text(p.stdout+p.stderr)
 if p.returncode:print(p.stderr);raise SystemExit(p.returncode)
 for scenario in ['success','wrong','cancel','error','missing']:
  p=subprocess.run([str(exe),scenario],capture_output=True,text=True);print(p.stdout+p.stderr,end='');rows.append({'mode':mode,'scenario':scenario,'returncode':p.returncode,'output':p.stdout+p.stderr})
(B/'QUEUE-RESULTS.json').write_text(json.dumps({'results':rows,'qualification':'Real extracted Queue/callback/Poll plus unchanged production action insertion/drain bodies; host Lua; engine guard and native ID supplied by test. State tests cover monotonic timeout/concurrency. Lua C objects retained from l22 host compile are not sanitizer-instrumented.'},indent=2)+'\n')
assert all(r['returncode']==0 for r in rows)
