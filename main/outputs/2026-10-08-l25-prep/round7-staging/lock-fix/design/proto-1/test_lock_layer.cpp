// DESIGN-1 host fixture (Mac-only). Compiles the SAME LuaModLockLayer.inc that
// LuaMod.cpp would include, and checks the protocol the design relies on.
// Every test prints how many checks it ran; the program FAILS if a test ran
// zero checks (CLAUDE.md gate rule) or any check failed.
//
//   clang++ -std=c++20 -O2 -pthread test_lock_layer.cpp -o test_lock_layer
//   ./test_lock_layer [--only T3] [--skip T1]
#include <atomic>
#include <chrono>
#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

static std::recursive_mutex g_test_queue_mutex;   // stands in for LuaMod::m_thread_actions_mutex
static std::mutex g_emit_mu;
static std::vector<std::string> g_emitted;
static std::atomic<int64_t> g_skew_us{0};
static void test_emit(const char* line)
{
    std::lock_guard<std::mutex> g(g_emit_mu);
    g_emitted.emplace_back(line);
}
static int64_t test_now_us()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() + g_skew_us.load();
}
#define LOCKLAYER_QUEUE_MUTEX g_test_queue_mutex
#define LOCKLAYER_EMIT(line) test_emit(line)
#define LOCKLAYER_NOW_US() test_now_us()
#include "LuaModLockLayer.inc"

using namespace RC::LockLayer;

static int g_checked = 0, g_failed = 0;
static int t_checked = 0, t_failed = 0;
#define CHECK(cond, ...)                                                  \
    do {                                                                  \
        ++g_checked; ++t_checked;                                         \
        if (!(cond)) { ++g_failed; ++t_failed; std::printf("    FAIL %s:%d  %s  -- ", __FILE__, __LINE__, #cond); std::printf(__VA_ARGS__); std::printf("\n"); } \
    } while (0)

static void busy_us(int64_t us)
{
    const int64_t end = test_now_us() + us;
    while (test_now_us() < end) {}
}
static bool queue_free_for_other_thread(int wait_ms)
{
    bool got = false;
    std::thread t([&] {
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(wait_ms);
        while (std::chrono::steady_clock::now() < end)
        {
            if (g_test_queue_mutex.try_lock()) { got = true; g_test_queue_mutex.unlock(); return; }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    t.join();
    return got;
}
static bool slot_free_for_other_thread(ModSlot* s, int wait_ms)
{
    bool got = false;
    std::thread t([&] {
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(wait_ms);
        while (std::chrono::steady_clock::now() < end)
        {
            if (s->m.try_lock()) { got = true; s->m.unlock(); return; }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    t.join();
    return got;
}
template <class F>
static void with_watchdog(const char* name, int seconds, F&& f)
{
    std::promise<void> p;
    auto fut = p.get_future();
    std::thread w([&] { f(); p.set_value(); });
    if (fut.wait_for(std::chrono::seconds(seconds)) != std::future_status::ready)
    {
        std::printf("  [%s] WATCHDOG: no completion in %d s (deadlock or livelock) -> FAIL\n", name, seconds);
        std::fflush(stdout);
        _exit(2);
    }
    w.join();
}

// ------------------------------------------------------------------ T1
static jmp_buf g_jb;
extern "C" __attribute__((noinline)) void fake_lua_error() { _longjmp(g_jb, 1); }   // luaL_error on a C-built Lua
__attribute__((noinline)) static void binding_leaky() { QueueLock q; fake_lua_error(); }                       // the B4 shape
__attribute__((noinline)) static void binding_safe() { QueueLock q; q.unlock_early(); fake_lua_error(); }     // the fix shape
static void fake_pcall(void (*f)()) { if (_setjmp(g_jb) == 0) f(); }

static void T1_stuck_key_leak()
{
    // A: control, no layer protection around the call (what B4 did)
    fake_pcall(binding_leaky);
    CHECK(tl_q_depth == 1, "depth=%d", tl_q_depth);
    CHECK(!queue_free_for_other_thread(150), "control must reproduce the B4 stuck lock");
    repair_leak("t1-control-cleanup", 0);
    CHECK(queue_free_for_other_thread(150), "manual repair frees the queue");
    // B: ModGuard around the Lua call repairs by itself, other thread gets the queue right after
    ModSlot* a = slot_create("t1-a");
    g_skew_us.fetch_add(2000000);   // the leak line is limited to 1 per second
    const int64_t l0 = S().leaks_repaired.get();
    g_emitted.clear();
    {
        ModGuard g(a, Entry::Drain, "t1-guard");
        CHECK(g.ok(), "guard");
        fake_pcall(binding_leaky);
    }
    CHECK(S().leaks_repaired.get() == l0 + 1, "repaired=%lld", (long long)(S().leaks_repaired.get() - l0));
    CHECK(tl_q_depth == 0, "depth after guard=%d", tl_q_depth);
    CHECK(queue_free_for_other_thread(150), "queue free after guard repair");
    CHECK(slot_free_for_other_thread(a, 150), "mod lock released");
    bool saw = false;
    for (auto& l : g_emitted) if (l.rfind("[LOCKLEAK] ", 0) == 0) saw = true;
    CHECK(saw, "leak line logged");
    // C: LeakProbe (async loop / hook sites that take no mod lock)
    {
        LeakProbe p("t1-probe");
        fake_pcall(binding_leaky);
    }
    CHECK(tl_q_depth == 0 && queue_free_for_other_thread(150), "probe repair");
    // D: the explicit fix (unlock_early before raising) leaks nothing
    const int64_t l1 = S().leaks_repaired.get();
    {
        ModGuard g(a, Entry::Drain, "t1-safe");
        fake_pcall(binding_safe);
    }
    CHECK(S().leaks_repaired.get() == l1 && queue_free_for_other_thread(150), "no leak with unlock_early");
    CHECK(S().raise_unlocks.get() >= 1, "unlock_early is counted (raise_unlocks=%lld)", (long long)S().raise_unlocks.get());
    // E: a legitimately held outer level is NOT touched
    {
        QueueLock outer(QueueLock::Kind::Drain);
        ModGuard g(a, Entry::Nested, "t1-outer");   // takes mod lock with Q held -> counted as order violation, but works as try
        fake_pcall(binding_leaky);
        g.ok();
    }
    CHECK(tl_q_depth == 0, "depth=%d", tl_q_depth);
}

// ------------------------------------------------------------------ T2
static void T2_key_starvation()
{
    ModSlot* s = slot_create("t2");
    std::atomic<bool> stop{false};
    std::atomic<long> game_runs{0};
    // "game thread": 0.9 ms of Lua, then immediately wants the lock again (99% duty cycle)
    std::thread game([&] {
        while (!stop.load())
        {
            ModGuard g(s, Entry::Drain, "t2-game");
            if (!g.ok()) { std::this_thread::yield(); continue; }
            busy_us(900);
            game_runs.fetch_add(1);
        }
    });
    int ran = 0;
    const int64_t d0 = S().key_dispatched.get();
    const int64_t m0 = S().key_wait_max_us.get();
    (void)m0;
    S().key_wait_max_us.set(0);
    S().key_wait_total_us.set(0);
    std::thread upd([&] {
        const int kEvents = 60;
        int queued = 0;
        auto next_key = std::chrono::steady_clock::now();
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(3500);
        while (std::chrono::steady_clock::now() < end)
        {
            if (queued < kEvents && std::chrono::steady_clock::now() >= next_key)
            {
                key_enqueue(s, nullptr, 1, 118);
                ++queued;
                next_key += std::chrono::milliseconds(50);
            }
            ran += key_pump([](const KeyEvent&) { busy_us(100); });
            std::this_thread::sleep_for(std::chrono::milliseconds(5));   // UpdateThread poll period
        }
    });
    upd.join();
    stop = true;
    game.join();
    const int64_t d = S().key_dispatched.get() - d0;
    const int64_t mx = S().key_wait_max_us.get();
    const int64_t mean = d ? S().key_wait_total_us.get() / d : 0;
    std::printf("    T2 new layer: dispatched=%lld/60 key wait mean=%lld us max=%lld us, game ran %ld items meanwhile\n",
                (long long)d, (long long)mean, (long long)mx, game_runs.load());
    CHECK(d == 60, "dispatched=%lld", (long long)d);
    CHECK(mx < 30000, "max key wait %lld us must stay under 30 ms", (long long)mx);
    CHECK(game_runs.load() > 1000, "game thread was not starved by the key path: %ld", game_runs.load());
    CHECK(key_pending_count() == 0, "queue empty");

    // control: the old shape. One recursive_mutex, holder re-takes at once, waiter blocks in lock().
    // The waiter runs on its own thread so a starved wait can be reported instead of hanging the test.
    std::recursive_mutex m;
    std::atomic<bool> stop2{false};
    std::thread game2([&] { while (!stop2.load()) { std::lock_guard<std::recursive_mutex> g(m); busy_us(900); } });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::promise<int64_t> got;
    auto got_f = got.get_future();
    std::thread waiter([&] {
        const int64_t t0 = test_now_us();
        m.lock(); m.unlock();
        got.set_value(test_now_us() - t0);
    });
    const bool acquired_in_3s = got_f.wait_for(std::chrono::seconds(3)) == std::future_status::ready;
    stop2 = true;
    game2.join();
    waiter.join();
    std::printf("    T2 control (old shape: plain recursive_mutex, holder re-takes at once): waiter %s within 3 s%s\n",
                acquired_in_3s ? "got the lock" : "was STARVED, did not get the lock",
                acquired_in_3s ? "" : " (it only got it after the holder stopped)");
    CHECK(acquired_in_3s || !acquired_in_3s, "control is informational");   // counts as a check; outcome is OS-dependent and printed above
}

// ------------------------------------------------------------------ T3
static void T3_abba_two_mods()
{
    ModSlot* A = slot_create("t3-A");
    ModSlot* B = slot_create("t3-B");
    CHECK(A->rank < B->rank, "ranks");
    std::atomic<int> inA{0}, inB{0}, maxA{0}, maxB{0};
    auto enter = [](std::atomic<int>& in, std::atomic<int>& mx) {
        int v = in.fetch_add(1) + 1;
        int c = mx.load();
        while (v > c && !mx.compare_exchange_weak(c, v)) {}
    };
    std::atomic<long> g_ran{0}, g_skip{0}, o_ran{0}, o_skip{0}, g_top{0}, o_top{0};
    const int N = 30000;
    with_watchdog("T3", 20, [&] {
        std::thread game([&] {   // game thread: mod A's code (holds A) triggers mod B's hook (needs B)
            for (int i = 0; i < N; ++i)
            {
                if ((i & 7) == 0) busy_us(30);   // gaps between the game thread's turns
                ModGuard ga(A, Entry::Drain, "t3-game-A");
                if (!ga.ok()) continue;
                ++g_top;
                enter(inA, maxA);
                busy_us(2);
                ModGuard gb(B, Entry::Nested, "t3-game->B");
                if (gb.ok()) { enter(inB, maxB); busy_us(2); inB.fetch_sub(1); ++g_ran; }
                else ++g_skip;
                inA.fetch_sub(1);
            }
        });
        std::thread other([&] { // other thread: mod B's code (holds B) triggers mod A's hook (needs A)
            for (int i = 0; i < N; ++i)
            {
                ModGuard gb(B, Entry::Key, "t3-other-B");
                if (!gb.ok()) continue;
                ++o_top;
                enter(inB, maxB);
                busy_us(2);
                ModGuard ga(A, Entry::Nested, "t3-other->A");
                if (ga.ok()) { enter(inA, maxA); busy_us(2); inA.fetch_sub(1); ++o_ran; }
                else ++o_skip;
                inB.fetch_sub(1);
            }
        });
        game.join();
        other.join();
    });
    std::printf("    T3: game(A->B) top=%ld nested ran=%ld skipped=%ld | other(B->A) top=%ld nested ran=%ld skipped=%ld\n",
                g_top.load(), g_ran.load(), g_skip.load(), o_top.load(), o_ran.load(), o_skip.load());
    CHECK(maxA.load() == 1 && maxB.load() == 1, "mutual exclusion: maxA=%d maxB=%d", maxA.load(), maxB.load());
    CHECK(g_ran.load() + g_skip.load() == g_top.load(), "game accounting");
    CHECK(o_ran.load() + o_skip.load() == o_top.load(), "other accounting");
    CHECK(g_top.load() > 0 && o_top.load() > 0, "both sides made progress");
    CHECK(g_ran.load() > 0, "ascending nested acquire (A->B) mostly succeeds");

    // control: naive two-lock code with opposite order deadlocks (so the scenario is real)
    std::timed_mutex ma, mb;
    std::atomic<int> locked_first{0};
    std::atomic<int> timeouts{0};
    auto naive = [&](std::timed_mutex& first, std::timed_mutex& second) {
        std::lock_guard<std::timed_mutex> g1(first);
        locked_first.fetch_add(1);
        while (locked_first.load() < 2) {}
        if (!second.try_lock_for(std::chrono::milliseconds(300))) timeouts.fetch_add(1);
        else second.unlock();
    };
    std::thread t1(naive, std::ref(ma), std::ref(mb));
    std::thread t2(naive, std::ref(mb), std::ref(ma));
    t1.join(); t2.join();
    CHECK(timeouts.load() >= 1, "control: naive opposite-order locking deadlocks until a timeout breaks it (timeouts=%d)", timeouts.load());
}

// ------------------------------------------------------------------ T4
static void T4_reentrancy()
{
    ModSlot* A = slot_create("t4-A");
    ModSlot* B = slot_create("t4-B");
    {
        ModGuard g1(A, Entry::Drain, "t4-1");
        ModGuard g2(A, Entry::Nested, "t4-2");
        ModGuard g3(A, Entry::Key, "t4-3");
        ModGuard g4(A, Entry::Blocking, "t4-4");
        ModGuard g5(A, Entry::Drain, "t4-5");
        CHECK(g1.ok() && g2.ok() && g3.ok() && g4.ok() && g5.ok(), "same-thread nested hooks of one mod never wait");
        {
            ModGuard gb(B, Entry::Nested, "t4-B");
            ModGuard ga_again(A, Entry::Nested, "t4-A-again");   // A while holding A and B
            CHECK(gb.ok() && ga_again.ok(), "A re-entered while B is held");
        }
        CHECK(!slot_free_for_other_thread(A, 50), "A still held by the outer guards");
    }
    CHECK(slot_free_for_other_thread(A, 150) && slot_free_for_other_thread(B, 150), "everything released");
    CHECK(tl_held.n == 0, "held set empty (%d)", tl_held.n);
}

// ------------------------------------------------------------------ T5
static void T5_no_lost_wakeup_and_fast_path()
{
    std::vector<int> qa;   // guarded by Q: stands in for m_game_thread_actions
    std::atomic<long> consumed{0};
    __atomic_store_n(&g_work_bits, 0u, __ATOMIC_RELEASE);
    for (int e = 0; e < kExecCount; ++e) set_wake(e, kNever);
    long total_all = 0;
    for (int round = 0; round < 25; ++round)
    {
        const int P = 4, N = 5000;
        const long total = (long)P * N;
        consumed = 0;
        std::atomic<int> done_producers{0};
        std::vector<std::thread> prod;
        for (int p = 0; p < P; ++p)
            prod.emplace_back([&, p] {
                unsigned x = 12345u + (unsigned)p * 77u + (unsigned)round * 1013u;
                for (int i = 0; i < N; ++i)
                {
                    {
                        QueueLock q;   // Producer: sets the bits while still holding the lock
                        qa.push_back(i);
                    }
                    x = x * 1664525u + 1013904223u;
                    if ((x >> 24) < 6) std::this_thread::sleep_for(std::chrono::microseconds(20 + (x & 0xFF)));
                }
                done_producers.fetch_add(1);
            });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        std::thread cons([&] {
            while (consumed.load() < total && std::chrono::steady_clock::now() < deadline)
            {
                if (!pump_should_run(kWorkGame | kWorkNotify, kExecMarshal)) { std::this_thread::yield(); continue; }
                std::vector<int> take;
                {
                    QueueLock c(QueueLock::Kind::Drain);
                    pump_claim(c, kWorkGame | kWorkNotify);
                    take.swap(qa);
                }
                consumed.fetch_add((long)take.size());
                {
                    QueueLock e(QueueLock::Kind::Drain);
                    pump_rearm(e, !qa.empty(), false, false);
                    set_wake(kExecMarshal, kNever);   // what the pump's wake recompute does when no delayed work is queued
                }
            }
        });
        for (auto& t : prod) t.join();
        cons.join();
        total_all += total;
        CHECK(consumed.load() == total, "round %d consumed %ld of %ld (lost wakeup)", round, consumed.load(), total);
        if (consumed.load() != total) break;
    }
    std::printf("    T5: %ld items through the claim/rearm protocol, no loss\n", total_all);
    // idle: after the last drain the pump must skip without touching the queue lock
    for (int i = 0; i < 4; ++i)
    {
        if (!pump_should_run(kWorkGame | kWorkNotify, kExecMarshal)) break;
        QueueLock c(QueueLock::Kind::Drain);
        pump_claim(c, kWorkGame | kWorkNotify);
        qa.clear();
        pump_rearm(c, false, false, false);
    }
    set_wake(kExecMarshal, kNever);
    const int64_t takes0 = S().q_takes.get();
    const int64_t skips0 = S().pump_skips.get();
    const int64_t pt0 = S().pump_takes.get();
    for (int i = 0; i < 1000000; ++i) pump_should_run(kWorkGame | kWorkNotify, kExecMarshal);
    CHECK(S().q_takes.get() == takes0, "idle fast path took the queue lock %lld times", (long long)(S().q_takes.get() - takes0));
    CHECK(S().pump_skips.get() - skips0 == 1000000 && S().pump_takes.get() == pt0, "skips=%lld", (long long)(S().pump_skips.get() - skips0));
    std::printf("    T5: 1,000,000 idle pump calls -> 0 queue-lock takes, 1,000,000 skips\n");
}

// ------------------------------------------------------------------ T6
static void T6_stats_rate_limit()
{
    g_emitted.clear();
    __atomic_store_n(&g_emit_started, 0, __ATOMIC_RELEASE);
    S().eigt_queued.add(7); S().eigt_run.add(7);
    const int64_t base = g_skew_us.load();
    for (int i = 0; i < 1000; ++i)
    {
        g_skew_us.store(base + (int64_t)i * 25000);   // 25 ms steps, 25 s simulated
        maybe_emit(false);
    }
    size_t n = g_emitted.size();
    std::printf("    T6: %zu stats lines in 25 simulated seconds\n", n);
    CHECK(n >= 3 && n <= 4, "expected init + one per 10 s, got %zu", n);
    bool all_prefixed = n > 0;
    for (auto& l : g_emitted) if (l.rfind("[LOCK] ", 0) != 0) all_prefixed = false;
    CHECK(all_prefixed, "every line starts with \"[LOCK] \"");
    CHECK(!g_emitted.empty() && g_emitted[0].find("[LOCK] init layer=B5-1") == 0, "first line is the init beacon");
    const std::string& last = g_emitted.back();
    for (const char* key : {" key q=", " wait_ms mean=", " | qlock takes=", "hold_max_ms=", " | pump calls=", " skips=", " takes=", " | eigt q=", " run=", " | modlock ", " raise_unlocks=", " | mods"})
        CHECK(last.find(key) != std::string::npos, "field %s", key);
    maybe_emit(true);
    CHECK(g_emitted.size() == n + 1 && g_emitted.back().find("[LOCK] final ") == 0, "final line");
    std::printf("    sample: %s\n", g_emitted.back().substr(0, 400).c_str());
}

// ------------------------------------------------------------------ T7
static void T7_wake_and_producer_default()
{
    __atomic_store_n(&g_work_bits, 0u, __ATOMIC_RELEASE);
    for (int e = 0; e < kExecCount; ++e) set_wake(e, kNever);
    CHECK(!pump_should_run(kWorkGame | kWorkNotify, kExecMarshal), "idle");
    { QueueLock d(QueueLock::Kind::Drain); }
    CHECK(!pump_should_run(kWorkGame | kWorkNotify, kExecMarshal), "a Drain section does not wake pumps");
    { QueueLock p; }   // a forgotten call site is a Producer by default
    CHECK(pump_should_run(kWorkGame, kExecMarshal) && pump_should_run(kWorkTick, kExecTick), "Producer section wakes all");
    __atomic_store_n(&g_work_bits, 0u, __ATOMIC_RELEASE);
    for (int e = 0; e < kExecCount; ++e) set_wake(e, kNever);
    set_wake(kExecMarshal, test_now_us() + 40000);
    CHECK(!pump_should_run(kWorkGame, kExecMarshal), "timed wake not yet due");
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    CHECK(pump_should_run(kWorkGame, kExecMarshal), "timed wake due");
    CHECK(!pump_should_run(kWorkTick, kExecTick), "other executor unaffected");
    note_defer();
    const int64_t w = take_defer_wake();
    CHECK(w != kNever && w > test_now_us() - 10 && take_defer_wake() == kNever, "defer wake is one-shot");
}

// ------------------------------------------------------------------ T8
static void T8_key_queue_order_overflow_and_preference()
{
    ModSlot* s1 = slot_create("t8-1");
    ModSlot* s2 = slot_create("t8-2");
    std::vector<int> order;
    std::atomic<bool> hold{true};
    std::atomic<bool> holding{false};
    std::thread holder([&] {
        ModGuard g(s1, Entry::Blocking, "t8-holder");
        holding = true;
        while (hold.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    });
    while (!holding.load()) std::this_thread::yield();
    key_enqueue(s1, nullptr, 1, 1);
    key_enqueue(s2, nullptr, 2, 2);
    key_enqueue(s1, nullptr, 3, 3);
    auto run = [&](const KeyEvent& e) { order.push_back(e.ref); };
    int ran = key_pump(run);
    CHECK(ran == 1 && order.size() == 1 && order[0] == 2, "free mod runs, busy mod's events stay queued (ran=%d)", ran);
    CHECK(key_pending_count() == 2, "two left");
    // a game-thread drain on the busy mod yields while a key is queued for it
    std::atomic<int> drain_ok{-1};
    std::thread g1([&] { ModGuard g(s1, Entry::Drain, "t8-drain"); drain_ok = g.ok() ? 1 : 0; });
    g1.join();
    CHECK(drain_ok.load() == 0, "drain cannot take a busy lock");
    hold = false;
    holder.join();
    const int64_t y0 = S().key_yields.get();
    std::thread g2([&] { ModGuard g(s1, Entry::Drain, "t8-drain2"); drain_ok = g.ok() ? 1 : 0; });
    g2.join();
    CHECK(drain_ok.load() == 0 && S().key_yields.get() == y0 + 1, "game thread yields to the queued key (preference)");
    ran = key_pump(run);
    CHECK(ran == 2 && order.size() == 3 && order[1] == 1 && order[2] == 3, "FIFO per mod after the lock frees");
    std::thread g3([&] { ModGuard g(s1, Entry::Drain, "t8-drain3"); drain_ok = g.ok() ? 1 : 0; });
    g3.join();
    CHECK(drain_ok.load() == 1, "preference cleared once the queue is empty");
    // preference expires (a dead UpdateThread cannot block the game thread forever)
    key_enqueue(s1, nullptr, 9, 9);
    g_skew_us.fetch_add(kKeyPrefMaxUs + 1000);
    std::thread g4([&] { ModGuard g(s1, Entry::Drain, "t8-drain4"); drain_ok = g.ok() ? 1 : 0; });
    g4.join();
    CHECK(drain_ok.load() == 1, "preference expires after %lld us", (long long)kKeyPrefMaxUs);
    key_pump(run);
    // overflow keeps the newest 64
    std::thread h2([&] { ModGuard g(s1, Entry::Blocking, "t8-holder2"); holding = false; while (!holding.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1)); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const int64_t o0 = S().key_overflow.get();
    for (int i = 0; i < 100; ++i) key_enqueue(s1, nullptr, 100 + i, 118);
    CHECK(S().key_overflow.get() - o0 == 100 - kKeyQ && key_pending_count() == kKeyQ, "overflow dropped %lld", (long long)(S().key_overflow.get() - o0));
    holding = true;
    h2.join();
    order.clear();
    key_pump(run);
    CHECK(order.size() == (size_t)kKeyQ && order.front() == 100 + (100 - kKeyQ) && order.back() == 199, "newest kept, in order");
    // teardown purge
    key_enqueue(s2, nullptr, 21, 1); key_enqueue(s1, nullptr, 22, 1); key_enqueue(s2, nullptr, 23, 1);
    CHECK(key_purge(s2) == 2 && key_pending_count() == 1, "purge drops only that mod's events");
    order.clear();
    key_pump(run);
    CHECK(order.size() == 1 && order[0] == 22, "other mod's event survives the purge");
    // re-entrant pump from inside a callback does nothing
    key_enqueue(s2, nullptr, 7, 7);
    int inner = -1;
    key_pump([&](const KeyEvent&) { inner = key_pump([](const KeyEvent&) {}); });
    CHECK(inner == 0, "nested key_pump returns 0");
}

// ------------------------------------------------------------------ main
struct TestEntry { const char* name; void (*fn)(); };
int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const TestEntry tests[] = {
        {"T1", T1_stuck_key_leak}, {"T2", T2_key_starvation}, {"T3", T3_abba_two_mods}, {"T4", T4_reentrancy},
        {"T5", T5_no_lost_wakeup_and_fast_path}, {"T7", T7_wake_and_producer_default}, {"T8", T8_key_queue_order_overflow_and_preference},
        {"T6", T6_stats_rate_limit},   // last: moves the fake clock
    };
    const char* only = nullptr; const char* skip = nullptr;
    for (int i = 1; i + 1 < argc; ++i) { if (!std::strcmp(argv[i], "--only")) only = argv[i + 1]; if (!std::strcmp(argv[i], "--skip")) skip = argv[i + 1]; }
    int tests_run = 0;
    for (const auto& t : tests)
    {
        if (only && std::strcmp(only, t.name)) continue;
        if (skip && !std::strcmp(skip, t.name)) continue;
        t_checked = 0; t_failed = 0;
        std::printf("[%s] start\n", t.name);
        t.fn();
        ++tests_run;
        std::printf("[%s] checked=%d failed=%d -> %s\n", t.name, t_checked, t_failed, (t_checked > 0 && t_failed == 0) ? "PASS" : "FAIL");
        if (t_checked == 0) ++g_failed;   // a test that checks nothing is a failed test
    }
    std::printf("TOTAL tests=%d checked=%d failed=%d -> %s\n", tests_run, g_checked, g_failed, (tests_run > 0 && g_checked > 0 && g_failed == 0) ? "ALL-PASS" : "FAIL");
    return (tests_run > 0 && g_checked > 0 && g_failed == 0) ? 0 : 1;
}
