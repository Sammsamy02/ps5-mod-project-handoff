from pathlib import Path
import json,subprocess,sys,re
O=Path(__file__).resolve().parent
baseline='--baseline' in sys.argv
source=Path(json.loads((O/'ORIGINALS.json').read_text())[0]['original']) if baseline else O/'src/LuaMod.cpp'
s=source.read_text()
def block(s,marker):
 p=s.index(marker);b=s.index('{',p);depth=0;i=b
 while i<len(s):
  if s.startswith('//',i):i=s.index('\n',i);continue
  if s.startswith('/*',i):i=s.index('*/',i)+2;continue
  if s[i] in '\"\'':
   q=s[i];i+=1
   while i<len(s):
    if s[i]=='\\':i+=2;continue
    if s[i]==q:i+=1;break
    i+=1
   continue
  if s[i]=='{':depth+=1
  elif s[i]=='}':
   depth-=1
   if not depth:return s[p:i+1]
  i+=1
 raise ValueError(marker)
drain=block(s,'static auto process_simple_actions(')
start=s.index('            SimpleLuaAction simpleAction{hook_lua, func_ref, lua_thread_registry_index};')
end=s.index('#if defined(__PROSPERO__)',start)
queue=s[start:end]
keypart=s[s.index('register_function("RegisterKeyBind"'):]
callback=block(keypart,'const auto lua_keybind_callback_lambda =')+';'
code=r'''
#include <vector>
#include <string>
#include <string_view>
#include <mutex>
#include <algorithm>
#include <iterator>
#include <atomic>
#include <stdexcept>
#include <format>
#include <iostream>
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}
#define STR(x) x
static std::vector<std::string> events;static int passed=0,failed=0,calls=0;
void check(bool ok,const char* name){if(ok)++passed;else{++failed;std::cerr<<"FAIL: "<<name<<"\n";}}
bool logged(std::string_view text){return std::any_of(events.begin(),events.end(),[&](const auto&s){return s.find(text)!=std::string::npos;});}
namespace RC {
enum class LogLevel{Error,Verbose};
template<class T>auto ensure_str(const T& s){return std::string(s);}
namespace Output{template<LogLevel Level=LogLevel::Verbose,class...A>void send(const char* s,A&&... a){events.push_back(std::vformat(s,std::make_format_args(a...)));}}
}
'''+('#include "'+str(O/'include/L22Diagnostics/Logging.hpp')+'"\n')+r'''
namespace LuaMadeSimple {struct Lua {lua_State* L;lua_State* get_lua_state()const{return L;}struct Registry{lua_State* L;void get_function_ref(int ref){lua_rawgeti(L,LUA_REGISTRYINDEX,ref);}};Registry registry()const{return{L};}void call_function(int n,int r)const{if(lua_pcall(L,n,r,0)!=LUA_OK){std::string e=lua_tostring(L,-1);lua_pop(L,1);throw std::runtime_error(e);}}std::string handle_error(const char* e)const{return e;}};}
namespace RC {
namespace Input{enum class Key{F7=118};}
enum class GameThreadExecutionMethod{ProcessEvent,EngineTick};
struct SimpleLuaAction{const LuaMadeSimple::Lua* lua;int lua_action_function_ref;int lua_thread_registry_index;};
struct LuaMod{using SimpleLuaAction=RC::SimpleLuaAction;inline static std::recursive_mutex m_thread_actions_mutex;inline static bool m_is_currently_executing_game_action=false,m_is_processing_actions=false;inline static std::vector<SimpleLuaAction> m_pending_engine_tick_actions,m_engine_tick_actions,m_pending_game_thread_actions,m_game_thread_actions;};
template<class F>void TRY(F&& f){try{f();}catch(const std::exception&e){events.push_back(e.what());}}
'''+drain+'\n'+callback+r'''
void enqueue(const LuaMadeSimple::Lua* hook_lua,int func_ref,int lua_thread_registry_index,GameThreadExecutionMethod method){
'''+queue+r'''
}
}
static int successful(lua_State*){events.push_back("callback ran");++calls;return 0;}
static int failing(lua_State* L){events.push_back("callback ran");++calls;return luaL_error(L,"callback sentinel");}
int main(){
 auto* L=luaL_newstate();LuaMadeSimple::Lua lua{L};
 for(bool processing:{false,true})for(auto method:{RC::GameThreadExecutionMethod::EngineTick,RC::GameThreadExecutionMethod::ProcessEvent}){
  events.clear();RC::LuaMod::m_is_processing_actions=processing;RC::enqueue(&lua,123,456,method);
  auto& v=method==RC::GameThreadExecutionMethod::EngineTick?(processing?RC::LuaMod::m_pending_engine_tick_actions:RC::LuaMod::m_engine_tick_actions):(processing?RC::LuaMod::m_pending_game_thread_actions:RC::LuaMod::m_game_thread_actions);
  check(v.size()==1&&v[0].lua==&lua&&v[0].lua_action_function_ref==123&&v[0].lua_thread_registry_index==456,"enqueue preserves destination and complete action");check(logged("ExecuteInGameThread queued"),"queue records progress after insertion");v.clear();
 }
 for(auto fn:{successful,failing}){
  events.clear();lua_pushcfunction(L,fn);int ref=luaL_ref(L,LUA_REGISTRYINDEX);std::vector<RC::SimpleLuaAction> a{{&lua,ref,0}};int before=calls;RC::process_simple_actions(a);
  check(a.empty()&&calls==before+1&&!RC::LuaMod::m_is_currently_executing_game_action,"drain runs once clears source and releases executing flag");check(logged("ExecuteInGameThread drained"),"drain logs callback entry");
  auto e=std::find_if(events.begin(),events.end(),[](const auto&s){return s.find("ExecuteInGameThread drained")!=std::string::npos;});auto run=std::find(events.begin(),events.end(),"callback ran");check(e<run,"drained marker precedes callback execution");
  check(logged("ExecuteInGameThread callback returned")== (fn==successful),"returned marker only follows successful callback");
  lua_rawgeti(L,LUA_REGISTRYINDEX,ref);check(!lua_isfunction(L,-1),"drain preserves callback unref");lua_pop(L,1);
 }
 events.clear();lua_pushcfunction(L,successful);int ref=luaL_ref(L,LUA_REGISTRYINDEX);std::vector<RC::SimpleLuaAction> a{{&lua,ref,0}};RC::LuaMod::m_is_currently_executing_game_action=true;int before=calls;RC::process_simple_actions(a);check(a.size()==1&&calls==before&&!logged("drained"),"reentrant action stays queued without false drained marker");RC::LuaMod::m_is_currently_executing_game_action=false;luaL_unref(L,LUA_REGISTRYINDEX,ref);
 events.clear();lua_pushcfunction(L,successful);ref=luaL_ref(L,LUA_REGISTRYINDEX);
'''+('RC::lua_keybind_callback_lambda(lua,ref);' if baseline else 'RC::lua_keybind_callback_lambda(lua,ref,RC::Input::Key::F7);')+r'''
 check(logged("keybind callback entry key=118"),"key callback logs actual key");check(logged("callback ran"),"key callback still executes referenced Lua closure");luaL_unref(L,LUA_REGISTRYINDEX,ref);
 lua_close(L);std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
'''
B=O/'tests';suffix='baseline' if baseline else 'candidate';cpp=B/f'progress-{suffix}.cpp';cpp.write_text(code)
lua=O.parents[2]/'.superpowers/native-dependency-spike/ue4ss-lua-vm-experiment-1/upstream/lua-official/lua-5.4.7/src'
cmd=['clang++','-std=c++20','-O0','-I'+str(lua),'-I'+str(B/'shims'),str(cpp),*[str(x) for x in sorted((B/'lua').glob('*.o'))],'-o',str(B/f'progress-{suffix}')]
p=subprocess.run(cmd,text=True,capture_output=True)
(B/f'progress-{suffix}-compile.log').write_text(p.stdout+p.stderr)
if p.returncode:print(p.stderr);sys.exit(p.returncode)
p=subprocess.run([str(B/f'progress-{suffix}')],text=True,capture_output=True);print(p.stdout+p.stderr)
(B/f'progress-{suffix}.json').write_text(json.dumps({'source':str(source),'baseline':baseline,'command':cmd,'returncode':p.returncode,'output':p.stdout+p.stderr,'actual_extracted_controls':['ExecuteInGameThread queue insertion block','process_simple_actions','RegisterKeyBind callback lambda']},indent=2)+'\n')
sys.exit(p.returncode)
