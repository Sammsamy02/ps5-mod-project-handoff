#ifndef MODBRIDGE_MEMORY_LUA_CHECK_H
#define MODBRIDGE_MEMORY_LUA_CHECK_H
#include <stddef.h>
struct mb_lua_expectation {
    const char *marker;
    size_t marker_length;
    long long integer;
    size_t maximum_source_length;
};
typedef void (*mb_lua_report)(void *context, const char *stage, int result);
/* Diagnostic only: a fresh Lua state with no standard libraries or engine API. */
int mb_lua_memory_check(const void *source, size_t length,
                        const struct mb_lua_expectation *expected,
                        mb_lua_report report, void *context);
#endif
