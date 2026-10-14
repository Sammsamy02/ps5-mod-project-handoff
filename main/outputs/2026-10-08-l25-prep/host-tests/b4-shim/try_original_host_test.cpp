// B4 host test (Mac-only): LuaPathShim try_original() — a second attempt in
// the same io.open call (lazy-create retry; sandbox read after the mod-root
// miss) must see the caller's arguments. Real Lua 5.4 C library.
//
// OLD = IQ's staged shape (set_path_argument + call_original on the originals:
//       lua_pcall consumes them, so attempt 2 runs backend(nil, nil)).
// NEW = B4 fix (backend called on a copy; originals stay on the stack).
#include <cstdio>
#include <string>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

// ---- OLD (copied shape) ----
static void set_path_argument(lua_State* L, int slot, int argc, const std::string& physical)
{
    lua_pushlstring(L, physical.data(), physical.size());
    lua_replace(L, slot);
    lua_settop(L, argc);
}
static int call_original(lua_State* L)
{
    const int argc = lua_gettop(L);
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    if (lua_pcall(L, argc, LUA_MULTRET, 0) != LUA_OK)
    {
        const char* message = lua_tostring(L, -1);
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_pushstring(L, message ? message : "io shim backend failed");
        return 2;
    }
    return lua_gettop(L);
}
static int try_original_old(lua_State* L, int argc, const std::string& physical)
{
    set_path_argument(L, 1, argc, physical);
    const int results = call_original(L);
    if (results >= 1 && !lua_isnil(L, 1)) return results;
    lua_pop(L, results);
    return 0;
}
// ---- NEW (B4 fix, same code as the staged LuaPathShim.cpp) ----
static int try_original_new(lua_State* L, int argc, const std::string& physical)
{
    lua_settop(L, argc);
    if (argc < 1 || !lua_checkstack(L, argc + 2)) return 0;
    const int base = lua_gettop(L);
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_pushlstring(L, physical.data(), physical.size());
    for (int i = 2; i <= argc; ++i) lua_pushvalue(L, i);
    if (lua_pcall(L, argc, LUA_MULTRET, 0) != LUA_OK)
    {
        lua_settop(L, base);
        return 0;
    }
    const int results = lua_gettop(L) - base;
    if (results >= 1 && !lua_isnil(L, base + 1)) return results;
    lua_settop(L, base);
    return 0;
}

template <int (*TRY)(lua_State*, int, const std::string&)>
static int wrapper(lua_State* L)
{
    const int argc = lua_gettop(L);
    if (int r = TRY(L, argc, "/modroot/missing.txt"); r > 0) return r;   // attempt 1 fails
    if (int r = TRY(L, argc, "/writable/present.txt"); r > 0) return r;  // attempt 2 must succeed
    lua_pushnil(L);
    lua_pushliteral(L, "not found");
    return 2;
}

int main()
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    // backend: succeeds only for the second location, echoes the mode it got
    luaL_dostring(L, "backend = function(p, m) if p == '/writable/present.txt' then return 'handle(' .. tostring(m) .. ')' end "
                     "return nil, 'no such file: ' .. tostring(p), 2 end");
    lua_getglobal(L, "backend");
    lua_pushcclosure(L, wrapper<try_original_old>, 1);
    lua_setglobal(L, "open_old");
    lua_getglobal(L, "backend");
    lua_pushcclosure(L, wrapper<try_original_new>, 1);
    lua_setglobal(L, "open_new");
    int fails = 0;
    auto run = [&](const char* expr) -> std::string {
        if (luaL_dostring(L, expr) != LUA_OK) { std::string e = lua_tostring(L, -1); lua_pop(L, 1); return "ERROR " + e; }
        std::string out = lua_tostring(L, -1) ? lua_tostring(L, -1) : "nil";
        lua_pop(L, 1);
        return out;
    };
    const std::string old_r = run("local h, e = open_old('settings.txt', 'r') return tostring(h) .. ' / ' .. tostring(e)");
    const std::string new_r = run("local h, e = open_new('settings.txt', 'r') return tostring(h) .. ' / ' .. tostring(e)");
    std::printf("       OLD: %s\n       NEW: %s\n", old_r.c_str(), new_r.c_str());
    auto check = [&](bool ok, const char* what) { std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what); fails += !ok; };
    check(old_r.find("handle(") == std::string::npos, "OLD: the second location can never open (defect reproduced)");
    check(new_r.rfind("handle(r)", 0) == 0, "NEW: second location opens with the caller's mode");
    const std::string first_ok = run("local h = open_new('x', 'w') return tostring(h)");
    check(first_ok == "handle(w)", "NEW: argument order kept (mode 'w' reaches the backend)");
    lua_close(L);
    std::printf("%s b4 try_original host test (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
