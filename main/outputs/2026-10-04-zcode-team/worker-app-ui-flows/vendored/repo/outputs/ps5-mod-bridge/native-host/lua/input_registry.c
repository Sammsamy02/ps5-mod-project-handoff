#include "input_registry.h"
#include "lauxlib.h"
#include <string.h>

static int reject(lua_State *L, mb_input_registry *context, const char *why) {
    if (context) ++context->rejected;
    return luaL_error(L, "%s", why);
}

static int register_binding(lua_State *L) {
    mb_input_registry *context = lua_touserdata(L, lua_upvalueindex(1));
    mb_input_binding binding;
    int callback, argc = lua_gettop(L);
    lua_Integer key;
    size_t count = 0, i;
    if (!context || !context->active || context->owner != L)
        return reject(L, context, "RegisterKeyBind: input owner is inactive");
    if ((argc != 2 && argc != 3) || !lua_isinteger(L, 1))
        return reject(L, context, "RegisterKeyBind: expected integer key and callback, with optional modifier array");
    key = lua_tointeger(L, 1);
    if (key < 0 || key > 255)
        return reject(L, context, "RegisterKeyBind: key must be a byte");
    memset(&binding, 0, sizeof(binding));
    binding.key = (uint8_t)key;
    if (argc == 3) {
        if (lua_type(L, 2) != LUA_TTABLE)
            return reject(L, context, "RegisterKeyBind: modifiers must be an array");
        count = lua_rawlen(L, 2);
        if (count > MB_INPUT_MODIFIER_CAP)
            return reject(L, context, "RegisterKeyBind: too many modifiers");
        /* Reject holes, non-array keys and extra hash entries without invoking
         * script metamethods. Argument errors leave no callback reference. */
        lua_pushnil(L);
        while (lua_next(L, 2)) {
            lua_Integer index;
            if (!lua_isinteger(L, -2) || !lua_isinteger(L, -1))
                return reject(L, context, "RegisterKeyBind: modifiers must be dense byte integers");
            index = lua_tointeger(L, -2);
            key = lua_tointeger(L, -1);
            if (index < 1 || (lua_Unsigned)index > count || key < 0 || key > 255)
                return reject(L, context, "RegisterKeyBind: modifiers must be dense byte integers");
            lua_pop(L, 1);
        }
        for (i = 0; i < count; ++i) {
            lua_rawgeti(L, 2, (lua_Integer)i + 1);
            if (!lua_isinteger(L, -1))
                return reject(L, context, "RegisterKeyBind: sparse modifier array");
            key = lua_tointeger(L, -1);
            if (key < 0 || key > 255)
                return reject(L, context, "RegisterKeyBind: modifier must be a byte");
            binding.modifiers[i] = (uint8_t)key;
            lua_pop(L, 1);
        }
    }
    binding.modifier_count = (uint8_t)count;
    callback = argc;
    if (lua_type(L, callback) != LUA_TFUNCTION)
        return reject(L, context, "RegisterKeyBind: callback must be a function");
    if (context->count == MB_INPUT_BINDING_CAP)
        return reject(L, context, "RegisterKeyBind: binding capacity reached");
    lua_pushvalue(L, callback);
    binding.reference = luaL_ref(L, LUA_REGISTRYINDEX);
    context->bindings[context->count++] = binding;
    return 0;
}

int mb_input_install(lua_State *L, mb_input_registry *context) {
    if (!L || !context || context->installed) return 0;
    memset(context, 0, sizeof(*context));
    context->owner = L;
    context->installed = context->active = 1;
    lua_pushlightuserdata(L, context);
    lua_pushcclosure(L, register_binding, 1);
    lua_setglobal(L, "RegisterKeyBind");
    return 1;
}

mb_input_status mb_input_dispatch(mb_input_registry *context,
                                 const mb_input_keydown *event) {
    uint32_t count, i, j;
    int failed = 0, top;
    lua_State *L;
    if (!context || !event) return MB_INPUT_BAD_ARGUMENT;
    if (!context->installed || !context->active || !context->owner)
        return MB_INPUT_INACTIVE;
    if (context->dispatching) return MB_INPUT_BUSY;
    if (event->repeat) return MB_INPUT_OK;
    L = context->owner;
    top = lua_gettop(L);
    count = context->count;
    context->dispatching = 1;
    for (i = 0; i < count; ++i) {
        const mb_input_binding *binding = &context->bindings[i];
        int matches = binding->key == event->key;
        for (j = 0; matches && j < binding->modifier_count; ++j) {
            unsigned m = binding->modifiers[j];
            matches = (event->held_keys[m >> 3] & (1u << (m & 7u))) != 0;
        }
        if (!matches) continue;
        lua_rawgeti(L, LUA_REGISTRYINDEX, binding->reference);
        ++context->delivered;
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            size_t length = 0;
            const char *error = lua_tolstring(L, -1, &length);
            ++context->callback_errors;
            failed = 1;
            if (!error) {
                error = "non-string Lua callback error";
                length = strlen(error);
            }
            if (length >= sizeof(context->last_error))
                length = sizeof(context->last_error) - 1;
            memcpy(context->last_error, error, length);
            context->last_error[length] = 0;
            lua_pop(L, 1);
        }
    }
    lua_settop(L, top);
    context->dispatching = 0;
    return failed ? MB_INPUT_CALLBACK_ERROR : MB_INPUT_OK;
}

int mb_input_dispose(mb_input_registry *context) {
    uint32_t i;
    if (!context || !context->installed || !context->owner || context->dispatching)
        return 0;
    context->active = 0;
    for (i = 0; i < context->count; ++i)
        luaL_unref(context->owner, LUA_REGISTRYINDEX, context->bindings[i].reference);
    context->count = 0;
    return 1;
}
