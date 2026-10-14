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
                Output::send(STR("[R3] marshal is the game-thread pump (initializer deferred its EngineTick registrations)\n"));
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
        process_delayed_actions<kFuncPtrMarshalExecutor>(LuaMod::m_delayed_game_thread_actions);

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
