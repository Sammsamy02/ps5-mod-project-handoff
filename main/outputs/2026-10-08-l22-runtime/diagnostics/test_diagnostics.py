#!/usr/bin/env python3
"""Compile actual selected control bodies with a real C Lua VM and host-only engine/logger stand-ins."""
from pathlib import Path
import subprocess,json,sys,re,hashlib
O=Path(__file__).resolve().parent
R=O.parents[2]
S=O/'src'
def body(text,marker,start=0):
 p=text.index(marker,start); b=text.index('{',p); depth=0; i=b; quote=None
 while i<len(text):
  if text.startswith('//',i): i=text.find('\n',i); continue
  if text.startswith('/*',i): i=text.index('*/',i)+2; continue
  if text[i] in '\"\'':
   q=text[i];i+=1
   while i<len(text):
    if text[i]=='\\':i+=2;continue
    if text[i]==q:i+=1;break
    i+=1
   continue
  if text[i]=='{':depth+=1
  elif text[i]=='}':
   depth-=1
   if depth==0:return text[p:i+1]
  i+=1
 raise AssertionError(marker)
u=(S/'LuaUObject.cpp').read_text(); f=(S/'FText.cpp').read_text(); g=(S/'UObjectGlobals.cpp').read_text();m=(S/'LuaMod.cpp').read_text();h=(O/'include/LuaType/LuaUObject.hpp').read_text()
protected=body(u,'static auto protected_ufunction_call')
wrapper=body(u,'auto call_ufunction_from_lua(',u.index('#ifdef __PROSPERO__',u.index('static auto call_ufunction_from_lua_impl',u.index('static auto protected_ufunction_call'))))
# Capture the entire shared pusher error builder, not a hand-written replacement.
prop=body(h,'auto throw_error(std::string_view handler_name')
ctor=body(f,'auto FText::construct_with_string')
sco=body(g,'static auto StaticConstructObject(',g.index('namespace Above55'))
B=O/'tests';B.mkdir(exist_ok=True)
helper=O/'include/L22Diagnostics/Logging.hpp'
helper_inc='#include "'+str(helper)+'"' if helper.exists() else ''
code=r'''
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
'''+helper_inc+r'''
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
'''+protected+'\n'+wrapper+r'''
struct Property {std::string GetFullName(){return "Unused";}};
auto to_string(const std::string& s){return s;}
struct PusherParams {
 const LuaMadeSimple::Lua& lua;void* base=nullptr;void* data=nullptr;Property* property=nullptr;int stored_at_index=1;bool create_new_if_get_non_trivial_local=false;
 std::string get_operation()const{return "Set";}
 std::string get_stack_dump()const{return "stack sentinel";}
 template<class... A> void throw_error_internal_append_args(std::string&,A&&...)const{}
 template<class... Args>
'''+prop+r'''
};
}
namespace RC::Unreal {
struct FString{};
struct FText {void construct_with_string(const FString&&);};
namespace UKismetTextLibrary {inline bool fail=false;inline FText Conv_StringToText(const FString&&){if(fail)throw std::runtime_error("text sentinel");return {};}}
'''+ctor+r'''
struct UObject{};struct UClass{};struct FStaticConstructObjectParameters {UClass* Class;};
static bool ready=true;static int sco_mode=0;static UObject object;
template<class> struct Function {Function(void*){} bool is_ready(){return ready;}UObject* operator()(const FStaticConstructObjectParameters&){if(sco_mode==2)throw std::runtime_error("sco sentinel");return sco_mode==1?nullptr:&object;}};
namespace GlobalState {struct Holder{void* get_function_address(){return nullptr;}};inline Holder StaticConstructObjectInternal426;}
'''+sco+r'''
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
'''
# Production includes are supplied by minimal host logger/string stand-ins above.
shim=B/'shims';(shim/'DynamicOutput').mkdir(parents=True,exist_ok=True);(shim/'Helpers').mkdir(exist_ok=True)
(shim/'DynamicOutput/Output.hpp').write_text('#pragma once\n');(shim/'Helpers/String.hpp').write_text('#pragma once\n')
(B/'extracted.cpp').write_text(code)
lua=R/'.superpowers/native-dependency-spike/ue4ss-lua-vm-experiment-1/upstream/lua-official/lua-5.4.7/src'
lib=B/'lua';lib.mkdir(exist_ok=True)
objects=[]
for p in sorted(lua.glob('*.c')):
 if p.name in ['lua.c','luac.c']:continue
 obj=lib/(p.stem+'.o');objects.append(str(obj))
 if not obj.exists():subprocess.run(['clang','-O0','-c',str(p),'-I'+str(lua),'-o',str(obj)],check=True,capture_output=True)
cmd=['clang++','-std=c++20','-O0','-I'+str(lua),'-I'+str(shim),str(B/'extracted.cpp'),*objects,'-o',str(B/'diagnostics-test')]
p=subprocess.run(cmd,text=True,capture_output=True);(B/'compile.log').write_text(p.stdout+p.stderr)
if p.returncode:print(p.stderr);sys.exit(p.returncode)
p=subprocess.run([str(B/'diagnostics-test')],text=True,capture_output=True);print(p.stdout+p.stderr)
receipt={'command':cmd,'execution_returncode':p.returncode,'output':p.stdout+p.stderr,'lua_version':'official local Lua 5.4.7 C VM','extracted':['protected_ufunction_call','call_ufunction_from_lua','PusherParams::throw_error','FText::construct_with_string','Above55::StaticConstructObject'],'source_sha256':{str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in [S/'LuaUObject.cpp',S/'FText.cpp',S/'UObjectGlobals.cpp',O/'include/LuaType/LuaUObject.hpp']}}
(B/('RED.json' if p.returncode else 'GREEN.json')).write_text(json.dumps(receipt,indent=2)+'\n')
sys.exit(p.returncode)
