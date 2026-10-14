// B6 host test (Mac-only). Compiles the SHIPPED B6 text (sliced out of the staged
// LuaMod.cpp / LuaMadeSimple.cpp / LuaMadeSimple.hpp by extract_core.py) against the
// REAL Lua 5.4 C sources built as C (setjmp/longjmp: the port's configuration), and
// drives it. Every assertion counts toward "checked"; zero checked is a failure.
//
// Not covered here (needs the console build): the glue that sits outside the CORE slices
// (Output::send emission, the pump call sites, the FindFirstOf/widget edits) and the real
// game-thread identity. Those are source-checked by b6_source_gate.py.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#define RC_LMS_API

static std::atomic<int> g_checked{0};
static std::atomic<int> g_failed{0};
#define CHECK(cond, ...)                                              \
    do {                                                              \
        ++g_checked;                                                  \
        if (!(cond)) {                                                \
            ++g_failed;                                               \
            std::printf("  FAIL  %s:%d  %s  ", __FILE__, __LINE__, #cond); \
            std::printf(__VA_ARGS__);                                 \
            std::printf("\n");                                        \
        }                                                             \
    } while (0)
#define PASSLINE(name) std::printf("  PASS  %s\n", name)

namespace RC
{
namespace LuaMadeSimple
{
#include "api.inc"

    // Stub with the contract of the file-local helper in LuaMadeSimple.cpp.
    static auto copy_text_bounded(char* out, size_t out_size, const char* text) noexcept -> void
    {
        if (out == nullptr || out_size == 0) { return; }
        size_t i = 0;
        if (text != nullptr)
        {
            for (; i + 1 < out_size && text[i] != '\0'; ++i) { out[i] = text[i]; }
        }
        out[i] = '\0';
    }

#include "regionA.inc"

    // Stands in for the lstate.h based glue in LuaMadeSimple.cpp.
    static auto diag_main_state(lua_State* lua_state) noexcept -> lua_State*
    {
        if (lua_state == nullptr) { return nullptr; }
        lua_rawgeti(lua_state, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
        lua_State* main_state = lua_tothread(lua_state, -1);
        lua_pop(lua_state, 1);
        return main_state;
    }
}

#include "regionB.inc"

// ---- glue for the tests ---------------------------------------------------------------
static std::mutex g_lines_mutex;
static std::vector<std::string> g_lines;
static thread_local bool tl_is_game = false;
namespace
{
namespace b6diag
{
    auto emit_line(const char* line) -> void
    {
        std::lock_guard<std::mutex> guard{g_lines_mutex};
        g_lines.emplace_back(line);
    }
    auto on_game_thread() -> bool { return tl_is_game; }
}
}

static auto lines_snapshot() -> std::vector<std::string>
{
    std::lock_guard<std::mutex> guard{g_lines_mutex};
    return g_lines;
}
static auto lines_clear() -> void
{
    std::lock_guard<std::mutex> guard{g_lines_mutex};
    g_lines.clear();
}
static auto count_lines_with(const char* needle) -> int
{
    int n = 0;
    for (const auto& l : lines_snapshot()) { if (l.find(needle) != std::string::npos) { ++n; } }
    return n;
}
static auto first_line_with(const char* needle) -> std::string
{
    for (const auto& l : lines_snapshot()) { if (l.find(needle) != std::string::npos) { return l; } }
    return {};
}

using b6diag::i64;

// ---- Lua plumbing: a registered function shaped like process_lua_function ------------
struct Recorded
{
    std::string kind, fn, text;
    int extra{0}, depth{0};
    lua_State* main_state{nullptr};
};
static std::vector<Recorded> g_recorded;
static std::atomic<int> g_call_begin_calls{0}, g_call_end_calls{0};
static std::atomic<long long> g_last_call_token{0};

static void rec_on_leak(lua_State* m, const char* kind, const char* fn, const char* text, int extra, int depth) noexcept
{
    g_recorded.push_back(Recorded{kind, fn, text, extra, depth, m});
}
static long long rec_call_begin() noexcept { ++g_call_begin_calls; return tl_is_game ? 777 : 0; }
static void rec_call_end(lua_State*, long long token) noexcept { ++g_call_end_calls; g_last_call_token = token; }

using Body = std::function<int(lua_State*)>;

// Shape of throw_error_impl's tail: the diag check, then the longjmp.
[[noreturn]] static void raise_like_throw_error(lua_State* L, const char* text)
{
    RC::LuaMadeSimple::diag_check_raise(L, text);
    luaL_error(L, "%s", text);
    std::abort();
}

// Shape of process_lua_function + TRY (B5 form): C++ exceptions are caught, the text is
// copied, the handler ENDS, then the Lua error is raised.
static int registered_dispatch(lua_State* L)
{
    auto* body = static_cast<Body*>(lua_touserdata(L, lua_upvalueindex(1)));
    const auto frame = RC::LuaMadeSimple::diag_fn_enter();
    int result = 0;
    char message[256];
    message[0] = '\0';
    bool raised = false;
    try
    {
        result = (*body)(L);
    }
    catch (std::exception& e)
    {
        std::snprintf(message, sizeof(message), "%s", e.what());
        raised = true;
    }
    if (raised)
    {
        raise_like_throw_error(L, message);
    }
    RC::LuaMadeSimple::diag_fn_return(L, frame);
    return result;
}

static Body* g_bodies[16];
static int g_body_count = 0;
static void define_registered(lua_State* L, const char* name, Body body)
{
    g_bodies[g_body_count] = new Body(std::move(body));
    lua_pushlightuserdata(L, g_bodies[g_body_count]);
    ++g_body_count;
    lua_pushcclosure(L, registered_dispatch, 1);
    lua_setglobal(L, name);
}

// Shape of Lua::call_function's pcall part.
static int call_function_like(lua_State* L, const char* chunk)
{
    if (luaL_loadstring(L, chunk) != LUA_OK) { return -1; }
    RC::LuaMadeSimple::DiagCallFrame frame{L};
    const int status = lua_pcall(L, 0, 0, 0);
    frame.after_pcall(status);
    if (status != LUA_OK) { lua_pop(L, 1); }
    return status;
}

static lua_State* new_lua()
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    return L;
}

// ---- tests -----------------------------------------------------------------------------
static void test_rate_limiter()
{
    b6diag::RateLimiter lim;
    int shown = 0;
    b6diag::RateLimiter::Verdict last{};
    for (int i = 0; i < 20; ++i)
    {
        last = lim.admit(1000 + i, 5, 1000);
        shown += last.show ? 1 : 0;
        CHECK(last.prev_suppressed == 0, "no summary inside the first window (i=%d)", i);
    }
    CHECK(shown == 5, "5 of 20 shown, got %d", shown);
    auto next = lim.admit(2100, 5, 1000);
    CHECK(next.show, "first event of the next window is shown");
    CHECK(next.prev_total == 20 && next.prev_suppressed == 15, "summary carries 20/15, got %d/%d", next.prev_total, next.prev_suppressed);
    auto again = lim.admit(2101, 5, 1000);
    CHECK(again.prev_suppressed == 0, "the summary is handed out once");
    // exactly at the limit: nothing suppressed, no summary
    b6diag::RateLimiter exact;
    for (int i = 0; i < 5; ++i) { exact.admit(10, 5, 1000); }
    CHECK(exact.admit(5000, 5, 1000).prev_suppressed == 0, "5 of 5 shown needs no summary");
    // flush from the pump ends an expired window
    b6diag::RateLimiter lim2;
    for (int i = 0; i < 8; ++i) { lim2.admit(100, 3, 1000); }
    CHECK(lim2.flush(500, 1000).prev_suppressed == 0, "flush inside the window reports nothing");
    auto fl = lim2.flush(1200, 1000);
    CHECK(fl.prev_total == 8 && fl.prev_suppressed == 5, "flush reports 8/5, got %d/%d", fl.prev_total, fl.prev_suppressed);
    CHECK(lim2.flush(1300, 1000).prev_suppressed == 0, "flush hands it out once");
    // concurrent admits never lose a count
    b6diag::RateLimiter lim3;
    std::vector<std::thread> threads;
    std::atomic<int> shown3{0};
    for (int t = 0; t < 4; ++t)
    {
        threads.emplace_back([&] { for (int i = 0; i < 5000; ++i) { if (lim3.admit(7, 5, 1000).show) { ++shown3; } } });
    }
    for (auto& t : threads) { t.join(); }
    CHECK(shown3 == 5, "20000 concurrent events show exactly 5, got %d", shown3.load());
    auto final3 = lim3.flush(9999, 1000);
    CHECK(final3.prev_total == 20000 && final3.prev_suppressed == 19995, "total kept: %d/%d", final3.prev_total, final3.prev_suppressed);
    PASSLINE("rate limiter");
}

static void test_depth_and_holder()
{
    std::recursive_mutex m;
    CHECK(RC::LuaMadeSimple::diag_scope_depth() == 0, "depth starts at 0");
    {
        b6diag::ActionsLock outer{m, 111};
        CHECK(RC::LuaMadeSimple::diag_scope_depth() == 1, "depth 1");
        CHECK(b6diag::g_holder.since_ms.load() > 0 && b6diag::g_holder.line.load() == 111, "holder recorded (line 111)");
        {
            b6diag::ActionsLock inner{m, 222};
            CHECK(RC::LuaMadeSimple::diag_scope_depth() == 2, "depth 2 (recursive)");
            CHECK(b6diag::g_holder.line.load() == 111, "inner scope does not replace the holder");
        }
        CHECK(RC::LuaMadeSimple::diag_scope_depth() == 1, "depth back to 1");
        CHECK(b6diag::g_holder.since_ms.load() > 0, "holder kept while the outer scope lives");
    }
    CHECK(RC::LuaMadeSimple::diag_scope_depth() == 0, "depth back to 0");
    CHECK(b6diag::g_holder.since_ms.load() == 0, "holder cleared at depth 0");
    bool other_can_lock = false;
    std::thread([&] { other_can_lock = m.try_lock(); if (other_can_lock) { m.unlock(); } }).join();
    CHECK(other_can_lock, "mutex really released");
    PASSLINE("depth counter + holder record");
}

static void test_lock_stress()
{
    std::recursive_mutex m;
    long counter = 0;
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
    {
        threads.emplace_back([&, t] {
            for (int i = 0; i < 20000; ++i)
            {
                b6diag::ActionsLock a{m, 1000 + t};
                b6diag::ActionsLock b{m, 2000 + t};
                ++counter;
            }
            CHECK(RC::LuaMadeSimple::diag_scope_depth() == 0, "per-thread depth back to 0");
        });
    }
    for (auto& t : threads) { t.join(); }
    CHECK(counter == 80000, "mutual exclusion kept: %ld", counter);
    CHECK(b6diag::g_holder.since_ms.load() == 0, "holder cleared after the stress");
    PASSLINE("lock stress (4 threads x 20000 nested)");
}

static void test_leak_detection_raw_hooks()
{
    RC::LuaMadeSimple::DiagHooks hooks{};
    hooks.on_leak = &rec_on_leak;
    hooks.call_begin = &rec_call_begin;
    hooks.call_end = &rec_call_end;
    RC::LuaMadeSimple::diag_install_hooks(hooks);

    // Run each scenario on its own thread (its own depth) with its own mutex.
    auto in_thread = [](const std::function<void()>& fn) { std::thread(fn).join(); };

    // (a) clean registered function: guard released, no report
    in_thread([] {
        lua_State* L = new_lua();
        std::recursive_mutex m;
        g_recorded.clear();
        define_registered(L, "CleanFn", [&](lua_State*) { b6diag::ActionsLock g{m, 1}; return 0; });
        CHECK(call_function_like(L, "CleanFn()") == LUA_OK, "clean call ok");
        CHECK(g_recorded.empty(), "clean function reports nothing");
        CHECK(RC::LuaMadeSimple::diag_scope_depth() == 0, "depth 0 after clean call");
        lua_close(L);
    });

    // (b) THE B4 DEFECT: raise (luaL_error = longjmp) while the tracked guard is alive
    in_thread([] {
        lua_State* L = new_lua();
        static std::recursive_mutex m;
        g_recorded.clear();
        define_registered(L, "LeakyFn", [&](lua_State* S) -> int {
            b6diag::ActionsLock g{m, 4242};
            raise_like_throw_error(S, "[RegisterHook] refused (test)");
            return 0;
        });
        const int status = call_function_like(L, "LeakyFn()");
        CHECK(status != LUA_OK, "the Lua error reached pcall");
        CHECK(RC::LuaMadeSimple::diag_scope_depth() == 1, "depth stays 1: destructor skipped (defect reproduced)");
        bool other_can_lock = true;
        std::thread([&] { other_can_lock = m.try_lock(); if (other_can_lock) { m.unlock(); } }).join();
        CHECK(!other_can_lock, "mutex really leaked: another thread cannot take it");
        CHECK(g_recorded.size() == 2, "two reports (raise + pcall), got %zu", g_recorded.size());
        if (g_recorded.size() == 2)
        {
            CHECK(g_recorded[0].kind == "raise" && g_recorded[0].extra == 1, "raise report extra=1");
            CHECK(g_recorded[0].fn == "LeakyFn", "raise report names the function, got '%s'", g_recorded[0].fn.c_str());
            CHECK(g_recorded[0].text.find("[RegisterHook] refused") != std::string::npos, "raise report carries the error text");
            CHECK(g_recorded[0].main_state == L, "raise report carries the main state");
            CHECK(g_recorded[1].kind == "pcall" && g_recorded[1].extra == 1, "pcall report extra=1");
        }
        lua_close(L);
    });

    // (c) the B5 shape: C++ exception inside the lock, converted after the guard is gone
    in_thread([] {
        lua_State* L = new_lua();
        std::recursive_mutex m;
        g_recorded.clear();
        define_registered(L, "FixedFn", [&](lua_State*) -> int {
            b6diag::ActionsLock g{m, 77};
            throw std::runtime_error("refused (B5 form)");
        });
        const int status = call_function_like(L, "FixedFn()");
        CHECK(status != LUA_OK, "the Lua error reached pcall");
        CHECK(RC::LuaMadeSimple::diag_scope_depth() == 0, "depth back to 0: destructor ran");
        bool other_can_lock = false;
        std::thread([&] { other_can_lock = m.try_lock(); if (other_can_lock) { m.unlock(); } }).join();
        CHECK(other_can_lock, "mutex released");
        CHECK(g_recorded.empty(), "B5 shape reports nothing (%zu)", g_recorded.size());
        lua_close(L);
    });

    // (d) an inner longjmp swallowed by an inner pcall skips a guard; the OUTER registered
    //     function then returns normally -> kind=return
    in_thread([] {
        lua_State* L = new_lua();
        static std::recursive_mutex m;
        g_recorded.clear();
        define_registered(L, "InnerLeak", [&](lua_State* S) -> int {
            b6diag::ActionsLock g{m, 5};
            luaL_error(S, "inner raise (not through throw_error)");  // VM-style raise, no diag_check_raise
            return 0;
        });
        define_registered(L, "OuterFn", [&](lua_State* S) -> int {
            lua_getglobal(S, "InnerLeak");
            lua_pcall(S, 0, 0, 0);     // swallows the error; the guard was skipped
            lua_settop(S, 0);
            return 0;
        });
        const int status = call_function_like(L, "OuterFn()");
        CHECK(status == LUA_OK, "outer call succeeded");
        bool saw_return = false, saw_raise = false;
        for (const auto& r : g_recorded)
        {
            saw_return |= (r.kind == "return" && r.extra == 1 && r.fn == "OuterFn");
            saw_raise |= (r.kind == "raise");
        }
        CHECK(saw_return, "return report for OuterFn (%zu reports)", g_recorded.size());
        CHECK(!saw_raise, "InnerLeak raised via luaL_error directly, so no raise report");
        lua_close(L);
    });

    // (e) a dispatch-style outer scope (depth 1 before any Lua) is the BASELINE, not a leak
    in_thread([] {
        lua_State* L = new_lua();
        std::recursive_mutex m;
        g_recorded.clear();
        define_registered(L, "PlainRaise", [&](lua_State* S) -> int {
            raise_like_throw_error(S, "plain error");
            return 0;
        });
        {
            b6diag::ActionsLock dispatch_like{m, 9};
            const int status = call_function_like(L, "PlainRaise()");
            CHECK(status != LUA_OK, "error reached pcall");
            CHECK(RC::LuaMadeSimple::diag_scope_depth() == 1, "dispatch scope still counted once");
        }
        CHECK(g_recorded.empty(), "no report: the outer scope is the baseline (%zu)", g_recorded.size());
        CHECK(RC::LuaMadeSimple::diag_scope_depth() == 0, "depth 0 at the end");
        lua_close(L);
    });

    // (g) a raise from a plain C function (NOT a registered one) under a dispatch-style scope:
    //     the call_function frame supplies the baseline, so it is not a leak either
    in_thread([] {
        lua_State* L = new_lua();
        std::recursive_mutex m;
        g_recorded.clear();
        lua_pushcfunction(L, [](lua_State* S) -> int { raise_like_throw_error(S, "from a plain cfunction"); return 0; });
        lua_setglobal(L, "PlainC");
        {
            b6diag::ActionsLock dispatch_like{m, 10};
            CHECK(call_function_like(L, "PlainC()") != LUA_OK, "error reached pcall");
        }
        CHECK(g_recorded.empty(), "no report for a raise outside any registered function (%zu)", g_recorded.size());
        lua_close(L);
    });

    // (f) timing hook: only the OUTERMOST call_function is timed, only on the "game" thread
    in_thread([] {
        lua_State* L = new_lua();
        g_call_begin_calls = 0; g_call_end_calls = 0;
        define_registered(L, "Nest", [&](lua_State* S) -> int {
            luaL_loadstring(S, "return 1");
            RC::LuaMadeSimple::DiagCallFrame inner{S};
            lua_pcall(S, 0, 0, 0);
            return 0;
        });
        tl_is_game = false;
        call_function_like(L, "Nest()");
        CHECK(g_call_begin_calls == 1 && g_call_end_calls == 0, "non-game thread: begin asked once, no end (%d/%d)", g_call_begin_calls.load(), g_call_end_calls.load());
        tl_is_game = true;
        g_call_begin_calls = 0; g_call_end_calls = 0;
        call_function_like(L, "Nest()");
        CHECK(g_call_begin_calls == 1 && g_call_end_calls == 1, "game thread: outermost timed once even with a nested frame (%d/%d)", g_call_begin_calls.load(), g_call_end_calls.load());
        CHECK(g_last_call_token == 777, "token handed to call_end");
        tl_is_game = false;
        lua_close(L);
    });
    PASSLINE("leak detection (real Lua longjmp): clean / B4 defect / B5 shape / swallowed leak / baseline / timing");
}

static void test_leak_log_line_and_rate()
{
    // Integration through the real on_leak: mod name, fn, one line per mod
    lines_clear();
    std::thread([] {
        lua_State* L = new_lua();
        b6diag::register_mod(L, "TestMod");
        b6diag::install_hooks_once();   // installs b6diag::on_leak / call hooks
        static std::recursive_mutex m;
        define_registered(L, "LeakyFn", [&](lua_State* S) -> int {
            b6diag::ActionsLock g{m, 4242};
            raise_like_throw_error(S, "[RegisterHook] line one\nline two\ttab");
            return 0;
        });
        for (int i = 0; i < 3; ++i)
        {
            // the first call leaks the lock for good; repeat the raise path via a lock-free
            // function so the later calls do not block on the leaked mutex
            if (i == 0) { call_function_like(L, "LeakyFn()"); }
        }
        define_registered(L, "QuietLeak", [&](lua_State* S) -> int {
            // fake leak: bump the depth without a mutex, then raise
            RC::LuaMadeSimple::diag_scope_enter();
            raise_like_throw_error(S, "again");
            return 0;
        });
        for (int i = 0; i < 50; ++i) { call_function_like(L, "QuietLeak()"); }
        // the state stays open on purpose: a registered mod's state is never freed, and a
        // reused address would make the next register_mod find this slot
    }).join();
    const int leak_lines = count_lines_with("[LOCKLEAK] ");
    CHECK(leak_lines == 1, "50+ leaks for one mod log ONE line, got %d", leak_lines);
    const std::string line = first_line_with("[LOCKLEAK] ");
    CHECK(line.find("mod=TestMod") != std::string::npos, "mod name in the line: %s", line.c_str());
    CHECK(line.find("fn=LeakyFn") != std::string::npos, "function name in the line: %s", line.c_str());
    CHECK(line.find("kind=raise") != std::string::npos, "kind=raise in the line");
    CHECK(line.find('\n') == std::string::npos && line.find('\t') == std::string::npos, "one-line text");
    CHECK(line.find("line one line two") != std::string::npos, "newline became a space: %s", line.c_str());
    CHECK(b6diag::find_mod(nullptr).name[0] == '?', "unknown state maps to the '?' slot");

    // the pure decision: later leaks are rate limited per mod, with a repeat count
    b6diag::ModSlot slot;
    auto v1 = b6diag::leak_decide(slot, 1000, 30000);
    CHECK(v1.log && v1.repeats == 1, "first leak logs");
    for (int i = 0; i < 10; ++i) { CHECK(!b6diag::leak_decide(slot, 2000 + i, 30000).log, "no second line inside 30 s"); }
    auto v2 = b6diag::leak_decide(slot, 31500, 30000);
    CHECK(v2.log && v2.repeats == 11, "line after 30 s carries repeats=11, got %lld", v2.repeats);
    CHECK(slot.leak_total.load() == 12, "total kept: %lld", slot.leak_total.load());

    // the pump check: depth stuck across two pumps outside a dispatch
    int last = 0;
    CHECK(!b6diag::pump_depth_stuck(0, false, last), "depth 0 is fine");
    CHECK(!b6diag::pump_depth_stuck(2, false, last), "first sighting is not enough");
    CHECK(b6diag::pump_depth_stuck(2, false, last), "same depth twice = stuck");
    CHECK(!b6diag::pump_depth_stuck(3, false, last), "changed depth restarts the count");
    CHECK(!b6diag::pump_depth_stuck(3, true, last), "dispatching is never stuck");
    CHECK(!b6diag::pump_depth_stuck(3, false, last), "and it reset the memory");
    PASSLINE("[LOCKLEAK] line, per-mod rate limit, pump depth check");
}

static void test_lockwait_real_contention()
{
    lines_clear();
    std::recursive_mutex m;
    const i64 slow_before = b6diag::g_slow_acquires.load();
    // uncontended: no slow path, no clock
    { b6diag::ActionsLock a{m, 31}; }
    CHECK(b6diag::g_slow_acquires.load() == slow_before, "uncontended acquisition takes no slow path");

    std::atomic<bool> holder_ready{false};
    std::thread holder([&] {
        std::lock_guard<std::recursive_mutex> g{m};
        holder_ready = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(420));
    });
    while (!holder_ready) { std::this_thread::yield(); }
    std::atomic<bool> got{false};
    std::thread waiter([&] {
        tl_is_game = false;
        b6diag::ActionsLock w{m, 6006};
        got = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(330));
    // while it is still waiting, the pump-side scan must see it (the lock never came back yet)
    char lines[4][260];
    const int stuck = b6diag::scan_stuck_waiters(b6diag::now_ms(), lines, 4);
    CHECK(stuck == 1, "pump scan sees the waiter, got %d", stuck);
    if (stuck == 1)
    {
        CHECK(std::strstr(lines[0], "[LOCKWAIT] still waiting site_line=6006 thread=other") != nullptr, "scan line: %s", lines[0]);
    }
    CHECK(b6diag::scan_stuck_waiters(b6diag::now_ms(), lines, 4) == 0, "same waiter is not repeated inside 5 s");
    CHECK(!got.load(), "waiter still blocked");
    holder.join();
    waiter.join();
    CHECK(got.load(), "waiter finished");
    CHECK(b6diag::g_slow_acquires.load() == slow_before + 1, "one slow acquisition counted");
    CHECK(b6diag::g_slow_max_ms.load() >= 250, "max wait recorded: %lld ms", b6diag::g_slow_max_ms.load());
    CHECK(count_lines_with("[LOCKWAIT] site_line=6006 thread=other waited_ms=") == 1, "one [LOCKWAIT] line after the acquisition");
    CHECK(b6diag::scan_stuck_waiters(b6diag::now_ms() + 20000, lines, 4) == 0, "slot freed after the wait ended");
    // short waits are counted but not logged
    lines_clear();
    std::thread holder2([&] { std::lock_guard<std::recursive_mutex> g{m}; std::this_thread::sleep_for(std::chrono::milliseconds(60)); });
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    { b6diag::ActionsLock w{m, 7}; }
    holder2.join();
    CHECK(count_lines_with("[LOCKWAIT]") == 0, "a 45 ms wait is not logged");
    PASSLINE("[LOCKWAIT]: real contention, pump-side stuck scan, no log under 250 ms");
}

static void test_lockwait_limiter_and_scan_rate()
{
    lines_clear();
    { std::lock_guard<std::mutex> g{b6diag::g_lockwait_limiter.mutex}; b6diag::g_lockwait_limiter.started = false; b6diag::g_lockwait_limiter.total = 0; b6diag::g_lockwait_limiter.shown = 0; }
    for (int i = 0; i < 30; ++i) { b6diag::log_lockwait(300 + i, 10 + i, 0); }
    CHECK(count_lines_with("[LOCKWAIT] site_line=") == 5, "30 slow acquisitions log 5 lines, got %d", count_lines_with("[LOCKWAIT] site_line="));
    // scan: stuck waiter, holder known
    for (auto& w : b6diag::g_wait) { w.since_ms = 0; }
    const i64 now = b6diag::now_ms();
    const int slot = b6diag::wait_begin(now - 900, 1234, 1);
    CHECK(slot >= 0, "wait slot claimed");
    b6diag::g_holder.line = 555; b6diag::g_holder.game = 1; b6diag::g_holder.since_ms = now - 5000;
    char lines[4][260];
    CHECK(b6diag::scan_stuck_waiters(now, lines, 4) == 1, "stuck waiter produces a line");
    CHECK(std::strstr(lines[0], "waited_ms=900") != nullptr && std::strstr(lines[0], "holder_line=555 holder_thread=game holder_held_ms=5000") != nullptr, "line: %s", lines[0]);
    CHECK(b6diag::scan_stuck_waiters(now + 4000, lines, 4) == 0, "rate limited 5 s per slot");
    CHECK(b6diag::scan_stuck_waiters(now + 5100, lines, 4) == 1, "again after 5 s");
    b6diag::g_holder.since_ms = 0;
    CHECK(b6diag::scan_stuck_waiters(now + 11000, lines, 4) == 1 && std::strstr(lines[0], "holder=none-recorded") != nullptr, "no holder recorded: %s", lines[0]);
    for (auto& w : b6diag::g_wait) { w.since_ms = 0; }
    CHECK(b6diag::scan_stuck_waiters(now + 20000, lines, 4) == 0, "freed slots report nothing");
    // all slots busy: wait_begin degrades to -1, never blocks
    for (int i = 0; i < b6diag::kWaitSlots; ++i) { CHECK(b6diag::wait_begin(now, i, 0) == i, "slot %d claimed", i); }
    CHECK(b6diag::wait_begin(now, 99, 0) == -1, "ninth waiter is untracked, not an error");
    for (auto& w : b6diag::g_wait) { w.since_ms = 0; }
    PASSLINE("[LOCKWAIT]: log rate limit, scan rate limit, holder info, slot exhaustion");
}

static void test_perf_format_and_deltas()
{
    // reset counters this test cares about
    b6diag::g_find_first_calls = 0; b6diag::g_find_first_us = 0; b6diag::g_find_all_calls = 0; b6diag::g_find_all_us = 0; b6diag::g_widgets = 0;
    b6diag::g_slow_acquires = 0; b6diag::g_slow_max_ms = 0; b6diag::g_lockwait_events = 0;

    // FindTimer counts on entry, adds time on exit
    { b6diag::FindTimer t{b6diag::g_find_first_calls, b6diag::g_find_first_us}; std::this_thread::sleep_for(std::chrono::milliseconds(3)); }
    { b6diag::FindTimer t{b6diag::g_find_all_calls, b6diag::g_find_all_us}; }
    { b6diag::FindTimer t{b6diag::g_find_all_calls, b6diag::g_find_all_us}; }
    CHECK(b6diag::g_find_first_calls == 1 && b6diag::g_find_first_us >= 2500, "FindFirstOf timer: %lld calls, %lld us", b6diag::g_find_first_calls.load(), b6diag::g_find_first_us.load());
    CHECK(b6diag::g_find_all_calls == 2, "FindAllOf timer counts 2");

    // per-mod rows: game-thread time attributed by the main state of ANY thread of that mod
    lua_State* LA = new_lua();
    lua_State* LB = new_lua();
    b6diag::register_mod(LA, "ModA");
    b6diag::register_mod(LB, "ModB");
    b6diag::register_mod(LA, "ModA-again");   // idempotent
    lua_State* child = lua_newthread(LA);
    b6diag::hook_call_end(RC::LuaMadeSimple::diag_main_state(child), 1);   // a coroutine of ModA
    b6diag::hook_call_end(RC::LuaMadeSimple::diag_main_state(child), 1);
    b6diag::hook_call_end(LB, 1);
    b6diag::g_widgets += 7;
    i64 last_ms = 0;
    b6diag::PerfInput in{};
    b6diag::QueueDepths q{}; q.ok = true; q.game = 4; q.delayed = 2;
    b6diag::collect_perf(in, 10000, last_ms, q, 0);
    char text[900];
    b6diag::format_perf(text, sizeof(text), in);
    std::printf("      sample: %s\n", text);
    CHECK(std::strncmp(text, "[PERF] dt=0ms lua_ms={", 22) == 0, "starts with the [PERF] marker");
    CHECK(std::strstr(text, "ModA:") != nullptr && std::strstr(text, "ModB:") != nullptr, "both mods listed");
    CHECK(std::strstr(text, "ModA-again") == nullptr, "register_mod is idempotent");
    CHECK(std::strstr(text, "/2") != nullptr, "ModA has 2 calls (via a coroutine state)");
    CHECK(std::strstr(text, "find_first=1/") != nullptr && std::strstr(text, "find_all=2/") != nullptr, "find counts");
    CHECK(std::strstr(text, "widgets=7") != nullptr, "widgets delta");
    CHECK(std::strstr(text, "q=g4,p0,d2,pd0,e0,pe0") != nullptr, "queue depths");
    // second line: everything is a delta since the last line
    b6diag::PerfInput in2{};
    b6diag::collect_perf(in2, 11000, last_ms, q, 3);
    b6diag::format_perf(text, sizeof(text), in2);
    std::printf("      sample: %s\n", text);
    {
        const std::string expected = "[PERF] dt=1000ms lua_ms={-} find_first=0/0.0ms find_all=0/0.0ms widgets=0";
        CHECK(std::string(text).compare(0, expected.size(), expected) == 0, "second line is all zero deltas: %s", text);
    }
    CHECK(std::strstr(text, "pump_depth=3") != nullptr, "pump depth printed");
    b6diag::hook_call_end(LB, 1);
    q.ok = false;
    b6diag::PerfInput in3{};
    b6diag::collect_perf(in3, 12000, last_ms, q, 0);
    b6diag::format_perf(text, sizeof(text), in3);
    CHECK(std::strstr(text, "ModB:") != nullptr && std::strstr(text, "ModA:") == nullptr, "only mods with calls are listed");
    CHECK(std::strstr(text, " q=busy ") != nullptr, "busy lock shows q=busy");
    // bounded output, never overruns
    for (size_t n : {size_t{1}, size_t{2}, size_t{17}, size_t{40}, size_t{63}})
    {
        char small[128];
        std::memset(small, 'Z', sizeof(small));
        const size_t len = b6diag::format_perf(small, n, in);
        CHECK(len < n && small[len] == '\0' && small[n] == 'Z', "n=%zu stays inside (len=%zu)", n, len);
    }
    CHECK(b6diag::format_perf(nullptr, 10, in) == 0 && b6diag::format_perf(text, 0, in) == 0, "null/zero buffers are refused");
    lua_close(LA);
    lua_close(LB);
    PASSLINE("[PERF] format, per-mod rows, deltas, bounds");
}

static void test_widget_gate()
{
    lines_clear();
    b6diag::g_widgets = 0;
    int shown = 0;
    for (int i = 0; i < 40; ++i)
    {
        auto v = b6diag::widget_gate(50000 + i);
        b6diag::emit_widget_summary(v);
        shown += v.show ? 1 : 0;
    }
    CHECK(shown == 5, "40 constructions in one second show 5 lines, got %d", shown);
    CHECK(b6diag::g_widgets == 40, "all 40 counted");
    CHECK(count_lines_with("construct summary") == 0, "no summary until the window ends");
    auto v = b6diag::widget_gate(51200);
    b6diag::emit_widget_summary(v);
    CHECK(v.show, "next window shows again");
    CHECK(count_lines_with("[l22] widget/object construct summary: 40 constructed in the last window, 35 log lines suppressed") == 1, "one summary line: %s",
          first_line_with("construct summary").c_str());
    // a burst with no later construction is reported by the pump flush
    for (int i = 0; i < 12; ++i) { b6diag::widget_gate(60000); }
    lines_clear();
    b6diag::emit_widget_summary(b6diag::g_widget_limiter.flush(61500, 1000));
    CHECK(count_lines_with("12 constructed in the last window, 7 log lines suppressed") == 1, "pump flush summary: %s", first_line_with("summary").c_str());
    PASSLINE("widget log rate limit + summary");
}

static void test_copy_one_line()
{
    char out[16];
    b6diag::copy_one_line(out, sizeof(out), "a\nb\tc\x01" "d");
    CHECK(std::strcmp(out, "a b c d") == 0, "control chars -> spaces: '%s'", out);
    b6diag::copy_one_line(out, sizeof(out), "0123456789ABCDEFGHIJ");
    CHECK(std::strlen(out) == 15, "truncated to n-1");
    b6diag::copy_one_line(out, sizeof(out), nullptr);
    CHECK(out[0] == '\0', "null text -> empty");
    b6diag::copy_one_line(nullptr, 5, "x");
    b6diag::copy_one_line(out, 0, "x");
    CHECK(true, "null / zero-size buffers are ignored");
    PASSLINE("copy_one_line");
}

static void test_uncontended_cost()
{
    // Cost of one OUTERMOST uncontended acquisition: std::lock_guard vs b6diag::ActionsLock
    // (try_lock + depth counter + holder_set = one steady_clock read + the game-thread check;
    // here on_game_thread() is the test's stub, so the real IsInGameThreadRaw() cost is NOT in
    // this number). Mac-only, informational: the only asserted bound is a very loose sanity limit.
    std::recursive_mutex m;
    const int kN = 400000;
    long sink = 0;
    auto time_ns = [&](auto&& body) {
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < kN; ++i) { body(); }
        const auto t1 = std::chrono::steady_clock::now();
        return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()) / kN;
    };
    const double std_ns = time_ns([&] { std::lock_guard<std::recursive_mutex> g{m}; ++sink; });
    const double outer_ns = time_ns([&] { b6diag::ActionsLock g{m, 7}; ++sink; });
    const double clock_ns = time_ns([&] { sink += static_cast<long>(b6diag::now_ms() & 1); });
    CHECK(sink > 0 && RC::LuaMadeSimple::diag_scope_depth() == 0, "bench loops ran, depth back to 0");
    CHECK(outer_ns < 50000.0, "outermost ActionsLock far below 50 us (%.1f ns)", outer_ns);
    CHECK(b6diag::g_holder.since_ms.load() == 0, "holder cleared after the bench");
    std::printf("  BENCH Mac-only, %d iterations: std::lock_guard %.1f ns, b6diag::ActionsLock (outermost, stub game-thread check) %.1f ns, "
                "one now_ms() read %.1f ns\n", kN, std_ns, outer_ns, clock_ns);
    PASSLINE("uncontended cost (informational)");
}

} // namespace RC

using namespace RC;

int main()
{
    std::printf("b6 host test\n");
    test_copy_one_line();
    test_rate_limiter();
    test_depth_and_holder();
    test_lock_stress();
    test_leak_detection_raw_hooks();
    test_leak_log_line_and_rate();
    test_lockwait_real_contention();
    test_lockwait_limiter_and_scan_rate();
    test_perf_format_and_deltas();
    test_widget_gate();
    test_uncontended_cost();
    std::printf("checked=%d failed=%d\n", g_checked.load(), g_failed.load());
    if (g_checked == 0)
    {
        std::printf("FAIL b6 host test (nothing checked)\n");
        return 1;
    }
    if (g_failed != 0)
    {
        std::printf("FAIL b6 host test (%d failures)\n", g_failed.load());
        return 1;
    }
    std::printf("PASS b6 host test (0 failures, %d checks)\n", g_checked.load());
    return 0;
}
