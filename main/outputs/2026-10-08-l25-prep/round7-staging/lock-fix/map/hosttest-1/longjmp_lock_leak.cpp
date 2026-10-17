// MAP-1 host test (Mac-only). Question: on clang / Itanium-ABI platforms, does a
// longjmp (what luaL_error does when Lua is built as C, LuaMadeSimple.cpp:1427,
// 1557-1558) skip the destructor of a std::lock_guard alive in a binding frame?
// Shape mirrors LuaMod.cpp:5947 (RegisterHook: lock_guard, then lua.throw_error
// at 5994 -> throw_error_impl -> luaL_error) called through a Lua pcall while the
// game thread already holds the same recursive mutex (NativeLuaActionDispatchScope,
// LuaMod.cpp:4590). No Lua, no runtime code: setjmp/_longjmp stand in for
// luaD_rawrunprotected / luaD_throw (ldo.c LUAI_TRY/LUAI_THROW, C build).
#include <chrono>
#include <csetjmp>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <thread>

static std::recursive_mutex g_mutex;   // stands in for LuaMod::m_thread_actions_mutex
static jmp_buf g_jb;                   // stands in for lua_longjmp::b

extern "C" __attribute__((noinline)) void fake_luaL_error() { _longjmp(g_jb, 1); }

__attribute__((noinline)) static int binding_longjmp()
{
    std::lock_guard<std::recursive_mutex> guard{g_mutex};
    fake_luaL_error();                 // raise while the guard is alive
    return 0;
}

__attribute__((noinline)) static int binding_cxx_throw()
{
    std::lock_guard<std::recursive_mutex> guard{g_mutex};
    throw std::runtime_error{"raise"}; // what a C++-built Lua (LUAI_THROW=throw) does
}

static bool other_thread_can_lock(int wait_ms)
{
    bool got = false;
    std::thread t([&] {
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(wait_ms);
        while (std::chrono::steady_clock::now() < end)
        {
            if (g_mutex.try_lock()) { got = true; g_mutex.unlock(); return; }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    t.join();
    return got;
}

int main()
{
    int checked = 0, failed = 0;

    // Case A: C-built Lua. Dispatch scope holds the lock, pcall -> binding -> longjmp.
    {
        std::unique_lock<std::recursive_mutex> dispatch_scope{g_mutex};
        if (_setjmp(g_jb) == 0) { binding_longjmp(); }
    } // dispatch scope released here, exactly like ~NativeLuaActionDispatchScope
    const bool a = other_thread_can_lock(200);
    ++checked;
    std::printf("A longjmp-out-of-lock_guard: other thread acquired within 200ms = %s (leak if false)\n", a ? "true" : "false");
    if (a) ++failed; // the hypothesis predicts a leak; a==true would falsify it

    // Case B: control on a fresh mutex state is impossible after A leaked (A's
    // owner is the main thread forever), so release A's leaked count here: the
    // main thread is the owner and may unlock its own leaked recursion level.
    g_mutex.unlock();
    const bool a_fixed = other_thread_can_lock(200);
    ++checked;
    std::printf("A' after the owner drops the one leaked level: other thread acquired = %s (expect true)\n", a_fixed ? "true" : "false");
    if (!a_fixed) ++failed;

    // Case C: C++-built Lua (exceptions). Same shape, destructor must run.
    {
        std::unique_lock<std::recursive_mutex> dispatch_scope{g_mutex};
        try { binding_cxx_throw(); } catch (const std::exception&) {}
    }
    const bool c = other_thread_can_lock(200);
    ++checked;
    std::printf("C exception-out-of-lock_guard: other thread acquired = %s (expect true)\n", c ? "true" : "false");
    if (!c) ++failed;

    std::printf("checked=%d failed=%d -> %s\n", checked, failed, (checked > 0 && failed == 0) ? "HYPOTHESIS-CONFIRMED" : "HYPOTHESIS-NOT-CONFIRMED");
    return (checked > 0 && failed == 0) ? 0 : 1;
}
