    // [R3-L10LM] File-local executor tag for the FuncPtr-marshal drain (this
    // rebase edition, worker-luamod-rebase 2026-10-04). DIFF-1's SettingsManager.hpp
    // enum append is deliberately NOT landed in this edition: the appended
    // enumerator's value would be 2 (ProcessEvent=0, EngineTick=1 in the base
    // enum, appended last per DIFF-1 hunk 1), so a file-local constexpr with
    // that exact value is assignment-/comparison-/template-argument-compatible
    // WITHOUT touching the shared header - zero additional recompile set. The
    // named Lua table key below is a string, not the C++ identifier, unchanged.
    static constexpr auto kFuncPtrMarshalExecutor = static_cast<GameThreadExecutionMethod>(2);

    // =====================================================================================
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

#ifdef __PROSPERO__
    // [R3-LADDER] (worker-r3-ladder 2026-10-05; RUNBOOK l11-marshal-integration
    // 5.1 item 1, the WithDelay-family remainder) Usable-pump predicate for the
    // ladder re-order. The marshal is the game-thread pump only while the rev-B
    // pairing law holds: the initializer deferred its EngineTick registrations
    // (R3MarshalPumpPending, DIFF 2). armed && !pump_pending is that pairing
    // contradiction - a marshal armed while the initializer owns the pump -
    // and is treated as a PROVABLY DEAD drain: the ExecuteInGameThread /
    // WithDelay-family ladders refuse loudly instead of silently queueing onto
    // it or silently arming a second EngineTick pump. Deliberately NOT a
    // "drain succeeded" test: between arming and the first carrier invocation
    // the identity publication is legitimately pending, and queueing then is
    // the designed lazy semantics (R3D 4.4a); publication refusal remains the
    // drain body's own fail-closed gate (funcptr_marshal_hook).
    static auto funcptr_marshal_usable() -> bool
    {
        return is_funcptr_marshal_armed() && Unreal::R3MarshalPumpPending();
    }
#endif
