// B5 host test (Mac-only): lock-then-raise leak, OLD vs NEW, against
//   * the REAL Lua 5.4.7 C sources compiled as C (setjmp/longjmp: the port's configuration), and
//   * the REAL B2 error-wrapper code (throw_error_impl / handle_error / TRY),
//     extracted byte-for-byte into real_pieces.inc by extract_real.py.
//
// OLD = LuaMod.cpp today: std::lock_guard<std::recursive_mutex> alive, then lua.throw_error
//       (= throw_error_impl -> luaL_error = longjmp, destructor skipped).
// NEW = proposed B5: std::lock_guard alive, then throw std::runtime_error; the real TRY
//       catches it (guard already destroyed by C++ unwinding), copies the text, and
//       raises the Lua error AFTER the handler exits.
// CONTROL = raise after the guard's block scope ended (the other valid shape).
//
// Thread A = "game thread" (owns the Lua state, runs every Lua call).
// Thread B = "keybind thread" (probe: try_lock polled with a deadline; never hangs).
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lstate.h"
}

#define RC_LMS_API

namespace RC::LuaMadeSimple
{
    // ---- stand-ins for things outside the extracted pieces -----------------------
    static std::unordered_map<lua_State*, std::string> lua_state_errors;   // LuaMadeSimple.cpp:468 (same type)
    static int g_notify_calls = 0;                                          // counts notify_error_callbacks()
    static auto notify_error_callbacks(lua_State*, const std::string&, const std::string&) -> void { ++g_notify_calls; }
    // declarations as in LuaMadeSimple.hpp:82-89 (no definitions needed beyond the pieces)
    auto capture_cxx_exception_text(char* buffer, size_t buffer_size, const char* what) noexcept -> void;
    auto throw_error_from_buffer(lua_State* lua_state, const char* message) -> void;
    auto handle_error(lua_State* lua_state, const std::string& error_message) -> const std::string;
    auto throw_error(lua_State* lua_state, const std::string& error_message) -> void;

#include "real_pieces.inc"

    // ---- the ONE non-verbatim piece: a minimal process_lua_function ---------------
    // Real one (LuaMadeSimple.cpp:1699-1730) looks up lua_instances/lua_functions and
    // ends in exactly this TRY(lua_state, [&]{ ... }) call; only the lookup differs.
    static std::vector<std::function<int(lua_State*)>> g_funcs;
    static auto process_lua_function(lua_State* lua_state) -> int
    {
        const size_t func_id = lua_tointeger(lua_state, lua_upvalueindex(1));
        return TRY(lua_state, [&] { return g_funcs[func_id](lua_state); });
    }
} // namespace RC::LuaMadeSimple

using namespace RC::LuaMadeSimple;

// ---- check bookkeeping ----------------------------------------------------------
static int g_checked = 0, g_failed = 0;
static void check(bool ok, const std::string& what)
{
    ++g_checked;
    if (!ok) ++g_failed;
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

// ---- thread A: the "game thread" ---------------------------------------------------
struct GameThread
{
    // NOTE: th is declared LAST so m/cv/q/stop are constructed before the thread starts
    // (declaring it first raced the thread against the mutex's construction).
    std::mutex m;
    std::condition_variable cv;
    std::deque<std::packaged_task<void()>> q;
    bool stop = false;
    std::thread th;
    GameThread() : th([this] {
        for (;;)
        {
            std::packaged_task<void()> t;
            {
                std::unique_lock<std::mutex> l(m);
                cv.wait(l, [&] { return stop || !q.empty(); });
                if (q.empty()) return;
                t = std::move(q.front());
                q.pop_front();
            }
            t();
        }
    }) {}
    template <typename F> void run(F f)  // run f on the game thread and wait for it
    {
        std::packaged_task<void()> t(std::move(f));
        auto fut = t.get_future();
        { std::lock_guard<std::mutex> l(m); q.push_back(std::move(t)); }
        cv.notify_one();
        fut.get();
    }
    ~GameThread()
    {
        { std::lock_guard<std::mutex> l(m); stop = true; }
        cv.notify_one();
        th.join();
    }
};

// ---- thread B: the "keybind thread" probe (never hangs) ---------------------------
static bool probe_acquire(std::recursive_mutex& mtx, int timeout_ms)
{
    bool acquired = false;
    std::thread b([&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (mtx.try_lock()) { mtx.unlock(); acquired = true; return; }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    b.join();
    return acquired;
}

// ---- exception type that counts construction/destruction (as in the B2 test) -------
static int g_ctor = 0, g_dtor = 0;
struct CountedError : std::runtime_error
{
    explicit CountedError(const char* m) : std::runtime_error(m) { ++g_ctor; }
    CountedError(const CountedError& o) : std::runtime_error(o) { ++g_ctor; }
    ~CountedError() override { ++g_dtor; }
};

enum class Variant { Old, New, Control };
static const char* vname(Variant v) { return v == Variant::Old ? "OLD" : v == Variant::New ? "NEW" : "CONTROL"; }

// ---- one fixture = one Lua state + one mutex + one game thread --------------------
struct Fixture
{
    Variant v;
    std::recursive_mutex mtx;
    GameThread game;
    lua_State* L = nullptr;

    // inner helper: takes the SAME mutex a second time (recursive) then raises
    void nested_lock_raise(lua_State* S, const char* m)
    {
        std::lock_guard<std::recursive_mutex> g2{mtx};
        raise_under_lock(S, m);
    }
    void raise_under_lock(lua_State* S, const char* m)
    {
        if (v == Variant::Old)
            throw_error(S, std::string{m});               // longjmp: skips every destructor above
        else
            throw CountedError(m);                        // NEW: C++ unwinding releases the guards
    }

    explicit Fixture(Variant variant) : v(variant)
    {
        game.run([this] {
            L = luaL_newstate();
            luaL_openlibs(L);
            // identical global names in every variant, so tracebacks are comparable
            reg("lock_raise", [this](lua_State* S) -> int {
                const char* m = lua_tostring(S, 1);
                if (v == Variant::Control)
                {
                    { std::lock_guard<std::recursive_mutex> g{mtx}; }   // lock released BEFORE the raise
                    throw_error(S, std::string{m});
                }
                std::lock_guard<std::recursive_mutex> g{mtx};
                raise_under_lock(S, m);
                return 0;
            });
            reg("lock_raise2", [this](lua_State* S) -> int {         // lock held twice (recursive)
                const char* m = lua_tostring(S, 1);
                std::lock_guard<std::recursive_mutex> g1{mtx};
                nested_lock_raise(S, m);
                return 0;
            });
            reg("lock_ok", [this](lua_State* S) -> int {              // success path under the lock
                std::lock_guard<std::recursive_mutex> g{mtx};
                lua_pushinteger(S, lua_tointeger(S, 1) + 1);
                return 1;
            });
        });
    }
    ~Fixture()
    {
        game.run([this] { lua_close(L); });
    }
    void reg(const char* name, std::function<int(lua_State*)> f)
    {
        g_funcs.push_back(std::move(f));
        lua_pushinteger(L, (lua_Integer)(g_funcs.size() - 1));
        lua_pushcclosure(L, process_lua_function, 1);
        lua_setglobal(L, name);
    }

    struct Result { bool call_ok; std::string a, b, c; int top_after; };
    // run a chunk on the game thread; the chunk returns up to three values (tostring'd)
    Result eval(const std::string& chunk)
    {
        Result r{false, "", "", "", -1};
        game.run([&] {
            int base = lua_gettop(L);
            if (luaL_loadstring(L, chunk.c_str()) != LUA_OK || lua_pcall(L, 0, 3, 0) != LUA_OK)
            {
                r.call_ok = false;
                r.a = lua_tostring(L, -1) ? lua_tostring(L, -1) : "?";
                lua_settop(L, base);
                r.top_after = lua_gettop(L);
                return;
            }
            r.call_ok = true;
            auto s = [&](int i) { return std::string{luaL_tolstring(L, i, nullptr)}; };
            r.a = s(base + 1); lua_pop(L, 1);
            r.b = s(base + 2); lua_pop(L, 1);
            r.c = s(base + 3); lua_pop(L, 1);
            lua_settop(L, base);
            r.top_after = lua_gettop(L);
        });
        return r;
    }

    // Owner (game thread) releases the leaked guards one by one, probing from thread B
    // before each release. Returns the number of leaked locks found (-1 if B got in too early).
    int drain_leaked(int expected)
    {
        int released = 0;
        bool early = false;
        game.run([&] {
            for (int i = 0; i < expected; ++i)
            {
                if (probe_acquire(mtx, 60)) { early = true; return; }   // B got in => fewer leaks than expected
                mtx.unlock();                                            // owner releases one leaked guard
                ++released;
            }
        });
        return early ? -1 : released;
    }
};

// helper: reset the per-variant counters before a scenario
struct Snap { size_t raised, cxx; int notify; int ctor, dtor; };
static Snap snap() { return {g_raised_errors.load(), g_cxx_exceptions.load(), g_notify_calls, g_ctor, g_dtor}; }

int main()
{
    std::setvbuf(stdout, nullptr, _IOLBF, 0);   // line-buffered so a crash shows the last line reached
    std::printf("B5 lock-then-raise host test (%s compiled as C; real B2 wrapper code)\n", LUA_RELEASE);

    // Text/type captured per scenario and variant, for the OLD == NEW comparison.
    struct Cap { std::string ok, type, text; size_t raised, cxx; int notify; int top; };
    auto run_err = [&](Fixture& fx, const std::string& chunk) {
        Snap s0 = snap();
        auto r = fx.eval(chunk);
        Snap s1 = snap();
        return Cap{r.a, r.b, r.c, s1.raised - s0.raised, s1.cxx - s0.cxx, s1.notify - s0.notify, r.top_after};
    };

    // =====================================================================================
    std::printf("\n[S1] one raise under one guard (RegisterHook refusal shape)\n");
    Cap s1[3];
    {
        const Variant vs[3] = {Variant::Old, Variant::New, Variant::Control};
        for (int i = 0; i < 3; ++i)
        {
            Fixture fx(vs[i]);
            g_ctor = g_dtor = 0;
            s1[i] = run_err(fx, "local ok, e = pcall(lock_raise, 'refused: HookProcessInternal=0') return ok, type(e), e");
            const bool free_to_B = probe_acquire(fx.mtx, 300);
            std::printf("       %-7s pcall ok=%s type=%s raised=%zu cxx_converted=%zu notify=%d lock_free_for_thread_B=%s\n",
                        vname(vs[i]), s1[i].ok.c_str(), s1[i].type.c_str(), s1[i].raised, s1[i].cxx, s1[i].notify, free_to_B ? "yes" : "NO");
            check(s1[i].ok == "false" && s1[i].type == "string", std::string(vname(vs[i])) + ": pcall returned false + string error");
            check(s1[i].text.find("refused: HookProcessInternal=0") != std::string::npos &&
                  s1[i].text.find("stack traceback") != std::string::npos,
                  std::string(vname(vs[i])) + ": error text carries the message and the traceback");
            if (vs[i] == Variant::Old)
            {
                check(!free_to_B, "OLD: thread B can NEVER acquire the mutex after the raise (leak reproduced)");
                // game thread is the owner: it can still relock (recursive) so the hang is invisible to it
                bool a_ok = false;
                fx.game.run([&] { a_ok = fx.mtx.try_lock(); if (a_ok) fx.mtx.unlock(); });
                check(a_ok, "OLD: owner thread A can still relock (why the game thread never noticed)");
                // real blocking keybind thread: stays blocked until A drains, then proceeds
                std::promise<void> got; auto fut = got.get_future();
                std::thread kb([&] { fx.mtx.lock(); fx.mtx.unlock(); got.set_value(); });
                const bool blocked = fut.wait_for(std::chrono::milliseconds(300)) == std::future_status::timeout;
                check(blocked, "OLD: a blocking lock() on thread B is still stuck after 300 ms");
                check(fx.drain_leaked(1) == 1, "OLD: exactly one leaked lock (owner releases it)");
                kb.join();
                check(probe_acquire(fx.mtx, 300), "OLD: after the owner releases the leaked lock thread B acquires");
            }
            else
            {
                check(free_to_B, std::string(vname(vs[i])) + ": thread B acquires the mutex right after the raise");
                std::promise<void> got; auto fut = got.get_future();
                std::thread kb([&] { fx.mtx.lock(); fx.mtx.unlock(); got.set_value(); });
                const bool prompt = fut.wait_for(std::chrono::milliseconds(300)) == std::future_status::ready;
                // failure path must not hang: if B is stuck, the owner releases leaked locks until B gets in
                for (int guard = 0; !prompt && guard < 100000 && fut.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready; ++guard)
                    fx.game.run([&] { fx.mtx.unlock(); });
                kb.join();
                check(prompt, std::string(vname(vs[i])) + ": a blocking lock() on thread B returns promptly");
            }
            if (vs[i] == Variant::New)
            {
                check(g_ctor == 1 && g_dtor == g_ctor, "NEW: the C++ exception object was created once and destroyed (no exception leak)");
                check(s1[i].cxx == 1 && s1[i].raised == 1, "NEW: error went through the TRY wrapper exactly once (cxx_converted=1, raised=1)");
            }
            if (vs[i] == Variant::Old)
                check(s1[i].cxx == 0 && s1[i].raised == 1, "OLD: no TRY conversion (cxx_converted=0, raised=1)");
            check(s1[i].top == 0, std::string(vname(vs[i])) + ": Lua stack balanced after pcall (top==0)");
        }
        check(s1[0].text == s1[1].text, "OLD vs NEW: Lua-visible error text is byte-identical");
        check(s1[0].type == s1[1].type && s1[0].ok == s1[1].ok, "OLD vs NEW: pcall status and error type are identical");
        check(s1[0].notify == s1[1].notify && s1[0].raised == s1[1].raised, "OLD vs NEW: error callbacks fired the same number of times");
        check(s1[1].text == s1[2].text, "NEW vs CONTROL (raise after unlock): error text identical");
        std::printf("       text (NEW) = [%s]\n", s1[1].text.c_str());
    }

    // =====================================================================================
    std::printf("\n[S2] raise while the SAME thread holds the mutex recursively twice\n");
    {
        Cap c[2];
        for (int i = 0; i < 2; ++i)
        {
            const Variant v = i == 0 ? Variant::Old : Variant::New;
            Fixture fx(v);
            c[i] = run_err(fx, "local ok, e = pcall(lock_raise2, 'twice-held') return ok, type(e), e");
            check(c[i].ok == "false" && c[i].type == "string" && c[i].text.find("twice-held") != std::string::npos,
                  std::string(vname(v)) + ": pcall returned false + string error with the message");
            if (v == Variant::Old)
            {
                check(!probe_acquire(fx.mtx, 300), "OLD: thread B cannot acquire (2 guards leaked)");
                // after ONE release B must still fail; after the SECOND release it must succeed
                bool after_one = true;
                fx.game.run([&] { fx.mtx.unlock(); });
                after_one = probe_acquire(fx.mtx, 150);
                check(!after_one, "OLD: still locked after the owner releases ONE guard (lock depth is 2)");
                check(fx.drain_leaked(1) == 1, "OLD: second release found the second leaked guard");
                check(probe_acquire(fx.mtx, 300), "OLD: after both releases thread B acquires (depth was exactly 2)");
            }
            else
            {
                check(probe_acquire(fx.mtx, 300), "NEW: thread B acquires (both recursive guards were destroyed by unwinding)");
                // the owner must NOT have been left holding any depth: relock/unlock from A then B again
                bool a_ok = false;
                fx.game.run([&] { a_ok = fx.mtx.try_lock(); if (a_ok) fx.mtx.unlock(); });
                check(a_ok && probe_acquire(fx.mtx, 300), "NEW: owner relock+unlock leaves it free for thread B (depth is 0, not negative/over-released)");
            }
        }
        check(c[0].text == c[1].text && c[0].type == c[1].type && c[0].ok == c[1].ok, "OLD vs NEW: identical Lua-visible error for the double-lock raise");
    }

    // =====================================================================================
    std::printf("\n[S3] nested pcall depth 12 (successful locked call at every level) + coroutine\n");
    {
        const std::string nested =
            "local function lvl(n)\n"
            "  if n == 0 then return lock_raise('deep-error') end\n"
            "  lock_ok(n)\n"
            "  local ok, e = pcall(lvl, n - 1)\n"
            "  if not ok then error(e, 0) end\n"
            "  return e\n"
            "end\n"
            "local ok, e = pcall(lvl, 12) return ok, type(e), e";
        const std::string coro =
            "local co = coroutine.create(function() return lock_raise('in-coroutine') end)\n"
            "local ok, e = coroutine.resume(co) return ok, type(e), e";
        Cap n[2], k[2];
        for (int i = 0; i < 2; ++i)
        {
            const Variant v = i == 0 ? Variant::Old : Variant::New;
            Fixture fx(v);
            n[i] = run_err(fx, nested);
            check(n[i].ok == "false" && n[i].type == "string" && n[i].text.find("deep-error") != std::string::npos,
                  std::string(vname(v)) + ": error from depth 12 reached the outer pcall with its text");
            check(n[i].top == 0, std::string(vname(v)) + ": Lua stack balanced after the nested run");
            const bool free_after_nested = probe_acquire(fx.mtx, 300);
            if (v == Variant::Old) { check(!free_after_nested, "OLD: mutex still leaked after the nested run"); fx.drain_leaked(1); }
            else check(free_after_nested, "NEW: mutex free after the nested run (12 locked successes + 1 locked raise)");
            k[i] = run_err(fx, coro);
            check(k[i].ok == "false" && k[i].type == "string" && k[i].text.find("in-coroutine") != std::string::npos,
                  std::string(vname(v)) + ": raise inside a coroutine is caught by resume() with its text");
            const bool free_after_coro = probe_acquire(fx.mtx, 300);
            if (v == Variant::Old) { check(!free_after_coro, "OLD: coroutine raise leaks the mutex too"); fx.drain_leaked(1); }
            else check(free_after_coro, "NEW: mutex free after the coroutine raise");
        }
        check(n[0].text == n[1].text, "OLD vs NEW: nested-pcall error text byte-identical");
        check(k[0].text == k[1].text, "OLD vs NEW: coroutine error text byte-identical");
    }

    // =====================================================================================
    std::printf("\n[S4] an OUTER lock held by the caller around the Lua call must survive (no over-release)\n");
    {
        Fixture fx(Variant::New);
        bool held_during = false, held_after_pcall = false;
        fx.game.run([&] {
            std::lock_guard<std::recursive_mutex> outer{fx.mtx};                 // e.g. ExecuteInGameThread loop
            luaL_loadstring(fx.L, "return pcall(lock_raise, 'inner-raise')");
            lua_pcall(fx.L, 0, 2, 0);
            lua_settop(fx.L, 0);
            held_after_pcall = !probe_acquire(fx.mtx, 150);                      // outer lock must still be held
            (void)held_during;
        });
        check(held_after_pcall, "NEW: caller's own lock is still held after the raise (guards release only their own depth)");
        check(probe_acquire(fx.mtx, 300), "NEW: after the caller leaves its scope thread B acquires");
    }

    // =====================================================================================
    std::printf("\n[S5] success path, no regression\n");
    {
        std::string out[2];
        for (int i = 0; i < 2; ++i)
        {
            const Variant v = i == 0 ? Variant::Old : Variant::New;
            Fixture fx(v);
            auto r = fx.eval("local s = 0 for i = 1, 1000 do s = s + lock_ok(i) end return s, type(s), lock_ok(41)");
            out[i] = r.a + "|" + r.b + "|" + r.c;
            check(r.call_ok && r.a == "501500" && r.b == "number" && r.c == "42", std::string(vname(v)) + ": 1000 locked calls return the right values");
            check(probe_acquire(fx.mtx, 300), std::string(vname(v)) + ": mutex free after the successful calls");
            check(r.top_after == 0, std::string(vname(v)) + ": stack balanced");
        }
        check(out[0] == out[1], "OLD vs NEW: identical results on the success path");
        // NEW: a successful call right AFTER an error still works (state not poisoned)
        Fixture fx(Variant::New);
        auto r1 = fx.eval("local ok = pcall(lock_raise, 'x') return ok, lock_ok(1), type(ok)");
        check(r1.call_ok && r1.a == "false" && r1.b == "2", "NEW: a locked success right after a locked raise works");
        check(probe_acquire(fx.mtx, 300), "NEW: mutex free after raise-then-success");
    }

    // =====================================================================================
    std::printf("\n[S6] repetition: 200 raises\n");
    {
        Fixture fo(Variant::Old), fn(Variant::New);
        g_ctor = g_dtor = 0;
        const char* loop = "local n = 0 for i = 1, 200 do if not pcall(lock_raise, 'r') then n = n + 1 end end return n, 0, 0";
        auto ro = fo.eval(loop);
        auto rn = fn.eval(loop);
        check(ro.a == "200" && rn.a == "200", "OLD and NEW: all 200 calls raised and were caught by pcall");
        check(!probe_acquire(fo.mtx, 300), "OLD: leaked after 200 raises");
        check(fo.drain_leaked(200) == 200, "OLD: lock depth is exactly 200 (one leaked guard per raise)");
        check(probe_acquire(fo.mtx, 300), "OLD: free only after 200 owner releases");
        check(probe_acquire(fn.mtx, 300), "NEW: free after 200 raises");
        check(g_ctor == 200 && g_dtor == 200, "NEW: 200 exception objects created, 200 destroyed");
    }

    std::printf("\nchecked %d, failed %d\n", g_checked, g_failed);
    if (g_checked == 0) { std::printf("FAIL b5 lock-raise host test (zero checks ran)\n"); return 2; }
    std::printf("%s b5 lock-raise host test (%d failures)\n", g_failed ? "FAIL" : "PASS", g_failed);
    return g_failed ? 1 : 0;
}
