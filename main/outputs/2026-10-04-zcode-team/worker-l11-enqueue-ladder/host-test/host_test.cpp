// HOST INTEGRATION TEST — worker-l11-enqueue-ladder (launch-11 §5.1 item 1).
//
// Drives the mod's ACTUAL default API: a real Lua 5.4 state (LuaRaw, compiled
// as C++ exactly like the shipped module per its CMakeLists LANGUAGE CXX) calls
// `ExecuteInGameThread(callback)` with ONE argument — the default-method path
// (SettingsManager.hpp:50 DefaultExecuteInGameThreadMethod = EngineTick; the
// main.lua:695 shape). The binding driven is the REAL staged registration code
// (binding.inc, extracted byte-exact from each lineage by extract_regions.py),
// on the REAL queue types, REAL process_simple_actions drain, REAL
// ensure_engine_tick_hooked / ensure_process_event_hooked (the traced throw
// sites), REAL port TRY (ExceptionHandling.hpp), and the REAL LuaMadeSimple
// wrapper. Only the PS5 platform surface (Unreal::Hook registration, address
// availability, Output device, FName identity) is a declared shim, each
// mirroring the W_FAILS_R_WORKS semantics recorded in the marshal-integration
// RUNBOOK §3.1/§5.1: address IS assigned (vtable fallback) while registration
// is refused.
//
// Worlds (L11EL_WORLD=control|fixed picks the region set at compile time):
//   S-ARMED   fixed: armed world   -> enqueued on the marshal queue + drained
//   S-WFAILS  fixed: W_FAILS world -> enqueued + loud stall log, queue survives
//   S-WORKS   fixed: hooks work    -> enqueued + armed + drained (no regression)
//   S-LADDER  both:  ET+PE unavailable, marshal armed -> ladder falls to the
//             marshal (pre-existing address-unavailable fallback; must survive)
//   S-CTRL-WFAILS control: W_FAILS world -> the recorded launch-11 loss
//             REPRODUCED: throw before enqueue, closure lost (test detects it)
//   S-CTRL-ARMED  control: armed world  -> same loss with the marshal armed and
//             bypassed (the exact RUNBOOK 5.1 finding)
//
// Compile with __PROSPERO__ defined: the regions' shipped PROSPERO branches are
// the code under test; the shims stand in for the platform API only.

#include <atomic>
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
#include <ExceptionHandling.hpp> // the REAL port TRY (single-arg)
#include <LuaMadeSimple/LuaMadeSimple.hpp>

#if !defined(L11EL_WORLD_FIXED) && !defined(L11EL_WORLD_CONTROL)
#error "define L11EL_WORLD_FIXED or L11EL_WORLD_CONTROL"
#endif

namespace RC
{
    // ------------------------------------------------------------------
    // The base GameThreadExecutionMethod enum (SettingsManager.hpp shape:
    // ProcessEvent=0, EngineTick=1). kFuncPtrMarshalExecutor(=2) comes from
    // the REAL marshal_state region below, exactly as staged.
    // ------------------------------------------------------------------
    enum class GameThreadExecutionMethod
    {
        ProcessEvent = 0,
        EngineTick = 1,
    };

    // ------------------------------------------------------------------
    // Harness knobs (the declared platform stand-ins)
    // ------------------------------------------------------------------
    static bool g_engine_tick_address_available = true;  // W_FAILS_R_WORKS: vtable fallback assigned the address
    static bool g_process_event_address_available = false;
    static bool g_registration_refused = true;           // BOTH_FAIL write-refusal initializer world
    static int32_t g_next_hook_id = 41;

    auto UE4SSRuntime::IsEngineTickAvailable() -> bool { return g_engine_tick_address_available; }
    auto UE4SSRuntime::IsProcessEventAvailable() -> bool { return g_process_event_address_available; }

    namespace Unreal
    {
        namespace Hook
        {
            static auto RegisterEngineTickPreCallback(EngineTickCallback, const RegistrationParams&) -> GlobalCallbackId
            {
                if (g_registration_refused) return Hook::ERROR_ID;
                return g_next_hook_id++;
            }
            static auto RegisterProcessEventPreCallback(ProcessEventCallback, const RegistrationParams&) -> GlobalCallbackId
            {
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
        struct SimpleLuaAction
        {
            const LuaMadeSimple::Lua* lua;
            int32_t lua_action_function_ref{};
            int32_t lua_action_thread_ref{};
        };

        static inline std::vector<SimpleLuaAction> m_game_thread_actions{};
        static inline std::vector<SimpleLuaAction> m_engine_tick_actions{};
        static inline std::vector<SimpleLuaAction> m_pending_game_thread_actions{};
        static inline std::vector<SimpleLuaAction> m_pending_engine_tick_actions{};
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

    // The binding code names `SimpleLuaAction` unqualified (class scope in the
    // real member function); the harness wrapper is a free function.
    using SimpleLuaAction = LuaMod::SimpleLuaAction;

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
#include "process_simple_actions.inc"

    // The REAL ExecuteInGameThread registration (binding.inc is a complete
    // `m_lua.register_function("ExecuteInGameThread", ...);` statement).
    static auto host_register_binding(LuaMadeSimple::Lua& m_lua) -> void
    {
#include "binding.inc"
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

    static auto capture_error_count() -> int64_t
    {
        int64_t count = 0;
        for (const auto& entry : output_capture())
        {
            if (entry.level >= LogLevel::Error) ++count;
        }
        return count;
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
    };

    static LuaMadeSimple::Lua* g_main_lua = nullptr;

    static auto setup_world(const World& world) -> LuaMadeSimple::Lua&
    {
        g_engine_tick_address_available = world.engine_tick_address_available;
        g_process_event_address_available = world.process_event_address_available;
        g_registration_refused = world.registration_refused;
        m_funcptr_marshal_armed.store(world.marshal_armed, std::memory_order_release);

        LuaMod::m_game_thread_actions.clear();
        LuaMod::m_engine_tick_actions.clear();
        LuaMod::m_pending_game_thread_actions.clear();
        LuaMod::m_pending_engine_tick_actions.clear();
        LuaMod::m_is_processing_actions = false;
        LuaMod::m_is_currently_executing_game_action = false;
        LuaMod::m_is_engine_tick_hooked = false;
        LuaMod::m_default_game_thread_method = GameThreadExecutionMethod::EngineTick; // SettingsManager.hpp:50 default
        g_harness_mod = LuaMod{};
        NativeProcessEventHookId = Unreal::Hook::ERROR_ID;
        output_capture().clear();
        g_next_hook_id = 41;

        // A REAL Lua state per scenario; the binding registers as a real global.
        auto& lua = LuaMadeSimple::new_state();
        luaL_openlibs(lua.get_lua_state());
        host_register_binding(lua); // registers global "ExecuteInGameThread"
        g_harness_mod.m_harness_main_lua = &lua;
        g_main_lua = &lua;
        return lua;
    }

    // The mod's ACTUAL default API call: ONE argument (closure) — default
    // method resolved from m_default_game_thread_method (main.lua:695 shape).
    static const char* kDefaultCallChunk =
        "ExecuteInGameThread(function() WITNESS = (WITNESS or 0) + 1 end)";

#if defined(L11EL_WORLD_FIXED)
    static auto scenario_armed_world() -> void
    {
        std::printf("[S-ARMED] fixed lineage, W_FAILS_R_WORKS + marshal armed (the launch-11 blocker world)\n");
        auto& lua = setup_world({true, false, true, true});
        std::string err;
        const bool chunk_ok = do_chunk(lua, kDefaultCallChunk, err);
        check(chunk_ok, "default-method ExecuteInGameThread does not throw (marshal outranks the address check)");
        check(LuaMod::m_game_thread_actions.size() == 1, "closure enqueued onto the marshal (game-thread) queue");
        check(LuaMod::m_engine_tick_actions.empty(), "EngineTick queue untouched (marshal path arms nothing)");
        check(!LuaMod::m_is_engine_tick_hooked, "no EngineTick detour was attempted while the marshal is armed");
        check(capture_error_count() == 0, "no error-level log lines in the armed world");
        process_simple_actions(LuaMod::m_game_thread_actions); // the marshal pump drain
        check(read_witness(lua) == 1, "marshal pump drained the closure (closure executed exactly once)");
        check(LuaMod::m_game_thread_actions.empty(), "marshal queue empty after drain");
    }

    static auto scenario_wfails_world() -> void
    {
        std::printf("[S-WFAILS] fixed lineage, W_FAILS world, marshal NOT armed\n");
        auto& lua = setup_world({true, false, true, false});
        std::string err;
        const bool chunk_ok = do_chunk(lua, kDefaultCallChunk, err);
        check(chunk_ok, "default-method ExecuteInGameThread does not throw (enqueue-first)");
        check(LuaMod::m_engine_tick_actions.size() == 1, "closure enqueued onto the EngineTick queue (queue survives arm failure)");
        check(!LuaMod::m_is_engine_tick_hooked, "EngineTick detour registration was refused (hooked flag stays false)");
        check(capture_has(LogLevel::Error, STR("closure enqueued but the EngineTick pump is not registered")),
              "loud stall log present: closure enqueued but the EngineTick pump is not registered");
        check(capture_has(LogLevel::Error, STR("EngineTick callback registration failed")),
              "the refused registration itself is logged loudly (TRY outputter)");
        // The stall: in this world the EngineTick detour never armed, so no pump
        // ever drains m_engine_tick_actions (the harness invokes NO pump here).
        check(LuaMod::m_engine_tick_actions.size() == 1, "closure stays queued (stalled, NOT discarded)");
        check(read_witness(lua) == 0, "closure has not executed (no drainer exists in this world)");
    }

    static auto scenario_works_world() -> void
    {
        std::printf("[S-WORKS] fixed lineage, hooks work (registration succeeds), marshal not armed\n");
        auto& lua = setup_world({true, true, false, false});
        std::string err;
        const bool chunk_ok = do_chunk(lua, kDefaultCallChunk, err);
        check(chunk_ok, "default-method ExecuteInGameThread succeeds");
        check(LuaMod::m_is_engine_tick_hooked, "EngineTick detour armed (normal world preserved)");
        check(LuaMod::m_engine_tick_actions.size() == 1, "closure enqueued onto the EngineTick queue");
        check(capture_error_count() == 0, "no stall log in the working world");
        process_simple_actions(LuaMod::m_engine_tick_actions); // the EngineTick pump drain
        check(read_witness(lua) == 1, "EngineTick pump drained the closure");
        check(LuaMod::m_engine_tick_actions.empty(), "EngineTick queue empty after drain");
    }
#endif

#if defined(L11EL_WORLD_FIXED) || defined(L11EL_WORLD_CONTROL)
    static auto scenario_ladder_unavailable_world() -> void
    {
#if defined(L11EL_WORLD_FIXED)
        std::printf("[S-LADDER] fixed lineage, ET+PE addresses unavailable, marshal armed (pre-existing fallback)\n");
#else
        std::printf("[S-LADDER] control lineage, ET+PE addresses unavailable, marshal armed (pre-existing fallback)\n");
#endif
        auto& lua = setup_world({false, false, true, true});
        std::string err;
        const bool chunk_ok = do_chunk(lua, kDefaultCallChunk, err);
        check(chunk_ok, "ladder falls to the armed marshal when no address is available (DIFF-1 fallback intact)");
        check(LuaMod::m_game_thread_actions.size() == 1, "closure enqueued onto the marshal queue");
        process_simple_actions(LuaMod::m_game_thread_actions);
        check(read_witness(lua) == 1, "marshal pump drained the closure");
    }
#endif

#if defined(L11EL_WORLD_CONTROL)
    static auto scenario_control_wfails_loss() -> void
    {
        std::printf("[S-CTRL-WFAILS] UNPATCHED control, W_FAILS world, marshal NOT armed\n");
        auto& lua = setup_world({true, false, true, false});
        std::string err;
        const bool chunk_ok = do_chunk(lua, kDefaultCallChunk, err);
        check(!chunk_ok, "the default call THROWS (recorded launch-11 blocker reproduced)");
        check(err.find("EngineTick callback registration failed") != std::string::npos,
              "the surfaced error is the traced throw site: EngineTick callback registration failed");
        check(LuaMod::m_engine_tick_actions.empty() && LuaMod::m_pending_engine_tick_actions.empty(),
              "EngineTick queue EMPTY: the closure never enqueued (enqueue loss)");
        check(LuaMod::m_game_thread_actions.empty() && LuaMod::m_pending_game_thread_actions.empty(),
              "marshal queue EMPTY: nothing enqueued anywhere");
        check(read_witness(lua) == 0, "closure lost: never executed");
    }

    static auto scenario_control_armed_loss() -> void
    {
        std::printf("[S-CTRL-ARMED] UNPATCHED control, W_FAILS_R_WORKS with the marshal ARMED (RUNBOOK 5.1 finding)\n");
        auto& lua = setup_world({true, false, true, true});
        std::string err;
        const bool chunk_ok = do_chunk(lua, kDefaultCallChunk, err);
        check(!chunk_ok, "the default call THROWS even though the marshal is armed");
        check(err.find("EngineTick callback registration failed") != std::string::npos,
              "throw site: ensure_engine_tick_hooked ran BEFORE any enqueue (address-availability trap)");
        check(m_funcptr_marshal_armed.load(std::memory_order_acquire), "the marshal is (still) armed");
        check(LuaMod::m_game_thread_actions.empty(), "the armed marshal was BYPASSED: nothing enqueued onto it");
        check(LuaMod::m_engine_tick_actions.empty(), "closure lost: no queue holds it");
        check(read_witness(lua) == 0, "the mod's F7 closure is lost (never drained)");
    }
#endif

    static auto run_all() -> int
    {
        std::printf("host integration test — worker-l11-enqueue-ladder\n");
#if defined(L11EL_WORLD_FIXED)
        std::printf("world: FIXED (staging luamod-l11-el-ladder/LuaMod.cpp f50976ae1e2626b0)\n");
#else
        std::printf("world: CONTROL (base worker-l11-marshal-integration/staging 04c9d91664dd82c7)\n");
#endif
        std::printf("platform define: __PROSPERO__ (shipped PROSPERO branches); real Lua 5.4 (C++ build), real LuaMadeSimple, real port TRY\n\n");

#if defined(L11EL_WORLD_FIXED)
        scenario_armed_world();
        scenario_wfails_world();
        scenario_works_world();
        scenario_ladder_unavailable_world();
#else
        scenario_control_wfails_loss();
        scenario_control_armed_loss();
        scenario_ladder_unavailable_world();
#endif

        std::printf("\n%s: %d checks passed, %d failed\n",
#if defined(L11EL_WORLD_FIXED)
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
