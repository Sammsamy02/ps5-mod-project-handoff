// HOST-TEST SHIM — luauser_host_shim.c (worker-l11-enqueue-ladder).
// The staged LuaRaw's src/luauser.c defines the LuaLock* hooks luaconf.h maps
// lua_lock/lua_unlock to; that TU includes <windows.h> (CRITICAL_SECTION) and
// only builds on the Windows/PS5 toolchain. The host harness is
// single-threaded, so the four hooks are no-ops here. The SHIPPED module is
// untouched: its luauser.c compiles on the PS5 toolchain exactly as staged.

#include "lua.h"

void LuaLockInitial(lua_State* L) { (void)L; }
void LuaLockFinal(lua_State* L) { (void)L; }
void LuaLock(lua_State* L) { (void)L; }
void LuaUnlock(lua_State* L) { (void)L; }
