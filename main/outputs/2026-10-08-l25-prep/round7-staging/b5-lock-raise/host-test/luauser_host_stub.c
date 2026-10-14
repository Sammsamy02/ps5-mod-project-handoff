/* Host-test stub for the port's lua_lock hooks (single-threaded test: no-ops).
   Upstream luauser.c is Windows-only; the port supplies its own. */
#include "lua.h"
void LuaLockInitial(lua_State* L) { (void)L; }
void LuaLockFinal(lua_State* L) { (void)L; }
void LuaLock(lua_State* L) { (void)L; }
void LuaUnlock(lua_State* L) { (void)L; }
