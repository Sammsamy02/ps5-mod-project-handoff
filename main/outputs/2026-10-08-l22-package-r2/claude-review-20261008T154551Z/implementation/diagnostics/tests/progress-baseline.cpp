
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
                lua_data.lua->call_function(0, 0);
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
const auto lua_keybind_callback_lambda = [](const LuaMadeSimple::Lua& lua, const int callback_register_index) -> void {
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
RC::lua_keybind_callback_lambda(lua,ref);
 check(logged("keybind callback entry key=118"),"key callback logs actual key");check(logged("callback ran"),"key callback still executes referenced Lua closure");luaL_unref(L,LUA_REGISTRYINDEX,ref);
 lua_close(L);std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
