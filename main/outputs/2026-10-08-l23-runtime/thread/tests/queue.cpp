#include <chrono>
#include "L23GameThreadProbe.hpp"

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
#include "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l22-runtime/diagnostics/include/L22Diagnostics/Logging.hpp"

namespace LuaMadeSimple {struct Lua {lua_State* L;lua_State* get_lua_state()const{return L;}struct Registry{lua_State* L;void get_function_ref(int ref){lua_rawgeti(L,LUA_REGISTRYINDEX,ref);}};Registry registry()const{return{L};}void call_function(int n,int r)const{if(lua_pcall(L,n,r,0)!=LUA_OK){std::string e=lua_tostring(L,-1);lua_pop(L,1);throw std::runtime_error(e);}}std::string handle_error(const char* e)const{return e;}};}
namespace RC {
namespace Input{enum class Key{F7=118};}
enum class GameThreadExecutionMethod{ProcessEvent,EngineTick};
struct SimpleLuaAction{const LuaMadeSimple::Lua* lua;int lua_action_function_ref;int lua_thread_registry_index;};
struct LuaMod{using SimpleLuaAction=RC::SimpleLuaAction;inline static std::recursive_mutex m_thread_actions_mutex;inline static bool m_is_currently_executing_game_action=false,m_is_processing_actions=false;inline static std::vector<SimpleLuaAction> m_pending_engine_tick_actions,m_engine_tick_actions,m_pending_game_thread_actions,m_game_thread_actions;};
template<class F>void TRY(F&& f){try{f();}catch(const std::exception&e){events.push_back(e.what());}}
static auto process_simple_actions(std::vector<LuaMod::SimpleLuaAction>& actions) -> void
    {
        std::vector<LuaMod::SimpleLuaAction> to_run{};
        {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            if (actions.empty())
            {
                return;
            }
            to_run = std::move(actions);
            actions.clear();
        }

        std::vector<LuaMod::SimpleLuaAction> deferred{};
        for (auto& lua_data : to_run)
        {
            if (LuaMod::m_is_currently_executing_game_action)
            {
                deferred.emplace_back(std::move(lua_data));
                continue;
            }

            LuaMod::m_is_currently_executing_game_action = true;
            TRY([&]() {
                lua_data.lua->registry().get_function_ref(lua_data.lua_action_function_ref);
                L22Diagnostics::observe([&]() {
                    Output::send(STR("[l22] ExecuteInGameThread drained state={} ref={} (callback entry)\n"),
                                 static_cast<void*>(lua_data.lua->get_lua_state()), lua_data.lua_action_function_ref);
                });
                lua_data.lua->call_function(0, 0);
                L22Diagnostics::observe([&]() {
                    Output::send(STR("[l22] ExecuteInGameThread callback returned state={} ref={}\n"),
                                 static_cast<void*>(lua_data.lua->get_lua_state()), lua_data.lua_action_function_ref);
                });
            });
            luaL_unref(lua_data.lua->get_lua_state(), LUA_REGISTRYINDEX, lua_data.lua_action_function_ref);
            LuaMod::m_is_currently_executing_game_action = false;
        }

        if (!deferred.empty())
        {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            actions.insert(
                actions.end(),
                std::make_move_iterator(deferred.begin()),
                std::make_move_iterator(deferred.end()));
        }
    }
const auto lua_keybind_callback_lambda = [](const LuaMadeSimple::Lua& lua, const int callback_register_index, Input::Key callback_key) -> void {
                    L22Diagnostics::observe([&]() {
                        Output::send(STR("[l22] keybind callback entry key={} ref={}\n"), static_cast<int>(callback_key), callback_register_index);
                    });
                    std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                    try
                    {
                        lua.registry().get_function_ref(callback_register_index);
                        lua.call_function(0, 0);
                    }
                    catch (std::runtime_error& e)
                    {
                        Output::send<LogLevel::Error>(STR("{}\n"), ensure_str(lua.handle_error(e.what())));
                    }
                };
void enqueue(const LuaMadeSimple::Lua* hook_lua,int func_ref,int lua_thread_registry_index,GameThreadExecutionMethod method){
            SimpleLuaAction simpleAction{hook_lua, func_ref, lua_thread_registry_index};
            if (method == GameThreadExecutionMethod::EngineTick)
            {
                // Use pending queue if we're currently iterating to prevent iterator invalidation
                if (LuaMod::m_is_processing_actions)
                {
                    LuaMod::m_pending_engine_tick_actions.emplace_back(simpleAction);
                }
                else
                {
                    LuaMod::m_engine_tick_actions.emplace_back(simpleAction);
                }
            }
            else
            {
                // Use pending queue if we're currently iterating to prevent iterator invalidation
                if (LuaMod::m_is_processing_actions)
                {
                    LuaMod::m_pending_game_thread_actions.emplace_back(simpleAction);
                }
                else
                {
                    LuaMod::m_game_thread_actions.emplace_back(simpleAction);
                }
            }

            L22Diagnostics::observe([&]() {
                Output::send(STR("[l22] ExecuteInGameThread queued method={} state={} ref={}\n"),
                             static_cast<int>(method), static_cast<void*>(hook_lua->get_lua_state()), func_ref);
            });


}
}
static int successful(lua_State*){events.push_back("callback ran");++calls;return 0;}
static int failing(lua_State* L){events.push_back("callback ran");++calls;return luaL_error(L,"callback sentinel");}

static bool game_thread=false;static int32_t native_id=7;
extern "C" int32_t scePthreadGetthreadid(void){return native_id;}
namespace RC { namespace Unreal { bool IsInGameThreadRaw(){return game_thread;} }
    namespace L23GameThreadProbe {
        extern "C" int32_t scePthreadGetthreadid(void);
        static State state;
        static uint64_t now_ms() noexcept {
            return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        }
        static int callback(lua_State*) noexcept {
            try {
                const int32_t tid=scePthreadGetthreadid();
                const bool genuine=Unreal::IsInGameThreadRaw();
                const auto result=state.ran(now_ms(),tid,genuine);
                L22Diagnostics::observe([&] {
                    if(result.accepted) Output::send(STR("[l23] game-thread self-test ran thread={} drain={}ms late={}\n"),result.thread,result.elapsed,result.late);
                    else Output::send(STR("[l23] game-thread self-test callback refused thread={} genuine={} state={}\n"),tid,genuine,static_cast<int>(state.status()));
                });
            } catch(...) { /* A diagnostic must not throw across the Lua C ABI. */ }
            return 0;
        }
        static int request(lua_State* L) {
            // Run all potentially raising Lua operations inside the caller's
            // pcall. No C++ owner lives on this Lua-longjmp frame.
            lua_getglobal(L,"ExecuteInGameThread");
            if(!lua_isfunction(L,-1)) return luaL_error(L,"ExecuteInGameThread is unavailable");
            lua_pushcfunction(L,callback);
            lua_call(L,1,0);
            return 0;
        }
        static void Queue(const void* owner,lua_State* L) noexcept {
            try {
                // Same lock as the production dispatcher: no concurrent drain
                // can race the queued marker or the protected main-state call.
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                if(!state.begin(owner,now_ms(),scePthreadGetthreadid())) return;
                const int top=lua_gettop(L);
                lua_pushcfunction(L,request);
                const int result=lua_pcall(L,0,0,0);
                if(result==LUA_OK) {
                    state.queued();
                    L22Diagnostics::observe([&]{Output::send(STR("[l23] game-thread self-test queued via ExecuteInGameThread deadline=30000ms\n"));});
                } else {
                    state.failed();
                    L22Diagnostics::observe([&]{const char* msg=lua_tostring(L,-1);Output::send(STR("[l23] game-thread self-test enqueue failed: {}\n"),ensure_str(msg?msg:"non-string Lua error"));});
                }
                lua_settop(L,top);
            } catch(...) { state.failed(); }
        }
        static void Cancel(const void* owner) noexcept {
            try { if(state.cancel(owner)) L22Diagnostics::observe([]{Output::send(STR("[l23] game-thread self-test cancelled before Lua owner teardown\n"));}); }
            catch(...) {}
        }
        void Poll() noexcept {
            try { if(state.timeout(now_ms())) L22Diagnostics::observe([]{Output::send(STR("[l23] game-thread self-test NOT DRAINED within 30000ms; startup continues; callback remains owned by the normal queue\n"));}); }
            catch(...) {}
        }
    }
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
