// DESIGN-3 host prototype (Mac-only). Verifies the lock primitives proposed in
// DESIGN-3.md. The block between LOCKCORE-BEGIN and LOCKCORE-END is the exact
// text proposed for LuaMod.cpp (file scope, namespace-private, no new #include).
// Needs only: <chrono> <thread> <cstdint> <cstddef> <cstdio> (all already in the TU).
#include <chrono>
#include <thread>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <csetjmp>
#include <mutex>
#include <vector>
#include <atomic>
#include <cstdlib>

// LOCKCORE-BEGIN
namespace lockcore
{
    using u32 = unsigned int;
    using u64 = unsigned long long;
    using i64 = long long;

    inline uintptr_t self_token() noexcept
    {
        static thread_local char tag;
        return reinterpret_cast<uintptr_t>(&tag);
    }
    inline i64 now_us() noexcept
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    inline void cpu_relax() noexcept
    {
#if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
#elif defined(__aarch64__)
        __asm__ __volatile__("yield");
#endif
    }
    inline void backoff(u32 spins) noexcept
    {
        if (spins < 64) { cpu_relax(); return; }
        if (spins < 2000) { std::this_thread::yield(); return; }
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    inline void max_update(u64* slot, u64 v) noexcept
    {
        u64 cur = __atomic_load_n(slot, __ATOMIC_RELAXED);
        while (v > cur && !__atomic_compare_exchange_n(slot, &cur, v, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
    }
    inline void add(u64* slot, u64 v) noexcept { __atomic_fetch_add(slot, v, __ATOMIC_RELAXED); }
    inline u64 get(const u64* slot) noexcept { return __atomic_load_n(slot, __ATOMIC_RELAXED); }

    // FIFO ticket lock, recursive by owner token. Fields are zero-initialised
    // (usable from static storage, no constructor order issue).
    struct FairLock
    {
        u32 next_ticket{0};   // __atomic only
        u32 now_serving{0};   // __atomic only; written by the owner only
        uintptr_t owner{0};   // __atomic relaxed; 0 = free
        u32 depth{0};         // owner only
        i64 acquired_at_us{0};// owner only

        auto owned_by_me() const noexcept -> bool { return __atomic_load_n(&owner, __ATOMIC_RELAXED) == self_token(); }
        auto my_depth() const noexcept -> u32 { return owned_by_me() ? depth : 0u; }

        auto try_lock() noexcept -> bool
        {
            const uintptr_t me = self_token();
            if (__atomic_load_n(&owner, __ATOMIC_RELAXED) == me) { ++depth; return true; }
            u32 s = __atomic_load_n(&now_serving, __ATOMIC_ACQUIRE);
            u32 expected = s;
            // Succeeds only when nobody holds the lock AND nobody is queued.
            if (!__atomic_compare_exchange_n(&next_ticket, &expected, s + 1, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) return false;
            __atomic_store_n(&owner, me, __ATOMIC_RELAXED);
            depth = 1;
            acquired_at_us = now_us();
            return true;
        }
        auto lock() noexcept -> void
        {
            const uintptr_t me = self_token();
            if (__atomic_load_n(&owner, __ATOMIC_RELAXED) == me) { ++depth; return; }
            const u32 t = __atomic_fetch_add(&next_ticket, 1u, __ATOMIC_RELAXED);
            for (u32 spins = 0; __atomic_load_n(&now_serving, __ATOMIC_ACQUIRE) != t; ++spins) backoff(spins);
            __atomic_store_n(&owner, me, __ATOMIC_RELAXED);
            depth = 1;
            acquired_at_us = now_us();
        }
        // Timed callers never take a ticket (a ticket cannot be abandoned), so they
        // cannot jump the queue: try_lock only succeeds when no one is waiting.
        auto lock_for(i64 budget_us) noexcept -> bool
        {
            if (try_lock()) return true;
            const i64 deadline = now_us() + budget_us;
            for (u32 spins = 0; now_us() < deadline; ++spins)
            {
                if (try_lock()) return true;
                backoff(spins < 2000 ? spins : 63);
            }
            return false;
        }
        auto unlock() noexcept -> void
        {
            if (--depth != 0) return;
            __atomic_store_n(&owner, uintptr_t{0}, __ATOMIC_RELAXED);
            const u32 s = __atomic_load_n(&now_serving, __ATOMIC_RELAXED);
            __atomic_store_n(&now_serving, s + 1, __ATOMIC_RELEASE);
        }
        // Leak repair: release levels a longjmp skipped. Owner only.
        auto reclaim_to(u32 expected_depth) noexcept -> u32
        {
            u32 reclaimed = 0;
            while (owned_by_me() && depth > expected_depth) { unlock(); ++reclaimed; }
            return reclaimed;
        }
    };

    struct Stats
    {
        u64 key_queued{0}, key_dispatched{0}, key_wait_sum_us{0}, key_wait_max_us{0};
        u64 q_takes{0}, q_hold_max_us{0}, fast_skips{0};
        u64 eigt_queued{0}, eigt_run{0};
        u64 defers{0}, escalations{0}, hook_skips{0}, nested_try_fail{0}, leak_reclaimed{0}, order_violations{0};
        u64 game_block_max_us{0}, game_lua_us{0}, game_lua_max_us{0};
    };
    inline Stats g_stats;

    // The shared QUEUE lock. Leaf lock: no Lua, no I/O, no waiting on other
    // threads, no Lua error raised while it is held.
    inline FairLock g_queue_lock;
    struct QueueGuard
    {
        QueueGuard() noexcept { g_queue_lock.lock(); }
        ~QueueGuard() noexcept
        {
            if (g_queue_lock.depth == 1)
            {
                add(&g_stats.q_takes, 1);
                max_update(&g_stats.q_hold_max_us, static_cast<u64>(now_us() - g_queue_lock.acquired_at_us));
            }
            g_queue_lock.unlock();
        }
        QueueGuard(const QueueGuard&) = delete;
        QueueGuard& operator=(const QueueGuard&) = delete;
    };

    struct ModCell
    {
        FairLock lock;
        char name[24]{};
        u32 closed{0};
        u32 consecutive_defers{0};
        u64 wait_max_us{0};
    };

    enum class Policy : unsigned char { Block, GameDrain, KeyEvent, Hook };
    inline thread_local u32 t_held_cells = 0;   // distinct mod locks this thread holds
    inline thread_local bool t_is_game = false; // set from IsInGameThreadRaw() in LuaMod.cpp

    constexpr i64 kKeyWaitUs = 1000, kEscalateWaitUs = 5000, kHookGameWaitUs = 20000, kHookWorkerWaitUs = 5000, kNestedGameWaitUs = 20000;
    constexpr u32 kEscalateAfter = 8;

    class ModLuaEntry
    {
        ModCell* cell_;
        bool held_{false};
        bool outer_{false};
        u32 q_depth_at_entry_{0};
    public:
        ModLuaEntry(ModCell* cell, Policy p) noexcept : cell_(cell)
        {
            q_depth_at_entry_ = g_queue_lock.my_depth();
            if (cell_->lock.owned_by_me()) { cell_->lock.lock(); held_ = true; return; } // recursion
            const bool holds_q = q_depth_at_entry_ != 0;
            const bool holds_other = t_held_cells != 0;
            const i64 t0 = now_us();
            bool ok = false;
            if (holds_q)
            {
                add(&g_stats.order_violations, 1);
                ok = cell_->lock.try_lock();                       // never block under the queue lock
            }
            else if (holds_other)
            {
                ok = t_is_game ? cell_->lock.lock_for(kNestedGameWaitUs) : cell_->lock.try_lock();
                if (!ok) add(&g_stats.nested_try_fail, 1);
            }
            else switch (p)
            {
            case Policy::Block: cell_->lock.lock(); ok = true; break;
            case Policy::GameDrain:
                ok = cell_->lock.try_lock();
                if (!ok && __atomic_load_n(&cell_->consecutive_defers, __ATOMIC_RELAXED) >= kEscalateAfter)
                { add(&g_stats.escalations, 1); ok = cell_->lock.lock_for(kEscalateWaitUs); }
                break;
            case Policy::KeyEvent:
                ok = cell_->lock.lock_for(kKeyWaitUs);
                if (!ok && __atomic_load_n(&cell_->consecutive_defers, __ATOMIC_RELAXED) >= kEscalateAfter)
                { add(&g_stats.escalations, 1); ok = cell_->lock.lock_for(kEscalateWaitUs * 10); }
                break;
            case Policy::Hook:
                ok = cell_->lock.lock_for(t_is_game ? kHookGameWaitUs : kHookWorkerWaitUs);
                if (!ok) add(&g_stats.hook_skips, 1);
                break;
            }
            const u64 waited = static_cast<u64>(now_us() - t0);
            max_update(&cell_->wait_max_us, waited);
            if (t_is_game) max_update(&g_stats.game_block_max_us, waited);
            if (ok && cell_->closed) { cell_->lock.unlock(); ok = false; }
            if (ok) { ++t_held_cells; outer_ = true; __atomic_store_n(&cell_->consecutive_defers, 0u, __ATOMIC_RELAXED); }
            else if (p == Policy::GameDrain || p == Policy::KeyEvent) { __atomic_fetch_add(&cell_->consecutive_defers, 1u, __ATOMIC_RELAXED); add(&g_stats.defers, 1); }
            held_ = ok;
        }
        ~ModLuaEntry() noexcept
        {
            if (!held_) return;
            // A Lua error that longjmp'ed past a QueueGuard leaves its level behind.
            const u32 r = g_queue_lock.reclaim_to(q_depth_at_entry_);
            if (r) add(&g_stats.leak_reclaimed, r);
            if (outer_) --t_held_cells;
            cell_->lock.unlock();
        }
        explicit operator bool() const noexcept { return held_; }
        ModLuaEntry(const ModLuaEntry&) = delete;
        ModLuaEntry& operator=(const ModLuaEntry&) = delete;
    };
}
// LOCKCORE-END

using namespace lockcore;

static int g_checked = 0, g_failed = 0;
#define CHECK(c, ...) do { ++g_checked; if (!(c)) { ++g_failed; std::printf("  FAIL %s:%d %s : ", __FILE__, __LINE__, #c); std::printf(__VA_ARGS__); std::printf("\n"); } else { std::printf("  ok   %s : ", #c); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

// ---------- T1: FIFO fairness and recursion -------------------------------------------
static void t1_basic()
{
    std::printf("T1 basic: recursion, try_lock, FIFO\n");
    FairLock l;
    l.lock(); l.lock();
    CHECK(l.my_depth() == 2, "depth=%u", l.my_depth());
    bool other_try = true;
    std::thread([&] { other_try = l.try_lock(); }).join();
    CHECK(!other_try, "other thread cannot try_lock while held");
    l.unlock(); l.unlock();
    CHECK(l.my_depth() == 0, "released");
    // FIFO order: main holds, 4 threads queue in a known order, expect served in order.
    std::vector<int> order; std::mutex om;
    l.lock();
    std::vector<std::thread> ts;
    std::atomic<int> started{0};
    for (int i = 0; i < 4; ++i)
    {
        ts.emplace_back([&, i] { started.fetch_add(1); l.lock(); { std::lock_guard<std::mutex> g(om); order.push_back(i); } l.unlock(); });
        while (started.load() != i + 1) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(30)); // let it take its ticket
    }
    l.unlock();
    for (auto& t : ts) t.join();
    bool fifo = order.size() == 4; for (int i = 0; i < 4 && fifo; ++i) fifo = order[i] == i;
    CHECK(fifo, "served 0,1,2,3 in ticket order");
}

// ---------- T2: starvation: busy re-taking holder vs a waiter -------------------------
template <class Lock> static u64 starvation_run(const char* label, int ms)
{
    Lock lk; std::atomic<bool> stop{false}; std::atomic<unsigned long long> holder_takes{0};
    std::thread holder([&] { // models the old dispatch scope: hold, work 30us, release, re-take at once
        while (!stop.load()) { lk.lock(); auto e = std::chrono::steady_clock::now() + std::chrono::microseconds(30); while (std::chrono::steady_clock::now() < e) {} lk.unlock(); holder_takes.fetch_add(1); }
    });
    u64 worst = 0, n = 0;
    std::thread waiter([&] {
        auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        while (std::chrono::steady_clock::now() < end) {
            auto t0 = std::chrono::steady_clock::now(); lk.lock(); auto w = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count(); lk.unlock();
            if ((u64)w > worst) worst = (u64)w; ++n;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    waiter.join(); stop = true; holder.join();
    std::printf("  %-22s waiter acquisitions=%llu worst_wait=%llu us  holder_takes=%llu\n", label, (unsigned long long)n, (unsigned long long)worst, holder_takes.load());
    return worst;
}
struct StdRecursive { std::recursive_mutex m; void lock() { m.lock(); } void unlock() { m.unlock(); } };
static void t2_starvation()
{
    std::printf("T2 starvation model (busy holder re-takes immediately; Mac scheduler, not PS5)\n");
    const u64 w_std = starvation_run<StdRecursive>("std::recursive_mutex", 2000);
    const u64 w_fair = starvation_run<FairLock>("FairLock (ticket)", 2000);
    CHECK(w_fair < 5000, "fair worst wait %llu us < 5 ms (std was %llu us)", (unsigned long long)w_fair, (unsigned long long)w_std);
}

// ---------- T3: the stuck key repro: leaked lock level (longjmp) ---------------------
static std::jmp_buf g_jb;
static void raising_binding_with_guard() { QueueGuard g; (void)g; std::longjmp(g_jb, 1); } // models RegisterHook raising under its guard
static void t3_stuck_key()
{
    std::printf("T3 stuck-key repro (game thread leaks a queue-lock level; keybind thread must still get through)\n");
    // OLD behaviour model: std::recursive_mutex, longjmp past lock_guard.
    {
        std::recursive_mutex m; std::atomic<bool> game_done{false}, key_got{false}; std::atomic<bool> release{false};
        std::thread game([&] {
            if (setjmp(g_jb) == 0) { std::lock_guard<std::recursive_mutex> g(m); (void)g; std::longjmp(g_jb, 1); }
            game_done = true; while (!release.load()) { std::lock_guard<std::recursive_mutex> again(m); std::this_thread::sleep_for(std::chrono::milliseconds(1)); } // game keeps re-entering, like the minimap
        });
        while (!game_done.load()) std::this_thread::yield();
        std::thread key([&] { m.lock(); key_got = true; m.unlock(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        CHECK(!key_got.load(), "OLD: keybind thread still blocked 300 ms after the leak (this is the B4 symptom)");
        release = true; game.join();
        // the leaked level remains forever (thread exited owning it) -> keep key thread detached
        key.detach();
    }
    // NEW design: game-thread entry scope reclaims the leaked level at the Lua-call boundary.
    {
        ModCell cell; std::strncpy(cell.name, "minimap", 23);
        std::atomic<bool> key_got{false};
        std::thread game([&] {
            t_is_game = true;
            { ModLuaEntry e{&cell, Policy::GameDrain};
              if (setjmp(g_jb) == 0) raising_binding_with_guard();
              CHECK(g_queue_lock.my_depth() == 1, "inside callback after the raise the queue lock is still leaked (depth=%u)", g_queue_lock.my_depth()); }
            CHECK(g_queue_lock.my_depth() == 0, "after the entry scope ends the leaked level was reclaimed (depth=%u)", g_queue_lock.my_depth());
            CHECK(get(&g_stats.leak_reclaimed) == 1, "leak_reclaimed counter = %llu", (unsigned long long)get(&g_stats.leak_reclaimed));
        });
        game.join();
        std::thread key([&] { ModLuaEntry k{&cell, Policy::KeyEvent}; QueueGuard g; key_got = true; });
        key.join();
        CHECK(key_got.load(), "NEW: keybind thread got mod lock and queue lock");
    }
}

// ---------- T4: ABBA between two mods ---------------------------------------------------
static void t4_abba()
{
    std::printf("T4 ABBA: mod A on game thread fires B's hook while B's async code fires A's hook\n");
    ModCell A, B; std::strncpy(A.name, "A", 23); std::strncpy(B.name, "B", 23);
    std::atomic<int> g_done{0}, x_done{0}; const int N = 300;
    std::thread game([&] { t_is_game = true; for (int i = 0; i < N; ++i) { ModLuaEntry a{&A, Policy::GameDrain}; if (!a) continue; std::this_thread::sleep_for(std::chrono::microseconds(100)); ModLuaEntry b{&B, Policy::Hook}; std::this_thread::sleep_for(std::chrono::microseconds(100)); g_done++; } });
    std::thread asyncB([&] { for (int i = 0; i < N; ++i) { ModLuaEntry b{&B, Policy::Block}; std::this_thread::sleep_for(std::chrono::microseconds(100)); ModLuaEntry a{&A, Policy::Hook}; std::this_thread::sleep_for(std::chrono::microseconds(100)); x_done++; } });
    auto t0 = std::chrono::steady_clock::now();
    game.join(); asyncB.join();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    CHECK(g_done.load() > 0 && x_done.load() == N, "both threads finished (game ok-iterations=%d, async=%d) in %lld ms: no deadlock", g_done.load(), x_done.load(), (long long)ms);
    std::printf("  nested_try_fail=%llu hook_skips=%llu defers=%llu\n", (unsigned long long)get(&g_stats.nested_try_fail), (unsigned long long)get(&g_stats.hook_skips), (unsigned long long)get(&g_stats.defers));
    // Control: the same pattern with plain blocking locks deadlocks. Detected with a
    // polling deadline on std::mutex (no timed_mutex: libc++ timed waits hung once in testing).
    std::mutex ma, mb; std::atomic<int> stuck{0};
    auto poll_lock = [](std::mutex& m, int ms) { const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms); while (!m.try_lock()) { if (std::chrono::steady_clock::now() > end) return false; std::this_thread::yield(); } return true; };
    std::thread c1([&] { for (int i = 0; i < N; ++i) { ma.lock(); std::this_thread::sleep_for(std::chrono::microseconds(100)); if (!poll_lock(mb, 200)) { stuck++; ma.unlock(); return; } mb.unlock(); ma.unlock(); } });
    std::thread c2([&] { for (int i = 0; i < N; ++i) { mb.lock(); std::this_thread::sleep_for(std::chrono::microseconds(100)); if (!poll_lock(ma, 200)) { stuck++; mb.unlock(); return; } ma.unlock(); mb.unlock(); } });
    c1.join(); c2.join();
    CHECK(stuck.load() > 0, "control (plain blocking ABBA) does deadlock, stuck threads=%d: the test is sensitive", stuck.load());
}

// ---------- T5: fast path cost ----------------------------------------------------------
static u32 g_dirty = 0;
static void t5_fastpath()
{
    std::printf("T5 fast path cost per engine event (single thread, arm64 Mac, -O2)\n");
    const int N = 20'000'000; std::recursive_mutex m; volatile unsigned sink = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) { for (int k = 0; k < 6; ++k) { std::lock_guard<std::recursive_mutex> g(m); sink = sink + 1; } } // old: scope + flag + 3 drains + merge
    auto t1 = std::chrono::steady_clock::now();
    volatile u32* dirty_slot = &g_dirty; // opaque to the optimiser, like a flag another thread may set
    for (int i = 0; i < N; ++i) { if (__atomic_load_n(dirty_slot, __ATOMIC_ACQUIRE) != 0) { QueueGuard q; sink = sink + 1; } else { sink = sink + 0; } }
    auto t2 = std::chrono::steady_clock::now();
    const double old_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / N, new_ns = std::chrono::duration<double, std::nano>(t2 - t1).count() / N;
    std::printf("  old: %.1f ns/event (6 recursive_mutex takes)   new: %.2f ns/event (1 atomic load)\n", old_ns, new_ns);
    CHECK(new_ns < old_ns, "fast path cheaper (sink=%u)", (unsigned)sink);
}


// ---------- T6: fast-path flag, no lost wake-up ----------------------------------------
static u32 g_q_dirty = 0;
static std::vector<int> g_q_items;
static void t6_lost_wakeup()
{
    std::printf("T6 fast path flag: producers enqueue under the queue lock and mark dirty; the consumer polls the flag lock-free\n");
    const int P = 3, PER = 100000; std::atomic<int> producers_done{0}; long long consumed = 0, skips = 0, takes = 0;
    std::vector<std::thread> prod;
    for (int p = 0; p < P; ++p) prod.emplace_back([&] {
        for (int i = 0; i < PER; ++i) { { QueueGuard q; g_q_items.push_back(i); __atomic_store_n(&g_q_dirty, 1u, __ATOMIC_RELEASE); } if ((i & 255) == 0) std::this_thread::yield(); }
        producers_done.fetch_add(1);
    });
    const auto start = std::chrono::steady_clock::now(); bool timed_out = false;
    while (consumed < (long long)P * PER)
    {
        if (__atomic_load_n(&g_q_dirty, __ATOMIC_ACQUIRE) == 0) { ++skips; if (std::chrono::steady_clock::now() - start > std::chrono::seconds(20)) { timed_out = true; break; } continue; }
        std::vector<int> mine;
        { QueueGuard q; mine.swap(g_q_items); __atomic_store_n(&g_q_dirty, g_q_items.empty() ? 0u : 1u, __ATOMIC_RELEASE); } // exact recompute under the lock
        ++takes; consumed += (long long)mine.size();
    }
    for (auto& t : prod) t.join();
    CHECK(!timed_out && consumed == (long long)P * PER, "all %lld items consumed, none lost (consumer queue-lock takes=%lld, lock-free skips=%lld)", consumed, takes, skips);
}

int main()
{
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    t1_basic(); t2_starvation(); t3_stuck_key(); t4_abba(); t5_fastpath(); t6_lost_wakeup();
    std::printf("checked=%d failed=%d\n", g_checked, g_failed);
    return g_failed == 0 && g_checked > 0 ? 0 : 1;
}
