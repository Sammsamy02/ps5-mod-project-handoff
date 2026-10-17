/* C caller test (Mac-only; lua-as-cpp track 2026-10-12). Same 8 Lua API calls
 * as the frozen packaging bootstrap unit lineage/probe/memory_lua.c:24-52
 * (luaL_newstate, luaL_loadbufferx, lua_pcall(k), lua_type, lua_isinteger,
 * lua_tolstring, lua_tointeger(x), lua_close), compiled as plain C. It must
 * LINK against the Lua build under test and a Lua error inside lua_pcall must
 * come back as a status, not cross this C frame. Exit 0 = PASS. */
#include <stdio.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
static int run(const char* src, int expect_status, const char* expect_marker, long long expect_value)
{
    lua_State* L = luaL_newstate();
    if (!L) return 1;
    int ok = 0;
    int st = luaL_loadbufferx(L, src, strlen(src), "@c-caller", "t");
    if (st == LUA_OK) st = lua_pcall(L, 0, 2, 0);
    if (st == expect_status) {
        if (st == LUA_OK) {
            size_t n = 0;
            if (lua_type(L, -2) == LUA_TSTRING && lua_isinteger(L, -1)) {
                const char* m = lua_tolstring(L, -2, &n);
                ok = m && strcmp(m, expect_marker) == 0 && lua_tointeger(L, -1) == expect_value;
            }
        } else {
            const char* m = lua_tolstring(L, -1, NULL);
            ok = m && strstr(m, expect_marker) != NULL;
        }
    }
    printf("  %s  C caller: status=%d (expected %d)\n", ok ? "PASS" : "FAIL", st, expect_status);
    lua_close(L);
    return !ok;
}
int main(void)
{
    int f = 0;
    f += run("return 'modbridge', 41 + 1", LUA_OK, "modbridge", 42);
    /* no luaL_openlibs (as in memory_lua.c), so raise without the base library */
    f += run("local t = nil return t.field", LUA_ERRRUN, "attempt to index a nil value", 0);
    printf("checks run: 2\n%s C caller test (%d failures)\n", f ? "FAIL" : "PASS", f);
    return f ? 1 : 0;
}
