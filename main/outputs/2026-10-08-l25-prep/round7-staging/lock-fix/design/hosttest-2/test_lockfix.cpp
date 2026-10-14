// DESIGN-2 host test (Mac-only). Compiles the proposed LOCKFIX-CORE text unchanged and
// exercises: the stuck-key repro (old vs new), fast path + lost-wakeup protocol, the
// two-mod ABBA scenario, re-entrancy, deferral order, key fallback executor, tombstones,
// and the [LOCK] statistics line. Every check prints; the program exits non-zero on any
// failure OR if zero checks ran.
#include <algorithm>
#include <chrono>
#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <atomic>      // test harness only; the production block uses __atomic builtins
#include <unistd.h>

#include "lockfix_core.inc"

static int g_checked = 0, g_failed = 0;
#define CHECK(cond, ...)                                              \
    do {                                                              \
        ++g_checked;                                                  \
        if (!(cond)) { ++g_failed; std::printf("  FAIL %s:%d ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } \
        else { std::printf("  ok   "); std::printf(__VA_ARGS__); std::printf("\n"); } \
    } while (0)

using namespace std::chrono_literals;
static void sleep_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

// Run fn on a thread; fail hard if it does not finish in limit_ms (deadlock watchdog).
template <class F>
static bool run_with_watchdog(int limit_ms, F&& fn)
{
    std::atomic<bool> done{false};
    std::thread t([&] { fn(); done.store(true); });
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(limit_ms);
    while (!done.load() && std::chrono::steady_clock::now() < end) sleep_ms(2);
    if (!done.load()) { std::printf("  WATCHDOG: thread did not finish in %d ms (deadlock?)\n", limit_ms); t.detach(); return false; }
    t.join();
    return true;
}

// ------------------------------------------------------------------------------------
// OLD model (B4 shape): one global recursive mutex H; dispatch scope holds H for the whole
// drain; the keybind callback takes H before Lua; a binding that raises (longjmp) while a
// lock_guard on H is alive leaks one ownership level on the game thread.
// ------------------------------------------------------------------------------------
namespace oldm
{
    std::recursive_mutex H;
    jmp_buf jb;
    __attribute__((noinline)) void fake_luaL_error() { _longjmp(jb, 1); }
    __attribute__((noinline)) void leaky_binding(std::recursive_mutex& m)
    {
        std::lock_guard<std::recursive_mutex> guard{m};   // RegisterHook:5947
        fake_luaL_error();                                 // lua.throw_error:5994 -> longjmp over ~lock_guard
    }
    __attribute__((noinline)) void do_leak(std::recursive_mutex& m)
    {
        if (_setjmp(jb) == 0) leaky_binding(m);             // pcall(RegisterHook, ...) in the minimap
    }
}

static void test_stuck_key_old_vs_new()
{
    std::printf("T1 stuck-key repro\n");
    // ---- OLD: leak on game thread, then game keeps pumping; input thread keybind needs H.
    std::atomic<bool> stop{false}, key_ran_old{false};
    std::atomic<long> old_events{0};
    std::thread game_old([&] {
        oldm::do_leak(oldm::H);
        while (!stop.load())
        {
            { std::unique_lock<std::recursive_mutex> scope{oldm::H}; }   // NativeLuaActionDispatchScope per event
            old_events.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    });
    sleep_ms(30);
    std::thread input_old([&] {
        std::lock_guard<std::recursive_mutex> guard{oldm::H};        // LuaMod.cpp:2231
        key_ran_old.store(true);
    });
    sleep_ms(1200);
    CHECK(!key_ran_old.load() && old_events.load() > 1000,
          "OLD: keybind never ran in 1.2 s while the game thread kept pumping (%ld events) -> reproduces the B4 symptom", old_events.load());
    stop.store(true);
    game_old.join();           // the game thread exits without unlocking: H is still leaked
    input_old.detach();        // blocked forever by design of the repro

    // ---- NEW: same leak of H, key path uses Q + M only.
    std::atomic<bool> stop2{false};
    std::atomic<int> key_ran{0};
    std::atomic<int64_t> t_enq_us{0}, t_run_us{0};
    lockfix::ModEntry* mA = lockfix::mod_register(reinterpret_cast<const void*>(0x1000), "modA");
    static std::recursive_mutex Hnew;
    std::thread game_new([&] {
        oldm::do_leak(Hnew);                                         // same longjmp leak of a (fresh) H on the NEW game thread
        while (!stop2.load())
        {
            const int64_t nm = lockfix::now_ms();
            lockfix::note_pump(lockfix::now_us());
            if (lockfix::pump_should_drain(lockfix::W_KEYS | lockfix::W_SIMPLE, 2, nm))
            {
                lockfix::g.fp_take.bump_single_writer();
                lockfix::drain_keys([&](const lockfix::KeyEv& ev) {
                    lockfix::ModScope s(lockfix::mod_find(ev.mod), lockfix::Policy::Try);
                    if (!s.ok()) return false;
                    t_run_us.store(lockfix::now_us());
                    key_ran.fetch_add(1);
                    return true;
                }, 0, nullptr);
            }
            else lockfix::g.fp_skip.bump_single_writer();
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    });
    sleep_ms(30);
    std::thread input_new([&] {
        t_enq_us.store(lockfix::now_us());
        lockfix::key_enqueue(lockfix::KeyEv{reinterpret_cast<const void*>(0x1000), nullptr, 7, 118, 0});
    });
    input_new.join();
    sleep_ms(100);
    CHECK(key_ran.load() == 1, "NEW: key dispatched once although H is leaked on the game thread");
    const int64_t lat = t_run_us.load() - t_enq_us.load();
    CHECK(lat >= 0 && lat < 20'000, "NEW: key press -> callback start = %lld us (< 20 ms)", (long long)lat);
    stop2.store(true);
    game_new.join();
    (void)mA;
}

static void test_fast_path_and_wakeups()
{
    std::printf("T2 fast path + lost-wakeup protocol\n");
    // emulated simple queue guarded by Q, bits published from truth
    static std::vector<int> simple;
    auto publish = [] { lockfix::bits_set_locked(lockfix::W_SIMPLE, !simple.empty()); };
    const uint64_t takes0 = lockfix::g.q_takes.get();
    std::atomic<bool> producers_done{false};
    constexpr int kProducers = 4, kPer = 20000;
    std::vector<std::atomic<int>> seen(kProducers * kPer);
    for (auto& s : seen) s.store(0);
    std::vector<int64_t> t_enq(kProducers * kPer, 0);
    std::vector<std::thread> prod;
    for (int p = 0; p < kProducers; ++p)
        prod.emplace_back([&, p] {
            for (int i = 0; i < kPer; ++i)
            {
                const int id = p * kPer + i;
                t_enq[id] = lockfix::now_us();
                { lockfix::QGuard q; simple.push_back(id); publish(); }
                if ((i & 63) == 0) std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        });
    std::thread joiner([&] { for (auto& t : prod) t.join(); producers_done.store(true); });
    uint64_t events = 0, max_lat = 0;
    int64_t last_progress = lockfix::now_us();
    size_t consumed = 0;
    std::vector<int> run;
    while (true)
    {
        ++events;
        const int64_t nm = lockfix::now_ms();
        if (lockfix::pump_should_drain(lockfix::W_SIMPLE, 2, nm))
        {
            run.clear();
            { lockfix::QGuard q; run.swap(simple); publish(); }
            const int64_t t = lockfix::now_us();
            for (int id : run) { seen[id].fetch_add(1); max_lat = std::max<uint64_t>(max_lat, (uint64_t)(t - t_enq[id])); }
            consumed += run.size();
            last_progress = t;
        }
        if (producers_done.load() && consumed == (size_t)kProducers * kPer) break;
        if (lockfix::now_us() - last_progress > 3'000'000) break;
    }
    joiner.join();
    size_t dup = 0, miss = 0;
    for (auto& s : seen) { if (s.load() > 1) ++dup; if (s.load() == 0) ++miss; }
    CHECK(dup == 0 && miss == 0, "every one of %d items ran exactly once (dup=%zu miss=%zu)", kProducers * kPer, dup, miss);
    CHECK(max_lat < 200'000, "max enqueue->run latency %llu us (< 200 ms; no lost wakeup stalls)", (unsigned long long)max_lat);
    std::printf("      events=%llu q_takes=%llu\n", (unsigned long long)events, (unsigned long long)(lockfix::g.q_takes.get() - takes0));

    // idle ratio: 2,000,000 events, 200 enqueues -> takes must be ~2x enqueues, not 2,000,000
    simple.clear();
    { lockfix::QGuard q; publish(); }
    const uint64_t t1 = lockfix::g.q_takes.get();
    std::atomic<bool> go{true};
    std::thread p2([&] { for (int i = 0; i < 200; ++i) { { lockfix::QGuard q; simple.push_back(i); publish(); } sleep_ms(1); } go.store(false); });
    uint64_t ev2 = 0; size_t got2 = 0;
    while (go.load() || !simple.empty())
    {
        ++ev2;
        if (lockfix::pump_should_drain(lockfix::W_SIMPLE, 2, lockfix::now_ms()))
        { lockfix::QGuard q; got2 += simple.size(); simple.clear(); publish(); }
    }
    p2.join();
    const uint64_t takes = lockfix::g.q_takes.get() - t1;
    CHECK(got2 == 200, "idle-ratio run consumed all 200 items");
    CHECK(takes <= 2 * 200 + 20 && ev2 > 100000, "queue-lock takes %llu for %llu events (old design: 1+ per event)", (unsigned long long)takes, (unsigned long long)ev2);
}

static void test_abba_and_reentrancy()
{
    std::printf("T3 two mods firing hooks at each other from two threads (ABBA) + re-entrancy\n");
    lockfix::g_cross_us = 2000;
    auto* A = lockfix::mod_register(reinterpret_cast<const void*>(0xA0), "modA_abba");
    auto* B = lockfix::mod_register(reinterpret_cast<const void*>(0xB0), "modB_abba");
    std::atomic<int> done_g{0}, done_s{0};
    const uint64_t f0 = lockfix::g.m_forced.get();
    bool finished = run_with_watchdog(30000, [&] {
        std::thread game([&] {            // game thread: runs mod A closure, which triggers a mod B hook
            for (int i = 0; i < 400; ++i)
            {
                lockfix::ModScope a(A, lockfix::Policy::Block);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                { lockfix::ModScope b(B, lockfix::Policy::Bounded, 50'000); (void)b; std::this_thread::sleep_for(std::chrono::microseconds(50)); }
                done_g.fetch_add(1);
            }
        });
        std::thread asyn([&] {            // B's async thread: runs a B loop, which triggers a mod A hook
            for (int i = 0; i < 400; ++i)
            {
                lockfix::ModScope b(B, lockfix::Policy::Block);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                { lockfix::ModScope a(A, lockfix::Policy::Bounded, 50'000); (void)a; std::this_thread::sleep_for(std::chrono::microseconds(50)); }
                done_s.fetch_add(1);
            }
        });
        game.join(); asyn.join();
    });
    CHECK(finished && done_g.load() == 400 && done_s.load() == 400, "ABBA: both threads finished 400 iterations each, no deadlock");
    std::printf("      forced entries (cross-mod wait expired) = %llu, max wait A=%llu us B=%llu us\n",
                (unsigned long long)(lockfix::g.m_forced.get() - f0), (unsigned long long)A->wait_max_us.get(), (unsigned long long)B->wait_max_us.get());
    CHECK(A->wait_max_us.get() < 50'000 && B->wait_max_us.get() < 50'000, "no single mod-lock wait exceeded 50 ms");

    // re-entrancy: same mod nested 6 deep under Try; A -> B -> A; none may self-deadlock or defer
    bool ok_nested = true;
    bool fin2 = run_with_watchdog(2000, [&] {
        lockfix::ModScope l1(A, lockfix::Policy::Try); ok_nested &= l1.ok();
        lockfix::ModScope l2(A, lockfix::Policy::Try); ok_nested &= l2.ok();
        lockfix::ModScope l3(A, lockfix::Policy::Try); ok_nested &= l3.ok();
        lockfix::ModScope l4(B, lockfix::Policy::Bounded, 10'000); ok_nested &= l4.ok();
        lockfix::ModScope l5(A, lockfix::Policy::Try); ok_nested &= l5.ok();
        lockfix::ModScope l6(A, lockfix::Policy::Try); ok_nested &= l6.ok();
    });
    CHECK(fin2 && ok_nested, "nested hooks inside one mod (and A->B->A) re-enter without deadlock or deferral");
    lockfix::g_cross_us = 10'000;
}

static void test_defer_order_and_fallback_and_tombstone()
{
    std::printf("T4 deferral keeps per-mod order; fallback executor; tombstone\n");
    auto* A = lockfix::mod_register(reinterpret_cast<const void*>(0xA1), "modA_def");
    auto* B = lockfix::mod_register(reinterpret_cast<const void*>(0xB1), "modB_def");
    std::vector<std::string> order;
    std::atomic<bool> hold{true}, held{false};
    std::thread holder([&] { lockfix::ModScope s(A, lockfix::Policy::Block); held.store(true); while (hold.load()) sleep_ms(1); });
    while (!held.load()) sleep_ms(1);
    for (int i = 1; i <= 3; ++i) lockfix::key_enqueue({reinterpret_cast<const void*>(0xA1), nullptr, i, 1, 0});
    lockfix::key_enqueue({reinterpret_cast<const void*>(0xB1), nullptr, 9, 2, 0});
    auto runner = [&](const lockfix::KeyEv& ev) {
        lockfix::ModScope s(lockfix::mod_find(ev.mod), lockfix::Policy::Try);
        if (!s.ok()) return false;
        order.push_back((ev.mod == reinterpret_cast<const void*>(0xA1) ? "A" : "B") + std::to_string(ev.ref));
        return true;
    };
    lockfix::drain_keys(runner, 0, nullptr);
    CHECK(order.size() == 1 && order[0] == "B9", "while mod A's Lua lock is busy: A's 3 keys deferred, B's key ran");
    hold.store(false); holder.join();
    sleep_ms(lockfix::kRetryMs + 1);
    lockfix::drain_keys(runner, 0, nullptr);
    CHECK(order.size() == 4 && order[1] == "A1" && order[2] == "A2" && order[3] == "A3", "after release A's keys run in original order A1,A2,A3");

    // fallback executor: no pump alive -> the mod's own thread runs a key older than 400 ms
    std::atomic<int> fb_runs{0}; std::atomic<int64_t> fb_at{0};
    const int64_t t0 = lockfix::now_us();
    lockfix::key_enqueue({reinterpret_cast<const void*>(0xA1), nullptr, 5, 3, 0});
    std::thread async_t([&] {
        const int64_t end = lockfix::now_us() + 900'000;
        while (lockfix::now_us() < end)
        {
            lockfix::drain_keys([&](const lockfix::KeyEv& ev) {
                lockfix::ModScope s(lockfix::mod_find(ev.mod), lockfix::Policy::Bounded, 50'000);
                fb_runs.fetch_add(1); fb_at.store(lockfix::now_us()); lockfix::g.key_fb.add(); return s.ok();
            }, lockfix::kKeyFallbackUs, reinterpret_cast<const void*>(0xA1));
            sleep_ms(5);
        }
    });
    async_t.join();
    const int64_t fb_ms = (fb_at.load() - t0) / 1000;
    CHECK(fb_runs.load() == 1 && fb_ms >= 395 && fb_ms <= 480, "fallback: ran exactly once, %lld ms after the press (target 400 ms)", (long long)fb_ms);

    // pump alive: fallback must not double-run a key the game pump already took
    std::atomic<int> dbl{0};
    lockfix::key_enqueue({reinterpret_cast<const void*>(0xA1), nullptr, 6, 3, 0});
    lockfix::drain_keys([&](const lockfix::KeyEv&) { dbl.fetch_add(1); return true; }, 0, nullptr);          // game pump
    lockfix::drain_keys([&](const lockfix::KeyEv&) { dbl.fetch_add(1); return true; }, 0, reinterpret_cast<const void*>(0xA1)); // fallback
    CHECK(dbl.load() == 1, "a key is delivered exactly once even with both executors polling");

    // tombstone: an item for an uninstalled mod is dropped, not run
    lockfix::mod_tombstone(B);
    lockfix::ModScope dead(B, lockfix::Policy::Try);
    CHECK(!dead.ok(), "tombstoned (uninstalled) mod: scope refuses, item would be dropped");
    lockfix::mod_register(reinterpret_cast<const void*>(0xB1), "modB_def");   // re-register revives
    lockfix::ModScope alive(B, lockfix::Policy::Try);
    CHECK(alive.ok(), "re-registered mod (hot reload) accepts again");
}

namespace oldm
{
    std::recursive_mutex H3;
    __attribute__((noinline)) void leaky_binding_hguard()
    {
        lockfix::HGuard<std::recursive_mutex> guard{H3};
        fake_luaL_error();
    }
    __attribute__((noinline)) void do_leak_hguard() { if (_setjmp(jb) == 0) leaky_binding_hguard(); }
}

static void test_defer_cap()
{
    std::printf("T7 deferral is capped (leaked or spin-held mod lock cannot starve a mod for ever)\n");
    auto* A = lockfix::mod_register(reinterpret_cast<const void*>(0xA7), "modA_cap");
    lockfix::g_defer_max_us = 150'000;
    std::atomic<bool> held{false}, release{false};
    std::thread holder([&] { lockfix::ModScope s(A, lockfix::Policy::Block); held.store(true); while (!release.load()) sleep_ms(1); });
    while (!held.load()) sleep_ms(1);
    int deferred = 0; int64_t first_forced_ms = -1; const int64_t t0 = lockfix::now_us();
    while (lockfix::now_us() - t0 < 600'000)
    {
        lockfix::ModScope s(A, lockfix::Policy::Try);
        if (!s.ok()) ++deferred;
        else if (s.forced()) { first_forced_ms = (lockfix::now_us() - t0) / 1000; break; }
        sleep_ms(5);
    }
    release.store(true); holder.join();
    CHECK(deferred > 5 && first_forced_ms >= 140 && first_forced_ms <= 260,
          "owner thread held the mod lock: %d deferrals, then forced after %lld ms (cap 150 ms in the test, 2000 ms in production)", deferred, (long long)first_forced_ms);
    lockfix::ModScope again(A, lockfix::Policy::Try);
    CHECK(again.ok() && !again.forced(), "after the holder released, the next Try gets the real lock and the streak resets");
    lockfix::g_defer_max_us = 2'000'000;
}

static void test_leak_ledger()
{
    std::printf("T6 H ledger detects a leaked guard (report only)\n");
    bool clean_before = true, suspect = false, clean_balanced = true;
    std::thread t([&] {
        clean_before = !lockfix::h_leak_suspect_at_pump();
        { lockfix::HGuard<std::recursive_mutex> ok{oldm::H3}; }
        clean_balanced = !lockfix::h_leak_suspect_at_pump();
        oldm::do_leak_hguard();
        suspect = lockfix::h_leak_suspect_at_pump();
    });
    t.join();
    CHECK(clean_before && clean_balanced, "no false positive on balanced HGuard use");
    CHECK(suspect && lockfix::g.h_leak_suspect.get() == 1, "longjmp out of an HGuard is flagged (leak_suspect=1)");
}

static void test_stats_line()
{
    std::printf("T5 [LOCK] statistics line\n");
    std::vector<std::string> lines;
    auto sink = [&](const char* l) { lines.emplace_back(l); };
    __atomic_store_n(&lockfix::g_next_log_ms, 0, __ATOMIC_RELAXED);
    lockfix::maybe_log(1000, false, sink);
    const size_t first = lines.size();
    lockfix::maybe_log(2000, false, sink);          // within 10 s: suppressed
    CHECK(first >= 1 && lines.size() == first, "rate limit: second call within 10 s printed nothing (%zu line(s) from the first)", first);
    lockfix::maybe_log(12001, false, sink);         // after 10 s: prints again
    CHECK(lines.size() > first, "line printed again after 10 s");
    const size_t before_final = lines.size();
    lockfix::maybe_log(12002, true, sink);          // final bypasses the limiter
    CHECK(lines.size() > before_final && lines[before_final].find("final=1") != std::string::npos, "final line printed regardless of the limiter");
    CHECK(!lines.empty() && lines[0].rfind("keys q=", 0) == 0, "line starts with the declared marker text");
    for (size_t i = 0; i < std::min<size_t>(lines.size(), 3); ++i) std::printf("      [LOCK] %s\n", lines[i].c_str());
    const auto& s = lines[0];
    const char* needles[] = {"keys q=", "d=", "wait_ms max=", "mean=", "hold_max_ms=", "fast skip=", " take=", "eigt q=", " run=", "defer=", "forced=", "leak_suspect="};
    int miss = 0; for (auto* n : needles) if (s.find(n) == std::string::npos) { ++miss; std::printf("      missing field %s\n", n); }
    CHECK(miss == 0, "all required statistics fields present");
}

int main()
{
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::printf("DESIGN-2 host test, LOCKFIX-CORE v1\n");
    test_stuck_key_old_vs_new();
    test_fast_path_and_wakeups();
    test_abba_and_reentrancy();
    test_defer_order_and_fallback_and_tombstone();
    test_defer_cap();
    test_leak_ledger();
    test_stats_line();
    std::printf("checked=%d failed=%d\n", g_checked, g_failed);
    if (g_checked == 0) { std::printf("FAIL: zero checks ran\n"); return 2; }
    std::fflush(stdout);
    _exit(g_failed ? 1 : 0);   // detached repro threads are blocked on purpose
}
