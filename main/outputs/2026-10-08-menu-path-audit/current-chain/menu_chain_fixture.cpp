// HOST FIXTURE — worker-menu-chain (the first F7/OPTIONS press, as ONE chain).
//
// Chains, on the macOS host, with REAL code end to end:
//   simulated F7 key event (REAL RingBufferSPSC queue + REAL Input::Handler
//   dispatch) -> the REAL RegisterKeyBind dispatch glue (LuaMod c92c92ff
//   binding: keydown callback -> registry ref -> call_function) -> main.lua's
//   ACTUAL F7 callback (canonical package-v3 main.lua f586043a..., loaded
//   byte-exact into a REAL vendored Lua 5.4.7 state) -> the REAL
//   ExecuteInGameThread binding (worker-r3-ladder staged unit 6f97f8a1...,
//   EL enqueue-ladder fold: marshal-first-when-armed) -> the REAL
//   funcptr_marshal_hook drain body (publication gate -> IsInGameThreadRaw ->
//   process_simple_actions) -> toggleMenu/showMenu/scanItems/buildUI running
//   for real until the first engine-object lookup that gates menu creation.
//
// Scenarios:
//   S1  R3 world (tick address assigned via vtable fallback, registration
//       write-refused, marshal armed, initializer deferred the pumps):
//       F7 -> marshal-first enqueue -> carrier invocation -> drain ->
//       menu chain runs to the in-game-only boundary
//       (FindFirstOf("PlayerController") at main.lua:21 via buildUI:493).
//   S1b same world + a recorded PlayerController stub armed: proves the chain
//       stops at the NEXT engine gate instead
//       (StaticFindObject UMG WidgetBlueprintLibrary, main.lua:499).
//   S2  NEGATIVE CONTROL: marshal NOT armed, tick+PE unavailable -> the loud
//       no-backend throw surfaces through the keybind lambda's error logging;
//       nothing enqueued, nothing executed (fail-closed, not a silent drop).
//   S3  marshal NOT armed, tick address assigned, registration refused (the
//       W_FAILS_R_WORKS world without a marshal): enqueue-first + the loud
//       stall line; the closure stays queued, never silently dropped.
//
// Declared stubs (each recorded; engine/platform-side only — see REPORT):
//   - UE4SSRuntime::Is*Available: address-availability knobs (r3-ladder shim).
//   - Unreal::Hook::Register*PreCallback: registration-refusal knobs + attempt
//     counters (r3-ladder shim).
//   - Unreal R3 bridges (R3MarshalPumpPending / R3MarshalPublishGameThread
//     Identity / R3MarshalQueryHarnessDispatch / IsInGameThreadRaw): the
//     initializer-side bridge surface; single-thread fixture, the carrier-
//     invoking thread becomes the game thread (calibration criterion C-4).
//   - SimulatedPadSource: PlatformInputSource for the genuine PS5 pad provider
//     (NativePadInput route); ONLY is_available() differs from the REAL
//     QueueInputSource (whose producer-less is_available() hard-codes false).
//   - get_mod_ref: returns the harness LuaMod (the real one reads the ModRef
//     userdata through the LuaType::LuaModRef layer).
//   - process_pending_notify_on_new_object_callbacks: the real body walks
//     GUObjectArray entries; the host has no UObject array and this chain
//     enqueues nothing, so the stub counts the drain pass only.
//   - engine_tick_hook / process_event_hook bodies: pump bodies for hooks this
//     chain never drives (ProcessEvent is not on the F7 path; the EngineTick
//     detour is refused in every scenario).
//   - Lua-side engine lookups: FindFirstOf / StaticFindObject /
//     StaticConstructObject / FName / FText / LoadAsset record every call and
//     return "no game objects exist on the host" (nil), except the recorded
//     PlayerController stub in S1b. RegisterLoadMapPreHook records the
//     registration (runtime-owned load-map event source). `require` is REAL
//     Lua: "UEHelpers" is absent on the host exactly as it is a runtime-bundled
//     module in-game; main.lua pcalls it and tolerates absence.
//   - print: real print PLUS a host capture (the fixture's oracle surface).

#include <atomic>
#include <algorithm>
#include <unordered_set>
extern "C" void diagnostic_event(const char*, unsigned int) {}
namespace RC::Unreal { class UFunction; }
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
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

#include <Input/Handler.hpp>
#include <Input/PlatformInputSource.hpp>
#include <Input/Platform/QueueInputSource.hpp>

static const char* g_main_lua_path = "main.lua";

namespace RC
{
    // ------------------------------------------------------------------
    // SettingsManager.hpp shape: ProcessEvent=0, EngineTick=1.
    // kFuncPtrMarshalExecutor(=2) comes from the REAL marshal_state region.
    // ------------------------------------------------------------------
    enum class GameThreadExecutionMethod
    {
        ProcessEvent = 0,
        EngineTick = 1,
    };

    enum class DelayedActionStatus_scoped
    {
        Active,
        Paused,
        Executing,
        PendingRemoval,
    };

    // ------------------------------------------------------------------
    // Harness knobs (declared platform stand-ins)
    // ------------------------------------------------------------------
    static bool g_engine_tick_address_available = true;
    static bool g_process_event_address_available = false;
    static bool g_registration_refused = true;
    static bool g_r3_pump_pending = true;
    static bool g_simulated_pad_available = true;
    static int32_t g_next_hook_id = 41;
    static int g_engine_tick_register_attempts = 0;
    static int g_process_event_register_attempts = 0;
    static int g_publish_calls = 0;
    static bool g_fake_publication_allowed = true;
    static bool g_fake_current_thread = true;
    static int g_notify_stub_calls = 0;
    static int g_get_mod_ref_calls = 0;

    auto UE4SSRuntime::IsEngineTickAvailable() -> bool { return g_engine_tick_address_available; }
    auto UE4SSRuntime::IsProcessEventAvailable() -> bool { return g_process_event_address_available; }

    namespace Unreal
    {
        // The REAL initializer-side bridge surface (staged LuaMod.cpp declares
        // these locally at :106-108; the definitions are UnrealInitializer
        // bridges on the target). Single-threaded fixture semantics: the
        // thread that invokes the carrier publishes as the game thread.
        auto R3MarshalPumpPending() -> bool { return g_r3_pump_pending; }
        auto R3MarshalPublishGameThreadIdentity() -> bool { ++g_publish_calls; return g_fake_publication_allowed; }
        auto R3MarshalQueryHarnessDispatch() -> void {}

        // Before the first accepted publication this is false for every thread
        // (the drain's own comment: "that predicate is false for every thread").
        auto IsInGameThreadRaw() -> bool { return g_publish_calls > 0 && g_fake_current_thread; }

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
    // Minimal LuaMod mirror: exactly the members the REAL regions use
    // (same mirror shape as the accepted r3-ladder host test, plus the
    // REAL-shaped Mod base: m_program is a UE4SSProgram reference in the real
    // Mod, hence the reference member here).
    // ------------------------------------------------------------------
    struct UE4SSProgramMirror
    {
        // Mirror of UE4SSProgram.cpp:2884-2903 (HAS_INPUT route): plain
        // forwarders onto the REAL Input::Handler; no availability gate on
        // registration, exactly as shipped.
        Input::Handler m_input_handler{};

        auto register_keydown_event(Input::Key key, const Input::EventCallbackCallable& callback, uint8_t custom_data, void* custom_data2) -> void
        {
            m_input_handler.register_keydown_event(key, callback, custom_data, custom_data2);
        }

        auto register_keydown_event(Input::Key key,
                                    const Input::Handler::ModifierKeyArray& modifier_keys,
                                    const Input::EventCallbackCallable& callback,
                                    uint8_t custom_data,
                                    void* custom_data2) -> void
        {
            m_input_handler.register_keydown_event(key, modifier_keys, callback, custom_data, custom_data2);
        }
    };

    struct Mod
    {
        UE4SSProgramMirror& m_program;
    };

    class LuaMod : public Mod
    {
      public:
        explicit LuaMod(UE4SSProgramMirror& program) : Mod(program) {}

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

        static auto ensure_engine_tick_hooked() -> void;
        static auto ensure_process_event_hooked(LuaMod* mod) -> void;

        // Pump bodies for hooks this chain never drives (stubs; recorded above).
        static auto engine_tick_hook(Unreal::Hook::TCallbackIterationData<void>&, Unreal::UEngine*, float, bool) -> void {}
        static auto process_event_hook(Unreal::Hook::TCallbackIterationData<void>&, Unreal::UObject*, Unreal::UFunction*, void*) -> void {}
    };

    using SimpleLuaAction = LuaMod::SimpleLuaAction;
    using DelayedGameThreadAction = LuaMod::DelayedGameThreadAction;

    // ------------------------------------------------------------------
    // STUB RECORD (engine-side): the real function walks GUObjectArray entries
    // for StaticConstructObject callbacks. The host has no UObject array and
    // this chain enqueues nothing, so the stub counts the drain pass only.
    // ------------------------------------------------------------------
    static auto process_pending_notify_on_new_object_callbacks() -> void
    {
        ++g_notify_stub_calls;
    }

    // ==================================================================
    // THE REAL REGIONS (byte-exact; hashes in regions/regions-manifest.json)
    // ==================================================================
#include "dispatch_scope.inc"
#include "marshal_state.inc"
    // Explicit fake engine seams: discovery and detour arming are outside this host path.
    static auto r3_marshal_lazy_arm() -> void {}
    static auto try_deferred_loadmap_arm() -> void {}
#include "process_simple_actions.inc"
#include "process_delayed_actions.inc"
#include "funcptr_marshal_hook.inc"
#include "availability.inc"
#include "hook_state.inc"
#include "ensure_hooks.inc"

    // ------------------------------------------------------------------
    // get_mod_ref: the bindings' mod lookup. The REAL one reads the "ModRef"
    // global userdata through the LuaType::LuaModRef layer (runtime Lua-type
    // machinery); the harness has one mod (recorded stub).
    // ------------------------------------------------------------------
    static std::unique_ptr<UE4SSProgramMirror> g_program{};
    static std::unique_ptr<LuaMod> g_harness_mod{};
    static auto get_mod_ref(const LuaMadeSimple::Lua&) -> LuaMod*
    {
        ++g_get_mod_ref_calls;
        return g_harness_mod.get();
    }

    // ------------------------------------------------------------------
    // Lua-side engine surface for main.lua (each call recorded)
    // ------------------------------------------------------------------
    static std::vector<std::string> g_engine_call_log{};
    static std::vector<std::string> g_print_log{};
    static bool g_arm_player_controller = false;
    static int g_pc_isvalid_calls = 0;
    static int g_loadmap_prehook_registrations = 0;

    static auto record_call(const char* fn, const char* arg) -> void
    {
        g_engine_call_log.emplace_back(std::string{fn} + "(" + arg + ")");
    }

    static auto has_print(const std::string& needle) -> bool
    {
        for (const auto& line : g_print_log)
        {
            if (line.find(needle) != std::string::npos) return true;
        }
        return false;
    }

    static int l_isvalid_true(lua_State* L)
    {
        (void)L;
        ++g_pc_isvalid_calls;
        lua_pushboolean(L, 1);
        return 1;
    }

    static int l_find_first_of(lua_State* L)
    {
        const char* name = luaL_checkstring(L, 1);
        record_call("FindFirstOf", name);
        if (g_arm_player_controller && std::string{name} == "PlayerController")
        {
            // The one recorded player-state stub (S1b only): a table whose
            // IsValid answers true. No UWorld exists on the host.
            lua_newtable(L);
            lua_pushcfunction(L, l_isvalid_true);
            lua_setfield(L, -2, "IsValid");
            return 1;
        }
        lua_pushnil(L); // no game objects exist on the host
        return 1;
    }

    static int l_static_find_object(lua_State* L)
    {
        record_call("StaticFindObject", luaL_checkstring(L, 1));
        lua_pushnil(L);
        return 1;
    }

    static int l_static_construct_object(lua_State* L)
    {
        record_call("StaticConstructObject", "<class,outer,name>");
        lua_pushnil(L);
        return 1;
    }

    static int l_fname(lua_State* L)
    {
        record_call("FName", luaL_checkstring(L, 1));
        lua_settop(L, 1); // identity stand-in: the name passes through
        return 1;
    }

    static int l_ftext(lua_State* L)
    {
        record_call("FText", luaL_checkstring(L, 1));
        lua_settop(L, 1);
        return 1;
    }

    static int l_load_asset(lua_State* L)
    {
        record_call("LoadAsset", luaL_checkstring(L, 1));
        lua_pushnil(L);
        return 1;
    }

    static int l_register_loadmap_prehook(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TFUNCTION);
        ++g_loadmap_prehook_registrations;
        // Deliberately NOT in g_engine_call_log: it fires at LOAD time (main.lua
        // :731) and is a runtime registration, not an engine-object lookup — the
        // call log must contain only the F7 chain's engine surface.
        return 0;
    }

    static int harness_print(lua_State* L)
    {
        int n = lua_gettop(L);
        std::string out;
        for (int i = 1; i <= n; ++i)
        {
            if (i > 1) out += "\t";
            size_t len = 0;
            const char* s = luaL_tolstring(L, i, &len);
            out += (s ? s : "");
            lua_pop(L, 1);
        }
        g_print_log.push_back(out);
        std::printf("%s\n", out.c_str());
        return 0;
    }

    // ------------------------------------------------------------------
    // STUB RECORD (platform-side): stand-in for the genuine PS5 pad provider
    // (NativePadInput / ConfigureRetainedPS5PadInput route) that the runtime
    // injects via Handler::init(input_source). ONLY is_available() differs
    // from QueueInputSource (producer-less, hard-codes false). Queueing
    // (RingBufferSPSC), activation state, event filtering and the
    // process_event body remain the REAL QueueInputSource code.
    // ------------------------------------------------------------------
    namespace Input
    {
        class SimulatedPadSource : public RC::Input::QueueInputSource
        {
          public:
            bool is_available() override { return g_simulated_pad_available; }
            bool activate() override { return m_activated = is_available(); }
        };
    } // namespace Input

    // ==================================================================
    // The REAL registrations (each .inc is a complete statement). The two
    // binding regions name their state parameter differently in the REAL
    // file (RegisterKeyBind lives in a `lua`-named setup; ExecuteInGameThread
    // in an `m_lua`-named one), so each gets its exact-shaped wrapper.
    // ==================================================================
    static auto host_register_key_table(LuaMadeSimple::Lua& lua) -> void
    {
#include "key_table.inc"
    }

    static auto host_register_keybind_glue(LuaMadeSimple::Lua& lua) -> void
    {
#include "binding_register_keybind.inc"
    }

    static auto host_register_execute_in_game_thread(LuaMadeSimple::Lua& m_lua) -> void
    {
#include "binding_execute_in_game_thread.inc"
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

    static auto any_error_capture() -> bool
    {
        for (const auto& entry : output_capture())
        {
            if (entry.level == LogLevel::Error) return true;
        }
        return false;
    }

    static LuaMadeSimple::Lua* g_main_lua = nullptr;
    static std::shared_ptr<Input::SimulatedPadSource> g_pad_source{};  // shared_ptr: the REAL Handler::init injection shape (NativeProgramContext.hpp:89)

    struct World
    {
        bool engine_tick_address_available;
        bool process_event_address_available;
        bool registration_refused;
        bool marshal_armed;
        bool pump_pending;
        bool arm_player_controller;
    };

    static auto setup_world(const World& world) -> LuaMadeSimple::Lua&
    {
        g_engine_tick_address_available = world.engine_tick_address_available;
        g_process_event_address_available = world.process_event_address_available;
        g_registration_refused = world.registration_refused;
        g_r3_pump_pending = world.pump_pending;
        g_simulated_pad_available = true;
        g_fake_publication_allowed = true;
        g_fake_current_thread = true;
        m_funcptr_marshal_armed.store(world.marshal_armed, std::memory_order_release);
        m_funcptr_marshal_identity_published = false;
        m_funcptr_marshal_pump_pending_logged = false;

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

        NativeProcessEventHookId = Unreal::Hook::ERROR_ID;
        g_next_hook_id = 41;
        g_engine_tick_register_attempts = 0;
        g_process_event_register_attempts = 0;
        g_publish_calls = 0;
        g_notify_stub_calls = 0;
        g_get_mod_ref_calls = 0;
        g_engine_call_log.clear();
        g_print_log.clear();
        g_arm_player_controller = world.arm_player_controller;
        g_pc_isvalid_calls = 0;
        g_loadmap_prehook_registrations = 0;
        output_capture().clear();

        g_program = std::make_unique<UE4SSProgramMirror>();
        g_harness_mod.reset(new LuaMod{*g_program});
        g_pad_source = std::make_shared<Input::SimulatedPadSource>();

        // A REAL Lua 5.4.7 state per scenario; REAL bindings register as real
        // globals; the REAL Handler::init injects the (stub-armed) pad source.
        auto& lua = LuaMadeSimple::new_state();
        luaL_openlibs(lua.get_lua_state());
        lua_register(lua.get_lua_state(), "print", harness_print);
        lua_register(lua.get_lua_state(), "FindFirstOf", l_find_first_of);
        lua_register(lua.get_lua_state(), "StaticFindObject", l_static_find_object);
        lua_register(lua.get_lua_state(), "StaticConstructObject", l_static_construct_object);
        lua_register(lua.get_lua_state(), "FName", l_fname);
        lua_register(lua.get_lua_state(), "FText", l_ftext);
        lua_register(lua.get_lua_state(), "LoadAsset", l_load_asset);
        lua_register(lua.get_lua_state(), "RegisterLoadMapPreHook", l_register_loadmap_prehook);
        host_register_key_table(lua);
        host_register_keybind_glue(lua);
        host_register_execute_in_game_thread(lua);
        g_harness_mod->m_harness_main_lua = &lua;
        g_main_lua = &lua;

        if (!g_program->m_input_handler.init(g_pad_source))
        {
            std::printf("    FAIL: Handler::init refused the simulated pad source\n");
        }
        return lua;
    }

    static auto load_main_lua(LuaMadeSimple::Lua& lua, std::string& err) -> bool
    {
        FILE* f = std::fopen(g_main_lua_path, "rb");
        if (!f)
        {
            err = "cannot open main.lua";
            return false;
        }
        std::string data;
        char buf[8192];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) data.append(buf, n);
        std::fclose(f);

        lua_State* L = lua.get_lua_state();
        // Chunk name "=main.lua" keeps runtime error positions readable
        // (e.g. "main.lua:66: Item database not ready...").
        if (luaL_loadbuffer(L, data.data(), data.size(), "=main.lua") != LUA_OK)
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

    // The simulated key edge: REAL QueueInputSource push + REAL Handler dispatch.
    static auto press_f7() -> void
    {
        Input::InputEvent ev{};
        ev.key = Input::Key::F7;
        g_pad_source->push_input_event(ev);
        g_program->m_input_handler.process_event();
    }

    // The simulated carrier invocation: the REAL drain body runs exactly as the
    // R3 carrier post-hook would invoke it (parms untouched, R3D 4.1/4.3).
    static auto drain_marshal_carrier() -> void
    {
        Unreal::UnrealScriptFunctionCallableContext ctx{};
        funcptr_marshal_hook(ctx, nullptr);
    }

    static auto engine_calls_str() -> std::string
    {
        std::string out = "[";
        for (size_t i = 0; i < g_engine_call_log.size(); ++i)
        {
            if (i) out += ", ";
            out += g_engine_call_log[i];
        }
        out += "]";
        return out;
    }

    // ------------------------------------------------------------------
    // S1: F7 -> dispatch -> ExecuteInGameThread marshal-first -> drain ->
    //     menu chain to the in-game-only boundary
    // ------------------------------------------------------------------
    static auto scenario_f7_marshal_first_to_boundary() -> void
    {
        std::printf("[S1] R3 world: F7 edge -> dispatch -> marshal-first enqueue -> carrier drain -> menu chain\n");
        auto& lua = setup_world({true, false, true, true, true, false});
        std::string err;
        check(load_main_lua(lua, err), "main.lua (f586043a) loads and runs on the REAL Lua 5.4.7 state");
        check(has_print("Loaded v1.0. Clean pickup-only release. F7 opens the menu."),
              "exact load-time observable: the main.lua banner line (:746)");
        check(g_loadmap_prehook_registrations == 1, "RegisterLoadMapPreHook recorded the load-map registration (:731)");
        check(g_program->m_input_handler.has_event_on_key(Input::Key::F7), "F7 keydown registered on the REAL Handler (binding ran)");
        check(g_get_mod_ref_calls >= 6, "the REAL binding resolved the mod per RegisterKeyBind call (6 binds in main.lua)");

        // --- the F7 edge ---------------------------------------------
        press_f7();
        check(g_harness_mod->m_game_thread_actions.size() == 1,
              "the F7 closure was enqueued onto the marshal-drained game-thread queue (marshal-first when armed)");
        check(g_harness_mod->m_engine_tick_actions.empty(),
              "NOT routed to the EngineTick queue (marshal outranks the ALSO-assigned tick address)");
        check(g_engine_tick_register_attempts == 0 && g_process_event_register_attempts == 0,
              "ZERO detour attempts (no second pump while the marshal is the pump)");
        check(g_harness_mod->m_hook_lua != nullptr,
              "the F7 Lua callback FIRED (ExecuteInGameThread ran make_hook_state: hook thread created)");
        check(!any_error_capture(), "dispatch + enqueue produced no error output (silent success path)");
        check(g_engine_call_log.empty(), "menu logic has NOT run at enqueue time (no engine lookups yet)");

        // --- the carrier invocation (game thread) --------------------
        drain_marshal_carrier();
        check(g_publish_calls >= 1, "drain publication gate consulted R3MarshalPublishGameThreadIdentity");
        check(capture_has(LogLevel::Default, L"marshal is the game-thread pump (initializer deferred its EngineTick registrations)"),
              "the REAL drain's first-publication oracle line fired");
        check(g_harness_mod->m_game_thread_actions.empty(), "the drain consumed the enqueued closure (queue empty)");
        check(g_notify_stub_calls >= 1, "the drain ran to its notify-callbacks step (full body executed)");
        check(has_print("Scan failed: ") && has_print("Item database not ready. Load a save first."),
              "exact pre-boundary observable: main.lua scanItems degradation log (:194/:66)");
        const std::string calls = engine_calls_str();
        check(g_engine_call_log.size() == 2 &&
                  g_engine_call_log[0] == "FindFirstOf(ItemDatabaseSubsystem)" &&
                  g_engine_call_log[1] == "FindFirstOf(PlayerController)",
              "chain stopped EXACTLY at the gate: " + calls);
        check(!has_print("Menu created."), "\"Menu created.\" (:643) never printed — in-game-only boundary holds");
        check(!has_print("UI creation failed:"), "no widget-construction attempt (boundary is BEFORE the UMG block)");
    }

    // ------------------------------------------------------------------
    // S1b: same chain with the recorded PlayerController stub armed —
    //      exposes the NEXT in-game-only gate (UMG WidgetBlueprintLibrary)
    // ------------------------------------------------------------------
    static auto scenario_f7_boundary_rung_two() -> void
    {
        std::printf("[S1b] boundary rung 2: PlayerController stub armed (recorded player-state stand-in)\n");
        auto& lua = setup_world({true, false, true, true, true, true});
        std::string err;
        check(load_main_lua(lua, err), "main.lua loads");
        press_f7();
        drain_marshal_carrier();
        check(g_pc_isvalid_calls == 2,
              "the armed PlayerController stub answered isValid() exactly twice: getPlayerController (:22) + buildUI re-check (:494)");
        const std::string calls = engine_calls_str();
        check(g_engine_call_log.size() == 3 &&
                  g_engine_call_log[2] == "StaticFindObject(/Script/UMG.Default__WidgetBlueprintLibrary)",
              "the next gate reached and recorded: " + calls);
        check(has_print("WidgetBlueprintLibrary not found."),
              "exact observable at the UMG gate: \"WidgetBlueprintLibrary not found.\" (:501)");
        check(!has_print("Menu created."), "\"Menu created.\" still absent — the UMG object gate is also in-game-only");
        check(g_harness_mod->m_game_thread_actions.empty(), "drain empty again (closure executed exactly once)");
    }

    // ------------------------------------------------------------------
    // S2: NEGATIVE CONTROL — marshal NOT armed, no tick/PE backend at all:
    //     the loud throw path, not a silent drop
    // ------------------------------------------------------------------
    static auto scenario_f7_no_backend_loud_throw() -> void
    {
        std::printf("[S2] NEGATIVE: marshal not armed, EngineTick+ProcessEvent unavailable (no backend)\n");
        auto& lua = setup_world({false, false, true, false, false, false});
        std::string err;
        check(load_main_lua(lua, err), "main.lua loads");
        press_f7();
        check(capture_has(LogLevel::Error, L"neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed"),
              "the no-backend throw SURFACED through the keybind lambda's error logging (loud, not silent)");
        check(g_harness_mod->m_game_thread_actions.empty() && g_harness_mod->m_engine_tick_actions.empty(),
              "nothing was enqueued anywhere (throw precedes any queueing)");
        check(g_harness_mod->m_hook_lua == nullptr,
              "make_hook_state never ran (the throw is selection-time, before the enqueue block)");
        check(g_engine_tick_register_attempts == 0 && g_process_event_register_attempts == 0,
              "no detour was attempted (the ladder refuses before any ensure)");
        check(g_engine_call_log.empty(), "the menu closure NEVER executed (no engine lookups)");
        check(!has_print("Menu created.") && !has_print("Scan failed:"), "no menu-chain output at all");
        check(g_program->m_input_handler.has_event_on_key(Input::Key::F7),
              "the keybind surface survives the failed dispatch (no teardown)");
    }

    // ------------------------------------------------------------------
    // S3: marshal NOT armed, tick address assigned, registration refused:
    //     enqueue-first + the loud stall line (closure kept, not dropped)
    // ------------------------------------------------------------------
    static auto scenario_f7_enqueue_first_loud_stall() -> void
    {
        std::printf("[S3] no marshal, W_FAILS_R_WORKS: enqueue-first, refused detour, loud stall\n");
        auto& lua = setup_world({true, false, true, false, false, false});
        std::string err;
        check(load_main_lua(lua, err), "main.lua loads");
        press_f7();
        check(g_harness_mod->m_engine_tick_actions.size() == 1,
              "closure FIRST enqueued onto the EngineTick queue (enqueue-before-arm order guarantee)");
        check(g_engine_tick_register_attempts == 1, "exactly ONE EngineTick detour attempt (after the enqueue)");
        check(!LuaMod::m_is_engine_tick_hooked, "the detour was refused (write-refused world)");
        check(capture_has(LogLevel::Error, L"EngineTick callback registration failed"),
              "the refused arm threw and TRY logged it loudly");
        check(capture_has(LogLevel::Error, L"[R3] ExecuteInGameThread: closure enqueued but the EngineTick pump is not registered (detour registration refused); it stays queued until a pump exists"),
              "the loud stall line named the queued closure (never silent)");
        check(g_engine_call_log.empty(), "the closure is NOT executed (no pump exists to drain it)");
        check(!has_print("Menu created."), "menu never built in this world");
    }


    static auto scenario_current_queue_before_carrier() -> void
    {
        std::printf("[S4] CURRENT production: pump pending, unarmed carrier, queued F7\n");
        auto& lua = setup_world({true, false, true, false, true, false});
        std::string err;
        check(load_main_lua(lua, err), "original release loads before carrier arming");
        press_f7();
        check(g_harness_mod->m_game_thread_actions.size() == 1, "unarmed pending-pump F7 queues exactly once on marshal queue");
        check(g_harness_mod->m_engine_tick_actions.empty(), "unarmed pending-pump F7 avoids stranded EngineTick queue");
        check(g_engine_tick_register_attempts == 0 && g_process_event_register_attempts == 0, "pending-pump enqueue attempts no refused detour");
        check(capture_has(LogLevel::Warning, L"carrier is not armed yet; the closure will await arming"), "unarmed queued closure is explicitly reported");
        check(g_engine_call_log.empty(), "enqueue cannot execute engine lookups");
        m_funcptr_marshal_armed.store(true, std::memory_order_release); // Explicit host carrier stand-in.
        drain_marshal_carrier();
        check(g_harness_mod->m_game_thread_actions.empty(), "later fake carrier drains the retained F7 closure");
        check(g_engine_call_log.size() == 2, "one accepted drain reaches exactly the two known lookup boundaries");
        const auto calls = g_engine_call_log.size();
        drain_marshal_carrier();
        check(g_engine_call_log.size() == calls, "second carrier invocation does not replay consumed F7 closure");
    }

    static auto scenario_current_thread_refusals() -> void
    {
        std::printf("[S5] CURRENT production: fake publication refusal and foreign-thread refusal\n");
        auto& lua = setup_world({true, false, true, true, true, false});
        std::string err;
        check(load_main_lua(lua, err), "original release loads for thread-refusal control");
        press_f7();
        g_fake_publication_allowed = false;
        drain_marshal_carrier();
        check(!m_funcptr_marshal_identity_published, "denied fake publication leaves production publication state false");
        check(g_harness_mod->m_game_thread_actions.size() == 1 && g_engine_call_log.empty(), "denied publication preserves queued callback and performs no lookup");
        g_fake_publication_allowed = true;
        g_fake_current_thread = false;
        drain_marshal_carrier();
        check(g_harness_mod->m_game_thread_actions.size() == 1 && g_engine_call_log.empty(), "foreign fake current-thread gate preserves queue and performs no lookup");
        g_fake_current_thread = true;
        drain_marshal_carrier();
        check(g_harness_mod->m_game_thread_actions.empty() && g_engine_call_log.size() == 2, "accepted fake thread finally consumes one callback");
    }

    static auto run_all() -> int
    {
        std::printf("CURRENT SELECTED host fixture — fake engine/thread witnesses\n");
        std::printf("chain: F7 key event -> Input::Handler dispatch -> RegisterKeyBind glue (current 8bd12c62) ->\n");
        std::printf("       main.lua F7 callback (f586043a, Lua 5.4.7) -> ExecuteInGameThread (current 8bd12c62) ->\n");
        std::printf("       funcptr_marshal_hook drain -> toggleMenu -> in-game-only boundary\n\n");

        scenario_f7_marshal_first_to_boundary();
        scenario_f7_boundary_rung_two();
        scenario_f7_no_backend_loud_throw();
        scenario_f7_enqueue_first_loud_stall();
        scenario_current_queue_before_carrier();
        scenario_current_thread_refusals();

        std::printf("\nworker-menu-chain fixture: %d checks passed, %d failed\n", g_checks_passed, g_checks_failed);
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

int main(int argc, char** argv)
{
    if (argc > 1) g_main_lua_path = argv[1];
    return RC::run_all();
}
