#!/usr/bin/env python3
"""worker-r3-diffs-authoring: anchored authoring of DIFF 1 + DIFF 2 (R3-TIER-DESIGN
BOTH_FAIL branch). Aborts on any non-unique anchor or base-pin drift; writes patched
copies + unified diffs + anchor receipt ONLY under this worker's output dir.
Nothing in the spike tree or staging tree is modified."""
import hashlib, json, subprocess, sys
from pathlib import Path

W = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
ME = W / "outputs/2026-10-04-zcode-team/worker-r3-diffs-authoring"
SRC = W / ".superpowers/native-dependency-spike/native-coherent-runtime-1/source"
SINI = W / "outputs/2026-10-04-zcode-team/worker-query-s2-arm/staging/UnrealInitializer.cpp"

PINS = {
    "LuaMod.cpp": (SRC / "port/ue4ss/src/Mod/LuaMod.cpp",
                   "a2d07f7c233a59d05f35779ece3a05b7068475b2ee9ab8a1709b72113f2cc4ae"),
    "SettingsManager.hpp": (SRC / "port/ue4ss/include/SettingsManager.hpp",
                            "a9b4abbe26f0cc1c9ec71029cb009e721dd88839395a72786dcbeda8f8b5aa2e"),
    "UnrealInitializer.cpp": (SINI,
                              "933d08a2f3e4dea1f80d216bd99779871b6e5b3ae1f26028d357d824051e2542"),
}

base, patched = ME / "work/base", ME / "work/patched"
receipts, diffs = ME / "receipts", ME / "diffs"
for d in (base, patched, receipts, diffs, ME / "host-tests"):
    d.mkdir(parents=True, exist_ok=True)

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

# ---- 1. verify pins, stage bases -------------------------------------------------
pin_receipt = {}
for name, (path, want) in PINS.items():
    got = sha(path)
    if got != want:
        sys.exit(f"BASE PIN DRIFT for {name}: {got} != {want}")
    (base / name).write_bytes(path.read_bytes())
    pin_receipt[name] = {"sha256": got, "source": str(path)}
print("base pins OK:", json.dumps({k: v["sha256"][:12] for k, v in pin_receipt.items()}))

EDITS = {}  # filename -> list of (anchor_id, old, new)

# =============================== DIFF 1 ===========================================
EDITS["SettingsManager.hpp"] = [
("sm-enum-append",
"""    enum class GameThreadExecutionMethod
    {
        ProcessEvent, // Use ProcessEvent hook
        EngineTick    // Use Engine Tick hook (once per frame)
    };""",
"""    enum class GameThreadExecutionMethod
    {
        ProcessEvent, // Use ProcessEvent hook
        EngineTick,   // Use Engine Tick hook (once per frame)
        // [R3] Drain via the R3-hooked carrier UFunction's FuncPtr post-hook (DIFF 1,
        // worker-r3-diffs-authoring 2026-10-04; R3-TIER-DESIGN 4/5.2). PROSPERO
        // both-fail route; appended last so existing integer values are unchanged.
        FuncPtrMarshal
    };"""),
]

EDITS["LuaMod.cpp"] = [
# ---- bridge declarations (namespace RC) ----
("lm-bridge-decls",
"""namespace RC
{
    LuaMadeSimple::Lua* LuaStatics::console_executor{};""",
"""namespace RC
{
    namespace Unreal
    {
        // =================================================================================
        // [R3] FuncPtr-marshal bridges (DIFF 1; R3-TIER-DESIGN 4.4a/4.4b). Defined in the
        // replaced initializer unit (DIFF 2). Deliberately declared here rather than in a
        // shared header: touching Unreal/UnrealInitializer.hpp would force a tree-wide
        // recompile. The definitions exist only under __PROSPERO__; every reference below
        // is inside #ifdef __PROSPERO__ so non-PROSPERO builds never odr-use them.
        // Signatures are part of the DIFF 1/DIFF 2 contract and must not drift.
        // =================================================================================
        auto R3MarshalPublishGameThreadIdentity() -> bool;
        auto R3MarshalQueryHarnessDispatch() -> void;
        auto R3MarshalPumpPending() -> bool;
    }

    LuaMadeSimple::Lua* LuaStatics::console_executor{};"""),
# ---- marshal state (before process_simple_actions) ----
("lm-state",
"""    static auto process_simple_actions(std::vector<LuaMod::SimpleLuaAction>& actions) -> void""",
"""    // =====================================================================================
    // [R3] FuncPtr-marshal state (DIFF 1, worker-r3-diffs-authoring 2026-10-04;
    // R3-TIER-DESIGN 4/5.2). The R3-compatible game-thread pump: ONE carrier UFunction
    // ("the calibration-selected carrier", R3D 4.2) is R3-hooked via RegisterPostHook
    // (UFunction.cpp:91-101 -> GetFunctionHookData :78-89: save current FuncPtr as the
    // original, then SetFuncPtr(&Internal::UnrealScriptFunctionHook) - ONE aligned
    // pointer store into already-RW heap memory, zero syscalls). Its post-callback body
    // (funcptr_marshal_hook below) drains the SAME m_game_thread_actions queue
    // process_event_hook drains. Arming is parameterized by the carrier's FName id:
    // the calibration owner calls arm_funcptr_marshal_drain; this unit owns readiness,
    // the drain, and the registration only. File scope by design: no LuaMod.hpp change,
    // so no additional unit recompiles.
    // =====================================================================================
    static std::atomic<bool> m_funcptr_marshal_armed{false};
    static int32_t m_funcptr_marshal_callback_id{-1};
    static Unreal::FName m_funcptr_marshal_carrier_id{};
    [[maybe_unused]] static bool m_funcptr_marshal_identity_published{false};
    [[maybe_unused]] static bool m_funcptr_marshal_pump_pending_logged{false};

    // Backend-reality predicate (R3D 4.5): "carrier armed" module state, NOT an AOB
    // address is_ready() - the ExecuteInGameThread/WithDelay availability trap of R3D 3
    // (address-availability is not arming) cannot recur through this path.
    static auto is_funcptr_marshal_armed() -> bool
    {
        return m_funcptr_marshal_armed.load(std::memory_order_acquire);
    }

    static auto process_simple_actions(std::vector<LuaMod::SimpleLuaAction>& actions) -> void"""),
# ---- drain body (before engine_tick_hook) ----
("lm-drain-body",
"""    auto static engine_tick_hook([[maybe_unused]] Unreal::Hook::TCallbackIterationData<void>& CallbackIterationData,
                                 [[maybe_unused]] Unreal::UEngine* Context,
                                 [[maybe_unused]] float DeltaSeconds,
                                 [[maybe_unused]] bool bIdle) -> void""",
"""    // =====================================================================================
    // [R3] The FuncPtr-marshal drain body (DIFF 1; R3D 4.1/4.3). Registered as a
    // POST-hook on the carrier: the original function has already executed unmodified;
    // this body never touches parms or RESULT_DECL and never alters control flow.
    // The PROSPERO prologue is process_event_hook's; the drain block is
    // process_event_hook's with the delayed-action executor tagged FuncPtrMarshal.
    // Thread honesty (R3D 4.3): until the initializer bridge publishes the game-thread
    // identity, the marshal DRAINS NOTHING except the publication step itself.
    // =====================================================================================
    static auto funcptr_marshal_hook([[maybe_unused]] Unreal::UnrealScriptFunctionCallableContext& Context,
                                     [[maybe_unused]] void* CustomData) -> void
    {
#ifdef __PROSPERO__
        // ORDER MATTERS (R3D 4.3): under BOTH_FAIL nothing else publishes the identity,
        // so the marshal CANNOT gate on IsInGameThreadRaw() first - before publication
        // that predicate is false for every thread and the pump would never start. The
        // first accepted invocation publishes via the initializer bridge (publish_current
        // is mutex-arbitrated: exactly one thread wins; every other thread, on this and
        // later invocations, refuses); from then on the raw gate refuses worker threads.
        // The publishing thread BECOMES the game thread - the same semantics as
        // HookedEngineTick's genuine-callback publication, backed by the calibration's
        // game-thread-stability criterion C-4 (CARRIER-CANDIDATES).
        if (!m_funcptr_marshal_identity_published)
        {
            // 4.4a: the HookedEngineTick publication body, routed onto the marshal.
            // Published/AlreadyCurrent both mean this caller IS the game thread;
            // Unavailable/DifferentThread/StartupWait-cancelled refuse (false).
            if (!Unreal::R3MarshalPublishGameThreadIdentity())
            {
                return;
            }
            m_funcptr_marshal_identity_published = true;
            if (Unreal::R3MarshalPumpPending() && !m_funcptr_marshal_pump_pending_logged)
            {
                m_funcptr_marshal_pump_pending_logged = true;
                Output::send(STR("[R3] marshal is the game-thread pump (initializer deferred its EngineTick registrations)\\n"));
            }
        }
        // The carrier may be invoked on worker threads. Do not inspect Lua action
        // state until the published game thread is this exact caller thread.
        if (!Unreal::IsInGameThreadRaw())
        {
            return;
        }
        NativeLuaActionDispatchScope action_dispatch;
        if (!action_dispatch.owns_dispatch())
            return;
#endif
        {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            LuaMod::m_is_processing_actions = true;
        }

        process_simple_actions(LuaMod::m_game_thread_actions);
        process_delayed_actions<GameThreadExecutionMethod::FuncPtrMarshal>(LuaMod::m_delayed_game_thread_actions);

        process_pending_notify_on_new_object_callbacks();

        {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            LuaMod::m_is_processing_actions = false;
            if (!LuaMod::m_pending_game_thread_actions.empty())
            {
                LuaMod::m_game_thread_actions.insert(
                    LuaMod::m_game_thread_actions.end(),
                    std::make_move_iterator(LuaMod::m_pending_game_thread_actions.begin()),
                    std::make_move_iterator(LuaMod::m_pending_game_thread_actions.end()));
                LuaMod::m_pending_game_thread_actions.clear();
            }
            if (!LuaMod::m_pending_delayed_game_thread_actions.empty())
            {
                LuaMod::m_delayed_game_thread_actions.insert(
                    LuaMod::m_delayed_game_thread_actions.end(),
                    std::make_move_iterator(LuaMod::m_pending_delayed_game_thread_actions.begin()),
                    std::make_move_iterator(LuaMod::m_pending_delayed_game_thread_actions.end()));
                LuaMod::m_pending_delayed_game_thread_actions.clear();
            }
        }
    }

    auto static engine_tick_hook([[maybe_unused]] Unreal::Hook::TCallbackIterationData<void>& CallbackIterationData,
                                 [[maybe_unused]] Unreal::UEngine* Context,
                                 [[maybe_unused]] float DeltaSeconds,
                                 [[maybe_unused]] bool bIdle) -> void"""),
# ---- pre-check + arm (after availability wrappers) ----
("lm-precheck-arm",
"""    static auto is_process_event_hook_available() -> bool
    {
        return UE4SSRuntime::IsProcessEventAvailable();
    }""",
"""    static auto is_process_event_hook_available() -> bool
    {
        return UE4SSRuntime::IsProcessEventAvailable();
    }

    // =====================================================================================
    // [R3] Identity pre-check (DIFF 1; R3D 4.3, the R3 port of VSD 2.1). Pure data reads
    // before ANY store: non-null FuncPtr, not the script-function pointer, FUNC_Native.
    // Refusal is loud and stores nothing. This is the same gate shape the Lua
    // RegisterHook native branch applies (LUA:4618-4620), so the R3 tier gets the
    // PLH-missing semantic validation for free.
    // =====================================================================================
    static auto r3_marshal_carrier_precheck(Unreal::UFunction* carrier) -> bool
    {
        if (!carrier)
        {
            Output::send<LogLevel::Error>(STR("[R3] marshal arm refused: carrier UFunction is null\\n"));
            return false;
        }
        const auto current_func_ptr = carrier->GetFuncPtr();
        if (!current_func_ptr)
        {
            Output::send<LogLevel::Error>(STR("[R3] marshal arm refused: carrier {} has a null FuncPtr\\n"), carrier->GetFullName());
            return false;
        }
        if (current_func_ptr == Unreal::UObject::ProcessInternalInternal.get_function_address())
        {
            Output::send<LogLevel::Error>(STR("[R3] marshal arm refused: carrier {} is a script function (Func == ProcessInternal)\\n"), carrier->GetFullName());
            return false;
        }
        if (!carrier->HasAnyFunctionFlags(Unreal::EFunctionFlags::FUNC_Native))
        {
            Output::send<LogLevel::Error>(STR("[R3] marshal arm refused: carrier {} does not have FUNC_Native\\n"), carrier->GetFullName());
            return false;
        }
        return true;
    }

    // =====================================================================================
    // [R3] Register the drain on the calibration-selected carrier (DIFF 1). Parameterized
    // by the carrier's FName id; the calibration owner (R3D 4.2 protocol, cc1 ridealong
    // lineage) selects and calls this. The exact hook registration per
    // UFunction.cpp:78-89: RegisterPostHook -> GetFunctionHookData() (emplaces the map
    // entry saving the CURRENT FuncPtr as the original, then
    // SetFuncPtr(&Internal::UnrealScriptFunctionHook)) -> AddPostCallback. Post-hook so
    // the original function always executes first and unmodified.
    // =====================================================================================
    static auto arm_funcptr_marshal_drain(Unreal::UFunction* carrier, const Unreal::FName& carrier_id) -> bool
    {
        if (is_funcptr_marshal_armed())
        {
            if (carrier_id != m_funcptr_marshal_carrier_id)
            {
                Output::send<LogLevel::Warning>(STR("[R3] marshal arm ignored: already armed on a different carrier\\n"));
                return false;
            }
            return true; // idempotent re-arm of the same carrier
        }
        if (!r3_marshal_carrier_precheck(carrier))
        {
            return false;
        }
        const int32_t callback_id = carrier->RegisterPostHook(&funcptr_marshal_hook, nullptr);
        if (callback_id < 0)
        {
            Output::send<LogLevel::Error>(STR("[R3] marshal arm refused: RegisterPostHook returned an invalid callback id for {}\\n"), carrier->GetFullName());
            return false;
        }
        m_funcptr_marshal_callback_id = callback_id;
        m_funcptr_marshal_carrier_id = carrier_id;
        m_funcptr_marshal_armed.store(true, std::memory_order_release);
        Output::send(STR("[R3] FuncPtr-marshal armed on carrier {} (callback id {})\\n"), carrier->GetFullName(), callback_id);
        return true;
    }"""),
# ---- delayed-action executor filter ----
("lm-delayed-filter",
"""                else if constexpr (Executor == GameThreadExecutionMethod::EngineTick)
                {
                    if (action.method != GameThreadExecutionMethod::EngineTick)
                    {
                        continue;
                    }
                }""",
"""                else if constexpr (Executor == GameThreadExecutionMethod::EngineTick)
                {
                    if (action.method != GameThreadExecutionMethod::EngineTick)
                    {
                        continue;
                    }
                }
                else if constexpr (Executor == GameThreadExecutionMethod::FuncPtrMarshal)
                {
                    // [R3] the marshal drains only marshal-tagged delayed actions
                    // (time-based; frame-based delays remain EngineTick-only, so the
                    // readiness block below leaves them untouched for this executor).
                    if (action.method != GameThreadExecutionMethod::FuncPtrMarshal)
                    {
                        continue;
                    }
                }"""),
# ---- EGameThreadMethod Lua table ----
("lm-lua-table",
"""            lua_pushinteger(L, static_cast<int>(GameThreadExecutionMethod::ProcessEvent));
            lua_setfield(L, -2, "ProcessEvent");
            lua_setglobal(L, "EGameThreadMethod");""",
"""            lua_pushinteger(L, static_cast<int>(GameThreadExecutionMethod::ProcessEvent));
            lua_setfield(L, -2, "ProcessEvent");
            // [R3] EGameThreadMethod.FuncPtrMarshal (DIFF 1; R3D 4.5)
            lua_pushinteger(L, static_cast<int>(GameThreadExecutionMethod::FuncPtrMarshal));
            lua_setfield(L, -2, "FuncPtrMarshal");
            lua_setglobal(L, "EGameThreadMethod");"""),
# ---- ExecuteInGameThread availability ladder ----
("lm-eigt-ladder",
"""            // Check hook availability before registering
            if (method == GameThreadExecutionMethod::EngineTick)
            {
                if (!is_engine_tick_hook_available())
                {
                    lua.throw_error("ExecuteInGameThread: EngineTick method requested but EngineTick hook is not available (AOB scan failed)");
                }
                LuaMod::ensure_engine_tick_hooked();
            }
            else if (method == GameThreadExecutionMethod::ProcessEvent)
            {
                if (!is_process_event_hook_available())
                {
                    lua.throw_error("ExecuteInGameThread: ProcessEvent method requested but ProcessEvent hook is not available (AOB scan failed)");
                }
                LuaMod::ensure_process_event_hooked(mod);
            }""",
"""            // Check hook availability before registering
            if (method == GameThreadExecutionMethod::EngineTick)
            {
                if (!is_engine_tick_hook_available())
                {
#if defined(__PROSPERO__)
                    // [R3] three-state ladder (DIFF 1; R3D 4.5):
                    // EngineTick -> ProcessEvent -> FuncPtrMarshal -> throw
                    if (is_process_event_hook_available())
                    {
                        LuaMod::ensure_process_event_hooked(mod);
                        method = GameThreadExecutionMethod::ProcessEvent;
                    }
                    else if (is_funcptr_marshal_armed())
                    {
                        method = GameThreadExecutionMethod::FuncPtrMarshal;
                    }
                    else
                    {
                        lua.throw_error("ExecuteInGameThread: EngineTick method requested but neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed");
                    }
#else
                    lua.throw_error("ExecuteInGameThread: EngineTick method requested but EngineTick hook is not available (AOB scan failed)");
#endif
                }
                else
                {
                    LuaMod::ensure_engine_tick_hooked();
                }
            }
            else if (method == GameThreadExecutionMethod::ProcessEvent)
            {
                if (!is_process_event_hook_available())
                {
#if defined(__PROSPERO__)
                    // [R3] three-state ladder (DIFF 1; R3D 4.5):
                    // ProcessEvent -> EngineTick -> FuncPtrMarshal -> throw
                    if (is_engine_tick_hook_available())
                    {
                        LuaMod::ensure_engine_tick_hooked();
                        method = GameThreadExecutionMethod::EngineTick;
                    }
                    else if (is_funcptr_marshal_armed())
                    {
                        method = GameThreadExecutionMethod::FuncPtrMarshal;
                    }
                    else
                    {
                        lua.throw_error("ExecuteInGameThread: ProcessEvent method requested but neither ProcessEvent nor EngineTick hooks are available and the R3 FuncPtr-marshal carrier is not armed");
                    }
#else
                    lua.throw_error("ExecuteInGameThread: ProcessEvent method requested but ProcessEvent hook is not available (AOB scan failed)");
#endif
                }
                else
                {
                    LuaMod::ensure_process_event_hooked(mod);
                }
            }
            else if (method == GameThreadExecutionMethod::FuncPtrMarshal)
            {
                // [R3] availability = carrier armed (module state), NOT an address
                // is_ready() (R3D 3 trap / 4.5). Enqueueing before arming is safe and
                // lazy (R3D 4.4a): the closure simply awaits the marshal drain.
                if (!is_funcptr_marshal_armed())
                {
                    Output::send<LogLevel::Warning>(STR("ExecuteInGameThread: FuncPtrMarshal requested but the carrier is not armed yet; the closure will await arming\\n"));
                }
            }"""),
]

# ---- three WithDelay-family ladders x two inner failure sites each ----
_LADDER_SITES = [
    ("lm-ladder-wd-1", "ExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available (AOB scans failed)\");", "ProcessEvent"),
    ("lm-ladder-wd-2", "ExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available (AOB scans failed)\");", "EngineTick"),
    ("lm-ladder-rt-1", "RetriggerableExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available (AOB scans failed)\");", "ProcessEvent"),
    ("lm-ladder-rt-2", "RetriggerableExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available (AOB scans failed)\");", "EngineTick"),
    ("lm-ladder-loop-1", "LoopInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available (AOB scans failed)\");", "ProcessEvent"),
    ("lm-ladder-loop-2", "LoopInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available (AOB scans failed)\");", "EngineTick"),
]
for aid, tail, meth in _LADDER_SITES:
    api = tail.split(":")[0]
    old = (
        '                        lua.throw_error("' + tail + '\n'
        '                    }\n'
        '                    method = GameThreadExecutionMethod::' + meth + ';\n'
    )
    new = (
        '#if defined(__PROSPERO__)\n'
        '                        // [R3] third ladder rung (DIFF 1; R3D 4.5):\n'
        '                        // EngineTick -> ProcessEvent -> FuncPtrMarshal -> throw.\n'
        '                        if (!is_funcptr_marshal_armed())\n'
        '                        {\n'
        '                            lua.throw_error("' + api + ': Neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed");\n'
        '                        }\n'
        '                        method = GameThreadExecutionMethod::FuncPtrMarshal;\n'
        '#else\n'
        '                        lua.throw_error("' + tail + '\n'
        '                        method = GameThreadExecutionMethod::' + meth + ';\n'
        '#endif\n'
        '                    }\n'
        '                    else\n'
        '                    {\n'
        '                        method = GameThreadExecutionMethod::' + meth + ';\n'
        '                    }\n'
    )
    EDITS["LuaMod.cpp"].append((aid, old, new))

# =============================== DIFF 2 ===========================================
EDITS["UnrealInitializer.cpp"] = [
# ---- [[maybe_unused]] on now-optionally-referenced statics (no-exec-gate precedent) ----
("ini-maybeunused-tick",
"""    auto HookedEngineTick([[maybe_unused]] Hook::TCallbackIterationData<void>& CallbackIterationData, UEngine*, float, bool) -> void""",
"""    [[maybe_unused]] auto HookedEngineTick([[maybe_unused]] Hook::TCallbackIterationData<void>& CallbackIterationData, UEngine*, float, bool) -> void"""),
("ini-maybeunused-qdispatch",
"""    static auto QueryHarnessEngineTickDispatch([[maybe_unused]] Hook::TCallbackIterationData<void>& CallbackIterationData, [[maybe_unused]] UEngine*, [[maybe_unused]] float, [[maybe_unused]] bool) -> void""",
"""    [[maybe_unused]] static auto QueryHarnessEngineTickDispatch([[maybe_unused]] Hook::TCallbackIterationData<void>& CallbackIterationData, [[maybe_unused]] UEngine*, [[maybe_unused]] float, [[maybe_unused]] bool) -> void"""),
# ---- gate core ----
("ini-gate",
"""    static Hook::GlobalCallbackId FNameConstructedHookId{};""",
"""    static Hook::GlobalCallbackId FNameConstructedHookId{};

    // =====================================================================================
    // [R3] FuncPtr-marshal mode switch (DIFF 2, worker-r3-diffs-authoring 2026-10-04;
    // R3-TIER-DESIGN 4.4/5.3; LAUNCH10-MATRIX R9 consumes the armed build). Preferred
    // sourcing: the build define (-DUE4SS_R3_MARSHAL_MODE=1) recorded in the run
    // manifest; the in-file fallback default 0 keeps THIS unit behavior-identical to
    // the staged initializer until a controller arms it (constexpr pattern per
    // TEAM:worker-no-exec-mode-gate, including its SETTINGS-V3 trap-2 rationale).
    // kR3MarshalMode=true compiles OUT the registrations that are FATAL under BOTH_FAIL
    // (the FName-ctor PLH detour verifier and the two __PROSPERO__ EngineTick detour
    // registrations) and routes the game-thread identity publication + query-harness
    // dispatch onto the R3 FuncPtr-marshal via the exported bridges at the bottom of
    // this file (consumed by LuaMod DIFF 1). There is no runtime re-arm path.
    // =====================================================================================
#ifndef UE4SS_R3_MARSHAL_MODE
#define UE4SS_R3_MARSHAL_MODE 0
#endif
    namespace R3MarshalGate
    {
        static constexpr bool kR3MarshalMode = UE4SS_R3_MARSHAL_MODE != 0;

        // Refusals run on the serialized startup thread (no atomics needed for the
        // counter). pump_pending is the cross-TU pending-pump state DIFF 1 consumes;
        // the marshal drain reads it from the game thread while startup wrote it once.
        static std::atomic_bool pump_pending{false};
        [[maybe_unused]] static unsigned refused_site_count{0};

        // One named line per refused detour-registration site (no-exec-gate shape).
        [[maybe_unused]] static auto refuse_registration_site(const CharType* site) -> void
        {
            ++refused_site_count;
            Output::send<LogLevel::Warning>(STR("[R3] refused: {} (detour registration compiled out under R3 marshal mode)\\n"), site);
        }
    }"""),
# ---- FName detour skip ----
("ini-fname-skip",
"""        if (AddressOverride)
        {
            FName::ConstructorInternal.assign_temp_address(AddressOverride);
        }

        // To ensure we don't use the FName constructor too early, hook it until something uses it.
        Output::send(STR("Phase 2: Verifying FName constructor...\\n"));""",
"""        if (AddressOverride)
        {
            FName::ConstructorInternal.assign_temp_address(AddressOverride);
        }

#ifdef __PROSPERO__
        if constexpr (R3MarshalGate::kR3MarshalMode)
        {
            // [R3] BOTH_FAIL (DIFF 2; R3D 4.4c/5.3): RegisterFNameConstructorPostCallback
            // arms a PLH detour (exec write) whose ERROR_ID path throws at startup before
            // any Lua loads. Compiled out under R3 mode. Honest labels: the constructor
            // is NOT verified by this unit; verification is delegated to the FC-1
            // read-only validator (VSD 5 C1) when that lands. The data-side address
            // override binding above is kept, and the verification latch is settled so
            // the detour path's blocking wait can never be reached, let alone deadlock.
            StaticStorage::FNameVerificationStatus.store(true, std::memory_order_release);
            StaticStorage::FNameVerificationStatus.notify_all();
            R3MarshalGate::refuse_registration_site(STR("FName constructor verification detour (verification delegated to the FC-1 read-only validator, VSD 5 C1)"));
            return;
        }
#endif
        // To ensure we don't use the FName constructor too early, hook it until something uses it.
        Output::send(STR("Phase 2: Verifying FName constructor...\\n"));"""),
# ---- EngineTick registration gate ----
("ini-tick-gate",
"""#ifdef __PROSPERO__
            // Native publication/verification retirement may fail transiently.
            // HookedEngineTick invalidates itself after those steps succeed.
            const auto initializer_callback = Hook::RegisterEngineTickPreCallback(HookedEngineTick, {false, false, STR("UE4SS"), STR("GameThreadInitializer")});
            if (initializer_callback == Hook::ERROR_ID)
            {
                throw std::runtime_error("Native game-thread initializer callback registration failed");
            }""",
"""#ifdef __PROSPERO__
            if constexpr (R3MarshalGate::kR3MarshalMode)
            {
                // [R3] BOTH_FAIL (DIFF 2; R3D 4.4a/4.4b/5.3): RegisterEngineTickPreCallback
                // arms a forwarding detour whose only installable backends (PLH / D-1
                // mprotect) are dead on this world; arming would throw here before any Lua
                // loads. The game-thread pump - identity publication + query-harness
                // dispatch - is DEFERRED to the R3 FuncPtr-marshal drain (LuaMod DIFF 1).
                // The deferral stays loud and counted; the runtime status never silently
                // proceeds as if the pump were armed.
                R3MarshalGate::refuse_registration_site(STR("EngineTick pre-callback registrations (GameThreadInitializer + QueryHarnessDispatch)"));
                Output::send(STR("[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal\\n"));
                R3MarshalGate::pump_pending.store(true, std::memory_order_release);
                R3MarshalGate::refused_site_count += 1; // the second refused site of this block
            }
            else
            {
            // Native publication/verification retirement may fail transiently.
            // HookedEngineTick invalidates itself after those steps succeed.
            const auto initializer_callback = Hook::RegisterEngineTickPreCallback(HookedEngineTick, {false, false, STR("UE4SS"), STR("GameThreadInitializer")});
            if (initializer_callback == Hook::ERROR_ID)
            {
                throw std::runtime_error("Native game-thread initializer callback registration failed");
            }"""),
("ini-tick-gate-close",
"""            const auto query_dispatch_callback = Hook::RegisterEngineTickPreCallback(QueryHarnessEngineTickDispatch, {false, false, STR("UE4SS"), STR("QueryHarnessDispatch")});
            if (query_dispatch_callback == Hook::ERROR_ID)
            {
                throw std::runtime_error("Query harness tick dispatch callback registration failed");
            }
#else""",
"""            const auto query_dispatch_callback = Hook::RegisterEngineTickPreCallback(QueryHarnessEngineTickDispatch, {false, false, STR("UE4SS"), STR("QueryHarnessDispatch")});
            if (query_dispatch_callback == Hook::ERROR_ID)
            {
                throw std::runtime_error("Query harness tick dispatch callback registration failed");
            }
            }
#else"""),
# ---- exported bridges ----
("ini-bridges",
"""    auto IsInGameThreadRaw() noexcept -> bool
    {
#ifdef __PROSPERO__
        return UnrealInitializer::NativeGameThreadIdentity.in_current_raw();
#else
        return IsGameThreadInitialized() && std::this_thread::get_id() == GetGameThreadId();
#endif
    }
}""",
"""    auto IsInGameThreadRaw() noexcept -> bool
    {
#ifdef __PROSPERO__
        return UnrealInitializer::NativeGameThreadIdentity.in_current_raw();
#else
        return IsGameThreadInitialized() && std::this_thread::get_id() == GetGameThreadId();
#endif
    }

#ifdef __PROSPERO__
    // =====================================================================================
    // [R3] FuncPtr-marshal bridges (DIFF 2; R3D 4.4a/4.4b). Called from the LuaMod
    // marshal drain (DIFF 1) once the calibration-selected carrier is armed. Declared by
    // the consumer locally (namespace Unreal in LuaMod.cpp) - no shared header change,
    // since a header edit would force a tree-wide recompile; the signatures are the
    // DIFF 1/DIFF 2 contract. Defined only under __PROSPERO__; the consumer references
    // them only under __PROSPERO__ as well.
    // =====================================================================================
    auto R3MarshalPublishGameThreadIdentity() -> bool
    {
        if constexpr (UnrealInitializer::R3MarshalGate::kR3MarshalMode)
        {
            // HookedEngineTick publication logic (SINI:152-180), reused, not re-invented:
            // cancellation check first, then publish_current; the first published ID is
            // immutable and publication never manufactures a worker identity.
            const auto& StartupWait = UnrealInitializer::StaticStorage::GlobalConfig.StartupWait;
            if (StartupWait && StartupWait->IsCancelled && StartupWait->IsCancelled())
            {
                // Startup refused/cancelled: never publish; the marshal drain gates on
                // this false return and drains nothing (refusal, not degradation).
                return false;
            }
            const auto publication = UnrealInitializer::NativeGameThreadIdentity.publish_current();
            if (publication == NativeThreadDetail::Publication::Unavailable)
            {
                static std::atomic<bool> logged_unavailable{false};
                if (!logged_unavailable.exchange(true))
                {
                    Output::send<LogLevel::Error>(STR("[R3] Unable to retrieve or synchronize ID of game thread\\n"));
                }
                return false;
            }
            // Published and AlreadyCurrent both mean this caller IS the game thread;
            // DifferentThread refuses. (NativeThreadDetail::Publication order:
            // Published, AlreadyCurrent, DifferentThread, Unavailable.)
            return publication != NativeThreadDetail::Publication::DifferentThread;
        }
        else
        {
            static std::atomic<bool> logged_disarmed{false};
            if (!logged_disarmed.exchange(true))
            {
                Output::send<LogLevel::Warning>(STR("[R3] publish bridge called while R3 marshal mode is disarmed; refusing\\n"));
            }
            return false;
        }
    }

    auto R3MarshalQueryHarnessDispatch() -> void
    {
        if constexpr (UnrealInitializer::R3MarshalGate::kR3MarshalMode)
        {
            // QueryHarnessEngineTickDispatch body (SINI:215-219) unchanged: per-
            // observation entry for the query harness; refusals are counted inside the
            // harness and never thrown. Semantic change declared per R3D 4.4b: the
            // harness's "two consecutive accepted reads" now count CARRIER invocations,
            // not engine ticks; rung instruments must be relabeled.
            RC::Platform::QueryHarness::Global().OnEngineTickDispatch();
        }
    }

    auto R3MarshalPumpPending() -> bool
    {
        return UnrealInitializer::R3MarshalGate::pump_pending.load(std::memory_order_acquire);
    }
#endif
}"""),
]

# ---- 2. apply with uniqueness-checked anchors ------------------------------------
anchor_receipt = {"anchors": [], "aborted": None}
def apply(fname, edits):
    text = (base / fname).read_text()
    for aid, old, new in edits:
        n = text.count(old)
        if n != 1:
            anchor_receipt["aborted"] = {"file": fname, "anchor": aid, "occurrences": n}
            sys.exit(f"ANCHOR FAILURE {fname}:{aid}: {n} occurrences (need exactly 1)")
        text = text.replace(old, new)
        anchor_receipt["anchors"].append({"file": fname, "anchor": aid, "occurrences": n})
    (patched / fname).write_text(text)
    return sha(patched / fname)

out = {}
for fname, edits in EDITS.items():
    out[fname] = {"base_sha256": pin_receipt[fname]["sha256"], "patched_sha256": apply(fname, edits), "n_anchors": len(edits)}
anchor_receipt["files"] = out
(receipts / "apply-anchors.json").write_text(json.dumps(anchor_receipt, indent=1) + "\n")
print(f"applied {sum(len(v) for v in EDITS.values())} anchors across {len(EDITS)} files, no drift")

# ---- 3. unified diffs --------------------------------------------------------------
DIFF_LABELS = {
    "LuaMod.cpp": ("a/source/port/ue4ss/src/Mod/LuaMod.cpp", "b/source/port/ue4ss/src/Mod/LuaMod.cpp"),
    "SettingsManager.hpp": ("a/source/port/ue4ss/include/SettingsManager.hpp", "b/source/port/ue4ss/include/SettingsManager.hpp"),
}
diff1 = ME / "diffs/r3-luamod-funcptr-marshal.diff"
diff2 = ME / "diffs/r3-initializer-r3-marshal-mode.diff"
for fname, outp, labels in (("SettingsManager.hpp", diff1, DIFF_LABELS["SettingsManager.hpp"]),
                            ("LuaMod.cpp", diff1, DIFF_LABELS["LuaMod.cpp"]),
                            ("UnrealInitializer.cpp", diff2, ("a/staging/UnrealInitializer.cpp", "b/staging/UnrealInitializer.cpp"))):
    p = subprocess.run(["diff", "-u", str(base / fname), str(patched / fname)], capture_output=True, text=True)
    body = p.stdout.replace(f"--- {base / fname}", f"--- {labels[0]}").replace(f"+++ {patched / fname}", f"+++ {labels[1]}")
    with open(outp, "a") as f:
        f.write(body)
h1 = {"path": str(diff1), "sha256": sha(diff1), "bytes": diff1.stat().st_size}
h2 = {"path": str(diff2), "sha256": sha(diff2), "bytes": diff2.stat().st_size}
(receipts / "diff-hashes.json").write_text(json.dumps({"diff1": h1, "diff2": h2}, indent=1) + "\n")
print("diff1:", h1["sha256"][:16], h1["bytes"], "bytes")
print("diff2:", h2["sha256"][:16], h2["bytes"], "bytes")
