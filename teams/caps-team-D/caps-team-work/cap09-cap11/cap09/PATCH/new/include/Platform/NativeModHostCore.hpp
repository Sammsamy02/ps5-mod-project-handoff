// NativeModHostCore.hpp -- engine-free host for ABI-v1 native C++ mods (#9).
//
// Header-only so it can be #included by an EXISTING runtime unit (the build
// driver replaces units; it does not add new ones), and compiled unchanged by
// the host tests. Contains no engine type: every engine action goes through the
// EngineServices interface, which the runtime implements in
// NativeModEngineServices.inc and the host tests implement with a fake.
//
// What this core owns (the parts PC UE4SS leaves implicit and that a separate
// PRX module boundary makes mandatory):
//   * the mbm_host_v1 service table handed to each mod;
//   * mod table validation (magic / version / size / required entries);
//   * lifecycle dispatch with fault isolation: a callback that returns non-OK
//     (the SDK turns any C++ exception into MBM_E_MOD_FAULT) marks the mod
//     FAULTED, revokes every registration and is logged once;
//   * registries for UFunction hooks, per-frame game-thread callbacks, key
//     binds and game-thread tasks, all keyed by host-owned ids, so the engine
//     only ever holds pointers to HOST code (never to mod code);
//   * revoke-then-drain-then-destroy on stop: no mod callback can be running or
//     start once destroy() (upstream uninstall_mod) is called; a stop requested
//     from inside the mod's own callback is deferred until that callback
//     returns; a drain that does not finish in time leaves the mod retained
//     (never destroyed under a running callback).
//   * stop listeners + CallScope, the interface #11 (GUI tabs) builds on.
#pragma once

#include <Mod/NativeModAbi.h>
#include <Platform/HookAdmission.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace RC::Platform::NativeMods
{
    // ------------------------------------------------------------------
    // Engine boundary. Everything here is implemented by the runtime
    // (NativeModEngineServices.inc) or by a host-test fake.
    // ------------------------------------------------------------------
    class EngineServices
    {
      public:
        virtual ~EngineServices() = default;
        virtual void log(int32_t level, std::string_view mod_name, std::string_view text) = 0;
        virtual auto facts() -> HookAdmission::Facts = 0;

        virtual auto find_object(const char* path, mbm_object* out) -> mbm_status = 0;
        virtual auto find_first_of(const char* class_name, mbm_object* out) -> mbm_status = 0;
        virtual auto find_all_of(const char* class_name, mbm_object* out, int32_t cap, int32_t* total) -> mbm_status = 0;
        virtual auto object_name(mbm_object obj, bool full, char* buf, int32_t cap, int32_t* len) -> mbm_status = 0;
        virtual auto object_class(mbm_object obj, mbm_object* out) -> mbm_status = 0;
        virtual auto object_is_a(mbm_object obj, const char* class_name, int32_t* out) -> mbm_status = 0;
        virtual auto object_is_valid(mbm_object obj, int32_t* out) -> mbm_status = 0;
        virtual auto property_get(mbm_object obj, const char* name, mbm_value* inout) -> mbm_status = 0;
        virtual auto property_set(mbm_object obj, const char* name, const mbm_value* in) -> mbm_status = 0;
        virtual auto property_address(mbm_object obj, const char* name, uint32_t type, void** out) -> mbm_status = 0;
        virtual auto call_function(mbm_object obj, const char* function, const mbm_value* args, int32_t argc, mbm_value* ret) -> mbm_status = 0;

        // Install an engine-side hook whose callbacks are HOST code that calls
        // Host::dispatch_ufunction(record, ...). Returns MBM_OK only when the
        // swap is installed now.
        virtual auto install_ufunction_hook(const char* function_path, uint64_t record, bool pre, bool post) -> mbm_status = 0;
        // Best effort; the host record is already inactive, so a hook the engine
        // cannot remove only ever reaches an inactive record.
        virtual void remove_ufunction_hook(uint64_t record) = 0;
        // Register a key bind whose callback is HOST code calling
        // Host::dispatch_key(record). The input handler cannot remove single
        // binds; inactive records make that harmless.
        virtual auto install_key(uint32_t key, uint32_t modifier_mask, uint64_t record) -> mbm_status = 0;
        virtual auto on_game_thread() -> bool = 0;
    };

    enum class ModState : uint8_t
    {
        Starting,  // inside the entry call (mod constructor)
        Running,
        Faulted,   // a callback failed: no more callbacks, destroy at stop
        Stopping,  // revoked, draining
        Stopped,   // destroy() returned
        Retained,  // drain timed out or destroy faulted: never touched again
    };

    inline auto to_string(ModState s) -> const char*
    {
        switch (s)
        {
        case ModState::Starting: return "starting";
        case ModState::Running: return "running";
        case ModState::Faulted: return "faulted";
        case ModState::Stopping: return "stopping";
        case ModState::Stopped: return "stopped";
        case ModState::Retained: return "retained";
        }
        return "?";
    }

    enum class Event : uint8_t
    {
        ProgramStart,
        UnrealInit,
        UiInit,
        Update,
        CppModsLoaded,
        DllLoad,
    };

    inline auto to_string(Event e) -> const char*
    {
        switch (e)
        {
        case Event::ProgramStart: return "on_program_start";
        case Event::UnrealInit: return "on_unreal_init";
        case Event::UiInit: return "on_ui_init";
        case Event::Update: return "on_update";
        case Event::CppModsLoaded: return "on_cpp_mods_loaded";
        case Event::DllLoad: return "on_dll_load";
        }
        return "?";
    }

    enum class PumpSource : uint8_t
    {
        GameThreadPump, // R3 FuncPtr-marshal carrier drain
        VmtEngineTick,  // copied-vtable UEngine::Tick drain (also delivers ENGINE_TICK)
    };

    // [FIX F5] At most one per-frame service frame (the #11 GUI render) per
    // engine frame, whatever the pump cadence. The pumps can be called many
    // times per frame (1700-2300 calls/s seen in earlier harvests) and both
    // pumps can be live at once. Rule:
    //   * once the copied-vtable Tick pump (exactly one call per UEngine::Tick)
    //     has been seen within `vmt_owner_window`, only it drives the frame;
    //   * otherwise the marshal pump drives it, at most once per
    //     `min_interval` (8 ms: never more than ~2 per frame at 60 fps and at
    //     most 1 per frame at 120 fps);
    //   * the VMT pump keeps a small floor too (`vmt_min_interval`), in case a
    //     future route calls it more than once per tick.
    // dt handed to the frame = time since the previous frame that ran.
    // Game thread only (both pumps run there); not thread-safe on purpose.
    class FrameGate
    {
      public:
        using Clock = std::chrono::steady_clock;
        struct Stats
        {
            uint64_t calls{}, due{}, skipped_interval{}, skipped_vmt_owned{};
        };
        explicit FrameGate(Clock::duration min_interval = std::chrono::milliseconds{8},
                           Clock::duration vmt_min_interval = std::chrono::milliseconds{4},
                           Clock::duration vmt_owner_window = std::chrono::milliseconds{250})
            : m_min(min_interval), m_vmt_min(vmt_min_interval), m_vmt_window(vmt_owner_window)
        {
        }
        auto due(Clock::time_point now, PumpSource source, float* dt_out) -> bool
        {
            ++m_stats.calls;
            if (source == PumpSource::VmtEngineTick)
            {
                m_vmt_seen = true;
                m_last_vmt = now;
            }
            else if (m_vmt_seen && now - m_last_vmt < m_vmt_window)
            {
                ++m_stats.skipped_vmt_owned;
                return false;
            }
            const auto floor = source == PumpSource::VmtEngineTick ? m_vmt_min : m_min;
            if (m_ran && now - m_last_run < floor)
            {
                ++m_stats.skipped_interval;
                return false;
            }
            const float dt = m_ran ? std::chrono::duration<float>(now - m_last_run).count() : 0.0f;
            m_ran = true;
            m_last_run = now;
            ++m_stats.due;
            if (dt_out) *dt_out = dt;
            return true;
        }
        auto stats() const -> const Stats& { return m_stats; }

      private:
        Clock::duration m_min, m_vmt_min, m_vmt_window;
        Clock::time_point m_last_run{}, m_last_vmt{};
        bool m_ran{}, m_vmt_seen{};
        Stats m_stats{};
    };

    struct Limits
    {
        size_t max_mods = 32;
        size_t max_registrations_per_mod = 256;
        size_t max_tasks_per_mod = 1024;
        std::chrono::milliseconds drain_timeout{2000};
    };

    class Host;

    namespace detail
    {
        // Per-thread stack of mod tokens whose callback is running on this
        // thread (re-entrancy detection for deferred stop).
        // [FIX B2] Trivially destructible on purpose: a thread_local with a
        // destructor (e.g. std::vector) makes clang emit __cxa_thread_atexit,
        // which pulls a libc++abi member carrying a NEW weak import
        // (__cxa_thread_atexit_impl) and fails the runtime's import-delta gate.
        // A fixed array + depth needs no TLS destructor and no TLS init guard.
        // Nesting deeper than this is refused (the callback does not run).
        inline constexpr uint32_t k_max_callback_depth = 32;
        struct ActiveTokens
        {
            uint64_t tokens[k_max_callback_depth];
            uint32_t depth;
        };
        static_assert(std::is_trivially_destructible_v<ActiveTokens>, "TLS stack must stay trivially destructible (no __cxa_thread_atexit)");
        inline constinit thread_local ActiveTokens t_active{};
        inline auto active_contains(uint64_t token) -> bool
        {
            for (uint32_t i = 0; i < t_active.depth; ++i)
                if (t_active.tokens[i] == token) return true;
            return false;
        }
        inline auto active_push(uint64_t token) -> bool
        {
            if (t_active.depth >= k_max_callback_depth) return false;
            t_active.tokens[t_active.depth++] = token;
            return true;
        }
        inline void active_pop()
        {
            if (t_active.depth) --t_active.depth;
        }
    }

    class Host
    {
      public:
        struct Counters
        {
            uint64_t lifecycle_calls{}, hook_calls{}, tick_calls{}, key_calls{}, task_calls{}, faults{}, refusals{};
        };

        explicit Host(EngineServices& engine, std::string build_id = "mbm-host", Limits limits = {})
            : m_engine(engine), m_build_id(std::move(build_id)), m_limits(limits)
        {
            std::memset(&m_table, 0, sizeof(m_table));
            m_table.magic = MBM_ABI_MAGIC;
            m_table.abi_version = MBM_ABI_VERSION;
            m_table.struct_size = sizeof(mbm_host_v1);
            m_table.host_build_id = m_build_id.c_str();
            m_table.ctx = this;
            m_table.log = &Host::abi_log;
            m_table.find_object = &Host::abi_find_object;
            m_table.find_first_of = &Host::abi_find_first_of;
            m_table.find_all_of = &Host::abi_find_all_of;
            m_table.object_name = &Host::abi_object_name;
            m_table.object_class = &Host::abi_object_class;
            m_table.object_is_a = &Host::abi_object_is_a;
            m_table.object_is_valid = &Host::abi_object_is_valid;
            m_table.property_get = &Host::abi_property_get;
            m_table.property_set = &Host::abi_property_set;
            m_table.property_address = &Host::abi_property_address;
            m_table.call_function = &Host::abi_call_function;
            m_table.hook_admission = &Host::abi_hook_admission;
            m_table.register_ufunction_hook = &Host::abi_register_ufunction_hook;
            m_table.register_hook = &Host::abi_register_hook;
            m_table.register_key_down = &Host::abi_register_key_down;
            m_table.execute_in_game_thread = &Host::abi_execute_in_game_thread;
            m_table.unregister = &Host::abi_unregister;
            m_table.gui = nullptr;
        }

        Host(const Host&) = delete;
        auto operator=(const Host&) -> Host& = delete;

        auto table() const -> const mbm_host_v1* { return &m_table; }
        auto engine() -> EngineServices& { return m_engine; }
        auto counters() const -> Counters
        {
            std::lock_guard g{m_mutex};
            return m_counters;
        }

        // #11 plugs its GUI table in before any mod is attached.
        void set_gui(const mbm_gui_v1* gui) { m_table.gui = gui; }

        // #11 (and any later per-mod registry) is told about every stop BEFORE
        // the drain, so it can revoke its own registrations for that mod.
        void add_stop_listener(std::function<void(uint64_t token)> fn)
        {
            std::lock_guard g{m_mutex};
            m_stop_listeners.push_back(std::move(fn));
        }

        // ---------------------------------------------------------------
        // Attach: validate and start one mod through its single export.
        // ---------------------------------------------------------------
        auto attach(std::string_view display_name, mbm_mod_entry_v1_fn entry, uint64_t* out_token, std::string* why = nullptr) -> mbm_status
        {
            auto fail = [&](mbm_status s, std::string text) {
                if (why) *why = text;
                m_engine.log(MBM_LOG_WARNING, display_name, "refused: " + text);
                std::lock_guard g{m_mutex};
                ++m_counters.refusals;
                return s;
            };
            if (!entry) return fail(MBM_E_INVALID, "module has no " MBM_ENTRY_SYMBOL " export");
            uint64_t token{};
            bool full{};
            {
                std::lock_guard g{m_mutex};
                // [FIX F3] A Stopped record is inert (destroy returned, no callback
                // can run or start): prune it so stop/attach cycles never exhaust
                // max_mods. Retained records stay (their code may still run).
                prune_stopped_locked();
                full = m_mods.size() >= m_limits.max_mods;
                if (!full)
                {
                    token = ++m_next_token;
                    auto rec = std::make_unique<ModRecord>();
                    rec->token = token;
                    rec->name = std::string(display_name);
                    rec->state = ModState::Starting;
                    m_mods.emplace(token, std::move(rec));
                    m_order.push_back(token);
                }
            }
            if (full) return fail(MBM_E_CAPACITY, "native mod limit reached"); // fail() takes the lock itself
            const mbm_mod_v1* mod{};
            mbm_status status{MBM_E_INTERNAL};
            {
                CallScope scope{*this, token, /*allow_starting=*/true};
                try
                {
                    status = entry(&m_table, token, &mod);
                }
                catch (...)
                {
                    status = MBM_E_MOD_FAULT; // the SDK should never let one escape
                }
            }
            std::string problem;
            if (status != MBM_OK)
                problem = "entry returned " + std::to_string(status);
            else if (!mod)
                problem = "entry returned no mod table";
            else if (mod->magic != MBM_ABI_MAGIC)
                problem = "mod table magic mismatch";
            else if (mod->abi_version != MBM_ABI_VERSION)
                problem = "mod ABI version " + std::to_string(mod->abi_version) + " (host " + std::to_string(MBM_ABI_VERSION) + ")";
            else if (mod->struct_size < sizeof(mbm_mod_v1))
                problem = "mod table too small (" + std::to_string(mod->struct_size) + " < " + std::to_string(sizeof(mbm_mod_v1)) + ")";
            else if (!mod->destroy)
                problem = "mod table has no destroy entry";
            if (!problem.empty())
            {
                // The mod may have registered things from its constructor: revoke.
                std::string text = problem;
                if (mod && mod->last_error)
                {
                    const char* e = safe_last_error(mod);
                    if (e && *e) text += std::string(": ") + e;
                }
                {
                    std::unique_lock lk{m_mutex};
                    auto* rec = find_locked(token);
                    if (rec)
                    {
                        rec->state = ModState::Retained; // never call into it again
                        revoke_locked(*rec, lk);
                    }
                }
                return fail(status == MBM_OK ? MBM_E_INVALID : status, text);
            }
            {
                std::lock_guard g{m_mutex};
                auto* rec = find_locked(token);
                rec->mod = mod;
                if (rec->state == ModState::Starting) rec->state = ModState::Running;
                if (mod->name && *mod->name) rec->reported_name = mod->name;
            }
            m_engine.log(MBM_LOG_NORMAL, display_name,
                         std::string("[NMOD] attached abi=") + std::to_string(MBM_ABI_VERSION) + " name='" + (mod->name ? mod->name : "") +
                                 "' version='" + (mod->version ? mod->version : "") + "' token=" + std::to_string(token));
            if (out_token) *out_token = token;
            return MBM_OK;
        }

        // Fire one lifecycle event on every running mod, in attach order
        // (used by #11 for on_ui_init when the GUI host comes up).
        auto fire_all(Event event) -> size_t
        {
            std::vector<uint64_t> order;
            {
                std::lock_guard g{m_mutex};
                order = m_order;
            }
            size_t n{};
            for (auto t : order)
                if (fire(t, event) == MBM_OK) ++n;
            return n;
        }

        auto state(uint64_t token) const -> ModState
        {
            std::lock_guard g{m_mutex};
            auto it = m_mods.find(token);
            return it == m_mods.end() ? ModState::Stopped : it->second->state;
        }

        // ---------------------------------------------------------------
        // Lifecycle events (UE4SS calls these through CppMod::fire_*).
        // ---------------------------------------------------------------
        auto fire(uint64_t token, Event event, const char* arg = nullptr) -> mbm_status
        {
            const mbm_mod_v1* mod{};
            {
                std::lock_guard g{m_mutex};
                auto* rec = find_locked(token);
                if (!rec || rec->state != ModState::Running || !rec->mod) return MBM_E_STATE;
                mod = rec->mod;
                ++m_counters.lifecycle_calls;
            }
            mbm_status (*fn)(void*) = nullptr;
            switch (event)
            {
            case Event::ProgramStart: fn = mod->on_program_start; break;
            case Event::UnrealInit: fn = mod->on_unreal_init; break;
            case Event::UiInit: fn = mod->on_ui_init; break;
            case Event::Update: fn = mod->on_update; break;
            case Event::CppModsLoaded: fn = mod->on_cpp_mods_loaded; break;
            case Event::DllLoad: break;
            }
            if (event == Event::DllLoad && !mod->on_dll_load) return MBM_OK;
            if (event != Event::DllLoad && !fn) return MBM_OK;
            mbm_status s{MBM_E_INTERNAL};
            {
                CallScope scope{*this, token};
                if (!scope.entered()) return MBM_E_STATE;
                try
                {
                    s = event == Event::DllLoad ? mod->on_dll_load(mod->instance, arg ? arg : "") : fn(mod->instance);
                }
                catch (...)
                {
                    s = MBM_E_MOD_FAULT;
                }
                if (s != MBM_OK) fault_in_scope(token, to_string(event), s, mod);
            }
            return s;
        }

        // ---------------------------------------------------------------
        // Game thread: called once per frame by the runtime's pump(s).
        // Runs the tasks queued BEFORE this call (tasks queued while running go
        // to the next frame, like upstream ExecuteInGameThread), then ticks.
        // ---------------------------------------------------------------
        void game_thread_frame(float delta_seconds, PumpSource source)
        {
            std::vector<Task> tasks;
            std::vector<Callback> ticks;
            {
                std::lock_guard g{m_mutex};
                m_pump_seen = true;
                tasks.swap(m_tasks);
                for (auto& [id, cb] : m_callbacks)
                {
                    if (!cb.active) continue;
                    if (cb.kind == Kind::Tick && (cb.family == MBM_HOOK_GAME_THREAD_TICK ||
                                                  (cb.family == MBM_HOOK_ENGINE_TICK && source == PumpSource::VmtEngineTick)))
                        ticks.push_back(cb);
                }
                // deterministic order = registration order
                std::sort(ticks.begin(), ticks.end(), [](const Callback& a, const Callback& b) { return a.id < b.id; });
                for (auto& t : tasks)
                    if (auto* rec = find_locked(t.token)) rec->queued_tasks = rec->queued_tasks ? rec->queued_tasks - 1 : 0;
            }
            for (auto& t : tasks)
            {
                CallScope scope{*this, t.token};
                if (!scope.entered()) continue;
                mbm_status s{MBM_E_INTERNAL};
                try { s = t.fn(t.user); } catch (...) { s = MBM_E_MOD_FAULT; }
                bump(&Counters::task_calls);
                if (s != MBM_OK) fault_in_scope(t.token, "execute_in_game_thread task", s, nullptr);
            }
            for (auto& cb : ticks)
            {
                if (!still_active(cb.id)) continue; // revoked by an earlier callback this frame
                CallScope scope{*this, cb.token};
                if (!scope.entered()) continue;
                mbm_status s{MBM_E_INTERNAL};
                try { s = cb.tick(cb.user, delta_seconds); } catch (...) { s = MBM_E_MOD_FAULT; }
                bump(&Counters::tick_calls);
                if (s != MBM_OK) fault_in_scope(cb.token, "tick", s, nullptr);
            }
        }

        // Engine-side hook trampolines call these with the HOST record id.
        void dispatch_ufunction(uint64_t record, bool pre, const mbm_hook_call& call)
        {
            Callback cb{};
            if (!snapshot(record, cb)) return;
            mbm_hook_fn fn = pre ? cb.pre : cb.post;
            if (!fn) return;
            CallScope scope{*this, cb.token};
            if (!scope.entered()) return;
            mbm_status s{MBM_E_INTERNAL};
            try { s = fn(cb.user, &call); } catch (...) { s = MBM_E_MOD_FAULT; }
            bump(&Counters::hook_calls);
            if (s != MBM_OK) fault_in_scope(cb.token, pre ? "ufunction pre-hook" : "ufunction post-hook", s, nullptr);
        }

        void dispatch_key(uint64_t record)
        {
            Callback cb{};
            if (!snapshot(record, cb) || !cb.key) return;
            CallScope scope{*this, cb.token};
            if (!scope.entered()) return;
            mbm_status s{MBM_E_INTERNAL};
            try { s = cb.key(cb.user); } catch (...) { s = MBM_E_MOD_FAULT; }
            bump(&Counters::key_calls);
            if (s != MBM_OK) fault_in_scope(cb.token, "key callback", s, nullptr);
        }

        // ---------------------------------------------------------------
        // Stop one mod: revoke -> notify listeners -> drain -> destroy.
        // Returns the final state.
        // ---------------------------------------------------------------
        auto stop(uint64_t token) -> ModState
        {
            // Re-entrant stop (from inside this mod's own callback on this
            // thread): revoke now, finish when the outermost callback returns.
            if (detail::active_contains(token))
            {
                std::unique_lock lk{m_mutex};
                auto* rec = find_locked(token);
                if (!rec) return ModState::Stopped;
                rec->stop_deferred = true;
                if (rec->state == ModState::Running || rec->state == ModState::Faulted || rec->state == ModState::Starting)
                {
                    rec->state = ModState::Stopping;
                    revoke_locked(*rec, lk);
                }
                return ModState::Stopping;
            }
            std::vector<std::function<void(uint64_t)>> listeners;
            const mbm_mod_v1* mod{};
            std::string name;
            {
                std::unique_lock lk{m_mutex};
                auto* rec = find_locked(token);
                if (!rec) return ModState::Stopped;
                if (rec->state == ModState::Stopped || rec->state == ModState::Retained) return rec->state;
                rec->state = ModState::Stopping;
                revoke_locked(*rec, lk);
                rec = find_locked(token);
                listeners = m_stop_listeners;
                mod = rec->mod;
                name = rec->name;
            }
            for (auto& l : listeners)
            {
                try { l(token); } catch (...) { m_engine.log(MBM_LOG_ERROR, name, "[NMOD] stop listener threw (contained)"); }
            }
            // Drain: wait until no callback of this mod runs on any thread.
            const auto deadline = std::chrono::steady_clock::now() + m_limits.drain_timeout;
            for (;;)
            {
                {
                    std::lock_guard g{m_mutex};
                    auto* rec = find_locked(token);
                    if (rec->in_flight == 0) break;
                }
                if (std::chrono::steady_clock::now() >= deadline)
                {
                    std::lock_guard g{m_mutex};
                    find_locked(token)->state = ModState::Retained;
                    m_engine.log(MBM_LOG_ERROR, name, "[NMOD] stop: callbacks still running after drain timeout; mod retained, destroy NOT called");
                    return ModState::Retained;
                }
                std::this_thread::yield();
            }
            return finish_destroy(token, mod, name);
        }

        // Stop every mod, newest first.
        void shutdown()
        {
            std::vector<uint64_t> order;
            {
                std::lock_guard g{m_mutex};
                order = m_order;
            }
            for (auto it = order.rbegin(); it != order.rend(); ++it) stop(*it);
        }

        // ---------------------------------------------------------------
        // Interface for per-mod services built on top (#11 GUI): run a mod
        // callback with the same in-flight accounting and fault policy.
        // ---------------------------------------------------------------
        class CallScope
        {
          public:
            CallScope(Host& host, uint64_t token, bool allow_starting = false) : m_host(host), m_token(token)
            {
                std::lock_guard g{host.m_mutex};
                auto* rec = host.find_locked(token);
                if (!rec) return;
                const bool ok = rec->state == ModState::Running || (allow_starting && rec->state == ModState::Starting);
                if (!ok) return;
                // [FIX B2] bounded per-thread nesting: refuse instead of growing.
                if (!detail::active_push(token))
                {
                    ++host.m_counters.refusals;
                    m_depth_refused = true;
                    return;
                }
                ++rec->in_flight;
                m_entered = true;
            }
            ~CallScope()
            {
                if (!m_entered) return;
                detail::active_pop();
                bool finish{};
                const mbm_mod_v1* mod{};
                std::string name;
                {
                    std::lock_guard g{m_host.m_mutex};
                    auto* rec = m_host.find_locked(m_token);
                    if (!rec) return; // cannot happen (entered => in_flight > 0 => not pruned); defensive
                    --rec->in_flight;
                    const bool outermost = !detail::active_contains(m_token);
                    if (rec->stop_deferred && rec->in_flight == 0 && outermost && rec->state == ModState::Stopping)
                    {
                        finish = true;
                        mod = rec->mod;
                        name = rec->name;
                        rec->stop_deferred = false;
                    }
                }
                if (finish) m_host.finish_deferred(m_token, mod, name);
            }
            CallScope(const CallScope&) = delete;
            auto operator=(const CallScope&) -> CallScope& = delete;
            auto entered() const -> bool { return m_entered; }
            auto depth_refused() const -> bool { return m_depth_refused; }

          private:
            Host& m_host;
            uint64_t m_token;
            bool m_entered{};
            bool m_depth_refused{};
        };

        // Fault a mod from a service built on top (#11), with the same policy.
        void fault(uint64_t token, const char* where, mbm_status status) { fault_in_scope(token, where, status, nullptr); }

        auto mod_name(uint64_t token) const -> std::string
        {
            std::lock_guard g{m_mutex};
            auto it = m_mods.find(token);
            return it == m_mods.end() ? std::string{} : it->second->name;
        }

        auto registration_count(uint64_t token) const -> size_t
        {
            std::lock_guard g{m_mutex};
            size_t n{};
            for (auto& [id, cb] : m_callbacks)
                if (cb.token == token && cb.active) ++n;
            return n;
        }

        // [FIX F4] entries in the callback table (active or not): revoked and
        // unregistered entries are erased, so this stays bounded by the active
        // registrations (max_registrations_per_mod per mod).
        auto callback_table_size() const -> size_t
        {
            std::lock_guard g{m_mutex};
            return m_callbacks.size();
        }
        auto mod_record_count() const -> size_t
        {
            std::lock_guard g{m_mutex};
            return m_mods.size();
        }

        auto admission(uint32_t family) -> HookAdmission::Decision { return HookAdmission::decide(family, m_engine.facts()); }

      private:
        enum class Kind : uint8_t
        {
            UFunction,
            Tick,
            Key
        };

        struct Callback
        {
            uint64_t id{};
            uint64_t token{};
            Kind kind{};
            uint32_t family{};
            bool active{};
            mbm_hook_fn pre{};
            mbm_hook_fn post{};
            mbm_tick_fn tick{};
            mbm_key_fn key{};
            void* user{};
        };

        struct Task
        {
            uint64_t token{};
            mbm_task_fn fn{};
            void* user{};
        };

        struct ModRecord
        {
            uint64_t token{};
            std::string name;
            std::string reported_name;
            ModState state{ModState::Starting};
            const mbm_mod_v1* mod{};
            uint32_t in_flight{};
            size_t queued_tasks{};
            bool stop_deferred{};
            bool fault_logged{};
        };

        auto find_locked(uint64_t token) const -> ModRecord*
        {
            auto it = m_mods.find(token);
            return it == m_mods.end() ? nullptr : it->second.get();
        }

        void bump(uint64_t Counters::*field)
        {
            std::lock_guard g{m_mutex};
            ++(m_counters.*field);
        }

        auto still_active(uint64_t id) -> bool
        {
            std::lock_guard g{m_mutex};
            auto it = m_callbacks.find(id);
            return it != m_callbacks.end() && it->second.active;
        }

        auto snapshot(uint64_t id, Callback& out) -> bool
        {
            std::lock_guard g{m_mutex};
            auto it = m_callbacks.find(id);
            if (it == m_callbacks.end() || !it->second.active) return false;
            out = it->second;
            return true;
        }

        // Deactivate every registration and drop queued tasks of a mod. The
        // engine removals run without the lock (they may call back).
        void revoke_locked(ModRecord& rec, std::unique_lock<std::mutex>& lk)
        {
            std::vector<uint64_t> engine_hooks;
            for (auto& [id, cb] : m_callbacks)
            {
                if (cb.token != rec.token || !cb.active) continue;
                cb.active = false;
                if (cb.kind == Kind::UFunction) engine_hooks.push_back(id);
            }
            m_tasks.erase(std::remove_if(m_tasks.begin(), m_tasks.end(), [&](const Task& t) { return t.token == rec.token; }), m_tasks.end());
            rec.queued_tasks = 0;
            lk.unlock();
            for (auto id : engine_hooks)
            {
                try { m_engine.remove_ufunction_hook(id); } catch (...) {}
            }
            lk.lock();
            // [FIX F4] erase (not only deactivate) once the engine hooks are
            // gone, so the per-pump walk stays bounded by ACTIVE registrations.
            // A late engine-side call with an erased id finds nothing and returns
            // (same as an inactive entry). Key binds cannot be removed from the
            // input handler; their record id simply no longer resolves.
            for (auto it = m_callbacks.begin(); it != m_callbacks.end();)
                it = (it->second.token == rec.token && !it->second.active) ? m_callbacks.erase(it) : std::next(it);
        }

        void prune_stopped_locked()
        {
            for (auto it = m_mods.begin(); it != m_mods.end();)
            {
                if (it->second->state == ModState::Stopped && it->second->in_flight == 0)
                {
                    const auto token = it->first;
                    m_order.erase(std::remove(m_order.begin(), m_order.end(), token), m_order.end());
                    it = m_mods.erase(it);
                }
                else
                    ++it;
            }
        }

        static auto safe_last_error(const mbm_mod_v1* mod) -> const char*
        {
            if (!mod || !mod->last_error) return nullptr;
            try { return mod->last_error(mod->instance); } catch (...) { return nullptr; }
        }

        void fault_in_scope(uint64_t token, const char* where, mbm_status status, const mbm_mod_v1* mod_hint)
        {
            std::string name;
            const mbm_mod_v1* mod = mod_hint;
            bool first{};
            {
                std::unique_lock lk{m_mutex};
                auto* rec = find_locked(token);
                if (!rec) return;
                if (!mod) mod = rec->mod;
                name = rec->name;
                if (rec->state == ModState::Running || rec->state == ModState::Starting) rec->state = ModState::Faulted;
                first = !rec->fault_logged;
                rec->fault_logged = true;
                ++m_counters.faults;
                revoke_locked(*rec, lk);
            }
            if (first)
            {
                const char* e = safe_last_error(mod);
                m_engine.log(MBM_LOG_ERROR, name,
                             std::string("[NMOD] fault in ") + where + " status=" + std::to_string(status) + (e && *e ? std::string(" error=") + e : std::string()) +
                                     "; mod disabled for this session (no further callbacks)");
            }
        }

        auto finish_destroy(uint64_t token, const mbm_mod_v1* mod, const std::string& name) -> ModState
        {
            mbm_status s{MBM_OK};
            if (mod && mod->destroy)
            {
                try { s = mod->destroy(mod->instance); } catch (...) { s = MBM_E_MOD_FAULT; }
            }
            std::lock_guard g{m_mutex};
            auto* rec = find_locked(token);
            rec->state = s == MBM_OK ? ModState::Stopped : ModState::Retained;
            m_engine.log(s == MBM_OK ? MBM_LOG_NORMAL : MBM_LOG_ERROR, name,
                         s == MBM_OK ? std::string("[NMOD] stopped (registrations revoked, callbacks drained, destroy returned)")
                                     : std::string("[NMOD] destroy failed status=") + std::to_string(s) + "; mod retained");
            return rec->state;
        }

        void finish_deferred(uint64_t token, const mbm_mod_v1* mod, const std::string& name)
        {
            std::vector<std::function<void(uint64_t)>> listeners;
            {
                std::lock_guard g{m_mutex};
                listeners = m_stop_listeners;
            }
            for (auto& l : listeners)
            {
                try { l(token); } catch (...) {}
            }
            finish_destroy(token, mod, name);
        }

        auto add_callback(uint64_t token, Callback cb, uint64_t* out_id) -> mbm_status
        {
            std::lock_guard g{m_mutex};
            auto* rec = find_locked(token);
            if (!rec || !(rec->state == ModState::Running || rec->state == ModState::Starting)) return MBM_E_STATE;
            size_t n{};
            for (auto& [id, c] : m_callbacks)
                if (c.token == token && c.active) ++n;
            if (n >= m_limits.max_registrations_per_mod) return MBM_E_CAPACITY;
            cb.id = ++m_next_id;
            cb.token = token;
            cb.active = true;
            m_callbacks.emplace(cb.id, cb);
            if (out_id) *out_id = cb.id;
            return MBM_OK;
        }

        // ---------------- ABI thunks (C function pointers) ----------------
        static auto self(void* ctx) -> Host* { return static_cast<Host*>(ctx); }

        template <typename F>
        static auto guard(void* ctx, F&& f) -> mbm_status
        {
            if (!ctx) return MBM_E_INVALID;
            try { return f(*self(ctx)); }
            catch (...) { return MBM_E_INTERNAL; }
        }

        auto known_token(uint64_t token) -> bool
        {
            std::lock_guard g{m_mutex};
            return find_locked(token) != nullptr;
        }

        // [FIX F6] Service gate: an unknown token is MBM_E_INVALID; a known mod
        // that is not Starting/Running (faulted, stopping, stopped, retained)
        // gets MBM_E_STATE, so "an error switches the mod off" also holds for
        // worker threads the mod started. Returns MBM_OK when the call may run.
        auto service_gate(uint64_t token) -> mbm_status
        {
            std::lock_guard g{m_mutex};
            auto* rec = find_locked(token);
            if (!rec) return MBM_E_INVALID;
            if (!(rec->state == ModState::Running || rec->state == ModState::Starting))
            {
                ++m_counters.refusals;
                return MBM_E_STATE;
            }
            return MBM_OK;
        }
        // log also stays open while destroy() runs (Stopping), so a mod's
        // destructor can still say goodbye; never after it faulted or stopped.
        auto log_allowed(uint64_t token) -> bool
        {
            std::lock_guard g{m_mutex};
            auto* rec = find_locked(token);
            return rec && (rec->state == ModState::Running || rec->state == ModState::Starting || rec->state == ModState::Stopping);
        }

        static void abi_log(void* ctx, uint64_t mod, int32_t level, const char* utf8)
        {
            if (!ctx || !utf8) return;
            try
            {
                auto& h = *self(ctx);
                if (!h.log_allowed(mod)) return; // [FIX F6]
                h.m_engine.log(level, h.mod_name(mod), utf8);
            }
            catch (...) {}
        }
        static auto abi_find_object(void* ctx, uint64_t mod, const char* path, mbm_object* out) -> mbm_status
        {
            return guard(ctx, [&](Host& h) -> mbm_status {
                if (!path || !out) return MBM_E_INVALID;
                if (const auto g = h.service_gate(mod); g != MBM_OK) return g; // [FIX F6]
                return h.m_engine.find_object(path, out);
            });
        }
        static auto abi_find_first_of(void* ctx, uint64_t mod, const char* name, mbm_object* out) -> mbm_status
        {
            return guard(ctx, [&](Host& h) -> mbm_status {
                if (!name || !out) return MBM_E_INVALID;
                if (const auto g = h.service_gate(mod); g != MBM_OK) return g; // [FIX F6]
                return h.m_engine.find_first_of(name, out);
            });
        }
        static auto abi_find_all_of(void* ctx, uint64_t mod, const char* name, mbm_object* out, int32_t cap, int32_t* total) -> mbm_status
        {
            return guard(ctx, [&](Host& h) -> mbm_status {
                if (!name || cap < 0 || (cap > 0 && !out) || !total) return MBM_E_INVALID;
                if (const auto g = h.service_gate(mod); g != MBM_OK) return g; // [FIX F6]
                return h.m_engine.find_all_of(name, out, cap, total);
            });
        }
        static auto abi_object_name(void* ctx, uint64_t mod, mbm_object obj, int32_t full, char* buf, int32_t cap, int32_t* len) -> mbm_status
        {
            return guard(ctx, [&](Host& h) -> mbm_status {
                if (!obj || cap < 0 || (cap > 0 && !buf) || !len) return MBM_E_INVALID;
                if (const auto g = h.service_gate(mod); g != MBM_OK) return g; // [FIX F6]
                return h.m_engine.object_name(obj, full != 0, buf, cap, len);
            });
        }
        static auto abi_object_class(void* ctx, uint64_t mod, mbm_object obj, mbm_object* out) -> mbm_status
        {
            return guard(ctx, [&](Host& h) -> mbm_status {
                if (!obj || !out) return MBM_E_INVALID;
                if (const auto g = h.service_gate(mod); g != MBM_OK) return g; // [FIX F6]
                return h.m_engine.object_class(obj, out);
            });
        }
        static auto abi_object_is_a(void* ctx, uint64_t mod, mbm_object obj, const char* name, int32_t* out) -> mbm_status
        {
            return guard(ctx, [&](Host& h) -> mbm_status {
                if (!obj || !name || !out) return MBM_E_INVALID;
                if (const auto g = h.service_gate(mod); g != MBM_OK) return g; // [FIX F6]
                return h.m_engine.object_is_a(obj, name, out);
            });
        }
        static auto abi_object_is_valid(void* ctx, uint64_t mod, mbm_object obj, int32_t* out) -> mbm_status
        {
            return guard(ctx, [&](Host& h) -> mbm_status {
                if (!out) return MBM_E_INVALID;
                if (const auto g = h.service_gate(mod); g != MBM_OK) return g; // [FIX F6]
                if (!obj) { *out = 0; return MBM_OK; }
                return h.m_engine.object_is_valid(obj, out);
            });
        }
        static auto abi_property_get(void* ctx, uint64_t mod, mbm_object obj, const char* name, mbm_value* v) -> mbm_status
        {
            return guard(ctx, [&](Host& h) -> mbm_status {
                if (!obj || !name || !v) return MBM_E_INVALID;
                if (const auto g = h.service_gate(mod); g != MBM_OK) return g; // [FIX F6]
                return h.m_engine.property_get(obj, name, v);
            });
        }
        static auto abi_property_set(void* ctx, uint64_t mod, mbm_object obj, const char* name, const mbm_value* v) -> mbm_status
        {
            return guard(ctx, [&](Host& h) -> mbm_status {
                if (!obj || !name || !v) return MBM_E_INVALID;
                if (const auto g = h.service_gate(mod); g != MBM_OK) return g; // [FIX F6]
                return h.m_engine.property_set(obj, name, v);
            });
        }
        static auto abi_property_address(void* ctx, uint64_t mod, mbm_object obj, const char* name, uint32_t type, void** out) -> mbm_status
        {
            return guard(ctx, [&](Host& h) -> mbm_status {
                if (!obj || !name || !out) return MBM_E_INVALID;
                if (const auto g = h.service_gate(mod); g != MBM_OK) return g; // [FIX F6]
                return h.m_engine.property_address(obj, name, type, out);
            });
        }
        static auto abi_call_function(void* ctx, uint64_t mod, mbm_object obj, const char* fn, const mbm_value* args, int32_t argc, mbm_value* ret) -> mbm_status
        {
            return guard(ctx, [&](Host& h) -> mbm_status {
                if (!obj || !fn || argc < 0 || (argc > 0 && !args)) return MBM_E_INVALID;
                if (const auto g = h.service_gate(mod); g != MBM_OK) return g; // [FIX F6]
                if (!h.m_engine.on_game_thread()) return MBM_E_STATE; // UFunctions run on the game thread only
                return h.m_engine.call_function(obj, fn, args, argc, ret);
            });
        }
        static auto abi_hook_admission(void* ctx, uint32_t family, mbm_admission* out) -> mbm_status
        {
            return guard(ctx, [&](Host& h) -> mbm_status {
                if (!out) return MBM_E_INVALID;
                const auto d = h.admission(family);
                out->family = family;
                out->route = d.route;
                out->live = d.live ? 1u : 0u;
                out->reserved = 0;
                out->route_name = d.route_name;
                out->reason = d.reason;
                return MBM_OK;
            });
        }
        static auto abi_register_ufunction_hook(void* ctx, uint64_t mod, const char* path, mbm_hook_fn pre, mbm_hook_fn post, void* user, uint64_t* out_id) -> mbm_status
        {
            return guard(ctx, [&](Host& h) -> mbm_status {
                if (!path || (!pre && !post)) return MBM_E_INVALID;
                if (const auto g = h.service_gate(mod); g != MBM_OK) return g; // [FIX F6]
                const auto d = h.admission(MBM_HOOK_UFUNCTION);
                if (!d.admitted() || !d.live)
                {
                    h.m_engine.log(MBM_LOG_WARNING, h.mod_name(mod), std::string("[NMOD] hook refused family=ufunction: ") + d.reason);
                    return MBM_E_REFUSED;
                }
                Callback cb{};
                cb.kind = Kind::UFunction;
                cb.family = MBM_HOOK_UFUNCTION;
                cb.pre = pre;
                cb.post = post;
                cb.user = user;
                uint64_t id{};
                if (auto s = h.add_callback(mod, cb, &id); s != MBM_OK) return s;
                const auto s = h.m_engine.install_ufunction_hook(path, id, pre != nullptr, post != nullptr);
                if (s != MBM_OK)
                {
                    std::lock_guard g{h.m_mutex};
                    h.m_callbacks.erase(id);
                    return s;
                }
                if (out_id) *out_id = id;
                return MBM_OK;
            });
        }
        static auto abi_register_hook(void* ctx, uint64_t mod, uint32_t family, mbm_tick_fn fn, void* user, uint64_t* out_id) -> mbm_status
        {
            return guard(ctx, [&](Host& h) -> mbm_status {
                if (!fn) return MBM_E_INVALID;
                if (const auto g = h.service_gate(mod); g != MBM_OK) return g; // [FIX F6]
                const auto d = h.admission(family);
                if (!d.admitted() || !HookAdmission::deliverable_by_register_hook(family))
                {
                    h.m_engine.log(MBM_LOG_WARNING, h.mod_name(mod),
                                   std::string("[NMOD] hook refused family=") + std::to_string(family) + " route=" + d.route_name + ": " +
                                           (d.admitted() ? "no ABI-v1 callback shape for this family (query hook_admission)" : d.reason));
                    return MBM_E_REFUSED;
                }
                Callback cb{};
                cb.kind = Kind::Tick;
                cb.family = family;
                cb.tick = fn;
                cb.user = user;
                return h.add_callback(mod, cb, out_id);
            });
        }
        static auto abi_register_key_down(void* ctx, uint64_t mod, uint32_t key, uint32_t mods, mbm_key_fn fn, void* user, uint64_t* out_id) -> mbm_status
        {
            return guard(ctx, [&](Host& h) -> mbm_status {
                if (!fn || key == 0 || key > 0xFF || (mods & ~0x7u)) return MBM_E_INVALID;
                if (const auto g = h.service_gate(mod); g != MBM_OK) return g; // [FIX F6]
                Callback cb{};
                cb.kind = Kind::Key;
                cb.key = fn;
                cb.user = user;
                uint64_t id{};
                if (auto s = h.add_callback(mod, cb, &id); s != MBM_OK) return s;
                const auto s = h.m_engine.install_key(key, mods, id);
                if (s != MBM_OK)
                {
                    std::lock_guard g{h.m_mutex};
                    h.m_callbacks.erase(id);
                    return s;
                }
                if (out_id) *out_id = id;
                return MBM_OK;
            });
        }
        static auto abi_execute_in_game_thread(void* ctx, uint64_t mod, mbm_task_fn fn, void* user) -> mbm_status
        {
            return guard(ctx, [&](Host& h) -> mbm_status {
                if (!fn) return MBM_E_INVALID;
                std::lock_guard g{h.m_mutex};
                auto* rec = h.find_locked(mod);
                if (!rec) return MBM_E_INVALID;
                if (!(rec->state == ModState::Running || rec->state == ModState::Starting)) return MBM_E_STATE;
                if (rec->queued_tasks >= h.m_limits.max_tasks_per_mod) return MBM_E_CAPACITY;
                ++rec->queued_tasks;
                h.m_tasks.push_back(Task{mod, fn, user});
                return MBM_OK;
            });
        }
        static auto abi_unregister(void* ctx, uint64_t mod, uint64_t id) -> mbm_status
        {
            return guard(ctx, [&](Host& h) -> mbm_status {
                bool engine_hook{};
                {
                    std::lock_guard g{h.m_mutex};
                    auto it = h.m_callbacks.find(id);
                    if (it == h.m_callbacks.end() || it->second.token != mod) return MBM_E_NOT_FOUND;
                    if (!it->second.active) return MBM_OK;
                    it->second.active = false;
                    engine_hook = it->second.kind == Kind::UFunction;
                }
                if (engine_hook)
                {
                    try { h.m_engine.remove_ufunction_hook(id); } catch (...) {}
                }
                // [FIX F4] erase after the engine hook is gone (bounded table).
                std::lock_guard g{h.m_mutex};
                h.m_callbacks.erase(id);
                return MBM_OK;
            });
        }

        EngineServices& m_engine;
        std::string m_build_id;
        Limits m_limits;
        mbm_host_v1 m_table{};
        mutable std::mutex m_mutex;
        std::unordered_map<uint64_t, std::unique_ptr<ModRecord>> m_mods;
        std::vector<uint64_t> m_order;
        std::unordered_map<uint64_t, Callback> m_callbacks;
        std::vector<Task> m_tasks;
        std::vector<std::function<void(uint64_t)>> m_stop_listeners;
        uint64_t m_next_token{};
        uint64_t m_next_id{};
        bool m_pump_seen{};
        Counters m_counters{};
    };
} // namespace RC::Platform::NativeMods
