// B7 host test (Mac-only): Lua built as C (today) versus Lua built as C++ with C names (staged B7),
// exercised with the REAL runtime code (extracted byte-for-byte by extract_real.py):
//   real_cur*.inc    the B2 error wrapper + UFunction call wrapper as they are TODAY (B4 candidate)
//   real_fixed*.inc  the same code with the staged B7 edits E1/E2 (catch (void*) { throw; })
//   real_try.inc     the free TRY(lua_State*, ...) template
//   real_probe.inc   the staged startup probe ("[LUA-CPP] mode=...")
// The same program is linked against both Lua builds (HOST_LUA_IS_CPP defined for the C++ one) and
// asserts the expected result of EACH build, so a pass means "C behaves like today, C++ behaves as
// designed". Lines starting with "T:" are compared across the two builds by run.sh.
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unordered_map>
#include <vector>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lstate.h"
int host_lua_lock_depth(void);
int host_lua_lock_free_for_other_threads(void);
void LuaUnlock(lua_State* L);
}

#define RC_LMS_API

#ifdef HOST_LUA_IS_CPP
static constexpr bool kLuaIsCpp = true;
#else
static constexpr bool kLuaIsCpp = false;
#endif

// ---- bookkeeping --------------------------------------------------------------------------
static int g_checked = 0, g_failed = 0;
static void check(bool ok, const std::string& what)
{
    ++g_checked;
    if (!ok) ++g_failed;
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}
static std::string sha_short(const std::string& s)  // tiny FNV-1a, enough to compare transcripts
{
    unsigned long long h = 1469598103934665603ULL;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; }
    char b[24]; std::snprintf(b, sizeof b, "%016llx", h);
    return b;
}

// ---- heap accounting for C++ allocations (counts blocks the Lua allocator does not own) -------------
static std::atomic<long> g_live_blocks{0};
void* operator new(std::size_t n) { void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc{}; g_live_blocks.fetch_add(1, std::memory_order_relaxed); return p; }
void operator delete(void* p) noexcept { if (p) { g_live_blocks.fetch_sub(1, std::memory_order_relaxed); std::free(p); } }
void operator delete(void* p, std::size_t) noexcept { if (p) { g_live_blocks.fetch_sub(1, std::memory_order_relaxed); std::free(p); } }

// ---- a Lua allocator that can be made to fail (to force LUA_ERRMEM) -------------------------------
struct FailAlloc
{
    long countdown = -1;     // -1 = never; N = fail the N-th growth request from now
    int window = 0;          // consecutive failures left (2 = the request and Lua's emergency-GC retry)
    long failures = 0;
};
static void* fail_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    auto* f = static_cast<FailAlloc*>(ud);
    if (nsize == 0) { std::free(ptr); return nullptr; }
    if (nsize > osize)
    {
        if (f->window > 0) { --f->window; ++f->failures; return nullptr; }
        if (f->countdown > 0 && --f->countdown == 0) { f->window = 1; ++f->failures; return nullptr; }
    }
    return std::realloc(ptr, nsize);
}
static lua_State* new_state(FailAlloc* fa)
{
    lua_State* L = lua_newstate(fail_alloc, fa);
    luaL_openlibs(L);
    return L;
}

// ---- a probe thread: can another thread take this mutex? (never hangs) --------------------------------
static bool other_thread_can_lock(std::recursive_mutex& mtx, int timeout_ms = 200)
{
    bool acquired = false;
    std::thread b([&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (mtx.try_lock()) { mtx.unlock(); acquired = true; return; }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    b.join();
    return acquired;
}

// ===================================================================================================
// Real runtime code, two generations, each in its own namespace (statics and globals do not clash).
// ===================================================================================================
namespace RC
{
    namespace LuaMadeSimple
    {
        struct Lua { lua_State* L; auto get_lua_state() const -> lua_State* { return L; } };
    }
}

static int g_uobj_alive = 0, g_uobj_dtor = 0;
enum class UMode { LuaError, StdError, Ok };
static UMode g_umode = UMode::Ok;

namespace RC { namespace Cur {
    using LuaMadeSimple::Lua;
    static std::unordered_map<lua_State*, std::string> lua_state_errors;
    static int g_notify_calls = 0;
    static auto notify_error_callbacks(lua_State*, const std::string&, const std::string&) -> void { ++g_notify_calls; }
    auto capture_cxx_exception_text(char* buffer, size_t buffer_size, const char* what) noexcept -> void;
    auto throw_error_from_buffer(lua_State* lua_state, const char* message) -> void;
    auto handle_error(lua_State* lua_state, const std::string& error_message) -> const std::string;
    auto throw_error(lua_State* lua_state, const std::string& error_message) -> void;
#include "real_cur.inc"
#include "real_try.inc"
    struct FNativeUFunctionCall
    {
        const LuaMadeSimple::Lua& Lua;
        std::exception_ptr PendingException{};
        explicit FNativeUFunctionCall(const LuaMadeSimple::Lua& InLua) : Lua(InLua) { ++g_uobj_alive; }
        FNativeUFunctionCall(const FNativeUFunctionCall&) = delete;
        ~FNativeUFunctionCall() { --g_uobj_alive; ++g_uobj_dtor; }
    };
    static auto call_ufunction_from_lua_impl(const LuaMadeSimple::Lua& lua, FNativeUFunctionCall&) -> int
    {
        if (g_umode == UMode::LuaError) throw_error(lua.get_lua_state(), "UFunction expected 2 parameters, received 1");
        if (g_umode == UMode::StdError) throw std::runtime_error("std error inside marshalling");
        lua_pushinteger(lua.get_lua_state(), 42);
        return 1;
    }
#include "real_uobj_cur.inc"
} }

namespace RC { namespace Fixed {
    using LuaMadeSimple::Lua;
    static std::unordered_map<lua_State*, std::string> lua_state_errors;
    static int g_notify_calls = 0;
    static auto notify_error_callbacks(lua_State*, const std::string&, const std::string&) -> void { ++g_notify_calls; }
    auto capture_cxx_exception_text(char* buffer, size_t buffer_size, const char* what) noexcept -> void;
    auto throw_error_from_buffer(lua_State* lua_state, const char* message) -> void;
    auto handle_error(lua_State* lua_state, const std::string& error_message) -> const std::string;
    auto throw_error(lua_State* lua_state, const std::string& error_message) -> void;
#include "real_fixed.inc"
#include "real_try.inc"
    struct FNativeUFunctionCall
    {
        const LuaMadeSimple::Lua& Lua;
        std::exception_ptr PendingException{};
        explicit FNativeUFunctionCall(const LuaMadeSimple::Lua& InLua) : Lua(InLua) { ++g_uobj_alive; }
        FNativeUFunctionCall(const FNativeUFunctionCall&) = delete;
        ~FNativeUFunctionCall() { --g_uobj_alive; ++g_uobj_dtor; }
    };
    static auto call_ufunction_from_lua_impl(const LuaMadeSimple::Lua& lua, FNativeUFunctionCall&) -> int
    {
        if (g_umode == UMode::LuaError) throw_error(lua.get_lua_state(), "UFunction expected 2 parameters, received 1");
        if (g_umode == UMode::StdError) throw std::runtime_error("std error inside marshalling");
        lua_pushinteger(lua.get_lua_state(), 42);
        return 1;
    }
#include "real_uobj_fixed.inc"
} }

namespace RC { namespace Probe {
#include "real_probe.inc"
} }

// ===================================================================================================
// Scenario plumbing
// ===================================================================================================
static std::recursive_mutex g_mtx;         // stands in for LuaMod::m_thread_actions_mutex
static int g_sentinels_made = 0, g_sentinels_dtor = 0;
struct Sentinel
{
    Sentinel() { ++g_sentinels_made; }
    Sentinel(const Sentinel&) = delete;
    ~Sentinel() { ++g_sentinels_dtor; }
};

enum class Shape { Old, New, Control };   // OLD: lua.throw_error under the guard; NEW: std::runtime_error under the guard (B5); CONTROL: raise after the guard
template <typename NS> struct Gen;
#define GEN_TRAITS(NSNAME)                                                                                   \
    template <> struct Gen<NSNAME##Tag> {                                                                      \
        static int tryf(lua_State* L, const std::function<int()>& f) { return RC::NSNAME::TRY(L, [&] { return f(); }); } \
        static void raise(lua_State* L, const std::string& m) { RC::NSNAME::throw_error(L, m); }               \
        static int notify() { return RC::NSNAME::g_notify_calls; }                                              \
        static int ufcall(lua_State* L) { RC::LuaMadeSimple::Lua lua{L}; return RC::NSNAME::call_ufunction_from_lua(lua); } \
    };
struct CurTag {}; struct FixedTag {};
GEN_TRAITS(Cur)
GEN_TRAITS(Fixed)

static Shape g_shape = Shape::Old;
static std::string g_msg = "[RegisterHook] refused: hook not supported";

// A registered Lua function, shaped like process_lua_function: TRY around a lambda.
template <typename G> static int lua_boom(lua_State* L)
{
    return G::tryf(L, [&]() -> int {
        std::string local = "frame-local string, long enough to leave the small-string buffer ..........................";
        Sentinel s;
        switch (g_shape)
        {
        case Shape::Old: { std::lock_guard<std::recursive_mutex> guard{g_mtx}; G::raise(L, g_msg); break; }
        case Shape::New: { std::lock_guard<std::recursive_mutex> guard{g_mtx}; throw std::runtime_error(g_msg); }
        case Shape::Control: { { std::lock_guard<std::recursive_mutex> guard{g_mtx}; } G::raise(L, g_msg); break; }
        }
        return 0;
    });
}
template <typename G> static int lua_ufcall(lua_State* L) { return G::tryf(L, [&]() -> int { return G::ufcall(L); }); }

struct PcallResult { int status; std::string err; };
static PcallResult run_pcall(lua_State* L, const char* fn_global)
{
    lua_settop(L, 0);
    lua_getglobal(L, fn_global);
    int st = lua_pcall(L, 0, 0, 0);
    PcallResult r{st, ""};
    if (st != LUA_OK) { const char* m = lua_tostring(L, -1); r.err = m ? m : "<non-string error object>"; }
    lua_settop(L, 0);
    return r;
}

template <typename G> static void scenario_b2_wrapper(const char* gen, bool expect_old_leak_in_c)
{
    std::printf("\n[B2 wrapper end to end, %s code] real TRY + real throw_error_impl; lock_guard alive when the error is raised\n", gen);
    FailAlloc fa; lua_State* L = new_state(&fa);
    lua_pushcfunction(L, lua_boom<G>); lua_setglobal(L, "boom");
    struct Row { const char* name; Shape shape; };
    std::string text_new, texts[3];
    for (Row row : {Row{"OLD", Shape::Old}, Row{"NEW", Shape::New}, Row{"CONTROL", Shape::Control}})
    {
        g_shape = row.shape; g_sentinels_made = g_sentinels_dtor = 0;
        const int depth0 = host_lua_lock_depth();
        const long blocks0 = g_live_blocks.load();
        auto r = run_pcall(L, "boom");
        const bool free_ = other_thread_can_lock(g_mtx);
        const bool lua_lock_free = host_lua_lock_free_for_other_threads() == 1;
        const int depth1 = host_lua_lock_depth();
        const long leaked_blocks = g_live_blocks.load() - blocks0;
        std::printf("       %-8s status=%d app-mutex-free=%d lua-lock-depth=%d->%d lua-lock-free=%d sentinel dtors=%d/%d leaked C++ blocks=%ld\n",
                    row.name, r.status, free_, depth0, depth1, lua_lock_free, g_sentinels_dtor, g_sentinels_made, leaked_blocks);
        std::printf("T: b2.%s.%s status=%d text=%s\n", gen, row.name, r.status, sha_short(r.err).c_str());
        check(r.status == LUA_ERRRUN, std::string(gen) + " " + row.name + ": pcall status is LUA_ERRRUN");
        check(r.err.find(g_msg) != std::string::npos && r.err.find("stack traceback:") != std::string::npos, std::string(gen) + " " + row.name + ": error text = message + traceback");
        check(depth1 == depth0 && lua_lock_free, std::string(gen) + " " + row.name + ": Lua's own global lock is balanced");
        if (row.shape == Shape::Old)
        {
            if (!kLuaIsCpp)
            {
                check(!free_, std::string(gen) + " OLD, Lua as C: the app mutex is LEAKED (the B4 launch-1 bug, reproduced with the real wrapper)");
                check(g_sentinels_dtor == 0, std::string(gen) + " OLD, Lua as C: frame destructors skipped (longjmp)");
                g_mtx.unlock();   // the owner (this thread) releases the leaked lock so the test can go on
            }
            else
            {
                check(free_, std::string(gen) + " OLD, Lua as C++: the app mutex is RELEASED by unwinding (B7 alone cures the lock-then-raise class)");
                check(g_sentinels_dtor == g_sentinels_made && g_sentinels_made == 1, std::string(gen) + " OLD, Lua as C++: frame destructors ran");
            }
        }
        else if (row.shape == Shape::New)
        {
            check(free_, std::string(gen) + " NEW: the app mutex is free (both builds)");
            check(g_sentinels_dtor == g_sentinels_made, std::string(gen) + " NEW: frame destructors ran (both builds; the throw is a plain C++ exception caught by the real TRY)");
        }
        else
        {
            check(free_, std::string(gen) + " CONTROL: the app mutex is free (both builds)");
            check(kLuaIsCpp ? g_sentinels_dtor == g_sentinels_made : g_sentinels_dtor == 0,
                  std::string(gen) + (kLuaIsCpp ? " CONTROL, Lua as C++: the frame's other locals are destroyed too" : " CONTROL, Lua as C: only the lock is saved; the frame's other locals are still skipped by the longjmp"));
        }
        if (row.shape == Shape::New) text_new = r.err;
        texts[static_cast<int>(row.shape)] = r.err;
    }
    check(texts[0] == texts[1] && texts[1] == texts[2], std::string(gen) + ": Lua-visible error text identical for OLD, NEW and CONTROL");
    (void)expect_old_leak_in_c;
    lua_close(L);
}

// ---- destructors across several C++ frames, many times -------------------------------------------
template <typename G> static int lua_deep(lua_State* L)
{
    struct Frame3 { static void go(lua_State* L) { std::string s(300, 'z'); std::vector<int> v(64, 7); Sentinel x; luaL_error(L, "%s", "deep error"); (void)s; (void)v; } };
    struct Frame2 { static void go(lua_State* L) { std::lock_guard<std::recursive_mutex> g{g_mtx}; Sentinel x; Frame3::go(L); } };
    std::string outer(200, 'o'); Sentinel x;
    Frame2::go(L);
    return 0;
}
static void scenario_destructors()
{
    std::printf("\n[destructors across frames] 1000 Lua errors, each crossing 3 C++ frames (std::string, std::vector, lock_guard, sentinel)\n");
    FailAlloc fa; lua_State* L = new_state(&fa);
    lua_pushcfunction(L, lua_deep<CurTag>); lua_setglobal(L, "deep");
    g_sentinels_made = g_sentinels_dtor = 0;
    const long blocks0 = g_live_blocks.load();
    int errors = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) if (run_pcall(L, "deep").status == LUA_ERRRUN) ++errors;
    const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / 1000.0;
    const long leaked = g_live_blocks.load() - blocks0;
    const bool free_ = other_thread_can_lock(g_mtx);
    std::printf("       errors=%d sentinel dtors=%d/%d leaked C++ blocks=%ld app-mutex-free=%d  %.2f us per error (host -O1)\n", errors, g_sentinels_dtor, g_sentinels_made, leaked, free_, us);
    std::printf("T: destructors errors=%d\n", errors);
    check(errors == 1000, "1000 of 1000 errors reach lua_pcall as LUA_ERRRUN");
    if (kLuaIsCpp)
    {
        check(g_sentinels_dtor == g_sentinels_made && g_sentinels_made == 3000, "Lua as C++: every frame destructor ran (3000/3000)");
        check(leaked == 0, "Lua as C++: 0 C++ heap blocks left behind");
        check(free_, "Lua as C++: the app mutex is free after 1000 errors");
    }
    else
    {
        check(g_sentinels_dtor == 0, "Lua as C (today): 0 of 3000 frame destructors ran");
        check(leaked == 3000, "Lua as C (today): 3000 C++ heap blocks leaked (3 per error: the three frames' string, vector, string)");
        check(!free_, "Lua as C (today): the app mutex is leaked");
        for (int i = 0; i < 1000; ++i) g_mtx.unlock();
    }
    lua_close(L);
}

// ---- allocator failure while the error text is built (real throw_error_impl), swept ---------------------
template <typename G> static int lua_raise_plain(lua_State* L) { G::raise(L, "plain error from a binding"); return 0; }
struct OomOutcome { int status; int depth_after; bool lua_lock_free; };
template <typename G> static OomOutcome oom_run(long k)
{
    FailAlloc fa; lua_State* L = new_state(&fa);
    lua_pushcfunction(L, lua_raise_plain<G>); lua_setglobal(L, "raiser");
    lua_getglobal(L, "raiser");
    const int d0 = host_lua_lock_depth();
    fa.countdown = k;
    int st = lua_pcall(L, 0, 0, 0);
    fa.countdown = -1; fa.window = 0;
    OomOutcome o{st, host_lua_lock_depth() - d0, host_lua_lock_free_for_other_threads() == 1};
    lua_settop(L, 0);
    lua_close(L);
    return o;
}
template <typename G> static void scenario_oom(const char* gen, bool expect_bug_on_cpp)
{
    constexpr long K = 40;
    int leaked_cases = 0, errmem_cases = 0, ok_cases = 0, errrun_cases = 0, other = 0;
    long first_leak_k = -1;
    for (long k = 1; k <= K; ++k)
    {
        auto o = oom_run<G>(k);
        if (o.depth_after != 0 || !o.lua_lock_free) { ++leaked_cases; if (first_leak_k < 0) first_leak_k = k; }
        if (o.status == LUA_OK) ++ok_cases; else if (o.status == LUA_ERRMEM) ++errmem_cases; else if (o.status == LUA_ERRRUN) ++errrun_cases; else ++other;
        while (host_lua_lock_depth() > 0) LuaUnlock(nullptr);   // restore a clean baseline for the next point
    }
    std::printf("       %-5s over allocator-failure points 1..%ld: status ERRMEM=%d ERRRUN=%d OK=%d other=%d; lock left held in %d case(s) (first at k=%ld)\n",
                gen, K, errmem_cases, errrun_cases, ok_cases, other, leaked_cases, first_leak_k);
    std::printf("T: oom.%s errmem=%d errrun=%d ok=%d other=%d leaked=%d\n", gen, errmem_cases, errrun_cases, ok_cases, other, leaked_cases);
    check(errmem_cases + errrun_cases + ok_cases == K && other == 0, std::string(gen) + ": every sweep point ends in a normal pcall status (no abort, no -1)");
    if (expect_bug_on_cpp)
    {
        if (kLuaIsCpp) check(leaked_cases >= 1, std::string(gen) + ", Lua as C++: BUG reproduced with the real code: an out-of-memory error while the text is built is swallowed and Lua's lock stays held");
        else           check(leaked_cases == 0, std::string(gen) + ", Lua as C (today): lock balanced at every point");
    }
    else
    {
        check(leaked_cases == 0, std::string(gen) + ": Lua's lock balanced at every sweep point (both builds)");
        if (kLuaIsCpp) check(errmem_cases >= 1, std::string(gen) + ", Lua as C++: the out-of-memory error reaches lua_pcall as LUA_ERRMEM");
    }
}

// ---- real UFunction call wrapper ------------------------------------------------------------------------
template <typename G> static void scenario_ufunction(const char* gen, bool expect_bug_on_cpp)
{
    std::printf("\n[UFunction call wrapper, %s code] real protected_ufunction_call + call_ufunction_from_lua\n", gen);
    FailAlloc fa; lua_State* L = new_state(&fa);
    lua_pushcfunction(L, lua_ufcall<G>); lua_setglobal(L, "ufcall");
    struct Row { const char* name; UMode mode; };
    for (Row row : {Row{"LUA-ERROR", UMode::LuaError}, Row{"STD-ERROR", UMode::StdError}, Row{"OK", UMode::Ok}})
    {
        g_umode = row.mode; g_uobj_alive = 0; g_uobj_dtor = 0;
        int errors = 0, msg_kept = 0, wrong_status = 0;
        for (int i = 0; i < 200; ++i)
        {
            auto r = run_pcall(L, "ufcall");
            if (row.mode == UMode::Ok) { if (r.status == LUA_OK) ++msg_kept; continue; }
            if (r.status == LUA_ERRRUN) ++errors; else ++wrong_status;
            if (r.err.find(row.mode == UMode::LuaError ? "UFunction expected 2 parameters" : "std error inside marshalling") != std::string::npos) ++msg_kept;
        }
        std::printf("       %-9s of 200: status ok-or-ERRRUN=%d message kept=%d wrong status=%d owner dtors=%d alive=%d lock-free=%d\n",
                    row.name, row.mode == UMode::Ok ? msg_kept : errors, msg_kept, wrong_status, g_uobj_dtor, g_uobj_alive, host_lua_lock_free_for_other_threads());
        std::printf("T: uf.%s.%s kept=%d wrong=%d dtors=%d\n", gen, row.name, msg_kept, wrong_status, g_uobj_dtor);
        const bool bug = expect_bug_on_cpp && kLuaIsCpp && row.mode == UMode::LuaError;
        if (bug) check(msg_kept == 0 && wrong_status == 200, std::string(gen) + " LUA-ERROR, Lua as C++: BUG reproduced with the real code: status -1, message lost, 200 of 200");
        else check(msg_kept == 200 && wrong_status == 0, std::string(gen) + " " + row.name + ": status and message correct 200 of 200" + (kLuaIsCpp ? " (Lua as C++)" : " (Lua as C)"));
        check(g_uobj_alive == 0 && g_uobj_dtor == 200, std::string(gen) + " " + row.name + ": the call owner was destroyed every time (200)");
        check(host_lua_lock_free_for_other_threads() == 1, std::string(gen) + " " + row.name + ": Lua's lock balanced");
    }
    lua_close(L);
}

// ---- child-process scenarios (terminate / abort cannot be asserted in-process) ------------------------------------------
static FailAlloc* g_child_fa = nullptr;
static int hook_push_unprotected(lua_State* L)
{
    std::string big(4096, 'q');
    lua_pushlstring(L, big.data(), big.size());   // allocation fails -> LUA_ERRMEM raised here
    return 0;
}
static void game_frame(void (*cb)(void*), void* ctx) noexcept { cb(ctx); }   // stands in for game code: an exception may not cross it
static void hook_body_unprotected(void* ctx)
{
    auto* L = static_cast<lua_State*>(ctx);
    g_child_fa->countdown = 1; g_child_fa->window = 0;
    hook_push_unprotected(L);
    g_child_fa->countdown = -1;
}
static int trampoline_push(lua_State* L) { return hook_push_unprotected(L); }
static void hook_body_protected(void* ctx)
{
    auto* L = static_cast<lua_State*>(ctx);
    g_child_fa->countdown = 1; g_child_fa->window = 0;
    lua_pushcfunction(L, trampoline_push);
    const int st = lua_pcall(L, 0, 0, 0);       // the H4 mitigation: pushes happen inside ONE protected call
    g_child_fa->countdown = -1; g_child_fa->window = 0;
    std::printf("CHILD protected hook: inner pcall status=%d\n", st);
    lua_settop(L, 0);
}
static int enter_game_unprotected(lua_State* L) { game_frame(hook_body_unprotected, L); return 0; }
static int enter_game_protected(lua_State* L) { game_frame(hook_body_protected, L); return 0; }
static int raw_std_exception(lua_State*) { throw std::runtime_error("escaped binding"); }
static int raw_wrapped(lua_State* L) { return RC::Fixed::TRY(L, [&]() -> int { throw std::runtime_error("converted by TRY"); }); }

static int child_main(const std::string& mode)
{
    FailAlloc fa; g_child_fa = &fa;
    lua_State* L = new_state(&fa);
    int status = -99;
    if (mode == "nested-unprotected" || mode == "nested-protected")
    {
        lua_pushcfunction(L, mode == "nested-unprotected" ? enter_game_unprotected : enter_game_protected);
        lua_setglobal(L, "enter_game");
        luaL_loadstring(L, "enter_game() return 'back in Lua'");
        status = lua_pcall(L, 0, 1, 0);
        std::printf("CHILD %s: survived, outer pcall status=%d\n", mode.c_str(), status);
        return 0;
    }
    if (mode == "raw-std-exception")
    {
        lua_pushcfunction(L, raw_std_exception); lua_setglobal(L, "raw");
        luaL_loadstring(L, "raw()");
        try { status = lua_pcall(L, 0, 0, 0); std::printf("CHILD raw-std-exception: pcall returned %d\n", status); }
        catch (const std::exception& e) { std::printf("CHILD raw-std-exception: exception escaped lua_pcall to the C++ caller: %s\n", e.what()); return 0; }
        return 0;
    }
    if (mode == "raw-wrapped")
    {
        lua_pushcfunction(L, raw_wrapped); lua_setglobal(L, "raw");
        luaL_loadstring(L, "raw()");
        status = lua_pcall(L, 0, 0, 0);
        std::printf("CHILD raw-wrapped: survived, pcall status=%d\n", status);
        return 0;
    }
    return 3;
}
struct ChildResult { bool exited; int code; int sig; std::string out; };
static ChildResult run_child(const char* self, const std::string& mode)
{
    std::fflush(stdout);
    const std::string cmd = std::string("exec '") + self + "' --child " + mode + " 2>&1";
    FILE* p = popen(cmd.c_str(), "r");
    ChildResult r{false, -1, 0, ""};
    if (!p) return r;
    char buf[512];
    while (std::fgets(buf, sizeof buf, p)) r.out += buf;
    int st = pclose(p);
    if (WIFEXITED(st)) { r.exited = true; r.code = WEXITSTATUS(st); }
    else if (WIFSIGNALED(st)) r.sig = WTERMSIG(st);
    // popen runs the child through /bin/sh; a signal can also surface as exit code 128+sig
    if (r.exited && r.code > 128) { r.exited = false; r.sig = r.code - 128; }
    return r;
}
static void scenario_children(const char* self)
{
    std::printf("\n[engine boundary and raw callbacks] child processes (terminate/abort cannot be asserted in-process)\n");
    auto show = [](const char* n, const ChildResult& r) {
        std::printf("       %-20s exited=%d code=%d signal=%d | %s", n, r.exited, r.code, r.sig, r.out.empty() ? "(no output)\n" : r.out.substr(0, r.out.find('\n') == std::string::npos ? r.out.size() : r.out.find('\n') + 1).c_str());
    };
    auto a = run_child(self, "nested-unprotected"); show("nested-unprotected", a);
    auto b = run_child(self, "nested-protected");   show("nested-protected", b);
    auto c = run_child(self, "raw-std-exception");  show("raw-std-exception", c);
    auto d = run_child(self, "raw-wrapped");        show("raw-wrapped", d);
    std::printf("T: child.nested-protected exited=%d code=%d\n", b.exited, b.code);
    std::printf("T: child.raw-wrapped exited=%d code=%d\n", d.exited, d.code);
    if (kLuaIsCpp)
    {
        check(!a.exited && (a.sig == SIGABRT), "H4, Lua as C++: a Lua error raised in a hook body that game code called crosses the game frame -> std::terminate (SIGABRT). This is why every engine-to-Lua entry needs ONE protected call before B7 flips");
        check(a.out.find("uncaught exception of type lua_longjmp*") != std::string::npos, "H4: the death message names the Lua error itself (uncaught exception of type lua_longjmp*)");
        check(c.exited ? true : (c.sig == SIGABRT), "H3, Lua as C++: a std::exception escaping a raw Lua C function without TRY ends in abort (Lua's catch(...) turns it into status -1 and the lock count goes wrong) or an escaped exception");
    }
    else
    {
        check(a.exited && a.code == 0 && a.out.find("status=4") != std::string::npos, "H4, Lua as C (today): the same error is a longjmp over the game frame and lua_pcall returns LUA_ERRMEM (survives, but the game frame was skipped)");
        check(c.exited && c.out.find("escaped lua_pcall") != std::string::npos, "H3, Lua as C (today): the std::exception flies out of lua_pcall to the C++ caller (Lua state is then suspect)");
    }
    check(b.exited && b.code == 0 && b.out.find("inner pcall status=4") != std::string::npos && b.out.find("outer pcall status=0") != std::string::npos,
          "H4 mitigation, BOTH builds: the hook body does its pushes inside one protected call -> inner pcall returns LUA_ERRMEM, no exception crosses the game frame, the outer pcall finishes normally");
    check(d.exited && d.code == 0 && d.out.find("pcall status=2") != std::string::npos, "H3 mitigation, BOTH builds: a raw Lua C function that wraps its body in the real TRY turns a std::exception into a normal Lua error (status 2)");
}

// ---- the startup probe ---------------------------------------------------------------------------------------------------
static void scenario_probe()
{
    std::printf("\n[LUA-CPP probe] the staged startup probe, compiled from the staged file\n");
    int out[9]{};
    RC::Probe::lua_cpp_probe(out);
    std::printf("       out[]: mode=%d pcall_status=%d err_dtor=%d resume_status=%d yield_dtor=%d ran=%d void_catch=%d swallowed=%d void_status=%d\n",
                out[0], out[1], out[2], out[3], out[4], out[5], out[6], out[7], out[8]);
    std::printf("T: probe mode=%d pcall=%d err_dtor=%d resume=%d yield_dtor=%d void_catch=%d swallowed=%d void_status=%d\n", out[0], out[1], out[2], out[3], out[4], out[6], out[7], out[8]);
    check(out[5] == 1 && out[1] == LUA_ERRRUN && out[3] == LUA_YIELD && out[8] == LUA_ERRRUN, "probe ran: pcall status LUA_ERRRUN, void-catch pcall LUA_ERRRUN, resume status LUA_YIELD");
    check(out[0] == (kLuaIsCpp ? 1 : 0), kLuaIsCpp ? "probe reports mode=cxx-throw (destructors ran in both frames)" : "probe reports mode=longjmp (destructors skipped in both frames)");
    check(kLuaIsCpp ? (out[6] == 1 && out[7] == 0) : (out[6] == 0 && out[7] == 0),
          kLuaIsCpp ? "Lua as C++: catch (void*) matches the Lua throw (a lua_longjmp* pointer) and the catch (...) behind it is NOT reached: the E1/E2 pass-through works with this libc++abi"
                    : "Lua as C: the handlers are never reached (longjmp), so the pass-through edits are dead code");
    check(host_lua_lock_free_for_other_threads() == 1 && host_lua_lock_depth() == 0, "the probe leaves Lua's global lock balanced");
}

int main(int argc, char** argv)
{
    if (argc >= 3 && std::string(argv[1]) == "--child") return child_main(argv[2]);
    std::printf("B7 host test -- Lua as %s (HOST_LUA_IS_CPP %s)\n", kLuaIsCpp ? "C++ with C names (staged B7)" : "C (today)", kLuaIsCpp ? "defined" : "not defined");
    scenario_probe();
    scenario_b2_wrapper<Gen<CurTag>>("cur", true);
    scenario_b2_wrapper<Gen<FixedTag>>("fixed", true);
    scenario_destructors();
    std::printf("\n[out-of-memory sweep] allocator fails (twice in a row) at point k while the real throw_error_impl builds the error text\n");
    scenario_oom<Gen<CurTag>>("cur", true);
    scenario_oom<Gen<FixedTag>>("fixed", false);
    scenario_ufunction<Gen<CurTag>>("cur", true);
    scenario_ufunction<Gen<FixedTag>>("fixed", false);
    scenario_children(argv[0]);
    std::printf("\nchecks run: %d, failed: %d\n", g_checked, g_failed);
    if (g_checked == 0) { std::printf("FAIL: zero checks\n"); return 1; }
    std::printf(g_failed == 0 ? "PASS b7 host test, Lua as %s (0 failures)\n" : "FAIL b7 host test, Lua as %s\n", kLuaIsCpp ? "C++" : "C");
    return g_failed == 0 ? 0 : 1;
}
