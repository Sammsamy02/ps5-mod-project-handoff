#include "invalid_object.h"
#include <string.h>
#include "lauxlib.h"

#define NULL_META "modbridge.null.uobject.v1"

typedef struct {
    mb_lua_invalid_context_t *context;
} null_uobject_t;

static null_uobject_t *check_null(lua_State *L, int index) {
    return (null_uobject_t *)luaL_checkudata(L, index, NULL_META);
}

static int push_null(lua_State *L, mb_lua_invalid_context_t *context) {
    null_uobject_t *object = lua_newuserdatauv(L, sizeof(*object), 0);
    object->context = context;
    luaL_setmetatable(L, NULL_META);
    ++context->created;
    return 1;
}

static int create_invalid_object(lua_State *L) {
    mb_lua_invalid_context_t *context =
        lua_touserdata(L, lua_upvalueindex(1));
    return push_null(L, context);
}

static int is_valid(lua_State *L) {
    null_uobject_t *object = check_null(L, 1);
    ++object->context->validity_queries;
    lua_pushboolean(L, 0);
    return 1;
}

static int get_address(lua_State *L) {
    (void)check_null(L, 1);
    lua_pushinteger(L, 0);
    return 1;
}

static int get_full_name(lua_State *L) {
    (void)check_null(L, 1);
    lua_pushnil(L);
    return 1;
}

static int object_type(lua_State *L) {
    (void)check_null(L, 1);
    lua_pushliteral(L, "UObject");
    return 1;
}

static int get_world(lua_State *L) {
    null_uobject_t *object = check_null(L, 1);
    return push_null(L, object->context);
}

static int null_equal(lua_State *L) {
    /* Every value this API can create wraps the same absent/null referent. */
    lua_pushboolean(L, luaL_testudata(L, 1, NULL_META) != NULL &&
                       luaL_testudata(L, 2, NULL_META) != NULL);
    return 1;
}

typedef struct { const char *name; lua_CFunction function; } null_method_t;
static const null_method_t methods[] = {
    {"IsValid", is_valid},
    {"GetAddress", get_address},
    {"GetFullName", get_full_name},
    {"GetWorld", get_world},
    {"type", object_type},
};

static int null_index(lua_State *L) {
    null_uobject_t *object = check_null(L, 1);
    const char *name;
    size_t length, i;
    if (lua_type(L, 2) == LUA_TSTRING) {
        name = lua_tolstring(L, 2, &length);
        for (i = 0u; i < sizeof(methods) / sizeof(methods[0]); ++i) {
            size_t known = strlen(methods[i].name);
            if (length == known && memcmp(name, methods[i].name, length) == 0) {
                lua_pushcfunction(L, methods[i].function);
                return 1;
            }
        }
    }
    ++object->context->unsupported_operations;
    return luaL_error(L, "null UObject property/reflection lookup is unsupported");
}

static int null_write(lua_State *L) {
    null_uobject_t *object = check_null(L, 1);
    ++object->context->unsupported_operations;
    return luaL_error(L, "cannot write a property on a null UObject");
}

static int null_call(lua_State *L) {
    null_uobject_t *object = check_null(L, 1);
    ++object->context->unsupported_operations;
    return luaL_error(L, "cannot call a member function on a null UObject");
}

int mb_lua_install_invalid_object(lua_State *L,
                                  mb_lua_invalid_context_t *context) {
    if (L == NULL || context == NULL)
        return 0;
    /* Prevent replacing the context captured by a prior installation. */
    if (!luaL_newmetatable(L, NULL_META)) {
        lua_pop(L, 1);
        return 0;
    }
    memset(context, 0, sizeof(*context));
    lua_pushcfunction(L, null_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, null_write);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, null_call);
    lua_setfield(L, -2, "__call");
    lua_pushcfunction(L, null_equal);
    lua_setfield(L, -2, "__eq");
    lua_pushliteral(L, "UObject");
    lua_setfield(L, -2, "__metatable");
    lua_pop(L, 1);
    lua_pushlightuserdata(L, context);
    lua_pushcclosure(L, create_invalid_object, 1);
    lua_setglobal(L, "CreateInvalidObject");
    return 1;
}
