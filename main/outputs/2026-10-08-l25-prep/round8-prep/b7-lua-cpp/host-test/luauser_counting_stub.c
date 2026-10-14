/* Host stand-in for the port's Lua lock hooks (Mac-only test helper).
   Same semantics as ue4ss-native-lua-lock-final-1/src/platform_locks.c:
   one process-wide RECURSIVE pthread mutex; LuaLockFinal balances the lock
   that lua_close takes without unlocking. Added for the test: a per-thread
   depth counter and a probe that asks whether ANOTHER thread could take the
   lock right now, so the test can prove every lua_lock is released.
   Built as C for the C build of Lua and as C++ for the C++ build (lua.h
   declares these hooks without extern "C", so their linkage follows Lua's). */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "lua.h"

static pthread_once_t lock_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t lua_mutex;
static __thread int lock_depth;

static void initialize_lua_mutex(void)
{
    pthread_mutexattr_t a;
    if (pthread_mutexattr_init(&a) != 0) abort();
    if (pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE) != 0) abort();
    if (pthread_mutex_init(&lua_mutex, &a) != 0) abort();
    (void)pthread_mutexattr_destroy(&a);
}

void LuaLockInitial(lua_State* L) { (void)L; if (pthread_once(&lock_once, initialize_lua_mutex) != 0) abort(); }
void LuaLock(lua_State* L)
{
    LuaLockInitial(L);
    if (pthread_mutex_lock(&lua_mutex) != 0) abort();
    ++lock_depth;
}
void LuaUnlock(lua_State* L)
{
    (void)L;
    --lock_depth;
    if (pthread_mutex_unlock(&lua_mutex) != 0)
    {
        /* platform_locks.c aborts here too; say why first */
        static const char msg[] = "luauser stub: lua_unlock of a lock this thread does not hold -> abort()\n";
        (void)fwrite(msg, 1, sizeof msg - 1, stderr);
        abort();
    }
}
void LuaLockFinal(lua_State* L) { LuaUnlock(L); }

#ifdef __cplusplus
extern "C" {
#endif
int host_lua_lock_depth(void) { return lock_depth; }

static void* try_from_other_thread(void* out)
{
    int r = pthread_mutex_trylock(&lua_mutex);
    if (r == 0) (void)pthread_mutex_unlock(&lua_mutex);
    *(int*)out = (r == 0);
    return NULL;
}
/* 1 = another thread could take the Lua lock now; 0 = it is still held. */
int host_lua_lock_free_for_other_threads(void)
{
    pthread_t t;
    int ok = -1;
    if (pthread_once(&lock_once, initialize_lua_mutex) != 0) return -1;
    if (pthread_create(&t, NULL, try_from_other_thread, &ok) != 0) return -1;
    (void)pthread_join(t, NULL);
    return ok;
}
#ifdef __cplusplus
}
#endif
