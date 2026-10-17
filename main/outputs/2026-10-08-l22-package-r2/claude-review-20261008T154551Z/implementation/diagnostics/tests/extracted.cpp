
#include <atomic>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <exception>
#include <format>
#include <utility>
#include <iostream>
#include <algorithm>
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#define STR(x) x
using StringType=std::string;
namespace fmt { using std::format; }
static std::vector<std::string> events;
static bool logger_throws=false;
static int failed=0,passed=0;
void check(bool ok,const char* n){if(ok){++passed;}else{++failed;std::cerr<<"FAIL: "<<n<<"\n";}}
namespace RC {
enum class LogLevel {Error,Warning,Verbose};
template<class T> auto ensure_str(const T& x) { return std::string(x); }
namespace Output {
template<LogLevel L=LogLevel::Verbose,class... A> void send(const char* s,A&&... a){if(logger_throws)throw std::runtime_error("logger failed");events.push_back(std::vformat(s,std::make_format_args(a...)));}
}
}
#include "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l22-runtime/diagnostics/include/L22Diagnostics/Logging.hpp"
namespace LuaMadeSimple {
struct Lua {lua_State* L; auto get_lua_state()const{return L;} void throw_error(const std::string& e)const {lua_pushlstring(L,e.data(),e.size());lua_error(L);} };
}
namespace RC::LuaType {
int mode=0; int object_ref=LUA_NOREF;
struct FNativeUFunctionCall {const LuaMadeSimple::Lua& Lua;std::exception_ptr PendingException{};explicit FNativeUFunctionCall(const LuaMadeSimple::Lua& l):Lua(l){}~FNativeUFunctionCall(){events.push_back("owner cleaned");}};
static int call_ufunction_from_lua_impl(const LuaMadeSimple::Lua& lua,FNativeUFunctionCall&){
 if(mode==1)throw std::runtime_error("native sentinel");
 if(mode==2){lua_pushliteral(lua.L,"lua sentinel");return lua_error(lua.L);}
 if(mode==3){lua_newtable(lua.L);lua_pushvalue(lua.L,-1);object_ref=luaL_ref(lua.L,LUA_REGISTRYINDEX);return lua_error(lua.L);}
 if(mode==4){lua_pushinteger(lua.L,42);return lua_error(lua.L);}
 lua_pushinteger(lua.L,7);lua_pushliteral(lua.L,"ok");return 2;
}
static auto protected_ufunction_call(lua_State* LuaState) -> int
    {
        auto* Invocation = static_cast<FNativeUFunctionCall*>(lua_touserdata(LuaState, lua_upvalueindex(1)));
        try
        {
            return call_ufunction_from_lua_impl(Invocation->Lua, *Invocation);
        }
        catch (const std::exception& error)
        {
            L22Diagnostics::error("UFunction", error.what());
            Invocation->PendingException = std::current_exception();
            return 0;
        }
        catch (...)
        {
            L22Diagnostics::error("UFunction", "non-standard C++ exception");
            // Keep C++ exceptions from crossing the C Lua protected frame. The
            // caller rethrows while its reference owner can still unwind normally.
            Invocation->PendingException = std::current_exception();
            return 0;
        }
    }
auto call_ufunction_from_lua(const LuaMadeSimple::Lua& lua) -> int
    {
        auto* LuaState = lua.get_lua_state();
        const int ArgumentCount = lua_gettop(LuaState);
        int Status;
        {
            FNativeUFunctionCall Invocation{lua};
            lua_pushlightuserdata(LuaState, &Invocation);
            lua_pushcclosure(LuaState, protected_ufunction_call, 1);
            lua_insert(LuaState, 1);
            // The inner C frame sees the original arguments at their same indices.
            Status = lua_pcall(LuaState, ArgumentCount, LUA_MULTRET, 0);
            if (Invocation.PendingException)
            {
                std::rethrow_exception(Invocation.PendingException);
            }
        } // Unref committed out tables on success, Lua error and C++ unwind.

        // Preserve the original error object, including non-string errors. All
        // owned C++ locals are gone before this final longjmp to the Lua caller.
        if (Status != LUA_OK)
        {
            // Do not coerce numeric/table error objects: propagate the same object.
            if (lua_type(LuaState, -1) == LUA_TSTRING)
                L22Diagnostics::error("UFunction Lua error", lua_tostring(LuaState, -1));
            else
                L22Diagnostics::error("UFunction Lua error", "non-string Lua error (object preserved)");
            return lua_error(LuaState);
        }
        return lua_gettop(LuaState);
    }
struct Property {std::string GetFullName(){return "Unused";}};
auto to_string(const std::string& s){return s;}
struct PusherParams {
 const LuaMadeSimple::Lua& lua;void* base=nullptr;void* data=nullptr;Property* property=nullptr;int stored_at_index=1;bool create_new_if_get_non_trivial_local=false;
 std::string get_operation()const{return "Set";}
 std::string get_stack_dump()const{return "stack sentinel";}
 template<class... A> void throw_error_internal_append_args(std::string&,A&&...)const{}
 template<class... Args>
auto throw_error(std::string_view handler_name, std::string_view format, Args&&... args) const -> void
        {
            std::string error_message{};
            error_message.append(fmt::format("[{}] Error:\n", handler_name));
            error_message.append(fmt::format(" {}\n", format));
            error_message.append(fmt::format("    Operation: {}\n", get_operation()));
            error_message.append(fmt::format("    Base: {}\n", static_cast<void*>(base)));
            error_message.append(fmt::format("    Data: {}\n", data));
            error_message.append(fmt::format("    Property ({}): {}\n", static_cast<void*>(property), property ? to_string(property->GetFullName()) : "null"));
            error_message.append(fmt::format("    StoredAtIndex: {}\n", stored_at_index));
            error_message.append(fmt::format("    CreateNewIfGetNonTrivialLocal: {}\n", create_new_if_get_non_trivial_local));
            throw_error_internal_append_args(error_message, args...);
            error_message.append(get_stack_dump());
            L22Diagnostics::throw_lua_error(lua, error_message, "property/UObject");
        }
};
}
namespace RC::Unreal {
struct FString{};
struct FText {void construct_with_string(const FString&&);};
namespace UKismetTextLibrary {inline bool fail=false;inline FText Conv_StringToText(const FString&&){if(fail)throw std::runtime_error("text sentinel");return {};}}
auto FText::construct_with_string(const FString&& String) -> void
    {
        try
        {
        *this = UKismetTextLibrary::Conv_StringToText(std::forward<const FString>(String));
    
        }
        catch (const std::exception& error)
        {
            L22Diagnostics::error("FText construction", error.what());
            throw;
        }
        catch (...)
        {
            L22Diagnostics::error("FText construction", "non-standard C++ exception");
            throw;
        }
    }
struct UObject{};struct UClass{};struct FStaticConstructObjectParameters {UClass* Class;};
static bool ready=true;static int sco_mode=0;static UObject object;
template<class> struct Function {Function(void*){} bool is_ready(){return ready;}UObject* operator()(const FStaticConstructObjectParameters&){if(sco_mode==2)throw std::runtime_error("sco sentinel");return sco_mode==1?nullptr:&object;}};
namespace GlobalState {struct Holder{void* get_function_address(){return nullptr;}};inline Holder StaticConstructObjectInternal426;}
static auto StaticConstructObject(const FStaticConstructObjectParameters& Params) -> UObject*
        {
            static Function<UObject*(const FStaticConstructObjectParameters&)> StaticConstructObjectInternal = [&]() {
                return GlobalState::StaticConstructObjectInternal426.get_function_address();
            }();

            if (!StaticConstructObjectInternal.is_ready())
            {
                L22Diagnostics::error("StaticConstructObject", "native function is not ready");
                return nullptr;
            }
            if (Params.Class == nullptr)
            {
                L22Diagnostics::error("StaticConstructObject", "class is null");
                return nullptr;
            }

            try
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
            }
        }
}
static int invoke(lua_State* L){const LuaMadeSimple::Lua lua{L};return RC::LuaType::call_ufunction_from_lua(lua);}
static int property_error(lua_State* L){const LuaMadeSimple::Lua lua{L};RC::LuaType::PusherParams{lua}.throw_error("pusher sentinel","bad property");return 0;}
bool logged(std::string_view part){return std::any_of(events.begin(),events.end(),[&](const auto&e){return e.find(part)!=std::string::npos;});}
int main(){
 auto* L=luaL_newstate();luaL_openlibs(L);
 RC::LuaType::mode=0;events.clear();lua_pushcfunction(L,invoke);int status=lua_pcall(L,0,LUA_MULTRET,0);check(status==LUA_OK&&lua_gettop(L)==2&&lua_tointeger(L,1)==7&&std::string(lua_tostring(L,2))=="ok","UFunction success returns unchanged");lua_settop(L,0);
 RC::LuaType::mode=1;events.clear();try{LuaMadeSimple::Lua lua{L};RC::LuaType::call_ufunction_from_lua(lua);check(false,"native exception preserved");}catch(const std::runtime_error&e){check(std::string(e.what())=="native sentinel","native exception preserved");check(logged("native sentinel"),"native exception logged before escape");check(events.back()=="owner cleaned","UFunction owner still unwinds");}lua_settop(L,0);
 RC::LuaType::mode=2;events.clear();lua_pushcfunction(L,invoke);status=lua_pcall(L,0,LUA_MULTRET,0);check(status==LUA_ERRRUN&&std::string(lua_tostring(L,-1))=="lua sentinel","Lua pcall preserves exact string error");check(logged("lua sentinel"),"swallowed pcall error logged before outer pcall resumes");lua_settop(L,0);
 RC::LuaType::mode=3;events.clear();lua_pushcfunction(L,invoke);status=lua_pcall(L,0,LUA_MULTRET,0);lua_rawgeti(L,LUA_REGISTRYINDEX,RC::LuaType::object_ref);check(status==LUA_ERRRUN&&lua_rawequal(L,-1,-2),"non-string Lua error identity preserved");check(logged("non-string"),"non-string error logged without string conversion");luaL_unref(L,LUA_REGISTRYINDEX,RC::LuaType::object_ref);lua_settop(L,0);
 RC::LuaType::mode=4;events.clear();lua_pushcfunction(L,invoke);status=lua_pcall(L,0,LUA_MULTRET,0);check(status==LUA_ERRRUN&&lua_isinteger(L,-1)&&lua_tointeger(L,-1)==42,"numeric Lua error type/value preserved");lua_settop(L,0);
 events.clear();lua_pushcfunction(L,property_error);status=lua_pcall(L,0,0,0);std::string error=lua_tostring(L,-1);check(status==LUA_ERRRUN&&error.find("pusher sentinel")!=std::string::npos&&error.find("stack sentinel")!=std::string::npos,"property error retains full original builder output");check(logged(error),"property error logs full message before swallowed pcall resumes");lua_settop(L,0);
 events.clear();RC::Unreal::UKismetTextLibrary::fail=true;try{RC::Unreal::FText t;t.construct_with_string(RC::Unreal::FString{});check(false,"FText exception preserved");}catch(const std::runtime_error&e){check(std::string(e.what())=="text sentinel","FText exception preserved");check(logged("text sentinel"),"FText failure logged before rethrow");}
 RC::Unreal::UClass klass;RC::Unreal::FStaticConstructObjectParameters params{&klass};events.clear();RC::Unreal::ready=false;check(RC::Unreal::StaticConstructObject(params)==nullptr,"SCO unready keeps null return");check(logged("not ready"),"SCO unready logs reason");
 events.clear();RC::Unreal::ready=true;params.Class=nullptr;check(RC::Unreal::StaticConstructObject(params)==nullptr,"SCO null class keeps null return");check(logged("class is null"),"SCO null class logs reason");params.Class=&klass;
 events.clear();RC::Unreal::sco_mode=1;check(RC::Unreal::StaticConstructObject(params)==nullptr,"SCO native null preserved");check(logged("returned null"),"SCO native null logged");
 events.clear();RC::Unreal::sco_mode=2;try{RC::Unreal::StaticConstructObject(params);check(false,"SCO exception preserved");}catch(const std::runtime_error&e){check(std::string(e.what())=="sco sentinel","SCO exception preserved");check(logged("sco sentinel"),"SCO exception logged before rethrow");}
 events.clear();RC::Unreal::sco_mode=0;check(RC::Unreal::StaticConstructObject(params)==&RC::Unreal::object,"SCO object return preserved");
 logger_throws=true;RC::Unreal::sco_mode=2;try{RC::Unreal::StaticConstructObject(params);}catch(const std::runtime_error&e){check(std::string(e.what())=="sco sentinel","logging failure cannot replace native exception");}logger_throws=false;
 lua_close(L);std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
