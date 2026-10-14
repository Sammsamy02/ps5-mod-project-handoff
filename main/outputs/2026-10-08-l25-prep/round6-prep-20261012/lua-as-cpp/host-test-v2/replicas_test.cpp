// Replica test for the two catch(...) sites the review found, plus their
// staged fix, plus coroutine yields (Mac-only; lua-as-cpp track 2026-10-12).
//
// Linked by run_v2.sh against the port's real Lua 5.4.7 built three ways:
//   c       Lua as C (today)                    -> lua_error = longjmp
//   cpp     Lua as C++, C++ linkage (upstream)  -> lua_error = throw
//   cppcabi Lua as C++, C linkage (staged)      -> lua_error = throw
//
// A  = shape of typescan-r4/candidate/src/LuaUObject.cpp:96-148 + :463-485
//      (FNativeUFunctionCall owner, protected_ufunction_call catch(...),
//       inner lua_pcall, rethrow of the stored C++ exception, final lua_error)
//      wrapped in the process_lua_function TRY shape (LuaMadeSimple.hpp:843-857).
// B  = shape of LuaMadeSimple.cpp:1561-1596 throw_error_impl: try { handle_error
//      (luaL_traceback, :1536) } catch(...) { raw text } then luaL_error, with
//      Lua's allocator failing inside luaL_traceback.
// FIX (staged runtime patch 0002): `catch (void*) { throw; }` placed BEFORE
//      the catch(...). A Lua error is thrown as `struct lua_longjmp*`
//      (ldo.c:57 `throw(c)`, c is a lua_longjmp*), an object pointer, which a
//      void* handler matches ([except.handle]/3: standard pointer conversion);
//      it is re-thrown unchanged to Lua's own catch. lua_longjmp is an
//      incomplete type outside ldo.c (lstate.h), so it cannot be named.
//      In the C build no pointer is ever thrown, so the handler is dead code.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <string>

#include <lua.hpp>

extern "C" int host_lua_lock_depth(void);
extern "C" int host_lua_lock_free_for_other_threads(void);

#if defined(LUA_COMPILED_AS_CPP) || defined(HOST_LUA_IS_CPP)
static constexpr bool kCxxLua = true;
#else
static constexpr bool kCxxLua = false;
#endif

static int fails = 0, checks = 0;
static void check(bool ok, const char* what) { ++checks; std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what); fails += !ok; }
static bool lock_balanced() { return host_lua_lock_depth() == 0 && host_lua_lock_free_for_other_threads() == 1; }

// ------------------------------------------------------------------ A
static int g_inv_dtor = 0, g_swallowed = 0, g_std_mode = 0;
struct Invocation { std::exception_ptr Pending{}; ~Invocation() { ++g_inv_dtor; } };
static int impl(lua_State* L)
{
    if (g_std_mode) throw std::runtime_error("std exception from marshalling");
    return luaL_error(L, "%s", "UFunction expected 2 parameters, received 1");
}
template <bool Fixed> static int protected_ufunction_call(lua_State* L)
{
    auto* inv = static_cast<Invocation*>(lua_touserdata(L, lua_upvalueindex(1)));
    try { return impl(L); }
    catch (void*) { if (Fixed) throw; ++g_swallowed; inv->Pending = std::current_exception(); return 0; }
    catch (...) { ++g_swallowed; inv->Pending = std::current_exception(); return 0; }
}
template <bool Fixed> static int call_ufunction_from_lua(lua_State* L)
{
    const int n = lua_gettop(L);
    int status;
    {
        Invocation inv;
        lua_pushlightuserdata(L, &inv);
        lua_pushcclosure(L, protected_ufunction_call<Fixed>, 1);
        lua_insert(L, 1);
        status = lua_pcall(L, n, LUA_MULTRET, 0);
        if (inv.Pending) std::rethrow_exception(inv.Pending);
    }
    if (status != LUA_OK) return lua_error(L);
    return lua_gettop(L);
}
template <bool Fixed> static int binding(lua_State* L)  // process_lua_function TRY shape
{
    char msg[1024];
    msg[0] = 0;
    try { return call_ufunction_from_lua<Fixed>(L); }
    catch (std::exception& e) { std::snprintf(msg, sizeof msg, "%s", e.what()); }
    return luaL_error(L, "%s", msg);
}
struct ARun { int status_errrun = 0, kept = 0, last_status = 0; std::string last; std::string lua_side; };
template <bool Fixed> static ARun run_a(lua_State* L, const char* expect)
{
    ARun r;
    for (int i = 0; i < 1000; ++i)
    {
        lua_pushcfunction(L, binding<Fixed>);
        lua_pushinteger(L, 7);
        r.last_status = lua_pcall(L, 1, 0, 0);
        const char* s = lua_tostring(L, -1);
        r.last = s ? s : "<non-string error object>";
        r.status_errrun += r.last_status == LUA_ERRRUN;
        r.kept += s && std::strstr(s, expect) != nullptr;
        lua_settop(L, 0);
    }
    lua_pushcfunction(L, binding<Fixed>);
    lua_setglobal(L, "CallFunction");
    if (luaL_dostring(L, "local ok, err = pcall(CallFunction, 7) return tostring(err)") == LUA_OK)
        r.lua_side = lua_tostring(L, -1) ? lua_tostring(L, -1) : "?";
    lua_settop(L, 0);
    return r;
}

// ------------------------------------------------------------------ B
static bool g_fail_alloc = false;
static void* failing_alloc(void*, void* p, size_t, size_t n)
{
    if (n == 0) { std::free(p); return nullptr; }
    if (g_fail_alloc) return nullptr;
    return std::realloc(p, n);
}
static int g_b_swallowed = 0;
template <bool Fixed> static int throw_error_impl_shape(lua_State* L)
{
    char final_text[256];
    std::snprintf(final_text, sizeof final_text, "%s", "raw error text");
    try
    {
        g_fail_alloc = true;
        luaL_traceback(L, L, "UFunction expected 2 parameters, received 1", 0);
        g_fail_alloc = false;
    }
    catch (void*) { g_fail_alloc = false; if (Fixed) throw; ++g_b_swallowed; }
    catch (...) { g_fail_alloc = false; ++g_b_swallowed; }
    g_fail_alloc = false;
    return luaL_error(L, "%s", final_text);
}
template <bool Fixed> static void run_b(const char* label, int expect_status_fixed)
{
    g_fail_alloc = false;
    lua_State* L = lua_newstate(failing_alloc, nullptr);
    if (!L) { check(false, "lua_newstate failed"); return; }
    g_b_swallowed = 0;
    lua_pushcfunction(L, throw_error_impl_shape<Fixed>);
    const int st = lua_pcall(L, 0, 0, 0);
    g_fail_alloc = false;  // the C build longjmps past the reset inside the shape
    const char* s = lua_tostring(L, -1);
    const int depth = host_lua_lock_depth();
    const int other = host_lua_lock_free_for_other_threads();
    std::printf("       %s: pcall status=%d (2=ERRRUN 4=ERRMEM) msg='%s' swallowed=%d lock depth after=%d other-thread-can-lock=%d\n",
                label, st, s ? s : "?", g_b_swallowed, depth, other);
    if (Fixed || !kCxxLua)
        check(st == expect_status_fixed && g_b_swallowed == 0 && depth == 0 && other == 1,
              Fixed ? "B fixed: out-of-memory inside the error formatter propagates as LUA_ERRMEM, Lua lock released (all builds)"
                    : "B current, C build: LUA_ERRMEM propagates, lock released");
    else
        check(st == LUA_ERRRUN && g_b_swallowed == 1 && depth == 1 && other == 0,
              "B current, C++ build: BUG reproduced -- LUA_ERRMEM swallowed, Lua's global lock left held (other threads would hang)");
    while (host_lua_lock_depth() > 0) LuaUnlock(L);  // test-only rebalance before close
    lua_close(L);
}

// ------------------------------------------------------------------ yields
static int g_yield_dtor = 0;
struct YieldGuard { ~YieldGuard() { ++g_yield_dtor; } };
static int c_yielder(lua_State* L)
{
    YieldGuard g;  // lives in a C function frame that lua_yield leaves
    return lua_yield(L, 0);
}

int main()
{
    std::printf("Lua build under test: %s (lua_error = %s)\n", kCxxLua ? "C++" : "C", kCxxLua ? "throw" : "longjmp");
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    // A current
    g_inv_dtor = g_swallowed = 0; g_std_mode = 0;
    ARun a = run_a<false>(L, "UFunction expected 2 parameters");
    std::printf("       A current: status==LUA_ERRRUN %d/1000, message kept %d/1000, last_status=%d last='%s'; swallowed=%d dtors=%d; Lua-side pcall err='%s'\n",
                a.status_errrun, a.kept, a.last_status, a.last.c_str(), g_swallowed, g_inv_dtor, a.lua_side.c_str());
    if (!kCxxLua)
        check(a.status_errrun == 1000 && a.kept == 1000 && g_swallowed == 0, "A current, C build: Lua error keeps status 2 and its message");
    else
        check(a.status_errrun == 0 && a.kept == 0 && a.last_status == -1 && g_swallowed == 1001,
              "A current, C++ build: BUG reproduced -- every Lua error in a UFunction call is swallowed (status -1, message lost)");
    check(lock_balanced(), "A current: Lua lock balanced afterwards (all builds)");

    // A fixed
    g_inv_dtor = g_swallowed = 0;
    a = run_a<true>(L, "UFunction expected 2 parameters");
    std::printf("       A fixed:   status==LUA_ERRRUN %d/1000, message kept %d/1000, last_status=%d; swallowed=%d dtors=%d; Lua-side pcall err='%s'\n",
                a.status_errrun, a.kept, a.last_status, g_swallowed, g_inv_dtor, a.lua_side.c_str());
    check(a.status_errrun == 1000 && a.kept == 1000 && g_swallowed == 0 && g_inv_dtor == 1001 &&
          a.lua_side.find("UFunction expected 2 parameters") != std::string::npos,
          "A fixed: Lua error keeps status 2 and its message, owner destructor runs every time (all builds)");
    check(lock_balanced(), "A fixed: Lua lock balanced afterwards (all builds)");

    // A fixed, std exception path still kept away from Lua and converted by TRY
    g_inv_dtor = g_swallowed = 0; g_std_mode = 1;
    a = run_a<true>(L, "std exception from marshalling");
    g_std_mode = 0;
    std::printf("       A fixed, std::exception: status==LUA_ERRRUN %d/1000, message kept %d/1000; stored=%d dtors=%d\n",
                a.status_errrun, a.kept, g_swallowed, g_inv_dtor);
    check(a.status_errrun == 1000 && a.kept == 1000 && g_swallowed == 1001 && g_inv_dtor == 1001,
          "A fixed: a C++ std::exception is still stored by catch(...), rethrown after the inner pcall and turned into a Lua error by TRY (all builds)");
    check(lock_balanced(), "A fixed, std::exception: Lua lock balanced afterwards (all builds)");

    // B current + fixed (separate states with a failing allocator)
    run_b<false>("B current", LUA_ERRMEM);
    run_b<true>("B fixed  ", LUA_ERRMEM);

    // coroutine yields: correctness + cost
    const char* co = "local co = coroutine.wrap(function() local n = 0 while true do n = n + 1 coroutine.yield(n) end end) "
                     "local last = 0 for i = 1, 100000 do last = co() end return last";
    auto t0 = std::chrono::steady_clock::now();
    int rc = luaL_dostring(L, co);
    auto t1 = std::chrono::steady_clock::now();
    long long last = rc == LUA_OK ? lua_tointeger(L, -1) : -1;
    lua_settop(L, 0);
    double us = std::chrono::duration<double, std::micro>(t1 - t0).count() / 100000.0;
    std::printf("       coroutine: rc=%d last=%lld, 100000 yields: %.3f us per yield\n", rc, last, us);
    check(rc == LUA_OK && last == 100000, "coroutine.yield/resume: 100000 round trips return the right values (all builds)");
    // a yield from a C function: does its frame's destructor run?
    lua_State* T = lua_newthread(L);
    g_yield_dtor = 0;
    lua_pushcfunction(T, c_yielder);
    int nres = 0;
    int st = lua_resume(T, L, 0, &nres);
    std::printf("       C-function yield: resume status=%d (1=LUA_YIELD), frame destructor ran %d time(s)\n", st, g_yield_dtor);
    if (kCxxLua) check(st == LUA_YIELD && g_yield_dtor == 1, "C++ build: a yield from a C function is a throw; that frame's destructor runs");
    else         check(st == LUA_YIELD && g_yield_dtor == 0, "C build: a yield from a C function longjmps; that frame's destructor is skipped");
    lua_settop(L, 0);
    check(lock_balanced(), "after coroutines: Lua lock balanced (all builds)");

    lua_close(L);
    std::printf("checks run: %d\n", checks);
    std::printf("%s replicas test, %s build (%d failures)\n", (fails || checks == 0) ? "FAIL" : "PASS", kCxxLua ? "C++" : "C", fails);
    return (fails || checks == 0) ? 1 : 0;
}
