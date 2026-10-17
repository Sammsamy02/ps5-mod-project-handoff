/* lua_lock_shim.c — worker-lua-mod-smoke
 * POSIX single-threaded stand-in for the port's Windows-only luauser.c
 * lock hooks (luaconf.h:796-800 in the port's LuaRaw).
 * The port (ue4ss-native-lua-lock-final-1) locks around every lua_* call
 * because game thread + keybind thread share one lua_State. This harness is
 * single-threaded, so a real pthread mutex keeps the semantics without the
 * Windows dependency. No port source is modified.
 */
#include <pthread.h>
#include "lua.h"

static pthread_mutex_t g_lua_mutex = PTHREAD_MUTEX_INITIALIZER;

void LuaLock(lua_State* L) { (void)L; pthread_mutex_lock(&g_lua_mutex); }
void LuaUnlock(lua_State* L) { (void)L; pthread_mutex_unlock(&g_lua_mutex); }
void LuaLockInitial(lua_State* L) { (void)L; }
void LuaLockFinal(lua_State* L) { (void)L; }
