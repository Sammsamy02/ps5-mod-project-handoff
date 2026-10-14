// HOST INTEGRATION TEST — worker-r3-ladder (launch-11 RUNBOOK §5.1 item 1,
// the WithDelay-family remainder).
//
// Drives the mod's ACTUAL delay APIs through a real Lua 5.4 state (LuaRaw,
// compiled as C++ exactly like the shipped module): the REAL staged binding
// code for ExecuteInGameThreadWithDelay / RetriggerableExecuteInGameThreadWithDelay /
// ExecuteInGameThreadAfterFrames / LoopInGameThreadWithDelay /
// LoopInGameThreadAfterFrames (extracted byte-exact per lineage), on the REAL
// queue types, REAL ensure_engine_tick_hooked / ensure_process_event_hooked
// (the traced throw sites, hash-proven identical in both lineages), REAL port
// TRY, REAL LuaMadeSimple. The PS5 platform surface is a declared shim, each
// mirroring the recorded world semantics (marshal-integration RUNBOOK §3.1,
// §5.1; pump-chain REPORT M3): address availability and detour registration
// are independent knobs — in W_FAILS_R_WORKS the address IS assigned (vtable
// fallback, UnrealInitializer e7f349af :2051-2054/:2060-2063) while the
// registration is refused (write-gate).
//
// Predicate order under test (the task's three cases):
//   P1 marshal armed + drain alive        => NO throw; the closure is enqueued
//      tagged kFuncPtrMarshalExecutor; no detour is attempted (deterministic
//      marshal win even with the EngineTick address assigned — "both alive").
//   P2 marshal armed + drain provably dead (pump_pending=false, the rev-B
//      pairing contradiction) => LOUD refusal throw, nothing enqueued, no
//      detour attempted — with or without the tick address ("never silently
//      fall back").
//   P3 no pump backend at all (marshal not armed, ET+PE unavailable) => the
//      pre-existing third-rung throw fires — the ONLY unhooked-tick throw.
// Plus the AfterFrames family: a usable marshal cannot count frames, so the
// request is refused loudly and precisely instead of attempting the detour or
// silently stalling; not-armed working worlds arm + enqueue exactly as before.
//
// CONTROL world (unpatched c92c92ff): P1's world THROWS
// "EngineTick callback registration failed" BEFORE any enqueue — the armed
// marshal bypassed and the closure lost (the RUNBOOK §5.1 trap, reproduced).
//
// Worlds: L11RL_WORLD=control|fixed picks the region set at compile time.
// Compile with __PROSPERO__ defined: the shipped PROSPERO branches are the
// code under test; the shims stand in for the platform API only.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include <Unreal/Hook.hpp>
#include <Unreal/FName.hpp>
#include <UE4SSRuntime.hpp>
#include <DynamicOutput/Output.hpp>
#include <Helpers/String.hpp>
#include <ExceptionHandling.hpp> // the REAL port TRY
#include <LuaMadeSimple/LuaMadeSimple.hpp>

#if !defined(L11RL_WORLD_FIXED) && !defined(L11RL_WORLD_CONTROL)
#error "define L11RL_WORLD_FIXED or L11RL_WORLD_CONTROL"
#endif

namespace RC
{
    // ------------------------------------------------------------------
    // Base GameThreadExecutionMethod enum (SettingsManager.hpp shape:
    // ProcessEvent=0, EngineTick=1). kFuncPtrMarshalExecutor(=2) comes from
    // the REAL marshal_state region, exactly as staged.
    // ------------------------------------------------------------------
    enum class GameThreadExecutionMethod
    {
        ProcessEvent = 0,
        EngineTick = 1,
    };

    // The REAL WithDelay-family bindings assign action.status from this enum
    // (LuaMod.hpp DelayedActionStatus; only these names are touched). It is a
    // LuaMod member in the real header, and the bindings qualify it as
    // LuaMod::DelayedActionStatus — so the mirror declares it in-class.
    enum class DelayedActionStatus_scoped
    {
        Active,
        Paused,
        Executing,
        PendingRemoval,
    };

    // ------------------------------------------------------------------
    // Harness knobs (the declared platform stand-ins)
    // ------------------------------------------------------------------
    static bool g_engine_tick_address_available = true;  // W_FAILS_R_WORKS: vtable fallback assigned the address
    static bool g_process_event_address_available = false;
    static bool g_registration_refused = true;           // BOTH_FAIL write-refusal initializer world
    static bool g_r3_pump_pending = true;                // DIFF-2 armed fold: the initializer deferred its registrations
    static int32_t g_next_hook_id = 41;
    static int g_engine_tick_register_attempts = 0;      // detour-attempt witness (P1/P2 assert ZERO)
    static int g_process_event_register_attempts = 0;

    auto UE4SSRuntime::IsEngineTickAvailable() -> bool { return g_engine_tick_address_available; }
    auto UE4SSRuntime::IsProcessEventAvailable() -> bool { return g_process_event_address_available; }

    namespace Unreal
    {
        // The DIFF-1/DIFF-2 bridge, exactly as declared in the staged LuaMod.cpp
        // (namespace Unreal, local declaration, defined only under __PROSPERO__).
        auto R3MarshalPumpPending() -> bool { return g_r3_pump_pending; }

        namespace Hook
        {
            static auto RegisterEngineTickPreCallback(EngineTickCallback, const RegistrationParams&) -> GlobalCallbackId
            {
                ++g_engine_tick_register_attempts;
                if (g_registration_refused) return Hook::ERROR_ID;
                return g_next_hook_id++;
            }
            static auto RegisterProcessEventPreCallback(ProcessEventCallback, const RegistrationParams&) -> GlobalCallbackId
            {
                ++g_process_event_register_attempts;
                if (g_registration_refused) return Hook::ERROR_ID;
                return g_next_hook_id++;
            }
        } // namespace Hook
    } // namespace Unreal

    // ------------------------------------------------------------------
    // Minimal LuaMod mirror: exactly the members the REAL staged regions use
    // (names/semantics from the real LuaMod.hpp + LuaMod.cpp file scope).
    // ------------------------------------------------------------------
    class LuaMod
    {
      public:
        enum class DelayedActionStatus
        {
            Active,
            Paused,
            Executing,
            PendingRemoval,
        };

        struct SimpleLuaAction
        {
            const LuaMadeSimple::Lua* lua;
            int32_t lua_action_function_ref{};
            int32_t lua_action_thread_ref{};
        };

        struct DelayedGameThreadAction
        {
            const LuaMadeSimple::Lua* lua;
            int32_t lua_action_function_ref{};
            int32_t lua_action_thread_ref{};
            GameThreadExecutionMethod method{GameThreadExecutionMethod::EngineTick};
            DelayedActionStatus status{DelayedActionStatus::Active};
            std::chrono::steady_clock::time_point execute_at{};
            int64_t time_remaining_ms{0};
            int64_t frames_remaining{0};
            int64_t delay_ms{0};
            int64_t delay_frames{0};
            int64_t handle{0};
            bool is_retriggerable{false};
            bool is_looping{false};
            bool pause_after_execution{false};
        };

        static inline std::vector<SimpleLuaAction> m_game_thread_actions{};
        static inline std::vector<SimpleLuaAction> m_engine_tick_actions{};
        static inline std::vector<SimpleLuaAction> m_pending_game_thread_actions{};
        static inline std::vector<SimpleLuaAction> m_pending_engine_tick_actions{};
        static inline std::vector<DelayedGameThreadAction> m_delayed_game_thread_actions{};
        static inline std::vector<DelayedGameThreadAction> m_pending_delayed_game_thread_actions{};
        static inline int64_t m_next_delayed_action_handle{1};
        static inline bool m_is_processing_actions{};
        static inline bool m_is_currently_executing_game_action{};
        static inline bool m_is_engine_tick_hooked{};
        static inline std::recursive_mutex m_thread_actions_mutex;
        static inline GameThreadExecutionMethod m_default_game_thread_method{GameThreadExecutionMethod::EngineTick};

        bool m_is_process_event_hooked{};
        LuaMadeSimple::Lua* m_hook_lua = nullptr;
        LuaMadeSimple::Lua* m_harness_main_lua = nullptr;

        auto lua() const -> LuaMadeSimple::Lua& { return *m_harness_main_lua; }

        // Declared here so the REAL staged member definitions in ensure_hooks.inc
        // (namespace-scope `auto LuaMod::ensure_*`) have their in-class declarations.
        static auto ensure_engine_tick_hooked() -> void;
        static auto ensure_process_event_hooked(LuaMod* mod) -> void;

        static auto engine_tick_hook(Unreal::Hook::TCallbackIterationData<void>&, Unreal::UEngine*, float, bool) -> void {}
        static auto process_event_hook(Unreal::Hook::TCallbackIterationData<void>&, Unreal::UObject*, Unreal::UFunction*, void*) -> void {}
    };

    // The binding code names these unqualified (class scope in the real member
    // function); the harness wrapper is a free function.
    using SimpleLuaAction = LuaMod::SimpleLuaAction;
    using DelayedGameThreadAction = LuaMod::DelayedGameThreadAction;

    // File-scope state the REAL ensure_process_event_hooked definition uses
    // (mirrors the staged LuaMod.cpp file scope).
    static Unreal::Hook::GlobalCallbackId NativeProcessEventHookId{Unreal::Hook::ERROR_ID};

    // get_mod_ref: the binding's mod lookup; the harness has one mod.
    static LuaMod g_harness_mod{};
    static auto get_mod_ref(const LuaMadeSimple::Lua&) -> LuaMod* { return &g_harness_mod; }

    // ==================================================================
    // THE REAL STAGED REGIONS (byte-exact, per-world region set)
    // ==================================================================
#include "marshal_state.inc"
#include "availability.inc"
#include "hook_state.inc"
#include "ensure_hooks.inc"

    // The REAL WithDelay-family registrations (each *.inc is a complete
    // `m_lua.register_function("...", ...);` statement).
    static auto host_register_bindings(LuaMadeSimple::Lua& m_lua) -> void
    {
#include "binding_withdelay.inc"
#include "binding_retriggerable.inc"
#include "binding_afterframes.inc"
#include "binding_loopwithdelay.inc"
#include "binding_loopafterframes.inc"
    }

    // ==================================================================
    // Scenario driver
    // ==================================================================
    static int g_checks_passed = 0;
    static int g_checks_failed = 0;
    static std::vector<std::string> g_failures{};

    static auto check(bool condition, const std::string& what) -> void
    {
        if (condition)
        {
            ++g_checks_passed;
            std::printf("    ok: %s\n", what.c_str());
        }
        else
        {
            ++g_checks_failed;
            g_failures.push_back(what);
            std::printf("    FAIL: %s\n", what.c_str());
        }
    }

    static auto capture_has(LogLevel level, const std::wstring& needle) -> bool
    {
        for (const auto& entry : output_capture())
        {
            if (entry.level == level && entry.text.find(needle) != std::wstring::npos)
            {
                return true;
            }
        }
        return false;
    }

    static auto do_chunk(LuaMadeSimple::Lua& lua, const char* chunk, std::string& err) -> bool
    {
        lua_State* L = lua.get_lua_state();
        if (luaL_loadstring(L, chunk) != LUA_OK)
        {
            err = lua_tostring(L, -1) ? lua_tostring(L, -1) : "(load failed)";
            lua_pop(L, 1);
            return false;
        }
        if (lua_pcall(L, 0, 0, 0) != LUA_OK)
        {
            err = lua_tostring(L, -1) ? lua_tostring(L, -1) : "(runtime error)";
            lua_pop(L, 1);
            return false;
        }
        return true;
    }

    static auto read_witness(LuaMadeSimple::Lua& lua) -> int64_t
    {
        lua_State* L = lua.get_lua_state();
        lua_getglobal(L, "WITNESS");
        const int64_t value = lua_isinteger(L, -1) ? lua_tointeger(L, -1) : 0;
        lua_pop(L, 1);
        return value;
    }

    struct World
    {
        bool engine_tick_address_available;
        bool process_event_address_available;
        bool registration_refused;
        bool marshal_armed;
        bool pump_pending;
    };

    static LuaMadeSimple::Lua* g_main_lua = nullptr;

    static auto setup_world(const World& world) -> LuaMadeSimple::Lua&
    {
        g_engine_tick_address_available = world.engine_tick_address_available;
        g_process_event_address_available = world.process_event_address_available;
        g_registration_refused = world.registration_refused;
        g_r3_pump_pending = world.pump_pending;
        m_funcptr_marshal_armed.store(world.marshal_armed, std::memory_order_release);

        LuaMod::m_game_thread_actions.clear();
        LuaMod::m_engine_tick_actions.clear();
        LuaMod::m_pending_game_thread_actions.clear();
        LuaMod::m_pending_engine_tick_actions.clear();
        LuaMod::m_delayed_game_thread_actions.clear();
        LuaMod::m_pending_delayed_game_thread_actions.clear();
        LuaMod::m_next_delayed_action_handle = 1;
        LuaMod::m_is_processing_actions = false;
        LuaMod::m_is_currently_executing_game_action = false;
        LuaMod::m_is_engine_tick_hooked = false;
        LuaMod::m_default_game_thread_method = GameThreadExecutionMethod::EngineTick; // SettingsManager.hpp default
        g_harness_mod = LuaMod{};
        g_harness_mod.m_is_process_event_hooked = false;
        NativeProcessEventHookId = Unreal::Hook::ERROR_ID;
        output_capture().clear();
        g_next_hook_id = 41;
        g_engine_tick_register_attempts = 0;
        g_process_event_register_attempts = 0;

        // A REAL Lua state per scenario; the bindings register as real globals.
        auto& lua = LuaMadeSimple::new_state();
        luaL_openlibs(lua.get_lua_state());
        host_register_bindings(lua);
        g_harness_mod.m_harness_main_lua = &lua;
        g_main_lua = &lua;
        return lua;
    }

    // The mod-shaped delay calls (time-based default-method path; the
    // WithDelay family reads ONLY m_default_game_thread_method — no explicit
    // method argument exists on these APIs).
    static const char* kWithDelayChunk =
        "ExecuteInGameThreadWithDelay(50, function() WITNESS = (WITNESS or 0) + 1 end)";
    static const char* kRetriggerableChunk =
        "RetriggerableExecuteInGameThreadWithDelay(7, 50, function() WITNESS = (WITNESS or 0) + 1 end)";
    static const char* kLoopChunk =
        "LoopInGameThreadWithDelay(50, function() WITNESS = (WITNESS or 0) + 1 end)";
    static const char* kAfterFramesChunk =
        "ExecuteInGameThreadAfterFrames(3, function() WITNESS = (WITNESS or 0) + 1 end)";
    static const char* kLoopAfterFramesChunk =
        "LoopInGameThreadAfterFrames(3, function() WITNESS = (WITNESS or 0) + 1 end)";

    static auto counts_total_delayed() -> size_t
    {
        return LuaMod::m_delayed_game_thread_actions.size() + LuaMod::m_pending_delayed_game_thread_actions.size();
    }

#if defined(L11RL_WORLD_FIXED)
    // ------------------------------------------------------------------
    // FIXED world scenarios
    // ------------------------------------------------------------------
    static auto scenario_armed_withdelay_family() -> void
    {
        std::printf("[P1] fixed, marshal armed + drain alive, tick address ALSO assigned (both alive)\n");
        auto& lua = setup_world({true, false, true, true, true});
        std::string err;
        bool ok = do_chunk(lua, kWithDelayChunk, err);
        check(ok, "ExecuteInGameThreadWithDelay does not throw (marshal outranks the address check)");
        ok = do_chunk(lua, kRetriggerableChunk, err);
        check(ok, "RetriggerableExecuteInGameThreadWithDelay does not throw");
        ok = do_chunk(lua, kLoopChunk, err);
        check(ok, "LoopInGameThreadWithDelay does not throw");
        check(counts_total_delayed() == 3, "all three closures enqueued onto the delayed queue");
        bool all_marshal_tagged = true;
        for (const auto& action : LuaMod::m_delayed_game_thread_actions)
        {
            if (action.method != kFuncPtrMarshalExecutor) all_marshal_tagged = false;
        }
        check(all_marshal_tagged, "every queued action is tagged kFuncPtrMarshalExecutor (marshal wins deterministically over the assigned tick address)");
        check(g_engine_tick_register_attempts == 0, "ZERO EngineTick detour attempts while the usable marshal exists");
        check(g_process_event_register_attempts == 0, "ZERO ProcessEvent detour attempts");
        check(!LuaMod::m_is_engine_tick_hooked, "no EngineTick detour was armed");
        check(read_witness(lua) == 0, "closures await the marshal drain (not executed at enqueue time)");
    }

    static auto scenario_dead_drain_withdelay() -> void
    {
        std::printf("[P2a] fixed, marshal ARMED but drain PROVABLY DEAD (pump_pending=false), no tick address\n");
        auto& lua = setup_world({false, false, true, true, false});
        std::string err;
        const bool ok = do_chunk(lua, kWithDelayChunk, err);
        check(!ok, "ExecuteInGameThreadWithDelay refuses loudly (no silent queueing onto a dead pump)");
        check(err.find("its drain is dead (pump_pending=false)") != std::string::npos,
              "the surfaced error names the dead drain: pump_pending=false refusal");
        check(counts_total_delayed() == 0, "NOTHING was enqueued (fail-closed, not queued-forever)");
        check(g_engine_tick_register_attempts == 0, "no EngineTick detour attempt (no second pump armed)");
        check(m_funcptr_marshal_armed.load(std::memory_order_acquire), "the marshal stays armed (the refusal does not disarm anything)");
    }

    static auto scenario_dead_drain_tick_assigned() -> void
    {
        std::printf("[P2b] fixed, marshal ARMED but drain dead, tick address assigned (fallback would be silent)\n");
        auto& lua = setup_world({true, false, true, true, false});
        std::string err;
        const bool ok = do_chunk(lua, kWithDelayChunk, err);
        check(!ok, "the dead-drain refusal holds even when the tick address is assigned (never silently falls back)");
        check(err.find("its drain is dead (pump_pending=false)") != std::string::npos,
              "the dead-drain refusal (not the ensure throw) is what surfaced");
        check(counts_total_delayed() == 0, "nothing enqueued");
        check(g_engine_tick_register_attempts == 0, "no detour attempt");
    }

    static auto scenario_no_backend_at_all() -> void
    {
        std::printf("[P3] fixed, marshal NOT armed, ET+PE addresses unavailable (no pump backend at all)\n");
        auto& lua = setup_world({false, false, true, false, false});
        std::string err;
        const bool ok = do_chunk(lua, kWithDelayChunk, err);
        check(!ok, "the call throws (throw-on-unhooked-tick preserved when NO backend exists)");
        check(err.find("Neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed") != std::string::npos,
              "the surfaced error is the pre-existing third-rung throw");
        check(counts_total_delayed() == 0, "nothing enqueued");
    }

    static auto scenario_works_world_regression() -> void
    {
        std::printf("[R1] fixed, marshal NOT armed, registrations WORK (normal world preserved)\n");
        auto& lua = setup_world({true, false, false, false, false});
        std::string err;
        const bool ok = do_chunk(lua, kWithDelayChunk, err);
        check(ok, "ExecuteInGameThreadWithDelay succeeds in the working world");
        check(LuaMod::m_is_engine_tick_hooked, "EngineTick detour armed (normal-world behavior intact)");
        check(counts_total_delayed() == 1, "the delayed action was enqueued");
        check(LuaMod::m_delayed_game_thread_actions[0].method == GameThreadExecutionMethod::EngineTick,
              "the action is tagged EngineTick (not diverted to the marshal)");
    }

    static auto scenario_afterframes_marshal_world() -> void
    {
        std::printf("[P4] fixed, usable marshal, frame-based requests (AfterFrames family)\n");
        auto& lua = setup_world({true, false, true, true, true});
        std::string err;
        bool ok = do_chunk(lua, kAfterFramesChunk, err);
        check(!ok, "ExecuteInGameThreadAfterFrames refuses loudly in the marshal world");
        check(err.find("its drain cannot count frames") != std::string::npos,
              "the refusal names the capability gap: the marshal drain cannot count frames");
        std::string err2;
        ok = do_chunk(lua, kLoopAfterFramesChunk, err2);
        check(!ok, "LoopInGameThreadAfterFrames refuses loudly as well");
        check(err2.find("its drain cannot count frames") != std::string::npos, "loop refusal names the same capability gap");
        check(counts_total_delayed() == 0, "no frame action was silently stalled on the marshal");
        check(g_engine_tick_register_attempts == 0, "no EngineTick detour attempt in the marshal world");
    }

    static auto scenario_afterframes_works_world() -> void
    {
        std::printf("[R2] fixed, marshal NOT armed, registrations WORK (AfterFrames normal world)\n");
        auto& lua = setup_world({true, false, false, false, false});
        std::string err;
        const bool ok = do_chunk(lua, kAfterFramesChunk, err);
        check(ok, "ExecuteInGameThreadAfterFrames succeeds in the working world");
        check(LuaMod::m_is_engine_tick_hooked, "EngineTick detour armed");
        check(counts_total_delayed() == 1, "frame action enqueued");
        check(LuaMod::m_delayed_game_thread_actions[0].method == GameThreadExecutionMethod::EngineTick,
              "frame action tagged EngineTick (frames remain EngineTick-only, as the drain requires)");
    }

    static auto scenario_afterframes_dead_and_notick() -> void
    {
        std::printf("[P5] fixed, marshal armed-but-dead + no tick address (AfterFrames family)\n");
        auto& lua = setup_world({false, false, true, true, false});
        std::string err;
        const bool ok = do_chunk(lua, kAfterFramesChunk, err);
        check(!ok, "ExecuteInGameThreadAfterFrames throws (no pump can serve frames)");
        check(counts_total_delayed() == 0, "nothing enqueued");
        check(g_engine_tick_register_attempts == 0, "no detour attempt");
    }
#endif

#if defined(L11RL_WORLD_FIXED) || defined(L11RL_WORLD_CONTROL)
    static auto scenario_ladder_unavailable_control_shape() -> void
    {
#if defined(L11RL_WORLD_FIXED)
        std::printf("[P3f] fixed lineage check of the no-backend throw (identical to control)\n");
#else
        std::printf("[C-NOBACKEND] control lineage, no backend at all\n");
#endif
        auto& lua = setup_world({false, false, true, false, false});
        std::string err;
        const bool ok = do_chunk(lua, kRetriggerableChunk, err);
        check(!ok, "RetriggerableExecuteInGameThreadWithDelay throws with no backend at all");
        check(err.find("Neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed") != std::string::npos,
              "third-rung throw intact");
    }
#endif

#if defined(L11RL_WORLD_CONTROL)
    static auto scenario_control_armed_withdelay_loss() -> void
    {
        std::printf("[C-ARMED-WD] UNPATCHED control, marshal ARMED + tick address assigned + registration refused (RUNBOOK 5.1 world)\n");
        auto& lua = setup_world({true, false, true, true, true});
        std::string err;
        const bool ok = do_chunk(lua, kWithDelayChunk, err);
        check(!ok, "the WithDelay call THROWS even though the marshal is armed and usable");
        check(err.find("EngineTick callback registration failed") != std::string::npos,
              "throw site: ensure_engine_tick_hooked ran BEFORE any enqueue (availability-first trap)");
        check(m_funcptr_marshal_armed.load(std::memory_order_acquire), "the marshal is (still) armed");
        check(counts_total_delayed() == 0, "the armed marshal was BYPASSED: nothing enqueued anywhere (closure lost)");
        check(g_engine_tick_register_attempts == 1, "the control attempted the forbidden detour exactly once");
        check(read_witness(lua) == 0, "the closure is lost (never drained)");
    }

    static auto scenario_control_armed_afterframes_loss() -> void
    {
        std::printf("[C-ARMED-AF] UNPATCHED control, same world, frame-based request\n");
        auto& lua = setup_world({true, false, true, true, true});
        std::string err;
        const bool ok = do_chunk(lua, kAfterFramesChunk, err);
        check(!ok, "ExecuteInGameThreadAfterFrames THROWS in the marshal world (availability-first)");
        check(err.find("EngineTick callback registration failed") != std::string::npos,
              "throw site: the ensure-before-enqueue trap on the frame path too");
        check(counts_total_delayed() == 0, "closure lost: nothing enqueued");
        check(read_witness(lua) == 0, "the closure is lost");
    }
#endif

    static auto run_all() -> int
    {
        std::printf("host integration test — worker-r3-ladder\n");
#if defined(L11RL_WORLD_FIXED)
        std::printf("world: FIXED (staging luamod-l11-r3-ladder/LuaMod.cpp 6f97f8a1727b7e49)\n");
#else
        std::printf("world: CONTROL (base worker-l11-merge/staging luamod-l11-merged/LuaMod.cpp c92c92ff66686bf6, UNPATCHED)\n");
#endif
        std::printf("platform define: __PROSPERO__ (shipped PROSPERO branches); real Lua 5.4 (C++ build), real LuaMadeSimple, real port TRY\n\n");

#if defined(L11RL_WORLD_FIXED)
        scenario_armed_withdelay_family();
        scenario_dead_drain_withdelay();
        scenario_dead_drain_tick_assigned();
        scenario_no_backend_at_all();
        scenario_works_world_regression();
        scenario_afterframes_marshal_world();
        scenario_afterframes_works_world();
        scenario_afterframes_dead_and_notick();
        scenario_ladder_unavailable_control_shape();
#else
        scenario_control_armed_withdelay_loss();
        scenario_control_armed_afterframes_loss();
        scenario_ladder_unavailable_control_shape();
#endif

        std::printf("\n%s: %d checks passed, %d failed\n",
#if defined(L11RL_WORLD_FIXED)
                    "FIXED WORLD",
#else
                    "CONTROL WORLD",
#endif
                    g_checks_passed, g_checks_failed);
        if (g_checks_failed > 0)
        {
            for (const auto& failure : g_failures)
            {
                std::printf("  FAILED: %s\n", failure.c_str());
            }
        }
        return g_checks_failed == 0 ? 0 : 1;
    }
} // namespace RC

int main()
{
    return RC::run_all();
}
