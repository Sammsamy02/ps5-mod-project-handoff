// MAP-2 host test (Mac-only). Second, independent check of the MAP-1 stand-in
// (hosttest-1): this one uses the REAL Lua 5.4 C sources that the PS5 build
// composes (deps/first/LuaRaw/src, compiled as C exactly like the composition:
// prospero-clang -std=c11, so ldo.c picks LUAI_THROW = longjmp), a REAL Lua
// chunk calling pcall(binding, ...) three times (the shape of a mod that does
// pcall(RegisterHook, path, fn) in a loop), and a binding shaped like
// LuaMod.cpp:5947-5996 (RegisterHook: lock_guard on the shared recursive mutex
// first, then lua.throw_error -> LuaMadeSimple throw_error_impl -> luaL_error).
//
// Question: after the pcall returns, does ANOTHER thread still get the mutex?
// Expected on clang/Itanium (macOS, FreeBSD/PS5): no, the raising thread keeps
// one ownership level per raise; it can itself keep re-locking (recursive).
// Control: the same raise with the guard's scope closed before luaL_error.
// No runtime code, no game code, no mod code.
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>

static std::recursive_mutex g_mutex; // stands in for LuaMod::m_thread_actions_mutex

// Shape of RegisterHook (LuaMod.cpp:5947 lock, 5994/6043 lua.throw_error).
static int binding_raise_under_guard(lua_State* L)
{
    std::lock_guard<std::recursive_mutex> guard{g_mutex};
    return luaL_error(L, "%s", "hook target not found"); // longjmp while guard is alive
}

// Fix shape: finish all locked work, close the scope, THEN raise.
static int binding_raise_after_scope(lua_State* L)
{
    bool failed = false;
    {
        std::lock_guard<std::recursive_mutex> guard{g_mutex};
        failed = true; // decide under the lock, raise outside it
    }
    if (failed)
    {
        return luaL_error(L, "%s", "hook target not found");
    }
    return 0;
}

static bool other_thread_can_lock(int wait_ms)
{
    bool got = false;
    std::thread t([&] {
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(wait_ms);
        while (std::chrono::steady_clock::now() < end)
        {
            if (g_mutex.try_lock())
            {
                got = true;
                g_mutex.unlock();
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    t.join();
    return got;
}

int main()
{
    int checked = 0, failed = 0;
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "binding_raise_under_guard", binding_raise_under_guard);
    lua_register(L, "binding_raise_after_scope", binding_raise_after_scope);

    // Case 1 (bug shape): three pcall'd raises from a Lua chunk, run inside
    // lua_pcall (luaL_dostring) like a game-thread callback drain does.
    const char* chunk_bug =
        "local n = 0 "
        "for i = 1, 3 do local ok = pcall(binding_raise_under_guard) if not ok then n = n + 1 end end "
        "return n";
    if (luaL_dostring(L, chunk_bug) != LUA_OK) { std::printf("chunk_bug failed to run\n"); return 2; }
    const int raised = static_cast<int>(lua_tointeger(L, -1));
    lua_settop(L, 0);
    const bool other_after_bug = other_thread_can_lock(300);
    ++checked; if (other_after_bug) ++failed;
    std::printf("1 raise-under-guard x%d via Lua pcall: other thread acquired within 300ms = %s (leak if false)\n",
                raised, other_after_bug ? "true" : "false");

    // Case 1b: the raising thread itself is not blocked (recursive owner keeps
    // running, exactly like a game thread that keeps dispatching).
    const bool self_relock = g_mutex.try_lock();
    if (self_relock) g_mutex.unlock();
    ++checked; if (!self_relock) ++failed;
    std::printf("1b raising thread re-locks freely = %s (expect true)\n", self_relock ? "true" : "false");

    // Case 1c: count the leaked levels by dropping them one at a time.
    int dropped = 0;
    while (!other_thread_can_lock(50) && dropped < 16) { g_mutex.unlock(); ++dropped; }
    ++checked; if (dropped != raised) ++failed;
    std::printf("1c leaked ownership levels = %d (expect %d, one per raise)\n", dropped, raised);

    // Case 2 (fix shape): same three raises, guard scope closed before luaL_error.
    const char* chunk_fix =
        "local n = 0 "
        "for i = 1, 3 do local ok = pcall(binding_raise_after_scope) if not ok then n = n + 1 end end "
        "return n";
    if (luaL_dostring(L, chunk_fix) != LUA_OK) { std::printf("chunk_fix failed to run\n"); return 2; }
    const int raised_fix = static_cast<int>(lua_tointeger(L, -1));
    lua_settop(L, 0);
    const bool other_after_fix = other_thread_can_lock(300);
    ++checked; if (!other_after_fix) ++failed;
    std::printf("2 raise-after-scope x%d via Lua pcall: other thread acquired = %s (expect true)\n",
                raised_fix, other_after_fix ? "true" : "false");

    lua_close(L);
    std::printf("checked=%d failed=%d -> %s\n", checked, failed,
                checked == 0 ? "NO-CHECKS (gate fail)" : (failed == 0 ? "LEAK-REPRODUCED-WITH-REAL-LUA" : "UNEXPECTED"));
    return (checked == 0 || failed != 0) ? 1 : 0;
}
