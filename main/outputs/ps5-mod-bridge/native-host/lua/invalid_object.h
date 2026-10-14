#ifndef MODBRIDGE_LUA_INVALID_OBJECT_H
#define MODBRIDGE_LUA_INVALID_OBJECT_H
#include <stddef.h>
#include "lua.h"

typedef struct {
    size_t created;
    size_t validity_queries;
    size_t unsupported_operations;
} mb_lua_invalid_context_t;

/* Install once in a fresh restricted Lua state. Context must outlive lua_close
 * and any coroutines. Objects can represent only a null Unreal referent; this
 * interface cannot create or bind a valid game object, or read game memory.
 * Supported methods: IsValid, GetAddress, GetFullName, GetWorld, type, equality.
 * Reflection, property reads/writes and engine calls are explicitly unsupported.
 */
int mb_lua_install_invalid_object(lua_State *L,
                                  mb_lua_invalid_context_t *context);
#endif
