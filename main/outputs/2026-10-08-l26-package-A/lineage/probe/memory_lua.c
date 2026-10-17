#include "memory_lua.h"
#include <string.h>
#include <lua.h>
#include <lauxlib.h>

_Static_assert(sizeof(lua_Integer) == sizeof(long long), "Match the native Lua integer ABI");

static void event(mb_lua_report report, void *context, const char *stage, int result)
{
    if (report != NULL)
        report(context, stage, result);
}

int mb_lua_memory_check(const void *source, size_t length,
                        const struct mb_lua_expectation *expected,
                        mb_lua_report report, void *context)
{
    if (source == NULL || expected == NULL || expected->marker == NULL ||
        length == 0 || length > expected->maximum_source_length) {
        event(report, context, "lua_input_invalid", -1);
        return -1;
    }
    event(report, context, "lua_state_begin", 0);
    lua_State *state = luaL_newstate();
    if (state == NULL) {
        event(report, context, "lua_state_allocation_failed", -2);
        return -2;
    }
    /* Text mode refuses bytecode. No luaL_openlibs: there is no filesystem,
       OS, package loader, or game API in this diagnostic state. */
    int status = luaL_loadbufferx(state, source, length, "@modbridge-owned-input", "t");
    event(report, context, "lua_load_result", status);
    int result = -3;
    if (status == LUA_OK) {
        status = lua_pcall(state, 0, 2, 0);
        event(report, context, "lua_call_result", status);
        result = -4;
        if (status == LUA_OK) {
            result = -5;
            if (lua_type(state, -2) == LUA_TSTRING && lua_isinteger(state, -1)) {
                size_t marker_length = 0;
                const char *marker = lua_tolstring(state, -2, &marker_length);
                lua_Integer value = lua_tointeger(state, -1);
                if (marker != NULL && marker_length == expected->marker_length &&
                    memcmp(marker, expected->marker, marker_length) == 0 &&
                    value == (lua_Integer)expected->integer)
                    result = 0;
            }
            event(report, context, "lua_result_match", result);
        }
    }
    lua_close(state);
    event(report, context, "lua_state_closed", 0);
    if (result == 0)
        event(report, context, "lua_memory_check_passed", 0);
    return result;
}
