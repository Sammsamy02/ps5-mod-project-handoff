// B4 host test (Mac-only): the LuaPathShim wrap_into() call shapes, run on the
// REAL Lua 5.4 C library.
//
// OLD (crashed builds d971cd2f / 1c48239d): wrap_into(L, -1, context_index, "remove", ...)
//   with the os table at the top of the stack. Inside wrap_into, after
//   lua_pushcclosure the closure is at -1, so lua_setfield(L, -1, name)
//   indexes the CLOSURE: Lua raises "attempt to index a function value".
//   install() runs outside any protected call, so on the console that error
//   is a Lua PANIC -> abort (stderr only, nothing in UE4SS.log) — matching
//   both crash logs ending right after the "[SHIM] write-area" marker.
// NEW (IQ fix, B4): absolute table index + registry-sourced context.
//
// The test runs each shape inside lua_pcall so the error can be observed
// instead of aborting the test process.
#include <cstdio>
#include <cstring>
#include <string>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

static int wrapper(lua_State* L) { lua_pushboolean(L, lua_touserdata(L, lua_upvalueindex(2)) != nullptr); return 1; }

static void wrap_into_old(lua_State* L, int table_index, int context_index, const char* name)
{
    lua_getfield(L, table_index, name);
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
    lua_pushvalue(L, context_index);
    lua_pushcclosure(L, wrapper, 2);
    lua_setfield(L, table_index, name);
}

static const char g_sentinel = 0;
static bool push_context(lua_State* L)
{
    lua_pushlightuserdata(L, const_cast<char*>(&g_sentinel));
    lua_gettable(L, LUA_REGISTRYINDEX);
    if (lua_isuserdata(L, -1)) return true;
    lua_pop(L, 1);
    return false;
}
static void wrap_into_new(lua_State* L, int table_index, const char* name)
{
    lua_getfield(L, table_index, name);
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
    if (!push_context(L)) { lua_pop(L, 1); return; }
    lua_pushcclosure(L, wrapper, 2);
    lua_setfield(L, table_index, name);
}

static int install_old(lua_State* L)
{
    lua_newuserdatauv(L, 8, 0);          // context at absolute index
    const int context_index = lua_gettop(L);
    lua_getglobal(L, "os");
    wrap_into_old(L, -1, context_index, "remove");   // the pre-fix call shape
    lua_pop(L, 1);
    return 0;
}

static int install_new(lua_State* L)
{
    lua_newuserdatauv(L, 8, 0);
    lua_pushlightuserdata(L, const_cast<char*>(&g_sentinel));
    lua_pushvalue(L, -2);
    lua_settable(L, LUA_REGISTRYINDEX);
    lua_pop(L, 1);
    const int base = lua_gettop(L);
    lua_getglobal(L, "os");
    const int os_index = lua_gettop(L);
    wrap_into_new(L, os_index, "remove");
    wrap_into_new(L, os_index, "rename");
    lua_pop(L, 1);
    if (lua_gettop(L) != base) return luaL_error(L, "stack not balanced");
    return 0;
}

int main()
{
    int fails = 0;
    auto check = [&](bool ok, const char* what) {
        std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
        fails += !ok;
    };
    {
        lua_State* L = luaL_newstate();
        luaL_openlibs(L);
        lua_pushcfunction(L, install_old);
        const int st = lua_pcall(L, 0, 0, 0);
        const std::string msg = st != LUA_OK && lua_tostring(L, -1) ? lua_tostring(L, -1) : "";
        std::printf("       OLD: status=%d message='%s'\n", st, msg.c_str());
        check(st == LUA_ERRRUN && msg.find("attempt to index a function value") != std::string::npos,
              "OLD shape raises 'attempt to index a function value' (outside pcall on console = PANIC/abort)");
        lua_close(L);
    }
    {
        lua_State* L = luaL_newstate();
        luaL_openlibs(L);
        lua_pushcfunction(L, install_new);
        const int st = lua_pcall(L, 0, 0, 0);
        check(st == LUA_OK, "NEW shape installs without error and leaves the stack balanced");
        const int r = luaL_dostring(L, "return os.remove('x'), os.rename('a','b')");
        check(r == LUA_OK && lua_toboolean(L, -1) && lua_toboolean(L, -2),
              "NEW: os.remove/os.rename wrappers see the registry context (upvalue 2)");
        lua_close(L);
    }
    std::printf("%s b4 wrap_into host test (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
