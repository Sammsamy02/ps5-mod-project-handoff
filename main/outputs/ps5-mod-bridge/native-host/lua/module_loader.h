#ifndef MODBRIDGE_INVENTORY_LUA_LOADER_H
#define MODBRIDGE_INVENTORY_LUA_LOADER_H

#include <stddef.h>
#include <stdint.h>
#include "lua.h"

#define MB_LUA_MODULE_CAP 32u
#define MB_LUA_MODULE_SOURCE_CAP (64u * 1024u)
#define MB_LUA_MODULE_TOTAL_CAP (256u * 1024u)
#define MB_LUA_REQUIRE_CAP 256u
#define MB_LUA_MODULE_DEPTH_CAP 8u

typedef struct {
    const char *name;
    const char *archive_member;
    const unsigned char *bytes;
    size_t length;
} mb_lua_module_t;

typedef struct {
    const mb_lua_module_t *modules;
    size_t count;
    unsigned char state[MB_LUA_MODULE_CAP];
    int references[MB_LUA_MODULE_CAP];
    unsigned int calls, loads, cache_hits, rejects, cycles, failures;
    unsigned int depth, peak_depth;
    const char *last_module;
} mb_lua_require_t;

/* Inventory is immutable, caller-owned, text Lua only. No path is opened.
 * Install once using a fresh context in a fresh Lua state. Context and inventory
 * must outlive that state (including any coroutines); reinstallation is not
 * supported, because registry references and the closure still own the context.
 */
int mb_lua_install_require(lua_State *L, mb_lua_require_t *context,
                           const mb_lua_module_t *modules, size_t count);

#endif
