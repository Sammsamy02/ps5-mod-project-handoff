#include "module_loader.h"
#include <string.h>
#include "lauxlib.h"

#define NAME_CAP 96u
#define MEMBER_CAP 192u
#define UNLOADED 0u
#define LOADING 1u
#define LOADED 2u

static int ascii_start(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static int safe_name(const char *name, size_t length) {
    size_t i;
    unsigned int segments = 1u;
    int start = 1;
    if (length == 0u || length > NAME_CAP)
        return 0;
    for (i = 0u; i < length; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (start) {
            if (!ascii_start(c))
                return 0;
            start = 0;
        } else if (c == '.') {
            if (++segments > MB_LUA_MODULE_DEPTH_CAP)
                return 0;
            start = 1;
        } else if (!ascii_start(c) && !(c >= '0' && c <= '9')) {
            return 0;
        }
    }
    return !start;
}

static int safe_member(const char *member, size_t length) {
    size_t i, segment_start = 0u;
    if (length < 4u || length > MEMBER_CAP ||
        memcmp(member + length - 4u, ".lua", 4u) != 0)
        return 0;
    for (i = 0u; i <= length; ++i) {
        unsigned char c = (unsigned char)member[i];
        if (i == length || c == '/') {
            size_t segment_length = i - segment_start;
            if (segment_length == 0u ||
                (segment_length == 1u && member[segment_start] == '.') ||
                (segment_length == 2u &&
                 memcmp(member + segment_start, "..", 2u) == 0))
                return 0;
            segment_start = i + 1u;
        } else if (c < 0x21u || c > 0x7eu || c == '\\' || c == ':') {
            return 0;
        }
    }
    return 1;
}

/* Protected by the outer require's pcall, including source compilation. */
static int execute_module(lua_State *L) {
    mb_lua_require_t *context = lua_touserdata(L, lua_upvalueindex(1));
    size_t index = (size_t)lua_tointeger(L, lua_upvalueindex(2));
    const mb_lua_module_t *module = &context->modules[index];
    char chunk_name[MEMBER_CAP + 2u];
    size_t length = strlen(module->archive_member);
    int status;
    chunk_name[0] = '@';
    memcpy(chunk_name + 1u, module->archive_member, length + 1u);
    status = luaL_loadbufferx(L, (const char *)module->bytes, module->length,
                             chunk_name, "t");
    if (status != LUA_OK)
        return lua_error(L);
    lua_pushstring(L, module->name);
    lua_pushstring(L, module->archive_member);
    lua_call(L, 2, 1);
    return 1;
}

static int inventory_require(lua_State *L) {
    mb_lua_require_t *context = lua_touserdata(L, lua_upvalueindex(1));
    const char *name;
    size_t length, index;
    int status;
    if (++context->calls > MB_LUA_REQUIRE_CAP) {
        ++context->rejects;
        return luaL_error(L, "approved Lua require call budget exceeded");
    }
    if (lua_type(L, 1) != LUA_TSTRING) {
        ++context->rejects;
        return luaL_error(L, "approved Lua require needs a string module name");
    }
    name = lua_tolstring(L, 1, &length);
    if (!safe_name(name, length)) {
        ++context->rejects;
        return luaL_error(L, "unsafe Lua module name");
    }
    for (index = 0u; index < context->count; ++index) {
        const char *candidate = context->modules[index].name;
        if (strlen(candidate) == length && memcmp(candidate, name, length) == 0)
            break;
    }
    if (index == context->count) {
        ++context->rejects;
        return luaL_error(L, "module is not in the approved Lua inventory");
    }
    if (context->state[index] == LOADING) {
        ++context->cycles;
        return luaL_error(L, "cyclic approved Lua module dependency");
    }
    if (context->state[index] == LOADED) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, context->references[index]);
        if (lua_toboolean(L, -1)) {
            ++context->cache_hits;
            return 1;
        }
        /* Lua require reloads modules whose cached value is false. */
        lua_pop(L, 1);
        luaL_unref(L, LUA_REGISTRYINDEX, context->references[index]);
        context->references[index] = LUA_NOREF;
        context->state[index] = UNLOADED;
    }
    if (context->depth >= MB_LUA_MODULE_DEPTH_CAP) {
        ++context->rejects;
        return luaL_error(L, "approved Lua module dependency depth exceeded");
    }
    lua_pushlightuserdata(L, context);
    lua_pushinteger(L, (lua_Integer)index);
    lua_pushcclosure(L, execute_module, 2);
    context->state[index] = LOADING;
    ++context->depth;
    if (context->depth > context->peak_depth)
        context->peak_depth = context->depth;
    ++context->loads;
    context->last_module = context->modules[index].name;
    status = lua_pcall(L, 0, 1, 0);
    --context->depth;
    context->state[index] = UNLOADED;
    if (status != LUA_OK) {
        ++context->failures;
        return lua_error(L);
    }
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushboolean(L, 1);
    }
    lua_pushvalue(L, -1);
    context->references[index] = luaL_ref(L, LUA_REGISTRYINDEX);
    context->state[index] = LOADED;
    /* Like Lua 5.4, fresh loads also return their loader data. */
    lua_pushstring(L, context->modules[index].archive_member);
    return 2;
}

int mb_lua_install_require(lua_State *L, mb_lua_require_t *context,
                           const mb_lua_module_t *modules, size_t count) {
    size_t i, j, total = 0u;
    if (L == NULL || context == NULL || (count != 0u && modules == NULL) ||
        count > MB_LUA_MODULE_CAP)
        return 0;
    for (i = 0u; i < count; ++i) {
        const mb_lua_module_t *module = &modules[i];
        size_t member_length;
        if (module->name == NULL || module->archive_member == NULL ||
            module->bytes == NULL ||
            !safe_name(module->name, strlen(module->name)) ||
            module->length == 0u || module->length > MB_LUA_MODULE_SOURCE_CAP ||
            module->length > MB_LUA_MODULE_TOTAL_CAP - total)
            return 0;
        member_length = strlen(module->archive_member);
        if (!safe_member(module->archive_member, member_length))
            return 0;
        for (j = 0u; j < i; ++j)
            if (strcmp(modules[j].name, module->name) == 0)
                return 0;
        total += module->length;
    }
    memset(context, 0, sizeof(*context));
    context->modules = modules;
    context->count = count;
    for (i = 0u; i < MB_LUA_MODULE_CAP; ++i)
        context->references[i] = LUA_NOREF;
    lua_pushlightuserdata(L, context);
    lua_pushcclosure(L, inventory_require, 1);
    lua_setglobal(L, "require");
    return 1;
}
