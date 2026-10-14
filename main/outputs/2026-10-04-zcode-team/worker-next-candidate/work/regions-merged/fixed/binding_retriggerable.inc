        m_lua.register_function("RetriggerableExecuteInGameThreadWithDelay", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'RetriggerableExecuteInGameThreadWithDelay'.
Overloads:
#1: RetriggerableExecuteInGameThreadWithDelay(integer handle, integer delayMs, LuaFunction callback))"};

            lua_State* L = lua.get_lua_state();
            if (!lua_isinteger(L, 1) || !lua_isinteger(L, 2) || !lua_isfunction(L, 3))
            {
                lua.throw_error(error_overload_not_found);
            }

            const auto mod = get_mod_ref(lua);

            // Use default method from config, fall back to the other if unavailable
            GameThreadExecutionMethod method = LuaMod::m_default_game_thread_method;
#if defined(__PROSPERO__)
            // [R3-LADDER] (RUNBOOK 5.1 item 1 remainder; distinct-ID revision
            // worker-r3-ladder 2026-10-05): marshal-first when the marshal is
            // the USABLE game-thread pump. Address-availability is NOT pump
            // ownership: in the walk-permitted world the initializer's vtable
            // fallback assigns TickInternal, so the availability-first order
            // below reached ensure_engine_tick_hooked() BEFORE the enqueue, its
            // refused detour registration threw "EngineTick callback
            // registration failed", and the closure was lost with the armed
            // marshal bypassed (the exact 5.1 trap). The usable marshal takes
            // the method FIRST - deterministically, EngineTick and ProcessEvent
            // defaults alike - and an armed-but-dead drain (pump_pending=false,
            // the rev-B pairing contradiction) refuses loudly instead of
            // falling back silently.
            if (funcptr_marshal_usable())
            {
                method = kFuncPtrMarshalExecutor;
            }
            else if (is_funcptr_marshal_armed())
            {
                lua.throw_error("RetriggerableExecuteInGameThreadWithDelay: R3 FuncPtr-marshal carrier is armed but its drain is dead (pump_pending=false); refusing to queue onto a dead pump and refusing to arm a second EngineTick pump");
            }
#endif
            if (method == GameThreadExecutionMethod::EngineTick)
            {
#if defined(__PROSPERO__)
                if (is_engine_tick_hook_available())
                    LuaMod::ensure_engine_tick_hooked();
#else
                LuaMod::ensure_engine_tick_hooked();
#endif
                if (!is_engine_tick_hook_available())
                {
#if defined(__PROSPERO__)
                    if (is_process_event_hook_available())
                        LuaMod::ensure_process_event_hooked(mod);
#else
                    LuaMod::ensure_process_event_hooked(mod);
#endif
                    if (!is_process_event_hook_available())
                    {
#if defined(__PROSPERO__)
                        // [R3] third ladder rung (DIFF 1; R3D 4.5):
                        // EngineTick -> ProcessEvent -> FuncPtrMarshal -> throw.
                        if (!is_funcptr_marshal_armed())
                        {
                            lua.throw_error("RetriggerableExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed");
                        }
                        method = kFuncPtrMarshalExecutor;
#else
                        lua.throw_error("RetriggerableExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available (AOB scans failed)");
                        method = GameThreadExecutionMethod::ProcessEvent;
#endif
                    }
                    else
                    {
                        method = GameThreadExecutionMethod::ProcessEvent;
                    }
                }
            }
            else if (method == GameThreadExecutionMethod::ProcessEvent)
            {
#if defined(__PROSPERO__)
                if (is_process_event_hook_available())
                    LuaMod::ensure_process_event_hooked(mod);
#else
                LuaMod::ensure_process_event_hooked(mod);
#endif
                if (!is_process_event_hook_available())
                {
#if defined(__PROSPERO__)
                    if (is_engine_tick_hook_available())
                        LuaMod::ensure_engine_tick_hooked();
#else
                    LuaMod::ensure_engine_tick_hooked();
#endif
                    if (!is_engine_tick_hook_available())
                    {
#if defined(__PROSPERO__)
                        // [R3] third ladder rung (DIFF 1; R3D 4.5):
                        // EngineTick -> ProcessEvent -> FuncPtrMarshal -> throw.
                        if (!is_funcptr_marshal_armed())
                        {
                            lua.throw_error("RetriggerableExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed");
                        }
                        method = kFuncPtrMarshalExecutor;
#else
                        lua.throw_error("RetriggerableExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available (AOB scans failed)");
                        method = GameThreadExecutionMethod::EngineTick;
#endif
                    }
                    else
                    {
                        method = GameThreadExecutionMethod::EngineTick;
                    }
                }
            }

            auto handle = lua_tointeger(L, 1);
            auto delay_ms = lua_tointeger(L, 2);

            // Acquire mutex before ANY Lua state operations to prevent race conditions
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};

            // Check if an action with this handle already exists (check both main and pending queues)
            for (auto& action : LuaMod::m_delayed_game_thread_actions)
            {
                if (action.handle == handle && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                {
                    // Reset the timer for the existing action
                    action.delay_ms = delay_ms;
                    action.execute_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
                    action.status = LuaMod::DelayedActionStatus::Active;  // Unpause if paused
                    lua.set_integer(handle);
                    return 1;
                }
            }
            for (auto& action : LuaMod::m_pending_delayed_game_thread_actions)
            {
                if (action.handle == handle && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                {
                    // Reset the timer for the existing action in pending queue
                    action.delay_ms = delay_ms;
                    action.execute_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
                    action.status = LuaMod::DelayedActionStatus::Active;  // Unpause if paused
                    lua.set_integer(handle);
                    return 1;
                }
            }

            // No existing action, create a new one
            auto [hook_lua, lua_thread_registry_index] = make_hook_state(mod);

            lua_pushvalue(L, 3);
            lua_xmove(L, hook_lua->get_lua_state(), 1);
            const auto func_ref = luaL_ref(hook_lua->get_lua_state(), LUA_REGISTRYINDEX);

            DelayedGameThreadAction action{};
            action.lua = hook_lua;
            action.lua_action_function_ref = func_ref;
            action.lua_action_thread_ref = lua_thread_registry_index;
            action.method = method;
            action.delay_ms = delay_ms;
            action.execute_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
            action.is_retriggerable = true;
            action.handle = handle;

            // Use pending queue if we're currently iterating to prevent iterator invalidation
            if (LuaMod::m_is_processing_actions)
            {
                LuaMod::m_pending_delayed_game_thread_actions.emplace_back(action);
            }
            else
            {
                LuaMod::m_delayed_game_thread_actions.emplace_back(action);
            }

            return 0;
        });
