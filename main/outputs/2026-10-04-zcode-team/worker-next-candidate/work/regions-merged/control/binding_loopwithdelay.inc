        m_lua.register_function("LoopInGameThreadWithDelay", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'LoopInGameThreadWithDelay'.
Overloads:
#1: LoopInGameThreadWithDelay(integer delayMs, LuaFunction callback) -> integer handle)"};

            lua_State* L = lua.get_lua_state();
            if (!lua.is_integer() || !lua_isfunction(L, 2))
            {
                lua.throw_error(error_overload_not_found);
            }

            const auto mod = get_mod_ref(lua);

            // Use default method from config, fall back to the other if unavailable
            GameThreadExecutionMethod method = LuaMod::m_default_game_thread_method;
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
                            lua.throw_error("LoopInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed");
                        }
                        method = kFuncPtrMarshalExecutor;
#else
                        lua.throw_error("LoopInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available (AOB scans failed)");
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
                            lua.throw_error("LoopInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed");
                        }
                        method = kFuncPtrMarshalExecutor;
#else
                        lua.throw_error("LoopInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available (AOB scans failed)");
                        method = GameThreadExecutionMethod::EngineTick;
#endif
                    }
                    else
                    {
                        method = GameThreadExecutionMethod::EngineTick;
                    }
                }
            }

            auto delay_ms = lua.get_integer();

            // Acquire mutex before ANY Lua state operations to prevent race conditions
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};

            auto [hook_lua, lua_thread_registry_index] = make_hook_state(mod);

            lua_pushvalue(L, 1);
            lua_xmove(L, hook_lua->get_lua_state(), 1);
            const auto func_ref = luaL_ref(hook_lua->get_lua_state(), LUA_REGISTRYINDEX);

            DelayedGameThreadAction action{};
            action.lua = hook_lua;
            action.lua_action_function_ref = func_ref;
            action.lua_action_thread_ref = lua_thread_registry_index;
            action.method = method;
            action.delay_ms = delay_ms;
            action.execute_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
            action.is_looping = true;
            action.handle = LuaMod::m_next_delayed_action_handle++;

            // Use pending queue if we're currently iterating to prevent iterator invalidation
            if (LuaMod::m_is_processing_actions)
            {
                LuaMod::m_pending_delayed_game_thread_actions.emplace_back(action);
            }
            else
            {
                LuaMod::m_delayed_game_thread_actions.emplace_back(action);
            }

            lua.set_integer(action.handle);
            return 1;
        });
