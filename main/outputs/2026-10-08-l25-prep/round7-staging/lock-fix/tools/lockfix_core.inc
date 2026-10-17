// ===== BEGIN LOCKFIX-CORE v2 (B5 lock fix; Mac-host-tested block) ===================
// This text is inserted verbatim into namespace RC of LuaMod.cpp and is also compiled
// unchanged by the host test (host-test/). It depends only on <mutex> <vector> <chrono>
// <thread> <cstdio> <cstdint> <limits>, all of which LuaMod.cpp already reaches. NO <atomic>:
// the compiler's __atomic builtins only (accepted pattern: LmsRelaxedCounter in
// LuaMadeSimple.cpp). No Unreal, Lua or LuaMod type appears in this block.
//
// WHY (B4 launch 1, 2026-10-12): one process-wide std::recursive_mutex
// (LuaMod::m_thread_actions_mutex = "H") was the only serializer of Lua state. The game
// thread re-took it on every carrier call and ran all game-thread Lua under it; the
// RegisterKeyBind callback (input thread) and the async threads then waited on it and
// never got through. Whether H was starved or leaked (a Lua error is a longjmp on this
// port and skips lock_guard destructors), the cure is the same: nothing that must stay
// responsive may wait on H.
//
// THREE LOCKS, FIXED ORDER   M (per mod, recursive)  ->  H (registry work)  ->  Q (leaf)
//   Q  g_q: pending-work queues + key queue + work bits. Leaf: nothing is called, logged,
//      or raised while Q is held; only vector pushes/swaps. Held for microseconds.
//   H  LuaMod::m_thread_actions_mutex: hook/registry containers only. Never held across a
//      pump drain. Taken through HGuard (ledger) so a leaked level can be repaired.
//   M  ModEntry::mu: exclusive right to run one mod's Lua. Taken through ModScope in C++
//      frames that sit ABOVE lua_pcall, so a Lua error (longjmp) cannot jump over them.
//   * Nobody waits for M while holding H or Q. A thread that already holds one M (or H)
//     waits at most g_cross_us for another M, then defers (Try) or forces (Bounded/Block),
//     so no wait-for cycle can contain an unbounded edge: ABBA cannot deadlock.
//   * Nothing in this block may be alive when a Lua error is raised, except through the
//     ledger repair below (guards unwind leaked levels of the same thread at scope exit).
namespace lockfix
{
    constexpr uint32_t W_SIMPLE = 1u;   // LuaMod::m_game_thread_actions (+ pending)
    constexpr uint32_t W_ENGINE = 2u;   // LuaMod::m_engine_tick_actions (+ pending)
    constexpr uint32_t W_KEYS   = 4u;   // g_keys
    constexpr uint32_t W_NOTIFY = 8u;   // LuaMod::m_pending_notify_on_new_object_callbacks (set/cleared by its H owner)
    constexpr int64_t  kKeyCap   = 256;
    constexpr int64_t  kRetryMs  = 2;                   // pump back-off after a deferral
    constexpr int64_t  kBlockUs  = 5'000'000;           // longest Block wait
    constexpr int64_t  kNoDue    = std::numeric_limits<int64_t>::max();
    inline int64_t g_key_fallback_us = 400'000;         // undrained key older than this is run by its mod's async thread
    inline int64_t g_defer_max_us    = 2'000'000;       // a Try entry deferred this long (continuously) is forced
    inline int64_t g_cross_us        = 10'000;          // longest wait for a 2nd lock while holding a first

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
        uint64_t take() noexcept { return __atomic_exchange_n(&v, 0, __ATOMIC_RELAXED); }
        void reset() noexcept { __atomic_store_n(&v, 0, __ATOMIC_RELAXED); }
    };

    struct Stats
    {
        Ctr key_q, key_d, key_wait_sum_us, key_wait_max_us, key_fb, key_drop, key_stale, key_cb_max_us, key_slow;
        Ctr q_takes, q_contended, q_hold_max_us, q_wait_max_us;
        Ctr fp_skip, fp_take;                                    // single writer: game thread
        Ctr eigt_q, eigt_run, dly_q, dly_run;
        Ctr m_defer, m_forced, m_unwrapped, m_stale_drop, m_nomod;
        Ctr h_repaired, m_repaired, led_overflow;                // ledger repairs (a Lua error skipped a guard)
        Ctr drains, drain_us_max, drain_win_us, gap_max_us;      // game thread
        Ctr gt_lua_us, gt_block_us, act_max_us, rearm_n, rearm_max_us;
        const void* act_max_ent{nullptr};                        // ModEntry* of the longest action (entries are never freed)
    };
    inline Stats g;

    // ---- queue lock Q (leaf) ---------------------------------------------------------
    inline std::mutex g_q;
    inline uint32_t   g_bits = 0;                                    // writes only under Q; lock-free reads
    inline int64_t    g_due_ms[3] = {kNoDue, kNoDue, kNoDue};       // per executor: 0=ProcessEvent 1=EngineTick 2=Marshal
    inline int64_t    g_retry_until_ms = 0;
    inline int64_t    g_last_pump_us = 0;
    inline thread_local bool tl_is_game = false;                     // set by the pump gate: this thread is the pumping (game) thread

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
            const uint64_t w = static_cast<uint64_t>(t1 - t0);
            g.q_wait_max_us.max_in(w);
            if (tl_is_game && w != 0) g.gt_block_us.add(w);
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

    // ---- per-mod Lua locks M -----------------------------------------------------------
    struct ModEntry
    {
        std::recursive_mutex mu;
        char    name[48]{};
        bool    alive{true};
        int64_t defer_since_us{0};          // start of the current deferral streak (0 = none)
        Ctr     acq, contended, wait_max_us, hold_max_us, defer, forced;
    };
    inline std::mutex g_tab_mu;                                   // leaf, tiny
    inline std::vector<std::pair<const void*, ModEntry*>> g_tab;  // never shrinks; entries never freed
    inline ModEntry* mod_find(const void* key) noexcept
    {
        if (!key) return nullptr;
        std::lock_guard<std::mutex> lk{g_tab_mu};
        for (auto& p : g_tab) if (p.first == key) return p.second;
        return nullptr;
    }
    inline ModEntry* mod_register(const void* key, const char* name) noexcept
    {
        try
        {
            std::lock_guard<std::mutex> lk{g_tab_mu};
            for (auto& p : g_tab) if (p.first == key) { __atomic_store_n(&p.second->alive, true, __ATOMIC_RELEASE); return p.second; }
            auto* e = new ModEntry{};
            for (size_t i = 0; name && name[i] && i + 1 < sizeof(e->name); ++i) e->name[i] = name[i];
            g_tab.emplace_back(key, e);
            return e;
        }
        catch (...) { return nullptr; }
    }
    inline void mod_alias(const void* extra_key, ModEntry* e) noexcept     // e.g. LuaMod::m_hook_lua -> its mod
    {
        if (!e || !extra_key) return;
        try
        {
            std::lock_guard<std::mutex> lk{g_tab_mu};
            for (auto& p : g_tab) if (p.first == extra_key) { p.second = e; return; }
            g_tab.emplace_back(extra_key, e);
        }
        catch (...) {}
    }
    inline void mod_tombstone(ModEntry* e) noexcept { if (e) __atomic_store_n(&e->alive, false, __ATOMIC_RELEASE); }

    // ---- lock ledger: makes a skipped guard (Lua error = longjmp on this port) repairable ----
    // Every HGuard / ModScope records the mutex it holds in a per-thread stack. A Lua error
    // raised inside such a scope skips the guard's destructor, so its entry stays on the
    // stack above the guard that encloses the pcall. When that enclosing guard (or a
    // LedgerMark) exits normally it unlocks the leaked levels, on the owning thread, so the
    // leak ends where the Lua call that caused it returns (counted: h_repaired / m_repaired).
    enum class LKind : uint8_t { H = 0, M = 1 };
    struct LedgerEnt { void* mu; ModEntry* ent; LKind kind; };
    constexpr int kLedgerMax = 24;
    inline thread_local LedgerEnt tl_led[kLedgerMax];
    inline thread_local int tl_led_n = 0;

    inline int led_push(void* mu, ModEntry* ent, LKind k) noexcept
    {
        if (tl_led_n >= kLedgerMax) { g.led_overflow.add(); return -1; }
        tl_led[tl_led_n] = LedgerEnt{mu, ent, k};
        return tl_led_n++;
    }
    inline void led_unlock_ent(const LedgerEnt& e) noexcept
    {
        if (e.kind == LKind::H) { static_cast<std::recursive_mutex*>(e.mu)->unlock(); g.h_repaired.add(); }
        else { e.ent->mu.unlock(); g.m_repaired.add(); }
    }
    // Release the guard's own slot idx; first unlock whatever a skipped inner guard left above it.
    inline void led_release(int idx) noexcept
    {
        if (idx < 0) return;
        while (tl_led_n > idx + 1) led_unlock_ent(tl_led[--tl_led_n]);
        if (tl_led_n == idx + 1) --tl_led_n;
    }
    inline bool led_holds_m(const ModEntry* e) noexcept
    {
        for (int i = 0; i < tl_led_n; ++i) if (tl_led[i].kind == LKind::M && tl_led[i].ent == e) return true;
        return false;
    }
    inline bool led_has_other_hold(const ModEntry* e) noexcept        // any H, or any M other than e
    {
        for (int i = 0; i < tl_led_n; ++i) if (tl_led[i].kind == LKind::H || tl_led[i].ent != e) return true;
        return false;
    }
    inline int led_depth() noexcept { return tl_led_n; }

    // Scope marker for a pump drain: at exit, unlocks anything the drain's Lua calls leaked.
    struct LedgerMark
    {
        int mark{tl_led_n};
        ~LedgerMark() noexcept { while (tl_led_n > mark) led_unlock_ent(tl_led[--tl_led_n]); }
        LedgerMark(const LedgerMark&) = delete;
        LedgerMark& operator=(const LedgerMark&) = delete;
        LedgerMark() = default;
    };

    // H = LuaMod::m_thread_actions_mutex. Replaces std::lock_guard / std::unique_lock on it.
    class HGuard
    {
        std::recursive_mutex& m_;
        int led_;
    public:
        explicit HGuard(std::recursive_mutex& m) : m_(m), led_(-1)
        {
            m_.lock();
            led_ = led_push(&m_, nullptr, LKind::H);
        }
        ~HGuard() noexcept
        {
            led_release(led_);
            m_.unlock();
        }
        HGuard(const HGuard&) = delete;
        HGuard& operator=(const HGuard&) = delete;
    };

    enum class Policy : uint8_t
    {
        Try,      // queue-run entries: one try, never wait; Busy => caller defers the item (forced after g_defer_max_us of continuous deferral)
        Bounded,  // hook / async-key entries that cannot be deferred: wait up to wait_us, then FORCE (run unlocked, counted)
        Block     // start_mod / uninstall / probe: wait up to kBlockUs, then FORCE (counted)
    };
    enum class Res : uint8_t { Ok, Busy, Stale };

    class ModScope
    {
        ModEntry* e_{};
        bool locked_{};
        bool forced_{};
        Res  res_{Res::Ok};
        int  led_{-1};
        int64_t t_acq_{};
    public:
        ModScope(ModEntry* e, Policy p, int64_t wait_us = 0) noexcept
        {
            if (!e) { g.m_nomod.add(); return; }                             // unmanaged state: behave like upstream
            if (!__atomic_load_n(&e->alive, __ATOMIC_ACQUIRE)) { res_ = Res::Stale; g.m_stale_drop.add(); return; }
            int64_t budget = (p == Policy::Try) ? 0 : (p == Policy::Bounded ? wait_us : kBlockUs);
            if (budget > g_cross_us && led_has_other_hold(e)) budget = g_cross_us;   // a holder of one lock never waits long for another
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
            const uint64_t w = static_cast<uint64_t>(t_acq_ - t0);
            e->wait_max_us.max_in(w);
            if (tl_is_game && w != 0) g.gt_block_us.add(w);
            e_ = e;
            // A mod torn down while we waited (uninstall tombstones after taking M): never run into its closed Lua state.
            if (!__atomic_load_n(&e->alive, __ATOMIC_ACQUIRE))
            {
                if (locked_) { e->mu.unlock(); locked_ = false; }
                res_ = Res::Stale; g.m_stale_drop.add();
                return;
            }
            if (locked_)
            {
                __atomic_store_n(&e->defer_since_us, 0, __ATOMIC_RELAXED);
                e->acq.add();
                led_ = led_push(&e->mu, e, LKind::M);
            }
            else if (p == Policy::Try)
            {
                // Liveness guarantee: no queue-run item is held back longer than g_defer_max_us by M
                // (covers a leaked M and a mod that busy-waits on its own closure).
                const int64_t since = __atomic_load_n(&e->defer_since_us, __ATOMIC_RELAXED);
                if (since == 0) __atomic_store_n(&e->defer_since_us, t_acq_, __ATOMIC_RELAXED);
                if (since != 0 && t_acq_ - since > g_defer_max_us) { e->forced.add(); g.m_forced.add(); forced_ = true; }
                else { e->defer.add(); g.m_defer.add(); res_ = Res::Busy; }
            }
            else { e->forced.add(); g.m_forced.add(); forced_ = true; }
        }
        ~ModScope() noexcept
        {
            if (!locked_) return;
            e_->hold_max_us.max_in(static_cast<uint64_t>(now_us() - t_acq_));
            led_release(led_);
            e_->mu.unlock();
        }
        ModScope(const ModScope&) = delete;
        ModScope& operator=(const ModScope&) = delete;
        bool ok() const noexcept { return res_ == Res::Ok; }
        bool busy() const noexcept { return res_ == Res::Busy; }
        bool stale() const noexcept { return res_ == Res::Stale; }
        bool forced() const noexcept { return forced_; }
        ModEntry* entry() const noexcept { return e_; }
    };

    // A binding that touches a mod's Lua stack (m_hook_lua, luaL_ref) reports here; if the caller does not
    // hold that mod's M the counter shows it in the next [LOCK] line (it should read 0).
    inline void note_binding(const ModEntry* e) noexcept
    {
        if (e && !led_holds_m(e)) g.m_unwrapped.add();
    }
    inline void note_action(const ModEntry* e, uint64_t us) noexcept
    {
        if (tl_is_game) g.gt_lua_us.add(us);
        if (us > g.act_max_us.get())
        {
            g.act_max_us.max_in(us);
            __atomic_store_n(&g.act_max_ent, static_cast<const void*>(e), __ATOMIC_RELAXED);
        }
    }

    // ---- work bits and due times: the lock-free fast path --------------------------------
    // Bits are a CACHE of "this queue is non-empty". They are recomputed from the real queues under Q on
    // every mutation (republish_locked), so at every moment Q is released: bits == f(queues). A consumer that
    // reads 0 therefore observed a moment with empty queues, and any later push publishes under Q after it.
    inline void bits_or_locked(uint32_t m) noexcept  { __atomic_store_n(&g_bits, g_bits | m, __ATOMIC_RELEASE); }
    inline void bits_clear_locked(uint32_t m) noexcept { __atomic_store_n(&g_bits, g_bits & ~m, __ATOMIC_RELEASE); }
    inline void due_set_locked(int method, int64_t due_ms) noexcept { __atomic_store_n(&g_due_ms[method], due_ms, __ATOMIC_RELEASE); }
    inline void retry_backoff(int64_t t_ms) noexcept { __atomic_store_n(&g_retry_until_ms, t_ms + kRetryMs, __ATOMIC_RELAXED); }

    struct KeyEv
    {
        const void* mod;      // LuaMod*
        const void* lua;      // const LuaMadeSimple::Lua* (state that registered the bind)
        int32_t     ref;      // registry ref of the Lua callback (created by RegisterKeyBind)
        int32_t     key;
        int64_t     t_us;
    };
    inline std::vector<KeyEv> g_keys;   // guarded by Q

    // View: simple() / engine() -> is the active or pending queue non-empty;
    //       due_ms(method) -> earliest due time in ms (0 = unknown/always, kNoDue = none).
    template <class View>
    inline void republish_locked(const View& v) noexcept
    {
        uint32_t b = g_bits & W_NOTIFY;                       // W_NOTIFY belongs to its H owner; preserved
        if (v.simple()) b |= W_SIMPLE;
        if (v.engine()) b |= W_ENGINE;
        if (!g_keys.empty()) b |= W_KEYS;
        __atomic_store_n(&g_bits, b, __ATOMIC_RELEASE);
        for (int m = 0; m < 3; ++m) due_set_locked(m, v.due_ms(m));
    }

    // THE FAST PATH: zero locks, zero allocation, three loads.
    inline bool pump_should_drain(uint32_t mask, int method, int64_t t_ms) noexcept
    {
        if (t_ms < __atomic_load_n(&g_retry_until_ms, __ATOMIC_RELAXED)) return false;
        if (__atomic_load_n(&g_bits, __ATOMIC_ACQUIRE) & mask) return true;
        return t_ms >= __atomic_load_n(&g_due_ms[method], __ATOMIC_ACQUIRE);
    }
    // Liveness and gap statistics; single writer (the pumping thread); called on every pump event.
    inline void note_pump(int64_t t_us) noexcept
    {
        tl_is_game = true;
        const int64_t last = __atomic_load_n(&g_last_pump_us, __ATOMIC_RELAXED);
        if (last != 0 && t_us > last) g.gap_max_us.max_in(static_cast<uint64_t>(t_us - last));
        __atomic_store_n(&g_last_pump_us, t_us, __ATOMIC_RELAXED);
    }
    inline int64_t last_pump_us() noexcept { return __atomic_load_n(&g_last_pump_us, __ATOMIC_RELAXED); }
    // The single entry check of every pump event. false => nothing to do, no lock was taken.
    inline bool pump_gate(uint32_t mask, int method) noexcept
    {
        const int64_t t_us = now_us();
        note_pump(t_us);
        if (!pump_should_drain(mask | W_KEYS | W_NOTIFY, method, t_us / 1000)) { g.fp_skip.bump_single_writer(); return false; }
        g.fp_take.bump_single_writer();
        return true;
    }

    struct DrainTimer
    {
        int64_t t0{now_us()};
        DrainTimer() = default;
        ~DrainTimer() noexcept
        {
            const uint64_t d = static_cast<uint64_t>(now_us() - t0);
            g.drains.bump_single_writer();
            g.drain_win_us.add(d);
            g.drain_us_max.max_in(d);
        }
        DrainTimer(const DrainTimer&) = delete;
        DrainTimer& operator=(const DrainTimer&) = delete;
    };

    // ---- queue drain with per-mod deferral (ExecuteInGameThread closures) ----------------
    enum class Run : uint8_t { Ran, Busy, Drop };
    // Swap the queue out under Q, release Q, run every item outside Q, re-queue (in order, at the front) the
    // items whose mod was busy. Once a mod reports Busy, its later items in the same batch are deferred
    // without being tried, so per-mod order is kept.
    // run_one(item) -> Run; key_of(item) -> const void* (the mod's identity).
    template <class Action, class View, class RunOne, class KeyOf>
    inline void drain_simple_queue(std::vector<Action>& actions, const View& view, RunOne&& run_one, KeyOf&& key_of)
    {
        std::vector<Action> to_run;
        {
            QGuard q;
            if (actions.empty()) return;
            to_run = std::move(actions);
            actions.clear();
            republish_locked(view);
        }
        std::vector<Action> deferred;
        const void* blocked[8];
        int nblocked = 0;
        for (auto& a : to_run)
        {
            const void* k = key_of(a);
            bool is_blocked = false;
            for (int i = 0; i < nblocked; ++i) if (blocked[i] == k) is_blocked = true;
            if (is_blocked) { deferred.emplace_back(std::move(a)); continue; }
            if (run_one(a) == Run::Busy)
            {
                if (nblocked < 8) blocked[nblocked++] = k;
                deferred.emplace_back(std::move(a));
            }
        }
        if (!deferred.empty())
        {
            QGuard q;
            actions.insert(actions.begin(), std::make_move_iterator(deferred.begin()), std::make_move_iterator(deferred.end()));
            republish_locked(view);
            retry_backoff(now_ms());
        }
    }

    // ---- key queue: the input thread only ever calls key_enqueue ---------------------------
    template <class View>
    inline void publish_keys_locked(const View& v) noexcept { republish_locked(v); }

    struct NoView                        // used where only the key bit matters (the other bits are re-derived by the next republish)
    {
        bool simple() const noexcept { return (__atomic_load_n(&g_bits, __ATOMIC_RELAXED) & W_SIMPLE) != 0; }
        bool engine() const noexcept { return (__atomic_load_n(&g_bits, __ATOMIC_RELAXED) & W_ENGINE) != 0; }
        int64_t due_ms(int m) const noexcept { return __atomic_load_n(&g_due_ms[m], __ATOMIC_RELAXED); }
    };

    // Never raises, never touches Lua, never waits on anything but Q (microseconds).
    inline void key_enqueue(KeyEv ev) noexcept
    {
        ev.t_us = now_us();
        try
        {
            QGuard q;
            if (static_cast<int64_t>(g_keys.size()) >= kKeyCap) { g_keys.erase(g_keys.begin()); g.key_drop.add(); }
            g_keys.push_back(ev);
            republish_locked(NoView{});
        }
        catch (...) { g.key_drop.add(); return; }
        g.key_q.add();
    }

    // Pops matching events under Q, runs them OUTSIDE Q, re-queues (in order, at the front) the ones run() declined.
    // run(ev) returns true if the event is consumed (Lua ran, or the mod is gone), false to defer it.
    template <class Run_>
    inline uint32_t drain_keys(Run_&& run, int64_t min_age_us, const void* only_mod) noexcept
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
            republish_locked(NoView{});
        }
        catch (...) { return 0; }

        std::vector<KeyEv> back;
        try { back.reserve(batch.size()); } catch (...) {}
        const void* blocked[8];
        int nblocked = 0;
        uint32_t n = 0;
        for (auto& ev : batch)
        {
            bool is_blocked = false;
            for (int i = 0; i < nblocked; ++i) if (blocked[i] == ev.mod) is_blocked = true;
            bool ran = false;
            if (!is_blocked)
            {
                const uint64_t waited = static_cast<uint64_t>(now_us() - ev.t_us);
                try { ran = run(ev); } catch (...) { ran = true; }
                if (ran)
                {
                    ++n;
                    g.key_d.add();
                    g.key_wait_sum_us.add(waited);
                    g.key_wait_max_us.max_in(waited);
                }
            }
            if (!ran)
            {
                if (!is_blocked && nblocked < 8) blocked[nblocked++] = ev.mod;
                try { back.push_back(ev); } catch (...) { g.key_drop.add(); }
            }
        }
        if (!back.empty())
        {
            try
            {
                QGuard q;
                g_keys.insert(g_keys.begin(), back.begin(), back.end());
                republish_locked(NoView{});
            }
            catch (...) { g.key_drop.add(back.size()); }
            retry_backoff(now_ms());
        }
        return n;
    }

    // ---- [LOCK] statistics: at most one block per 10 s (+ init and final) --------------------
    inline void (*g_sink)(const char*) = nullptr;       // set by LuaMod.cpp (Output::send "[LOCK] {}")
    inline int64_t g_next_log_ms = 0;

    inline int fmt_main(char* out, size_t cap, bool final_line) noexcept
    {
        typedef unsigned long long U;
        const uint64_t kd = g.key_d.get();
        const uint64_t wmax = g.key_wait_max_us.get();
        const uint64_t wmean = kd ? g.key_wait_sum_us.get() / kd : 0;
        const uint64_t hold = g.q_hold_max_us.get();
        const uint64_t qwait = g.q_wait_max_us.get();
        const uint64_t cb = g.key_cb_max_us.get();
        const uint64_t win = g.drain_win_us.take();          // window counter: reset per line
        return std::snprintf(out, cap,
                      "keys q=%llu d=%llu drop=%llu fb=%llu slow=%llu stale=%llu wait_ms max=%llu.%03llu mean=%llu.%03llu cb_max_ms=%llu.%03llu"
                      " | qlock takes=%llu contended=%llu hold_max_ms=%llu.%03llu wait_max_ms=%llu.%03llu"
                      " | fast skip=%llu take=%llu"
                      " | eigt q=%llu run=%llu dly q=%llu run=%llu"
                      " | defer=%llu forced=%llu unwrapped=%llu nomod=%llu"
                      " | pump gap_max_ms=%llu drain_win_ms=%llu drain_max_ms=%llu%s",
                      (U)g.key_q.get(), (U)kd, (U)g.key_drop.get(), (U)g.key_fb.get(), (U)g.key_slow.get(), (U)g.key_stale.get(),
                      (U)(wmax / 1000), (U)(wmax % 1000), (U)(wmean / 1000), (U)(wmean % 1000), (U)(cb / 1000), (U)(cb % 1000),
                      (U)g.q_takes.get(), (U)g.q_contended.get(), (U)(hold / 1000), (U)(hold % 1000), (U)(qwait / 1000), (U)(qwait % 1000),
                      (U)g.fp_skip.get(), (U)g.fp_take.get(),
                      (U)g.eigt_q.get(), (U)g.eigt_run.get(), (U)g.dly_q.get(), (U)g.dly_run.get(),
                      (U)g.m_defer.get(), (U)g.m_forced.get(), (U)g.m_unwrapped.get(), (U)g.m_nomod.get(),
                      (U)(g.gap_max_us.get() / 1000), (U)(win / 1000), (U)(g.drain_us_max.get() / 1000), final_line ? " final=1" : "");
    }
    inline int fmt_extra(char* out, size_t cap) noexcept
    {
        typedef unsigned long long U;
        const auto* ent = static_cast<const ModEntry*>(__atomic_load_n(&g.act_max_ent, __ATOMIC_RELAXED));
        const uint64_t am = g.act_max_us.get();
        const uint64_t rm = g.rearm_max_us.get();
        return std::snprintf(out, cap,
                      "extra gt_lua_ms=%llu act_max_ms=%llu.%03llu act_max_mod=%s blk_ms=%llu rearm n=%llu max_ms=%llu repaired h=%llu m=%llu led_overflow=%llu",
                      (U)(g.gt_lua_us.get() / 1000), (U)(am / 1000), (U)(am % 1000), ent ? ent->name : "-",
                      (U)(g.gt_block_us.get() / 1000), (U)g.rearm_n.get(), (U)(rm / 1000),
                      (U)g.h_repaired.get(), (U)g.m_repaired.get(), (U)g.led_overflow.get());
    }
    inline int fmt_mod(char* out, size_t cap, const ModEntry* e) noexcept
    {
        typedef unsigned long long U;
        const uint64_t w = e->wait_max_us.get(), h = e->hold_max_us.get();
        return std::snprintf(out, cap, "mod=%s lua_lock wait_max_ms=%llu.%03llu hold_max_ms=%llu.%03llu acq=%llu contended=%llu defer=%llu forced=%llu",
                      e->name, (U)(w / 1000), (U)(w % 1000), (U)(h / 1000), (U)(h % 1000),
                      (U)e->acq.get(), (U)e->contended.get(), (U)e->defer.get(), (U)e->forced.get());
    }
    // Prints the full block through g_sink. Rate limit: the first call only arms the timer; later calls print at most
    // once per 10 s (compare-and-swap, so concurrent callers print once). final_line bypasses the limit.
    inline bool maybe_log(int64_t t_ms, bool final_line) noexcept
    {
        if (!g_sink) return false;
        int64_t nxt = __atomic_load_n(&g_next_log_ms, __ATOMIC_RELAXED);
        if (!final_line)
        {
            if (nxt == 0)
            {
                __atomic_compare_exchange_n(&g_next_log_ms, &nxt, t_ms + 10'000, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
                return false;
            }
            if (t_ms < nxt) return false;
            if (!__atomic_compare_exchange_n(&g_next_log_ms, &nxt, t_ms + 10'000, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) return false;
        }
        char line[1024];
        fmt_main(line, sizeof(line), final_line);
        g_sink(line);
        fmt_extra(line, sizeof(line));
        g_sink(line);
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
            fmt_mod(line, sizeof(line), snap[i]);
            g_sink(line);
        }
        return true;
    }
    inline void heartbeat() noexcept { maybe_log(now_ms(), false); }
}
// ===== END LOCKFIX-CORE v2 =========================================================
