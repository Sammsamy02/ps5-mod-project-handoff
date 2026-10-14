from pathlib import Path
import re,json,hashlib
O=Path(__file__).resolve().parent
# Originals are immutable; regeneration always starts from their pinned bytes.
entries=json.loads((O/'ORIGINALS.json').read_text())
texts={Path(x['copy']).name:Path(x['original']).read_text() for x in entries}
def change(s,a,b,n=1):
 assert s.count(a)==n,(a[:100],s.count(a),n)
 return s.replace(a,b)
def match_end(s,start,op='(',cl=')'):
 depth=0;i=start
 while i<len(s):
  if s.startswith('//',i):i=s.find('\n',i);continue
  if s.startswith('/*',i):i=s.index('*/',i)+2;continue
  if s[i] in '\"\'':
   q=s[i];i+=1
   while i<len(s):
    if s[i]=='\\':i+=2;continue
    if s[i]==q:i+=1;break
    i+=1
   continue
  if s[i]==op:depth+=1
  elif s[i]==cl:
   depth-=1
   if depth==0:return i
  i+=1
 raise Exception('unclosed')
def log_direct_errors(s,context):
 needle='lua.throw_error(';off=0
 while (pos:=s.find(needle,off))>=0:
  start=pos+len(needle)-1;end=match_end(s,start)
  replacement='L22Diagnostics::throw_lua_error(lua, '+s[start+1:end]+', "'+context+'")'
  s=s[:pos]+replacement+s[end+1:];off=pos+len(replacement)
 return s
def catch_body(s,marker,context):
 p=s.index(marker);b=s.index('{',p);e=match_end(s,b,'{','}')
 original=s[b+1:e]
 wrapped='''{
        try
        {'''+original+'''
        }
        catch (const std::exception& error)
        {
            L22Diagnostics::error("'''+context+'''", error.what());
            throw;
        }
        catch (...)
        {
            L22Diagnostics::error("'''+context+'''", "non-standard C++ exception");
            throw;
        }
    }'''
 return s[:b]+wrapped+s[e+1:]
helper=O/'include/L22Diagnostics/Logging.hpp';helper.parent.mkdir(parents=True,exist_ok=True)
helper.write_text('''#pragma once

#include <atomic>
#include <DynamicOutput/Output.hpp>
#include <Helpers/String.hpp>

namespace RC::L22Diagnostics
{
    // Diagnostics must never replace the original return value or exception.
    // No game call is made here; formatting/logger failures are observational.
    template <typename Callable>
    inline void observe(Callable&& callback) noexcept
    {
        try { callback(); } catch (...) {}
    }

    template <typename Message>
    inline void error(const char* context, const Message& message) noexcept
    {
        observe([&]() {
            Output::send<LogLevel::Error>(STR("[l22] {}: {}\\n"), ensure_str(context), ensure_str(message));
        });
    }

    template <typename Lua, typename Message>
    inline void throw_lua_error(const Lua& lua, const Message& message, const char* context)
    {
        error(context, message);
        lua.throw_error(message);
    }

    // Reuse the member name/index already read by Lua property lookup. Do not
    // introduce FName construction, string conversion, or a game call to label progress.
    inline std::atomic<int> AddToViewportNameIndex{-1};
    inline std::atomic<void*> AddToViewportFunction{nullptr};

    template <typename Name>
    inline void record_member_lookup(const Name& name, int index) noexcept
    {
        if (name == STR("AddToViewport")) AddToViewportNameIndex.store(index, std::memory_order_relaxed);
    }

    inline void record_function_lookup(void* function, int index) noexcept
    {
        if (index == AddToViewportNameIndex.load(std::memory_order_relaxed))
            AddToViewportFunction.store(function, std::memory_order_relaxed);
    }

    inline bool is_add_to_viewport(void* function) noexcept
    {
        return function && function == AddToViewportFunction.load(std::memory_order_relaxed);
    }
}
''')
# Shared property error builder logs the completed existing message before Lua raises.
h=texts['LuaUObject.hpp'];h=change(h,'#include <format>','#include <L22Diagnostics/Logging.hpp>\n#include <format>')
h=log_direct_errors(h,'property/UObject')
h=change(h,'            Unreal::FName property_name = Unreal::FName(member_name, Unreal::FNAME_Find);','            Unreal::FName property_name = Unreal::FName(member_name, Unreal::FNAME_Find);\n            L22Diagnostics::record_member_lookup(member_name, property_name.GetComparisonIndex());')
h=catch_body(h,'auto static prepare_to_handle(const Operation operation, const LuaMadeSimple::Lua& lua) -> void','property access/set')
texts['LuaUObject.hpp']=h
u=texts['LuaUObject.cpp'];u='#include <L22Diagnostics/Logging.hpp>\n'+u
u=log_direct_errors(u,'UFunction/property')
u=change(u,'                luaL_error(Lua.get_lua_state(), "UFunction out-parameter capacity exceeded");','                L22Diagnostics::error("UFunction", "UFunction out-parameter capacity exceeded");\n                luaL_error(Lua.get_lua_state(), "UFunction out-parameter capacity exceeded");')
u=change(u,'        catch (...)\n        {\n            // Keep C++ exceptions', '''        catch (const std::exception& error)
        {
            L22Diagnostics::error("UFunction", error.what());
            Invocation->PendingException = std::current_exception();
            return 0;
        }
        catch (...)
        {
            L22Diagnostics::error("UFunction", "non-standard C++ exception");
            // Keep C++ exceptions''')
u=change(u,'        if (Status != LUA_OK) return lua_error(LuaState);','''        if (Status != LUA_OK)
        {
            // Do not coerce numeric/table error objects: propagate the same object.
            if (lua_type(LuaState, -1) == LUA_TSTRING)
                L22Diagnostics::error("UFunction Lua error", lua_tostring(LuaState, -1));
            else
                L22Diagnostics::error("UFunction Lua error", "non-string Lua error (object preserved)");
            return lua_error(LuaState);
        }''')
u=change(u,'        calling_context->ProcessEvent(func, dynamic_unreal_function_data.data);','''        calling_context->ProcessEvent(func, dynamic_unreal_function_data.data);
        if (L22Diagnostics::is_add_to_viewport(func))
        {
            L22Diagnostics::observe([&]() {
                Output::send(STR("[l22] AddToViewport returned object={} function={} (void return; not visual proof)\\n"),
                             static_cast<void*>(calling_context), static_cast<void*>(func));
            });
        }''')
u=change(u,'                push_functionproperty(FunctionPusherParams{.lua = lua, .base = base, .function = func});','                L22Diagnostics::record_function_lookup(func, property_name.GetComparisonIndex());\n                push_functionproperty(FunctionPusherParams{.lua = lua, .base = base, .function = func});')
u=catch_body(u,'auto handle_unreal_property_value(','property access/set')
u=catch_body(u,'auto RemoteUnrealParam::prepare_to_handle(','property access/set')
texts['LuaUObject.cpp']=u
f=texts['FText.cpp'];f='#include <L22Diagnostics/Logging.hpp>\n'+f
f=catch_body(f,'auto FText::construct_with_string(','FText construction')
f=catch_body(f,'FText::FText(const CharType* String)','FText construction')
texts['FText.cpp']=f
lf=texts['LuaFText.cpp'];lf='#include <L22Diagnostics/Logging.hpp>\n'+lf
lf=log_direct_errors(lf,'FText')
# Construction arguments and Lua wrapping can throw independently of engine conversion.
lf=change(lf,'            LuaType::FText::construct(lua, Unreal::FText(text_string));','''            try
            {
                LuaType::FText::construct(lua, Unreal::FText(text_string));
            }
            catch (const std::exception& error)
            {
                L22Diagnostics::error("FText Lua construction", error.what());
                throw;
            }
            catch (...)
            {
                L22Diagnostics::error("FText Lua construction", "non-standard C++ exception");
                throw;
            }''')
texts['LuaFText.cpp']=lf
g=texts['UObjectGlobals.cpp'];g='#include <L22Diagnostics/Logging.hpp>\n'+g
# Both modern wrapper shapes retain their exact early-null return behavior.
g=change(g,'            if (!StaticConstructObjectInternal.is_ready()) { return nullptr; }','''            if (!StaticConstructObjectInternal.is_ready())
            {
                L22Diagnostics::error("StaticConstructObject", "native function is not ready");
                return nullptr;
            }''',2)
g=change(g,'            if (Params.Class == nullptr) { return nullptr; }','''            if (Params.Class == nullptr)
            {
                L22Diagnostics::error("StaticConstructObject", "class is null");
                return nullptr;
            }''',2)
g=change(g,'            return StaticConstructObjectInternal(Params);','''            try
            {
                auto* result = StaticConstructObjectInternal(Params);
                if (!result) L22Diagnostics::error("StaticConstructObject", "native function returned null");
                return result;
            }
            catch (const std::exception& error)
            {
                L22Diagnostics::error("StaticConstructObject", error.what());
                throw;
            }
            catch (...)
            {
                L22Diagnostics::error("StaticConstructObject", "non-standard C++ exception");
                throw;
            }''',2)
texts['UObjectGlobals.cpp']=g
ss=texts['LuaUScriptStruct.cpp'];ss='#include <L22Diagnostics/Logging.hpp>\n'+ss
ss=log_direct_errors(ss,'nested property access/set')
ss=catch_body(ss,'auto UScriptStruct::handle_unreal_property_value(','nested property access/set')
ss=catch_body(ss,'auto UScriptStruct::prepare_to_handle(','nested property access/set')
texts['LuaUScriptStruct.cpp']=ss
m=texts['LuaMod.cpp'];m='#include <L22Diagnostics/Logging.hpp>\n'+m
# Restrict direct-error instrumentation to the requested Lua entrypoints.
for entry in ['RegisterKeyBind','StaticConstructObject','ExecuteInGameThread']:
 start=m.index('register_function("'+entry+'"');b=m.index('{',start);end=match_end(m,b,'{','}')
 m=m[:b+1]+log_direct_errors(m[b+1:end],entry)+m[end:]
# Only the synchronous key path used by the frozen mod is changed.
start=m.index('register_function("RegisterKeyBind"');end=m.index('register_function("UnregisterHook"',start)
p=m[start:end]
p=change(p,'const int callback_register_index) -> void {','const int callback_register_index, Input::Key callback_key) -> void {')
p=change(p,'                    std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};','''                    L22Diagnostics::observe([&]() {
                        Output::send(STR("[l22] keybind callback entry key={} ref={}\\n"), static_cast<int>(callback_key), callback_register_index);
                    });
                    std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};''')
p=change(p,'[&lua, lua_callback_registry_index, lua_keybind_callback_lambda]()','[&lua, lua_callback_registry_index, lua_keybind_callback_lambda, key_to_register]()',3)
p=change(p,'lua_keybind_callback_lambda(lua, lua_callback_registry_index);','lua_keybind_callback_lambda(lua, lua_callback_registry_index, key_to_register);',3)
# Mark each void registration after it returns; modifier list reflects supplied values.
first='''                            1, mod);
                }
                else if (lua.is_table())'''
p=change(p,first,'''                            1, mod);
                    L22Diagnostics::observe([&]() {
                        Output::send(STR("[l22] RegisterKeyBind returned key={} modifiers=0 (registration call returned)\\n"), static_cast<int>(key_to_register));
                    });
                }
                else if (lua.is_table())''')
last='''                                1, mod);
                    }
                }
                else'''
p=change(p,last,'''                                1, mod);
                    }
                    L22Diagnostics::observe([&]() {
                        Output::send(STR("[l22] RegisterKeyBind returned key={} modifiers={} (registration call returned)\\n"), static_cast<int>(key_to_register), table_counter);
                        for (uint8_t index = 0; index < table_counter; ++index)
                            Output::send(STR("[l22] RegisterKeyBind modifier[{}]={} key={}\\n"), index, static_cast<int>(modifier_keys[index]), static_cast<int>(key_to_register));
                    });
                }
                else''')
m=m[:start]+p+m[end:]
# C++ failures during the binding body are logged before the existing Lua bridge catches them.
for entry in ['RegisterKeyBind','StaticConstructObject','ExecuteInGameThread']:
 m=catch_body(m,'register_function("'+entry+'"',entry)
m=change(m,'            Unreal::UObject* created_object = Unreal::UObjectGlobals::StaticConstructObject(params);','''            Unreal::UObject* created_object = Unreal::UObjectGlobals::StaticConstructObject(params);
            if (created_object)
            {
                L22Diagnostics::observe([&]() {
                    Output::send(STR("[l22] widget/object constructed via StaticConstructObject class={} object={} (non-null return)\\n"),
                                 static_cast<void*>(param_class), static_cast<void*>(created_object));
                });
            }
            else
            {
                L22Diagnostics::error("StaticConstructObject Lua", "returned null; preserving invalid UObject result");
            }''')
m=change(m,'            // [R3-L11EL] THE ORDER GUARANTEE','''            // [R3-L11EL] THE ORDER GUARANTEE''') # anchor asserted below
needle='''#if defined(__PROSPERO__)
            // [R3-L11EL] THE ORDER GUARANTEE'''
m=change(m,needle,'''            L22Diagnostics::observe([&]() {
                Output::send(STR("[l22] ExecuteInGameThread queued method={} state={} ref={}\\n"),
                             static_cast<int>(method), static_cast<void*>(hook_lua->get_lua_state()), func_ref);
            });

#if defined(__PROSPERO__)
            // [R3-L11EL] THE ORDER GUARANTEE''')
m=change(m,'''                lua_data.lua->registry().get_function_ref(lua_data.lua_action_function_ref);
                lua_data.lua->call_function(0, 0);''','''                lua_data.lua->registry().get_function_ref(lua_data.lua_action_function_ref);
                L22Diagnostics::observe([&]() {
                    Output::send(STR("[l22] ExecuteInGameThread drained state={} ref={} (callback entry)\\n"),
                                 static_cast<void*>(lua_data.lua->get_lua_state()), lua_data.lua_action_function_ref);
                });
                lua_data.lua->call_function(0, 0);
                L22Diagnostics::observe([&]() {
                    Output::send(STR("[l22] ExecuteInGameThread callback returned state={} ref={}\\n"),
                                 static_cast<void*>(lua_data.lua->get_lua_state()), lua_data.lua_action_function_ref);
                });''')
m=change(m,'''                    Output::send<LogLevel::Error>(STR("Failed to execute main script: {}\\n"), ensure_str(main_script_path));
                }''','''                    Output::send<LogLevel::Error>(STR("Failed to execute main script: {}\\n"), ensure_str(main_script_path));
                }
                else
                {
                    L22Diagnostics::observe([&]() {
                        Output::send(STR("[l22] Lua mod started: {} (main script returned)\\n"), m_mod_name);
                    });
                }''')
texts['LuaMod.cpp']=m
for x in entries:Path(x['copy']).write_text(texts[Path(x['copy']).name])
print('Instrumented',len(entries),'copied sources/headers; added no-throw logging helper.')
