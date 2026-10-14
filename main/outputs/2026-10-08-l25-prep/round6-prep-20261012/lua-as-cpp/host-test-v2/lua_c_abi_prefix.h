/* lua_c_abi_prefix.h -- STAGED (lua-as-cpp track, 2026-10-12, Mac-only).
 *
 * Forced include (-include) for the 33 LuaRaw units ONLY, when they are
 * compiled as C++. Lua then raises errors with C++ throw (ldo.c:55-61, chosen
 * by __cplusplus), so C++ destructors run in every frame a Lua error crosses,
 * but every public Lua symbol KEEPS ITS C NAME, exactly as in today's C build:
 *
 *  - luaconf.h:282-286 is Lua's documented customisation point for LUA_API
 *    ("CHANGE them if you need to define those functions in some special
 *    way"); luaconf.h:297 sets it to plain `extern` with no #ifndef guard, so
 *    a -D on the command line would be overridden. luaconf.h has an include
 *    guard, so including it here first and then redefining LUA_API sticks.
 *    LUALIB_API / LUAMOD_API expand to LUA_API (luaconf.h:305-306).
 *  - lua.h:522-525 declares the four lock hooks without LUA_API; declaring
 *    them extern "C" first makes lua.h's later plain redeclarations keep C
 *    linkage (a redeclaration without a linkage-specification inherits it).
 *  - lprefix.h is included first, as every Lua .c file does itself, so the
 *    feature-test macros are set before the first system header.
 *  - LUA_LIB is defined first because luaconf.h is now read before the .c
 *    file's own `#define LUA_CORE` / `#define LUA_LIB` line. In the headers
 *    both names are only ever tested together, as (LUA_CORE || LUA_LIB)
 *    (luaconf.h:289 DLL branch, unused here, and :694 l_likely/l_unlikely),
 *    and no Lua .c file tests either one, so every Lua unit sees exactly the
 *    configuration it saw before. The .c file's later empty #define is an
 *    identical redefinition (no warning). Mac-only check: host build has 0
 *    warnings and exports the same public names as the C build.
 *
 * Why not upstream's LUA_COMPILED_AS_CPP (C++ linkage everywhere)? The frozen
 * packaging bootstrap unit probe/memory_lua.c is plain C and calls 8 Lua API
 * functions by their C names (llvm-nm -u memory_lua.o). With C++ linkage
 * those references would stay unresolved. With this prefix nobody outside the
 * 33 Lua units changes: callers keep including <lua.hpp> in its extern "C"
 * form, no caller is recompiled, no symbol is renamed.
 *
 * Not for any other unit. Not a mod file. Contains no game- or mod-specific
 * value.
 */
#ifndef MODBRIDGE_LUA_C_ABI_PREFIX_H
#define MODBRIDGE_LUA_C_ABI_PREFIX_H
#ifndef __cplusplus
#error "lua_c_abi_prefix.h is only for LuaRaw units compiled as C++"
#endif
#ifdef LUA_COMPILED_AS_CPP
#error "lua_c_abi_prefix.h keeps C linkage; do not combine it with LUA_COMPILED_AS_CPP"
#endif
#define LUA_LIB
#include "lprefix.h"
#include "luaconf.h"
#undef LUA_API
#define LUA_API extern "C"
typedef struct lua_State lua_State;
extern "C" {
void LuaLock(lua_State* L);
void LuaUnlock(lua_State* L);
void LuaLockInitial(lua_State* L);
void LuaLockFinal(lua_State* L);
}
#endif
