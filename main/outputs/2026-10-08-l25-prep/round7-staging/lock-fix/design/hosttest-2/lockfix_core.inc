// ===== BEGIN LOCKFIX-CORE v1 (DESIGN-2 prototype, Mac-only) =====================
// Production target: candidate/src/LuaMod.cpp, inside namespace RC, ahead of the
// L23GameThreadProbe block. Depends only on <mutex> <vector> <chrono> <thread>
// <cstdio> <cstdint>, all of which LuaMod.cpp already includes (directly or via
// Mod/LuaMod.hpp). No <atomic>: compiler __atomic builtins only (LmsRelaxedCounter
// pattern, LuaMadeSimple.cpp:22-28). The host test includes this text unchanged.
//
// LOCK ORDER (document verbatim in the source):
//   ModScope (M, per mod)  ->  H (LuaMod::m_thread_actions_mutex, registry ops)  ->  Q (g_q, leaf)
//   * Q is a leaf: nothing is acquired, logged, allocated-with-throw-handling-skipped,
//     or called into Lua while Q is held.
//   * Nobody acquires M while holding H or Q.
//   * A thread that already holds one M acquires another M only with a bounded wait
//     (kCrossUs) and falls back (Try: defer, Bounded/Block: force). So no wait-for cycle
//     can contain an unbounded edge from a thread that holds an M.
//   * No guard in this block may be alive when a Lua error is raised (longjmp on PS5).
namespace lockfix
{
    constexpr uint32_t W_SIMPLE = 1u;   // LuaMod::m_game_thread_actions (+ pending)
    constexpr uint32_t W_ENGINE = 2u;   // LuaMod::m_engine_tick_actions (+ pending)
    constexpr uint32_t W_KEYS   = 4u;   // g_keys
    constexpr int64_t  kKeyCap          = 256;
    constexpr int64_t  kRetryMs         = 2;       // backoff after a deferral
    constexpr int64_t  kKeyFallbackUs   = 400'000; // key older than this and undrained -> mod async thread runs it
    constexpr int64_t  kBlockUs         = 5'000'000;
    constexpr int64_t  kNoDue           = INT64_MAX;
    inline int64_t     g_defer_max_us   = 2'000'000;   // a Try entry that has been deferred this long (continuously) is forced

    inline int64_t now_us() noexcept
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    inline int64_t now_ms() noexcept { return now_us() / 1000; }

    struct Ctr
    {
        uint64_t v{0};
        void add(uint64_t n = 1) noexcept { __atomic_fetch_add(&v, n, __ATOMIC_RELAXED); }
        void bump_single_writer() noexcept { __atomic_store_n(&v, __atomic_load_n(&v, __ATOMIC_RELAXED) + 1, __ATOMIC_RELAXED); }
        uint64_t get() const noexcept { return __atomic_load_n(&v, __ATOMIC_RELAXED); }
        void max_in(uint64_t x) noexcept
        {
            uint64_t c = __atomic_load_n(&v, __ATOMIC_RELAXED);
            while (x > c && !__atomic_compare_exchange_n(&v, &c, x, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
        }
        void reset() noexcept { __atomic_store_n(&v, 0, __ATOMIC_RELAXED); }
    };

    struct Stats
    {
        Ctr key_q, key_d, key_wait_sum_us, key_wait_max_us, key_fb, key_drop, key_cb_max_us, key_slow;
        Ctr q_takes, q_contended, q_hold_max_us, q_wait_max_us;
        Ctr fp_skip, fp_take;                         // single writer: game thread
        Ctr eigt_q, eigt_run, dly_q, dly_run;
        Ctr m_defer, m_forced, m_unwrapped, m_stale_drop, h_leak_suspect;
        Ctr drains, drain_us_max, drain_win_us, gap_max_us;   // game thread
    };
    inline Stats g;

    // ---- queue lock Q (leaf) ----------------------------------------------------
    inline std::mutex g_q;
    inline uint32_t   g_bits = 0;                 // writes only under Q; lock-free reads
    inline int64_t    g_due_ms[3] = {kNoDue, kNoDue, kNoDue};   // per executor: 0=ProcessEvent 1=EngineTick 2=Marshal
    inline int64_t    g_retry_until_ms = 0;
    inline int64_t    g_last_pump_us = 0;

    struct QGuard
    {
        int64_t t1{};
        QGuard() noexcept
        {
            const int64_t t0 = now_us();
            if (!g_q.try_lock())
            {
                g.q_contended.add();
                g_q.lock();
            }
            t1 = now_us();
            g.q_wait_max_us.max_in(static_cast<uint64_t>(t1 - t0));
            g.q_takes.add();
        }
        ~QGuard() noexcept
        {
            g.q_hold_max_us.max_in(static_cast<uint64_t>(now_us() - t1));
            g_q.unlock();
        }
        QGuard(const QGuard&) = delete;
        QGuard& operator=(const QGuard&) = delete;
    };

    // bits are a cache of "queue non-empty". They are recomputed from the queues under Q on
    // every mutation, so they cannot drift and cannot lose a wakeup (invariant I: at every
    // Q release, g_bits == f(queues)).
    inline void bits_set_locked(uint32_t m, bool on) noexcept
    {
        const uint32_t b = on ? (g_bits | m) : (g_bits & ~m);
        __atomic_store_n(&g_bits, b, __ATOMIC_RELEASE);
    }
    inline void due_set_locked(int method, int64_t due_ms) noexcept { __atomic_store_n(&g_due_ms[method], due_ms, __ATOMIC_RELEASE); }
    inline void due_min_locked(int method, int64_t due_ms) noexcept
    {
        if (due_ms < g_due_ms[method]) due_set_locked(method, due_ms);
    }
    inline void due_unknown_locked() noexcept { for (int m = 0; m < 3; ++m) due_set_locked(m, 0); }  // next event of that pump re-derives it
    inline void retry_backoff(int64_t now_ms_) noexcept { __atomic_store_n(&g_retry_until_ms, now_ms_ + kRetryMs, __ATOMIC_RELAXED); }

    // THE FAST PATH: zero locks, zero allocation, three relaxed/acquire loads.
    inline bool pump_should_drain(uint32_t mask, int method, int64_t now_ms_) noexcept
    {
        if (now_ms_ < __atomic_load_n(&g_retry_until_ms, __ATOMIC_RELAXED)) return false;
        if (__atomic_load_n(&g_bits, __ATOMIC_ACQUIRE) & mask) return true;
        return now_ms_ >= __atomic_load_n(&g_due_ms[method], __ATOMIC_ACQUIRE);
    }
    // liveness + gap statistics; single writer (game thread); called on every pump event
    inline void note_pump(int64_t t_us) noexcept
    {
        const int64_t last = __atomic_load_n(&g_last_pump_us, __ATOMIC_RELAXED);
        if (last != 0 && t_us - last > 0) g.gap_max_us.max_in(static_cast<uint64_t>(t_us - last));
        __atomic_store_n(&g_last_pump_us, t_us, __ATOMIC_RELAXED);
    }
    inline int64_t last_pump_us() noexcept { return __atomic_load_n(&g_last_pump_us, __ATOMIC_RELAXED); }

    struct DrainTimer
    {
        int64_t t0{now_us()};
        ~DrainTimer() noexcept
        {
            const uint64_t d = static_cast<uint64_t>(now_us() - t0);
            g.drains.bump_single_writer();
            g.drain_win_us.add(d);
            g.drain_us_max.max_in(d);
        }
    };

    // ---- H ledger: replaces std::lock_guard<std::recursive_mutex> on LuaMod::m_thread_actions_mutex.
    // A Lua error raised (longjmp on PS5) while an HGuard is alive skips ~HGuard, so tl_h_depth stays
    // non-zero on that thread. The game-thread pump prologue checks it (detect and report only; the
    // design never "heals" by unlocking, because a wrong unlock is undefined behaviour).
    inline thread_local int tl_h_depth = 0;
    template <class Mu>
    struct HGuard
    {
        Mu& m_;
        explicit HGuard(Mu& m) : m_(m) { m_.lock(); ++tl_h_depth; }
        ~HGuard() { --tl_h_depth; m_.unlock(); }
        HGuard(const HGuard&) = delete;
        HGuard& operator=(const HGuard&) = delete;
    };
    inline bool h_leak_suspect_at_pump() noexcept
    {
        if (tl_h_depth == 0) return false;
        g.h_leak_suspect.add();
        return true;
    }

    // ---- per-mod Lua locks M ------------------------------------------------------
    struct ModEntry
    {
        std::recursive_mutex mu;
        char   name[48]{};
        bool   alive{true};
        int64_t defer_since_us{0};          // start of the current deferral streak (0 = none)
        Ctr    acq, contended, wait_max_us, hold_max_us, defer, forced;
    };
    inline std::mutex g_tab_mu;                                   // leaf, tiny
    inline std::vector<std::pair<const void*, ModEntry*>> g_tab;  // never shrinks; entries never freed
    inline ModEntry* mod_find(const void* key) noexcept
    {
        std::lock_guard<std::mutex> lk{g_tab_mu};
        for (auto& p : g_tab) if (p.first == key) return p.second;
        return nullptr;
    }
    inline ModEntry* mod_register(const void* key, const char* name) noexcept
    {
        try
        {
            std::lock_guard<std::mutex> lk{g_tab_mu};
            for (auto& p : g_tab) if (p.first == key) { p.second->alive = true; return p.second; }
            auto* e = new ModEntry{};
            for (size_t i = 0; name && name[i] && i + 1 < sizeof(e->name); ++i) e->name[i] = name[i];
            g_tab.emplace_back(key, e);
            return e;
        }
        catch (...) { return nullptr; }
    }
    inline void mod_alias(const void* extra_key, ModEntry* e) noexcept
    {
        if (!e) return;
        try
        {
            std::lock_guard<std::mutex> lk{g_tab_mu};
            for (auto& p : g_tab) if (p.first == extra_key) { p.second = e; return; }
            g_tab.emplace_back(extra_key, e);
        }
        catch (...) {}
    }
    inline void mod_tombstone(ModEntry* e) noexcept { if (e) __atomic_store_n(&e->alive, false, __ATOMIC_RELEASE); }

    inline int64_t g_cross_us = 10'000;           // bound for acquiring a 2nd mod's lock (tests may lower it)
    inline thread_local ModEntry* tl_held[8];
    inline thread_local int tl_held_n = 0;
    inline bool tl_holds(const ModEntry* e) noexcept { for (int i = 0; i < tl_held_n; ++i) if (tl_held[i] == e) return true; return false; }
    inline int tl_distinct_held() noexcept { return tl_held_n; }

    enum class Policy : uint8_t
    {
        Try,      // queue-run entries: one try, never wait; !ok() => caller defers the item (forced after g_defer_max_us of continuous deferral)
        Bounded,  // hook/async-key entries that cannot be deferred: wait up to wait_us, then FORCE (run unlocked)
        Block     // start_mod / uninstall / probe: wait up to kBlockUs, then FORCE and count
    };

    class ModScope
    {
        ModEntry* e_{};
        bool locked_{};
        bool pushed_{};
        bool ok_{};
        bool forced_{};
        int64_t t_acq_{};
    public:
        ModScope(ModEntry* e, Policy p, int64_t wait_us = 0) noexcept
        {
            if (!e) { ok_ = true; return; }                                    // unmanaged state: behave like upstream
            if (!__atomic_load_n(&e->alive, __ATOMIC_ACQUIRE)) { ok_ = false; g.m_stale_drop.add(); return; }
            int64_t budget = (p == Policy::Try) ? 0 : (p == Policy::Bounded ? wait_us : kBlockUs);
            if (tl_held_n > 0 && !tl_holds(e) && budget > g_cross_us) budget = g_cross_us;   // rule: holder of one M never waits long for another
            const int64_t t0 = now_us();
            bool waited = false;
            for (;;)
            {
                if (e->mu.try_lock()) { locked_ = true; break; }
                if (!waited) { waited = true; e->contended.add(); }
                if (now_us() - t0 >= budget) break;
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            t_acq_ = now_us();
            e->wait_max_us.max_in(static_cast<uint64_t>(t_acq_ - t0));
            e_ = e;
            if (locked_)
            {
                __atomic_store_n(&e->defer_since_us, 0, __ATOMIC_RELAXED);
                e->acq.add();
                if (tl_held_n < 8) { tl_held[tl_held_n++] = e; pushed_ = true; }
                ok_ = true;
            }
            else if (p == Policy::Try)
            {
                // Liveness guarantee: no queue-run item is held back longer than g_defer_max_us by M
                // (covers a leaked M and a mod that spin-waits on its own closure).
                const int64_t since = __atomic_load_n(&e->defer_since_us, __ATOMIC_RELAXED);
                if (since == 0) __atomic_store_n(&e->defer_since_us, t_acq_, __ATOMIC_RELAXED);
                if (since != 0 && t_acq_ - since > g_defer_max_us) { e->forced.add(); g.m_forced.add(); forced_ = true; ok_ = true; }
                else { e->defer.add(); g.m_defer.add(); ok_ = false; }
            }
            else { e->forced.add(); g.m_forced.add(); forced_ = true; ok_ = true; }
        }
        ~ModScope() noexcept
        {
            if (!locked_) return;
            e_->hold_max_us.max_in(static_cast<uint64_t>(now_us() - t_acq_));
            if (pushed_ && tl_held_n > 0) --tl_held_n;
            e_->mu.unlock();
        }
        ModScope(const ModScope&) = delete;
        ModScope& operator=(const ModScope&) = delete;
        bool ok() const noexcept { return ok_; }
        bool forced() const noexcept { return forced_; }
    };

    // ---- key queue: input thread only ever calls key_enqueue ------------------------
    struct KeyEv
    {
        const void* mod;      // LuaMod*
        const void* lua;      // const LuaMadeSimple::Lua* (state that registered the bind)
        int32_t     ref;      // registry ref of the Lua callback (already created by RegisterKeyBind)
        int32_t     key;
        int64_t     t_us;
    };
    inline std::vector<KeyEv> g_keys;   // guarded by Q

    inline void publish_keys_locked() noexcept { bits_set_locked(W_KEYS, !g_keys.empty()); }

    inline void key_enqueue(KeyEv ev) noexcept          // never raises, never touches Lua, never waits on anything but Q
    {
        ev.t_us = now_us();
        try
        {
            QGuard q;
            if (static_cast<int64_t>(g_keys.size()) >= kKeyCap) { g_keys.erase(g_keys.begin()); g.key_drop.add(); }
            g_keys.push_back(ev);
            publish_keys_locked();
        }
        catch (...) { g.key_drop.add(); return; }
        g.key_q.add();
    }

    // Pops matching events under Q, runs them OUTSIDE Q, re-queues the ones run() declined.
    // run(ev) must return true if it executed the callback (Lua), false to defer.
    template <class Run>
    inline uint32_t drain_keys(Run&& run, int64_t min_age_us, const void* only_mod) noexcept
    {
        std::vector<KeyEv> batch;
        try
        {
            QGuard q;
            if (g_keys.empty()) return 0;
            const int64_t t = now_us();
            std::vector<KeyEv> keep;
            keep.reserve(g_keys.size());
            for (auto& ev : g_keys)
            {
                const bool take = (only_mod == nullptr || ev.mod == only_mod) && (t - ev.t_us) >= min_age_us;
                (take ? batch : keep).push_back(ev);
            }
            if (batch.empty()) return 0;
            g_keys.swap(keep);
            publish_keys_locked();
        }
        catch (...) { return 0; }

        std::vector<KeyEv> back;
        uint32_t n = 0;
        for (auto& ev : batch)
        {
            const uint64_t waited = static_cast<uint64_t>(now_us() - ev.t_us);
            bool ran = false;
            try { ran = run(ev); } catch (...) { ran = true; }
            if (ran)
            {
                ++n;
                g.key_d.add();
                g.key_wait_sum_us.add(waited);
                g.key_wait_max_us.max_in(waited);
            }
            else { try { back.push_back(ev); } catch (...) { g.key_drop.add(); } }
        }
        if (!back.empty())
        {
            try
            {
                QGuard q;
                g_keys.insert(g_keys.begin(), back.begin(), back.end());
                publish_keys_locked();
            }
            catch (...) { g.key_drop.add(back.size()); }
            retry_backoff(now_ms());
        }
        return n;
    }

    // ---- rate-limited [LOCK] statistics ------------------------------------------
    inline int64_t g_next_log_ms = 0;
    inline int64_t g_log_start_ms = 0;
    template <class Sink>
    inline void maybe_log(int64_t t_ms, bool final_line, Sink&& sink) noexcept
    {
        int64_t nxt = __atomic_load_n(&g_next_log_ms, __ATOMIC_RELAXED);
        if (!final_line)
        {
            if (t_ms < nxt) return;
            if (!__atomic_compare_exchange_n(&g_next_log_ms, &nxt, t_ms + 10'000, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) return;
        }
        char line[768];
        const uint64_t kd = g.key_d.get();
        const uint64_t wmax = g.key_wait_max_us.get();
        const uint64_t wmean = kd ? g.key_wait_sum_us.get() / kd : 0;
        const uint64_t hold = g.q_hold_max_us.get();
        const uint64_t win = g.drain_win_us.get();
        __atomic_store_n(&g.drain_win_us.v, 0, __ATOMIC_RELAXED);   // window counter: reset per line
        std::snprintf(line, sizeof(line),
                      "keys q=%llu d=%llu drop=%llu fb=%llu slow=%llu wait_ms max=%llu.%03llu mean=%llu.%03llu cb_max_ms=%llu.%03llu"
                      " | qlock takes=%llu contended=%llu hold_max_ms=%llu.%03llu wait_max_ms=%llu.%03llu"
                      " | fast skip=%llu take=%llu"
                      " | eigt q=%llu run=%llu dly q=%llu run=%llu"
                      " | defer=%llu forced=%llu unwrapped=%llu stale=%llu leak_suspect=%llu"
                      " | pump gap_max_ms=%llu drain_win_ms=%llu drain_max_ms=%llu%s",
                      (unsigned long long)g.key_q.get(), (unsigned long long)kd, (unsigned long long)g.key_drop.get(),
                      (unsigned long long)g.key_fb.get(), (unsigned long long)g.key_slow.get(),
                      (unsigned long long)(wmax / 1000), (unsigned long long)(wmax % 1000),
                      (unsigned long long)(wmean / 1000), (unsigned long long)(wmean % 1000),
                      (unsigned long long)(g.key_cb_max_us.get() / 1000), (unsigned long long)(g.key_cb_max_us.get() % 1000),
                      (unsigned long long)g.q_takes.get(), (unsigned long long)g.q_contended.get(),
                      (unsigned long long)(hold / 1000), (unsigned long long)(hold % 1000),
                      (unsigned long long)(g.q_wait_max_us.get() / 1000), (unsigned long long)(g.q_wait_max_us.get() % 1000),
                      (unsigned long long)g.fp_skip.get(), (unsigned long long)g.fp_take.get(),
                      (unsigned long long)g.eigt_q.get(), (unsigned long long)g.eigt_run.get(),
                      (unsigned long long)g.dly_q.get(), (unsigned long long)g.dly_run.get(),
                      (unsigned long long)g.m_defer.get(), (unsigned long long)g.m_forced.get(),
                      (unsigned long long)g.m_unwrapped.get(), (unsigned long long)g.m_stale_drop.get(), (unsigned long long)g.h_leak_suspect.get(),
                      (unsigned long long)(g.gap_max_us.get() / 1000), (unsigned long long)(win / 1000),
                      (unsigned long long)(g.drain_us_max.get() / 1000), final_line ? " final=1" : "");
        sink(line);
        // per-mod lines (copy the pointer list under the tiny leaf lock, format outside it)
        ModEntry* snap[32];
        int n = 0;
        {
            std::lock_guard<std::mutex> lk{g_tab_mu};
            for (auto& p : g_tab)
            {
                bool dup = false;
                for (int i = 0; i < n; ++i) if (snap[i] == p.second) { dup = true; break; }
                if (!dup && n < 32) snap[n++] = p.second;
            }
        }
        for (int i = 0; i < n; ++i)
        {
            const ModEntry* e = snap[i];
            char ml[256];
            const uint64_t w = e->wait_max_us.get(), h = e->hold_max_us.get();
            std::snprintf(ml, sizeof(ml), "mod=%s lua_lock wait_max_ms=%llu.%03llu hold_max_ms=%llu.%03llu acq=%llu contended=%llu defer=%llu forced=%llu",
                          e->name, (unsigned long long)(w / 1000), (unsigned long long)(w % 1000),
                          (unsigned long long)(h / 1000), (unsigned long long)(h % 1000),
                          (unsigned long long)e->acq.get(), (unsigned long long)e->contended.get(),
                          (unsigned long long)e->defer.get(), (unsigned long long)e->forced.get());
            sink(ml);
        }
    }
}
// ===== END LOCKFIX-CORE v1 ========================================================
