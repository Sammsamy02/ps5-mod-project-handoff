#define _XOPEN_SOURCE 700
#include <pthread.h>
#include <stdlib.h>

#include "lua.h"

/* Scratch-only equivalent for LuaRaw's Windows CRITICAL_SECTION callbacks. */
static pthread_once_t lock_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t lua_mutex;
static int lock_initialization_failed;

static void initialize_lua_mutex(void) {
    pthread_mutexattr_t attributes;
    if (pthread_mutexattr_init(&attributes) != 0) {
        lock_initialization_failed = 1;
        return;
    }
    if (pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE) != 0 ||
        pthread_mutex_init(&lua_mutex, &attributes) != 0) {
        lock_initialization_failed = 1;
        (void)pthread_mutexattr_destroy(&attributes);
        return;
    }
    if (pthread_mutexattr_destroy(&attributes) != 0)
        lock_initialization_failed = 1;
}

static void ensure_lua_mutex(void) {
    if (pthread_once(&lock_once, initialize_lua_mutex) != 0 ||
        lock_initialization_failed)
        abort();
}

void LuaLockInitial(lua_State *L) {
    (void)L;
    ensure_lua_mutex();
}

void LuaLockFinal(lua_State *L) {
    /* luai_userstateclose runs for a complete main state inside lua_close.
       lua_close acquired the lock but does not call lua_unlock afterwards.
       Balance exactly that entry; retain the process-lifetime mutex for other
       live states/coroutines and preserve any outer recursive acquisition.
       Failed partial opens and coroutine frees do not call this hook.
       As with Lua itself, callers must quiesce uses before closing a state. */
    LuaUnlock(L);
}

void LuaLock(lua_State *L) {
    (void)L;
    ensure_lua_mutex();
    if (pthread_mutex_lock(&lua_mutex) != 0)
        abort();
}

void LuaUnlock(lua_State *L) {
    (void)L;
    if (pthread_mutex_unlock(&lua_mutex) != 0)
        abort();
}

