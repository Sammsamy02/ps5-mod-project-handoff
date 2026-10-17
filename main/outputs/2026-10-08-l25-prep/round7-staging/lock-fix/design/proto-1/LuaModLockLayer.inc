// LuaModLockLayer.inc  --  DESIGN-1 PROTOTYPE (Mac-only, not yet in the candidate tree).
//
// One self-contained block, shared verbatim by LuaMod.cpp (production) and by
// the host fixture (test_lock_layer.cpp), the same way VmtTickPumpCore.inc is
// shared by UnrealInitializer.cpp and its fixture.
//
// Needs, already included by the includer: <chrono> <cstdint> <cstdio>
// <cstring> <mutex> <thread>.  No <atomic>: counters and flags use the
// compiler __atomic builtins (the l26 include-graph gate refused <atomic> in B2).
//
// The includer must define BEFORE including this file:
//   LOCKLAYER_QUEUE_MUTEX   the std::recursive_mutex used as the QUEUE lock
//                           (production: LuaMod::m_thread_actions_mutex)
//   LOCKLAYER_EMIT(line)    write one finished UTF-8 line to the log
// and may define:
//   LOCKLAYER_NOW_US()      steady clock in microseconds (tests override it)
//
// LOCK CLASSES (see DESIGN-1.md section 3):
//   Q  queue lock   short; guards queues, flags, containers, registry refs.
//                   Never held across Lua code, a Lua error raise, I/O, or a wait.
//   M  mod lock     one per Lua mod (ModSlot); held while non-async Lua of that
//                   mod runs (keybind, game-thread drains).  Re-entrant.
//   C  Lua core lock (platform_locks.c), innermost, not touched here.
// ORDER: M may be taken while holding nothing or other M's under the rank rule;
//        Q is only ever taken inside M or alone; M is NEVER requested while Q is held.

#ifndef LOCKLAYER_NOW_US
#define LOCKLAYER_NOW_US() (static_cast<int64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()))
#endif

namespace RC
{
namespace LockLayer
{
    constexpr int64_t kNever = 0x7fffffffffffffffLL;
    constexpr int64_t kKeySpinUs = 3000;        // UpdateThread: spin for the mod lock this long, then leave the key queued
    constexpr int64_t kNestedWaitUs = 2000;     // ascending nested acquire: bounded wait
    constexpr int64_t kKeyPrefMaxUs = 100000;   // game thread yields to a queued key for at most this long
    constexpr int64_t kDeferBackoffUs = 1000;   // a deferred item is retried no sooner than this
    constexpr int64_t kEmitIntervalUs = 10000000;

    inline int64_t now_us() noexcept { return LOCKLAYER_NOW_US(); }

    // ---------------------------------------------------------------- counters
    struct Ctr
    {
        int64_t v{0};
        void add(int64_t n = 1) noexcept { __atomic_fetch_add(&v, n, __ATOMIC_RELAXED); }
        int64_t get() const noexcept { return __atomic_load_n(&v, __ATOMIC_RELAXED); }
        void set(int64_t n) noexcept { __atomic_store_n(&v, n, __ATOMIC_RELAXED); }
        int64_t exchange(int64_t n) noexcept { return __atomic_exchange_n(&v, n, __ATOMIC_RELAXED); }
        void max_to(int64_t x) noexcept
        {
            int64_t c = get();
            while (x > c && !__atomic_compare_exchange_n(&v, &c, x, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
        }
    };

    struct Stats
    {
        Ctr key_queued, key_dispatched, key_deferred, key_overflow, key_run_errors;
        Ctr key_wait_total_us, key_wait_max_us;
        Ctr q_takes, q_contended, q_wait_max_us, q_hold_max_us;
        Ctr pump_calls, pump_skips, pump_takes, pump_empty_takes, pump_over_4ms, pump_us_max, pump_us_max_win;
        Ctr eigt_queued, eigt_run, dly_queued, dly_run;
        Ctr game_defers, key_yields, nested_skips, order_violations, leaks_repaired, hook_skips, raise_unlocks;
    };
    inline Stats g_stats;
    inline Stats& S() noexcept { return g_stats; }

    inline thread_local int tl_q_depth = 0;

    // ------------------------------------------------------- work flags / wakes
    enum : unsigned { kWorkGame = 1u, kWorkTick = 2u, kWorkNotify = 4u, kWorkAll = 7u };
    enum : int { kExecProcessEvent = 0, kExecTick = 1, kExecMarshal = 2, kExecCount = 3 };
    inline unsigned g_work_bits = kWorkAll;                         // atomic; starts "look once"
    inline int64_t g_wake_us[kExecCount] = {0, 0, 0};               // atomic; earliest time a pump must look (delayed + deferred work)
    inline thread_local int64_t tl_defer_wake_us = kNever;

    inline unsigned work_bits() noexcept { return __atomic_load_n(&g_work_bits, __ATOMIC_ACQUIRE); }
    inline void set_wake(int exec, int64_t us) noexcept { __atomic_store_n(&g_wake_us[exec], us, __ATOMIC_RELEASE); }
    inline int64_t get_wake(int exec) noexcept { return __atomic_load_n(&g_wake_us[exec], __ATOMIC_ACQUIRE); }
    // A drain that puts work back (busy mod lock) asks to be revisited after the backoff.
    inline void note_defer() noexcept
    {
        const int64_t t = now_us() + kDeferBackoffUs;
        if (t < tl_defer_wake_us) tl_defer_wake_us = t;
    }
    inline int64_t take_defer_wake() noexcept
    {
        const int64_t t = tl_defer_wake_us;
        tl_defer_wake_us = kNever;
        return t;
    }

    // --------------------------------------------------------------- queue lock
    // Replaces every  std::lock_guard<std::recursive_mutex> guard{...m_thread_actions_mutex}.
    //   Producer (default): the section may add work.  On release, while still holding the
    //                       lock, it sets every work bit and zeroes every wake (a missed
    //                       call site is therefore SAFE: it only costs one empty look).
    //   Drain:              drain-internal section (claim / swap-out / requeue / final merge).
    //                       Never sets bits; the drain re-arms from real queue contents.
    class QueueLock
    {
      public:
        enum class Kind : uint8_t { Producer, Drain };
        explicit QueueLock(Kind kind = Kind::Producer) : kind_(kind)
        {
            S().q_takes.add();
            if (!LOCKLAYER_QUEUE_MUTEX.try_lock())
            {
                const int64_t t0 = now_us();
                S().q_contended.add();
                LOCKLAYER_QUEUE_MUTEX.lock();
                S().q_wait_max_us.max_to(now_us() - t0);
            }
            ++tl_q_depth;
            t_acq_ = now_us();
        }
        QueueLock(const QueueLock&) = delete;
        QueueLock& operator=(const QueueLock&) = delete;
        ~QueueLock() { release_(); }
        // Call BEFORE any Lua error is raised from a binding frame: on PS5 the raise is a
        // longjmp and would skip this destructor (the B4 stuck-key bug).
        void unlock_early() noexcept { if (owns_) S().raise_unlocks.add(); release_(); }
      private:
        void release_() noexcept
        {
            if (!owns_) return;
            owns_ = false;
            if (kind_ == Kind::Producer)
            {
                __atomic_fetch_or(&g_work_bits, kWorkAll, __ATOMIC_RELEASE);
                for (int i = 0; i < kExecCount; ++i) set_wake(i, 0);
            }
            S().q_hold_max_us.max_to(now_us() - t_acq_);
            --tl_q_depth;
            LOCKLAYER_QUEUE_MUTEX.unlock();
        }
        Kind kind_;
        bool owns_{true};
        int64_t t_acq_{0};
    };

    // ------------------------------------------------------------------ leaks
    inline void emit_leak_line_(const char* site, int levels) noexcept
    {
        static int64_t s_last_us = 0;
        const int64_t now = now_us();
        const int64_t last = __atomic_load_n(&s_last_us, __ATOMIC_RELAXED);
        if (now - last < 1000000 && last != 0) return;
        __atomic_store_n(&s_last_us, now, __ATOMIC_RELAXED);
        char buf[160];
        std::snprintf(buf, sizeof(buf), "[LOCKLEAK] repaired queue-lock levels=%d site=%s (a Lua error skipped a lock release)", levels, site ? site : "?");
        LOCKLAYER_EMIT(buf);
    }
    // Called on the SAME thread that ran a Lua call, from a frame ABOVE the pcall.
    // Any queue-lock level still counted beyond what this frame entered with was
    // skipped by a longjmp, so it is released here.
    inline void repair_leak(const char* site, int entry_depth) noexcept
    {
        const int extra = tl_q_depth - entry_depth;
        if (extra <= 0) return;
        S().leaks_repaired.add(extra);
        while (tl_q_depth > entry_depth)
        {
            --tl_q_depth;
            LOCKLAYER_QUEUE_MUTEX.unlock();
        }
        emit_leak_line_(site, extra);
    }
    class LeakProbe
    {
      public:
        explicit LeakProbe(const char* site) noexcept : site_(site), entry_(tl_q_depth) {}
        ~LeakProbe() { repair_leak(site_, entry_); }
        LeakProbe(const LeakProbe&) = delete;
        LeakProbe& operator=(const LeakProbe&) = delete;
      private:
        const char* site_;
        int entry_;
    };

    // --------------------------------------------------------------- mod slots
    constexpr int kMaxSlots = 32;
    constexpr int kOrphan = kMaxSlots - 1;   // unknown Lua states serialize here (highest rank)
    struct ModSlot
    {
        std::recursive_mutex m;
        int rank{0};
        char name[40]{};
        int64_t key_pending_since_us{0};   // atomic
        int32_t waiters{0};                // atomic
        Ctr takes, defers, skips, max_wait_us, max_hold_us;
    };
    inline ModSlot g_slots[kMaxSlots];
    inline int g_slot_count = 0;           // atomic; slots [0, count) are live
    struct Bind { const void* key; int slot; };
    inline Bind g_binds[256];
    inline int g_bind_count = 0;           // atomic; append-only
    inline std::mutex g_reg_mu;            // leaf; writers only

    inline ModSlot* slot_create(const char* name)
    {
        std::lock_guard<std::mutex> g(g_reg_mu);
        const int n = g_slot_count;
        if (n >= kOrphan) return &g_slots[kOrphan];
        ModSlot& s = g_slots[n];
        s.rank = n;
        std::snprintf(s.name, sizeof(s.name), "%s", name ? name : "?");
        __atomic_store_n(&g_slot_count, n + 1, __ATOMIC_RELEASE);
        return &s;
    }
    inline void slot_bind(const void* key, ModSlot* s)
    {
        std::lock_guard<std::mutex> g(g_reg_mu);
        const int n = g_bind_count;
        if (n >= 256 || key == nullptr) return;
        g_binds[n].key = key;
        g_binds[n].slot = static_cast<int>(s - g_slots);
        __atomic_store_n(&g_bind_count, n + 1, __ATOMIC_RELEASE);
    }
    inline ModSlot* slot_for(const void* key) noexcept
    {
        const int n = __atomic_load_n(&g_bind_count, __ATOMIC_ACQUIRE);
        for (int i = 0; i < n; ++i)
            if (__atomic_load_n(&g_binds[i].key, __ATOMIC_RELAXED) == key) return &g_slots[g_binds[i].slot];
        return &g_slots[kOrphan];
    }
    inline void slot_unbind_all(ModSlot* s) noexcept
    {
        const int n = __atomic_load_n(&g_bind_count, __ATOMIC_ACQUIRE);
        for (int i = 0; i < n; ++i)
            if (&g_slots[g_binds[i].slot] == s) __atomic_store_n(&g_binds[i].key, static_cast<const void*>(nullptr), __ATOMIC_RELAXED);
    }

    struct Held { ModSlot* s[8]; int d[8]; int n; };
    inline thread_local Held tl_held{};
    inline int held_find(const ModSlot* s) noexcept
    {
        for (int i = 0; i < tl_held.n; ++i) if (tl_held.s[i] == s) return i;
        return -1;
    }
    inline int held_max_rank() noexcept
    {
        int r = -1;
        for (int i = 0; i < tl_held.n; ++i) if (tl_held.s[i]->rank > r) r = tl_held.s[i]->rank;
        return r;
    }
    inline bool key_preference_active(const ModSlot& s, int64_t now) noexcept
    {
        if (__atomic_load_n(&s.waiters, __ATOMIC_ACQUIRE) > 0) return true;
        const int64_t since = __atomic_load_n(&s.key_pending_since_us, __ATOMIC_ACQUIRE);
        return since != 0 && (now - since) < kKeyPrefMaxUs;
    }
    inline bool spin_try_(ModSlot* s, int64_t budget_us) noexcept
    {
        const int64_t end = now_us() + budget_us;
        for (int it = 0;; ++it)
        {
            if (s->m.try_lock()) return true;
            if (now_us() >= end) return false;
            if (it < 64) std::this_thread::yield();
            else std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    }

    enum class Entry : uint8_t
    {
        Drain,     // game-thread drain: try once, never wait, yields to a queued key
        Key,       // UpdateThread keybind: waits up to kKeySpinUs, then the event stays queued
        Nested,    // lock needed while this thread already holds another mod lock (or from a hook)
        Blocking   // teardown on the UpdateThread: may wait, only when nothing else is held
    };

    // Re-entrant for the same thread (any entry mode).  ABBA-safe: a thread that holds
    // mod X may only WAIT for mod Y when rank(Y) > rank(X); for rank(Y) < rank(X) it
    // makes a single try_lock and reports failure (the caller defers or skips).  A
    // wait-for cycle needs at least one descending edge, and descending edges never wait.
    class ModGuard
    {
      public:
        ModGuard(ModSlot* slot, Entry mode, const char* site) noexcept : slot_(slot), site_(site), q_entry_(tl_q_depth)
        {
            acquire_(mode);
        }
        ModGuard(const ModGuard&) = delete;
        ModGuard& operator=(const ModGuard&) = delete;
        ~ModGuard() { release_(); }
        bool ok() const noexcept { return ok_; }
      private:
        void acquire_(Entry mode) noexcept
        {
            if (!slot_) return;
            const int64_t t0 = now_us();
            const int hi = held_find(slot_);
            if (hi >= 0)   // same thread already holds this mod: recursion never waits
            {
                slot_->m.lock();
                ++tl_held.d[hi];
                ok_ = true;
                reentry_ = true;
                return;
            }
            if (tl_q_depth > 0 || tl_held.n >= 8)
            {
                S().order_violations.add();            // M requested with Q held: never wait
                mode = Entry::Nested;
            }
            bool got = false;
            switch (mode)
            {
            case Entry::Drain:
                if (key_preference_active(*slot_, t0)) { S().key_yields.add(); slot_->defers.add(); S().game_defers.add(); return; }
                got = slot_->m.try_lock();
                if (!got) { slot_->defers.add(); S().game_defers.add(); }
                break;
            case Entry::Key:
                if (tl_held.n > 0) { got = (slot_->rank > held_max_rank()) ? spin_try_(slot_, kNestedWaitUs) : slot_->m.try_lock(); break; }
                __atomic_fetch_add(&slot_->waiters, 1, __ATOMIC_ACQ_REL);
                got = spin_try_(slot_, kKeySpinUs);
                __atomic_fetch_sub(&slot_->waiters, 1, __ATOMIC_ACQ_REL);
                break;
            case Entry::Nested:
            {
                const int mr = held_max_rank();
                got = (mr >= 0 && slot_->rank < mr) ? slot_->m.try_lock() : spin_try_(slot_, kNestedWaitUs);
                if (!got) { slot_->skips.add(); S().nested_skips.add(); }
                break;
            }
            case Entry::Blocking:
                if (tl_held.n == 0 && tl_q_depth == 0)
                {
                    __atomic_fetch_add(&slot_->waiters, 1, __ATOMIC_ACQ_REL);
                    slot_->m.lock();
                    __atomic_fetch_sub(&slot_->waiters, 1, __ATOMIC_ACQ_REL);
                    got = true;
                }
                else got = spin_try_(slot_, kNestedWaitUs);
                break;
            }
            if (!got) return;
            tl_held.s[tl_held.n] = slot_;
            tl_held.d[tl_held.n] = 1;
            ++tl_held.n;
            ok_ = true;
            t_take_ = now_us();
            slot_->takes.add();
            slot_->max_wait_us.max_to(t_take_ - t0);
        }
        void release_() noexcept
        {
            if (!ok_) return;
            ok_ = false;
            const int hi = held_find(slot_);
            if (reentry_)
            {
                if (hi >= 0) --tl_held.d[hi];
                slot_->m.unlock();
                return;
            }
            slot_->max_hold_us.max_to(now_us() - t_take_);
            repair_leak(site_, q_entry_);          // a Lua error under Q inside this call
            if (hi >= 0)
            {
                for (int i = hi; i + 1 < tl_held.n; ++i) { tl_held.s[i] = tl_held.s[i + 1]; tl_held.d[i] = tl_held.d[i + 1]; }
                --tl_held.n;
            }
            slot_->m.unlock();
        }
        ModSlot* slot_;
        const char* site_;
        int q_entry_;
        bool ok_{false};
        bool reentry_{false};
        int64_t t_take_{0};
    };

    // --------------------------------------------------------------- key queue
    constexpr int kKeyQ = 64;
    struct KeyEvent { const void* lua; int32_t ref; int32_t key; int64_t t_us; ModSlot* slot; uint64_t seq; };
    inline KeyEvent g_keyq[kKeyQ];
    inline int g_keyq_n = 0;       // guarded by Q
    inline uint64_t g_key_seq = 0; // guarded by Q
    inline thread_local bool tl_in_key_pump = false;

    // Short Q section only: no Lua, no mod lock.
    inline void key_enqueue(ModSlot* slot, const void* lua, int ref, int key) noexcept
    {
        S().key_queued.add();
        QueueLock q(QueueLock::Kind::Drain);
        if (g_keyq_n >= kKeyQ)
        {
            for (int i = 1; i < g_keyq_n; ++i) g_keyq[i - 1] = g_keyq[i];
            --g_keyq_n;
            S().key_overflow.add();
        }
        const int64_t t = now_us();
        g_keyq[g_keyq_n++] = KeyEvent{lua, ref, key, t, slot, ++g_key_seq};
        if (__atomic_load_n(&slot->key_pending_since_us, __ATOMIC_RELAXED) == 0)
            __atomic_store_n(&slot->key_pending_since_us, t, __ATOMIC_RELEASE);
    }
    inline int key_pending_count() noexcept
    {
        QueueLock q(QueueLock::Kind::Drain);
        return g_keyq_n;
    }
    // Mod teardown: forget queued key events of a mod whose Lua state is about to close.
    inline int key_purge(ModSlot* slot) noexcept
    {
        QueueLock q(QueueLock::Kind::Drain);
        int kept = 0, dropped = 0;
        for (int i = 0; i < g_keyq_n; ++i)
        {
            if (g_keyq[i].slot == slot) { ++dropped; continue; }
            g_keyq[kept++] = g_keyq[i];
        }
        g_keyq_n = kept;
        __atomic_store_n(&slot->key_pending_since_us, static_cast<int64_t>(0), __ATOMIC_RELEASE);
        return dropped;
    }
    // UpdateThread only.  Runs queued key callbacks whose mod lock is free; an event whose
    // mod is busy stays queued (FIFO per mod) and is retried on the next call.  Never waits
    // longer than kKeySpinUs per busy mod per call.  run(ev) is called with the mod lock held.
    template <class Run>
    inline int key_pump(Run&& run) noexcept
    {
        if (tl_in_key_pump) return 0;
        struct Reset { ~Reset() { tl_in_key_pump = false; } } reset;
        tl_in_key_pump = true;
        ModSlot* blocked[8];
        int nb = 0, ran = 0;
        for (;;)
        {
            KeyEvent ev{};
            bool have = false;
            {
                QueueLock q(QueueLock::Kind::Drain);
                for (int i = 0; i < g_keyq_n && !have; ++i)
                {
                    bool b = false;
                    for (int j = 0; j < nb; ++j) if (blocked[j] == g_keyq[i].slot) b = true;
                    if (!b) { ev = g_keyq[i]; have = true; }
                }
            }
            if (!have) break;
            ModGuard g(ev.slot, Entry::Key, "keybind");
            if (!g.ok())
            {
                S().key_deferred.add();
                if (nb >= 8) break;
                blocked[nb++] = ev.slot;
                continue;
            }
            bool found = false;
            {
                QueueLock q(QueueLock::Kind::Drain);
                for (int i = 0; i < g_keyq_n; ++i)
                {
                    if (g_keyq[i].seq != ev.seq) continue;
                    for (int k = i + 1; k < g_keyq_n; ++k) g_keyq[k - 1] = g_keyq[k];
                    --g_keyq_n;
                    found = true;
                    break;
                }
                int64_t oldest = 0;
                for (int i = 0; i < g_keyq_n; ++i)
                    if (g_keyq[i].slot == ev.slot) { oldest = g_keyq[i].t_us; break; }
                __atomic_store_n(&ev.slot->key_pending_since_us, oldest, __ATOMIC_RELEASE);
            }
            if (!found) continue;
            const int64_t waited = now_us() - ev.t_us;
            S().key_wait_total_us.add(waited);
            S().key_wait_max_us.max_to(waited);
            try { run(ev); }
            catch (...) { S().key_run_errors.add(); }
            S().key_dispatched.add();
            ++ran;
        }
        return ran;
    }

    // ---------------------------------------------------------------- fast path
    // First thing every engine-event hook does.  Two atomic loads (+ one clock read only
    // when a timed wake is armed).  true = take the queue lock and look.
    inline void maybe_emit(bool force) noexcept;
    inline bool pump_should_run(unsigned mask, int exec) noexcept
    {
        const int64_t calls = S().pump_calls.get() + 1;
        S().pump_calls.add();
        bool run = (work_bits() & mask) != 0;
        if (!run)
        {
            const int64_t w = get_wake(exec);
            run = (w != kNever) && now_us() >= w;
        }
        if (run) S().pump_takes.add(); else S().pump_skips.add();
        if ((calls & 0x3FF) == 0) maybe_emit(false);
        return run;
    }
    // Under the Drain-kind Q section that starts a drain: this pump now owns these bits' work.
    inline void pump_claim(QueueLock&, unsigned mask) noexcept { __atomic_fetch_and(&g_work_bits, ~mask, __ATOMIC_ACQ_REL); }
    // Under the Drain-kind Q section that ends a drain (after pending queues were merged):
    // re-arm from what is REALLY still queued, so nothing a drain left behind can be missed.
    inline void pump_rearm(QueueLock&, bool game_left, bool tick_left, bool notify_left) noexcept
    {
        const unsigned b = (game_left ? kWorkGame : 0u) | (tick_left ? kWorkTick : 0u) | (notify_left ? kWorkNotify : 0u);
        if (b) __atomic_fetch_or(&g_work_bits, b, __ATOMIC_RELEASE);
    }
    // Drains call note_work() when they really ran something; the scope that owns the drain
    // measures how long the game thread spent in it (stutter evidence) and counts empty looks.
    inline thread_local bool tl_pump_worked = false;
    inline void note_work() noexcept { tl_pump_worked = true; }
    class PumpTimer
    {
      public:
        PumpTimer() noexcept {}
        PumpTimer(const PumpTimer&) = delete;
        PumpTimer& operator=(const PumpTimer&) = delete;
        void begin() noexcept { t0_ = now_us(); on_ = true; tl_pump_worked = false; }
        ~PumpTimer()
        {
            if (!on_) return;
            const int64_t us = now_us() - t0_;
            S().pump_us_max.max_to(us);
            S().pump_us_max_win.max_to(us);
            if (us > 4000) S().pump_over_4ms.add();
            if (!tl_pump_worked) S().pump_empty_takes.add();
            tl_pump_worked = false;
        }
      private:
        int64_t t0_{0};
        bool on_{false};
    };

    // ------------------------------------------------------------------- stats
    inline int64_t g_last_emit_us = 0;
    inline int g_emit_started = 0;
    inline int64_t g_start_us = 0;
    inline void emit_now_(int64_t now, bool first, bool final_line) noexcept
    {
        Stats& s = S();
        char buf[1600];
        int n = 0;
        auto ms3 = [](int64_t us, long long& whole, long long& frac) { whole = us / 1000; frac = us % 1000; if (frac < 0) frac = -frac; };
        long long kmw, kmf, kxw, kxf, qww, qwf, qhw, qhf, pmw, pmf, pww, pwf;
        const int64_t kd = s.key_dispatched.get();
        ms3(kd > 0 ? s.key_wait_total_us.get() / kd : 0, kmw, kmf);
        ms3(s.key_wait_max_us.get(), kxw, kxf);
        ms3(s.q_wait_max_us.get(), qww, qwf);
        ms3(s.q_hold_max_us.get(), qhw, qhf);
        ms3(s.pump_us_max.get(), pmw, pmf);
        ms3(s.pump_us_max_win.exchange(0), pww, pwf);
        n += std::snprintf(buf + n, sizeof(buf) - n,
            "[LOCK] %s t=%llds key q=%lld d=%lld deferred=%lld ovf=%lld wait_ms mean=%lld.%03lld max=%lld.%03lld"
            " | qlock takes=%lld contended=%lld wait_max_ms=%lld.%03lld hold_max_ms=%lld.%03lld"
            " | pump calls=%lld skips=%lld takes=%lld empty=%lld ms_max=%lld.%03lld ms_win=%lld.%03lld over4ms=%lld"
            " | eigt q=%lld run=%lld dly_q=%lld dly_run=%lld"
            " | modlock game_defers=%lld key_yields=%lld nested_skips=%lld hook_skips=%lld order_viol=%lld leaks=%lld raise_unlocks=%lld",
            first ? "init layer=B5-1" : (final_line ? "final" : "stats"),
            static_cast<long long>((now - g_start_us) / 1000000),
            static_cast<long long>(s.key_queued.get()), static_cast<long long>(kd), static_cast<long long>(s.key_deferred.get()),
            static_cast<long long>(s.key_overflow.get()), kmw, kmf, kxw, kxf,
            static_cast<long long>(s.q_takes.get()), static_cast<long long>(s.q_contended.get()), qww, qwf, qhw, qhf,
            static_cast<long long>(s.pump_calls.get()), static_cast<long long>(s.pump_skips.get()), static_cast<long long>(s.pump_takes.get()),
            static_cast<long long>(s.pump_empty_takes.get()), pmw, pmf, pww, pwf, static_cast<long long>(s.pump_over_4ms.get()),
            static_cast<long long>(s.eigt_queued.get()), static_cast<long long>(s.eigt_run.get()),
            static_cast<long long>(s.dly_queued.get()), static_cast<long long>(s.dly_run.get()),
            static_cast<long long>(s.game_defers.get()), static_cast<long long>(s.key_yields.get()), static_cast<long long>(s.nested_skips.get()),
            static_cast<long long>(s.hook_skips.get()), static_cast<long long>(s.order_violations.get()), static_cast<long long>(s.leaks_repaired.get()),
            static_cast<long long>(s.raise_unlocks.get()));
        n += std::snprintf(buf + n, sizeof(buf) - n, " | mods");
        const int cnt = __atomic_load_n(&g_slot_count, __ATOMIC_ACQUIRE);
        for (int i = 0; i < cnt && n < static_cast<int>(sizeof(buf)) - 80; ++i)
        {
            long long w, f;
            ms3(g_slots[i].max_wait_us.get(), w, f);
            n += std::snprintf(buf + n, sizeof(buf) - n, " %s:wait_max_ms=%lld.%03lld/takes=%lld/defers=%lld/skips=%lld",
                               g_slots[i].name, w, f, static_cast<long long>(g_slots[i].takes.get()),
                               static_cast<long long>(g_slots[i].defers.get()), static_cast<long long>(g_slots[i].skips.get()));
        }
        LOCKLAYER_EMIT(buf);
    }
    // At most one "[LOCK] " line per 10 s (CAS-claimed, any thread); force = shutdown/unload line.
    inline void maybe_emit(bool force) noexcept
    {
        const int64_t now = now_us();
        if (force)
        {
            emit_now_(now, false, true);
            return;
        }
        if (__atomic_load_n(&g_emit_started, __ATOMIC_ACQUIRE) == 0)
        {
            int expected = 0;
            if (__atomic_compare_exchange_n(&g_emit_started, &expected, 1, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
            {
                g_start_us = now;
                __atomic_store_n(&g_last_emit_us, now, __ATOMIC_RELEASE);
                emit_now_(now, true, false);
            }
            return;
        }
        int64_t last = __atomic_load_n(&g_last_emit_us, __ATOMIC_ACQUIRE);
        if (now - last < kEmitIntervalUs) return;
        if (!__atomic_compare_exchange_n(&g_last_emit_us, &last, now, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) return;
        emit_now_(now, false, false);
    }
} // namespace LockLayer
} // namespace RC
