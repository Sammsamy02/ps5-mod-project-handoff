#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <LuaLibrary.hpp>
#include <LuaMadeSimple/LuaMadeSimple.hpp>
#include <polyhook2/OwnedMemory.hpp>

static_assert(sizeof(int32_t) == 4);
static_assert(sizeof(uintptr_t) == 8);
static_assert(sizeof(lua_Integer) == 8);
static_assert(LUA_VERSION_NUM == 504);
#ifdef __PROSPERO__
static_assert(sizeof(long) == 8);
static_assert(sizeof(wchar_t) == 2);
#endif
using RC::LuaMadeSimple::Lua;
static int cases;
static void require(bool condition, const char* label) {
    if (!condition) { std::fprintf(stderr,"FAIL %s\n",label); std::abort(); }
}
static void returned(lua_State* state, int nargs, bool integer, int32_t expected, const char* label) {
    require(lua_pcall(state,nargs,1,0)==LUA_OK,label);
    require(lua_gettop(state)==1,label);
    require(integer ? (lua_isinteger(state,-1) && lua_tointeger(state,-1)==expected) : lua_isnil(state,-1),label);
    lua_pop(state,1); ++cases;
}
static void read(lua_State* state, lua_Integer address, bool integer, int32_t expected, const char* label) {
    require(lua_gettop(state)==0,label);
    lua_getglobal(state,"DerefToInt32"); lua_pushinteger(state,address);
    returned(state,1,integer,expected,label);
}
static lua_Integer address(const void* p) { return static_cast<lua_Integer>(reinterpret_cast<uintptr_t>(p)); }
extern "C" int owned_lua_deref_controls() {
 try {
    auto& lua=RC::LuaMadeSimple::new_state();
    lua_State* state=lua.get_lua_state();
    lua.register_function("DerefToInt32",RC::LuaLibrary::deref_to_int32);
    int32_t value=1234567;
    {
      PLH::OwnedMemory::ScopedRegion source(&value,sizeof(value),PLH::R);
      read(state,address(&value),true,value,"positive");
      value=0;read(state,address(&value),true,0,"zero is successful integer");
      value=-876543;read(state,address(&value),true,value,"negative value");
      value=std::numeric_limits<int32_t>::max();read(state,address(&value),true,value,"maximum int32");
      value=std::numeric_limits<int32_t>::min();read(state,address(&value),true,value,"minimum int32");
      read(state,address(&value)+1,false,0,"four-byte read crosses bound");
      read(state,address(&value)+4,false,0,"one past source");
      // Repeated destination scopes would overlap if an earlier token leaked.
      value=42;for(int i=0;i<8;++i)read(state,address(&value),true,42,"repeat read releases destination");
    }
    read(state,address(&value),false,0,"expired source registration");
    {
      PLH::OwnedMemory::ScopedRegion source(&value,sizeof(value),PLH::W);
      read(state,address(&value),false,0,"owned source lacks read permission");
    }
    {
      PLH::OwnedMemory::ScopedRegion source(&value,sizeof(value)-1,PLH::R);
      read(state,address(&value),false,0,"owned source short by one byte");
    }
    read(state,0,false,0,"null label");read(state,1,false,0,"unknown label");
    read(state,-1,false,0,"negative label");
    read(state,std::numeric_limits<lua_Integer>::max(),false,0,"unknown high label");
    lua_getglobal(state,"DerefToInt32");returned(state,0,false,0,"missing argument");
    lua_getglobal(state,"DerefToInt32");lua_pushinteger(state,address(&value));lua_pushinteger(state,0);returned(state,2,false,0,"extra argument");
    lua_getglobal(state,"DerefToInt32");lua_pushstring(state,"123");returned(state,1,false,0,"string argument");
    lua_getglobal(state,"DerefToInt32");lua_pushnumber(state,1.0);returned(state,1,false,0,"float argument");
    lua_getglobal(state,"DerefToInt32");lua_pushboolean(state,1);returned(state,1,false,0,"boolean argument");
    lua_getglobal(state,"DerefToInt32");lua_pushnil(state);returned(state,1,false,0,"nil argument");
    lua_getglobal(state,"DerefToInt32");lua_newtable(state);returned(state,1,false,0,"table argument");
    lua_getglobal(state,"DerefToInt32");lua_pushlightuserdata(state,&value);returned(state,1,false,0,"userdata argument");
    require(lua_gettop(state)==0,"final stack clean");
    lua_close(state);std::printf("PASS genuine LuaMadeSimple/LuaRaw owned deref cases=%d (8 repeated lifetime checks)\n",cases);return 0;
 } catch (...) { return 9; }
}
int main() { return owned_lua_deref_controls(); }
