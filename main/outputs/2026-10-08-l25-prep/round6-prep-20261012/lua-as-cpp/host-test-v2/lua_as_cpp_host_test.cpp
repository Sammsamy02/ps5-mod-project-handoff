// Lua-as-C++ host test (Mac-only), lua-as-cpp track, 2026-10-12.
//
// Extends the overnight B2 test (host-tests/b2-error-wrapper/, copied in
// unchanged spirit, not modified there) and runs against the REAL Lua 5.4.7
// sources of the port (LuaRaw/src + the two patched stdio files), built TWO
// ways by run.sh:
//   C   build: Lua compiled as C   -> lua_error = longjmp (today's PS5 port)
//   C++ build: Lua compiled as C++ -> lua_error = throw (-DLUA_COMPILED_AS_CPP,
//              upstream UE4SS's LUA_COMPILE_AS_CPP option)
// Every check states what it expects in EACH build, so one source proves both
// the defect (C) and the fix (C++). macOS longjmp, like FreeBSD/PS5 longjmp,
// does not unwind C++ frames (Windows/MSVC longjmp does — not testable here).
//
// Lua is included the way the runtime does it (LuaMadeSimple.hpp:8):
// #include <lua.hpp> — extern "C" only when LUA_COMPILED_AS_CPP is not set.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

#include <lua.hpp>

extern "C" int host_lua_lock_depth(void);
extern "C" int host_lua_lock_free_for_other_threads(void);

#if defined(LUA_COMPILED_AS_CPP) || defined(HOST_LUA_IS_CPP)
static constexpr bool kCxxLua = true;
#else
static constexpr bool kCxxLua = false;
#endif

// ---- counting global operator new/delete (std::string / std::vector) --------
static long g_live_blocks = 0;
void* operator new(std::size_t n) { ++g_live_blocks; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc{}; }
void* operator new[](std::size_t n) { ++g_live_blocks; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc{}; }
void operator delete(void* p) noexcept { if (p) { --g_live_blocks; std::free(p); } }
void operator delete[](void* p) noexcept { if (p) { --g_live_blocks; std::free(p); } }
void operator delete(void* p, std::size_t) noexcept { if (p) { --g_live_blocks; std::free(p); } }
void operator delete[](void* p, std::size_t) noexcept { if (p) { --g_live_blocks; std::free(p); } }

// ---- objects with destructors ------------------------------------------------
static int g_ctor = 0, g_dtor = 0;
struct CountedError : std::runtime_error
{
    explicit CountedError(const char* m) : std::runtime_error(m) { ++g_ctor; }
    CountedError(const CountedError& o) : std::runtime_error(o) { ++g_ctor; }
    ~CountedError() override { ++g_dtor; }
};
static int g_guard_ctor = 0, g_guard_dtor = 0;
struct Guard
{
    Guard() { ++g_guard_ctor; }
    ~Guard() { ++g_guard_dtor; }
};

// ---- B2 wrapper copies (from host-tests/b2-error-wrapper/wrapper_host_test.cpp)
static bool g_format_throws = false;
static std::string handle_error(const char* m)
{
    if (g_format_throws) throw std::bad_alloc{};
    return std::string{"[handled] "} + m;
}
static void copy_text_bounded(char* out, size_t n, const char* t)
{
    size_t i = 0;
    if (t) for (; i + 1 < n && t[i]; ++i) out[i] = t[i];
    out[i] = '\0';
}
static void throw_error_impl(lua_State* L, const char* msg)  // NEW (B2)
{
    char final_text[4096];
    try { auto f = handle_error(msg); copy_text_bounded(final_text, sizeof final_text, f.c_str()); }
    catch (...) { copy_text_bounded(final_text, sizeof final_text, msg); }
    luaL_error(L, "%s", final_text);
}
template <typename F> static int TRY_NEW(lua_State* L, F f)
{
    char message[1024];
    message[0] = '\0';
    try { return f(); }
    catch (std::exception& e) { copy_text_bounded(message, sizeof message, e.what()); }
    throw_error_impl(L, message);
    return 0;
}
static void throw_error_old(lua_State* L, const std::string& m)  // OLD (pre-B2)
{
    auto f = handle_error(m.c_str());
    luaL_error(L, f.c_str());
}
template <typename F> static int TRY_OLD(lua_State* L, F f)
{
    try { return f(); }
    catch (std::exception& e) { throw_error_old(L, e.what()); return 0; }
}

// ---- bindings ----------------------------------------------------------------
static int b_new(lua_State* L) { return TRY_NEW(L, []() -> int { throw CountedError("boom-new"); }); }
static int b_old(lua_State* L) { return TRY_OLD(L, []() -> int { throw CountedError("boom-old"); }); }
static int b_pct(lua_State* L) { return TRY_NEW(L, []() -> int { throw CountedError("path %s %d %%"); }); }
// A binding with live C++ objects when it raises a Lua error directly.
static int b_frames(lua_State* L)
{
    Guard g;
    std::string big(200, 'x');           // heap block (beyond the small-string buffer)
    std::vector<int> v(1000, 7);         // heap block
    (void)big; (void)v;
    return luaL_error(L, "boom-frames");
}
// The process_lua_function shape: a std::string temporary alive in the CALLER
// while throw_error raises (throw_error(L, fmt::format(...)), LuaMadeSimple.cpp:1706).
static void throw_error_str(lua_State* L, const std::string& m) { throw_error_impl(L, m.c_str()); }
static int b_caller_temp(lua_State* L)
{
    throw_error_str(L, std::string(100, 'm') + " [process_lua_function] caller-frame temporary");
    return 0;
}
static std::mutex g_mutex;
static int b_lock(lua_State* L)
{
    std::lock_guard<std::mutex> hold(g_mutex);
    return luaL_error(L, "boom-while-locked");
}
// TRY-shaped catch(std::exception&) around a raising Lua API: must not intercept.
static int b_try_argerror(lua_State* L) { return TRY_NEW(L, [&]() -> int { (void)luaL_checkinteger(L, 1); return 0; }); }
// HAZARD 1: a catch(...) that swallows. Only the C++ build lets it see Lua errors.
static int g_swallowed = 0;
static int b_catch_all_swallow(lua_State* L)
{
    try { (void)luaL_checkinteger(L, 1); }
    catch (...) { ++g_swallowed; }
    return 0;
}
// Safe catch-all: clean up, then rethrow.
static int g_cleanups = 0;
static int b_catch_all_rethrow(lua_State* L)
{
    try { (void)luaL_checkinteger(L, 1); }
    catch (...) { ++g_cleanups; throw; }
    return 0;
}
// HAZARD 2: a Lua error crossing a noexcept frame.
static void raise_inside_noexcept(lua_State* L) noexcept { luaL_error(L, "boom-noexcept"); }
static int b_noexcept(lua_State* L) { raise_inside_noexcept(L); return 0; }
// A non-std C++ exception escaping a binding with no TRY at all.
static int b_bare_throw(lua_State*) { throw 42; }

static int g_last_status = 0;
static std::string run(lua_State* L, lua_CFunction fn, bool push_string_arg = false)
{
    lua_pushcfunction(L, fn);
    int nargs = 0;
    if (push_string_arg) { lua_pushstring(L, "not-a-number"); nargs = 1; }
    int st = lua_pcall(L, nargs, 0, 0);
    g_last_status = st;
    std::string out = st == LUA_OK ? "OK" : (lua_tostring(L, -1) ? lua_tostring(L, -1) : "<non-string error object>");
    if (st != LUA_OK) lua_pop(L, 1);
    return out;
}

static int fails = 0;
static void check(bool ok, const char* what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    fails += !ok;
}
static bool lock_balanced() { return host_lua_lock_depth() == 0 && host_lua_lock_free_for_other_threads() == 1; }

int main()
{
    std::printf("Lua build under test: %s (lua_error = %s)\n", kCxxLua ? "C++" : "C", kCxxLua ? "throw" : "longjmp");
    lua_State* L = luaL_newstate();
    constexpr int N = 1000;

    // 1-2 B2 NEW wrapper (both builds)
    g_ctor = g_dtor = 0;
    for (int i = 0; i < N; ++i) run(L, b_new);
    std::string m = run(L, b_new);
    check(g_ctor == g_dtor, "B2 NEW wrapper: every caught exception object is destroyed (both builds)");
    check(m.find("[handled] boom-new") != std::string::npos, "B2 NEW wrapper: real exception text kept (both builds)");

    // 3 B2 OLD wrapper: leaks in C, not in C++
    g_ctor = g_dtor = 0;
    for (int i = 0; i < N; ++i) run(L, b_old);
    std::printf("       OLD wrapper: ctor=%d dtor=%d (never released: %d)\n", g_ctor, g_dtor, g_ctor - g_dtor);
    if (kCxxLua) check(g_ctor == g_dtor, "B2 OLD wrapper: no exception object leaks once Lua is C++");
    else         check(g_ctor - g_dtor == N, "B2 OLD wrapper: leaks 1000/1000 with Lua as C (overnight defect reproduced)");

    // 4-5 B2 edge cases (both builds)
    m = run(L, b_pct);
    check(m.find("path %s %d %%") != std::string::npos, "B2: '%s' in a message is passed literally (both builds)");
    g_format_throws = true;
    m = run(L, b_new);
    g_format_throws = false;
    check(m.find("boom-new") != std::string::npos && m.find("[handled]") == std::string::npos,
          "B2: formatting failure raises the raw text (both builds)");

    // 6 THE CORE QUESTION: destructors of frames a Lua error crosses
    g_guard_ctor = g_guard_dtor = 0;
    long blocks_before = g_live_blocks;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) m = run(L, b_frames);
    auto t1 = std::chrono::steady_clock::now();
    long leaked_blocks = g_live_blocks - blocks_before;
    double us = std::chrono::duration<double, std::micro>(t1 - t0).count() / N;
    std::printf("       frames: guard ctor=%d dtor=%d; heap blocks leaked=%ld (string+vector per error); %.2f us per error\n",
                g_guard_ctor, g_guard_dtor, leaked_blocks, us);
    check(m.find("boom-frames") != std::string::npos, "error text reaches lua_pcall (both builds)");
    if (kCxxLua) check(g_guard_dtor == N && leaked_blocks == 0, "C++ build: destructors run in every frame a Lua error crosses (1000/1000, 0 blocks leaked)");
    else         check(g_guard_dtor == 0 && leaked_blocks == 2L * N, "C build: destructors skipped (0/1000 run, 2000 blocks leaked) = the PS5 memory-growth mechanism");

    // 7 caller-frame temporary (process_lua_function -> throw_error(L, fmt::format(...)))
    blocks_before = g_live_blocks;
    for (int i = 0; i < N; ++i) run(L, b_caller_temp);
    leaked_blocks = g_live_blocks - blocks_before;
    std::printf("       caller-frame temporary: blocks leaked=%ld\n", leaked_blocks);
    if (kCxxLua) check(leaked_blocks == 0, "C++ build: caller-frame std::string freed (B2 alone cannot fix this one)");
    else         check(leaked_blocks == N, "C build: caller-frame std::string leaks once per error even with the B2 wrapper");

    // 8 a C++ lock held across the error
    m = run(L, b_lock);
    bool relocked = g_mutex.try_lock();
    if (relocked) g_mutex.unlock();
    if (kCxxLua) check(relocked, "C++ build: a std::lock_guard held when the error is raised is released");
    else         check(!relocked, "C build: a std::lock_guard held when the error is raised stays locked forever");

    // 9 Lua's own (recursive) lock is balanced after all of the above (both builds)
    check(lock_balanced(), "Lua's global lock balanced after 4,000+ errors (both builds)");

    // 10 TRY's catch(std::exception&) does not intercept Lua errors (both builds)
    m = run(L, b_try_argerror, true);
    check(m.find("bad argument #1") != std::string::npos && m.find("[handled]") == std::string::npos,
          "TRY catch(std::exception&) lets a Lua error pass unchanged (both builds)");
    check(lock_balanced(), "Lua lock balanced after the TRY case (both builds)");

    // 11 safe catch-all: cleanup + rethrow
    g_cleanups = 0;
    m = run(L, b_catch_all_rethrow, true);
    check(m.find("bad argument #1") != std::string::npos && lock_balanced(), "catch(...) + rethrow keeps the Lua error and the lock intact (both builds)");
    if (kCxxLua) check(g_cleanups == 1, "C++ build: catch(...) cleanup code now runs for Lua errors");
    else         check(g_cleanups == 0, "C build: catch(...) cleanup code never runs for Lua errors");

    // 12 a non-std C++ exception escaping a binding without TRY (child process:
    //    it breaks containment in BOTH builds, in different ways)
    std::fflush(stdout);
    pid_t pid12 = fork();
    if (pid12 == 0)
    {
        lua_State* L2 = luaL_newstate();
        try { run(L2, b_bare_throw); }
        catch (...) { std::_Exit(3); }          // the exception flew past lua_pcall
        std::_Exit(lock_balanced() ? 0 : 4);
    }
    int w12 = 0;
    waitpid(pid12, &w12, 0);
    bool abort12 = WIFSIGNALED(w12) && WTERMSIG(w12) == SIGABRT;
    int code12 = WIFEXITED(w12) ? WEXITSTATUS(w12) : -1;
    std::printf("       bare throw child: %s\n", abort12 ? "SIGABRT" : (code12 == 3 ? "exception escaped past lua_pcall" : (code12 == 0 ? "contained, lock balanced" : "other")));
    if (kCxxLua) check(abort12, "C++ build HAZARD: a non-std exception escaping a binding (no TRY) aborts: Lua's catch(...) takes it, then lua_pcall unlocks a lock nobody re-took (platform_locks.c abort())");
    else         check(code12 == 3, "C build: a non-std exception escaping a binding flies past lua_pcall (Lua state left corrupt)");

    // 13 HAZARD: a Lua error crossing a noexcept frame (child process)
    std::fflush(stdout);
    pid_t pid = fork();
    if (pid == 0)
    {
        lua_State* L2 = luaL_newstate();
        run(L2, b_noexcept);
        std::_Exit(0);
    }
    int wst = 0;
    waitpid(pid, &wst, 0);
    bool aborted = WIFSIGNALED(wst) && WTERMSIG(wst) == SIGABRT;
    std::printf("       noexcept child: %s\n", aborted ? "killed by SIGABRT (std::terminate)" : (WIFEXITED(wst) ? "exited normally" : "other"));
    if (kCxxLua) check(aborted, "C++ build HAZARD: a Lua error crossing a noexcept function calls std::terminate");
    else         check(WIFEXITED(wst) && WEXITSTATUS(wst) == 0, "C build: a Lua error silently jumps over a noexcept function");

    // 14 HAZARD (run last): a catch(...) that swallows the Lua error
    g_swallowed = 0;
    m = run(L, b_catch_all_swallow, true);
    int depth = host_lua_lock_depth();
    int free_other = host_lua_lock_free_for_other_threads();
    std::printf("       swallow: caught=%d pcall status=%d message='%s' lock depth=%d other-thread-can-lock=%d\n",
                g_swallowed, g_last_status, m.c_str(), depth, free_other);
    if (kCxxLua)
        check(g_swallowed == 1 && depth == 1 && free_other == 0,
              "C++ build HAZARD: catch(...) swallowing a Lua error leaves Lua's lock held (other threads would block)");
    else
        check(g_swallowed == 0 && g_last_status == LUA_ERRRUN && depth == 0,
              "C build: catch(...) never sees Lua errors");
    if (kCxxLua && depth == 1) LuaUnlock(L);  // test-only rebalance before close

    lua_close(L);
    std::printf("%s lua-as-cpp host test, %s build (%d failures)\n", fails ? "FAIL" : "PASS", kCxxLua ? "C++" : "C", fails);
    return fails ? 1 : 0;
}
