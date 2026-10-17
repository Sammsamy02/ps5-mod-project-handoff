        m_lua.register_function("ExecuteInGameThreadWithDelay", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'ExecuteInGameThreadWithDelay'.
Overloads:
#1: ExecuteInGameThreadWithDelay(integer delayMs, LuaFunction callback) -> integer handle
#2: ExecuteInGameThreadWithDelay(integer handle, integer delayMs, LuaFunction callback) -> nil (only creates if handle doesn't exist))"};

            lua_State* L = lua.get_lua_state();

            // Determine which overload based on argument count
            int num_args = lua_gettop(L);
            bool has_handle = (num_args >= 3 && lua_isinteger(L, 1) && lua_isinteger(L, 2) && lua_isfunction(L, 3));
            bool no_handle = (num_args >= 2 && lua_isinteger(L, 1) && lua_isfunction(L, 2));

            if (!has_handle && !no_handle)
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
                            lua.throw_error("ExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed");
                        }
                        method = kFuncPtrMarshalExecutor;
#else
                        lua.throw_error("ExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available (AOB scans failed)");
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
                            lua.throw_error("ExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed");
                        }
                        method = kFuncPtrMarshalExecutor;
#else
                        lua.throw_error("ExecuteInGameThreadWithDelay: Neither EngineTick nor ProcessEvent hooks are available (AOB scans failed)");
                        method = GameThreadExecutionMethod::EngineTick;
#endif
                    }
                    else
                    {
                        method = GameThreadExecutionMethod::EngineTick;
                    }
                }
            }

            if (has_handle)
            {
                // Overload #2: ExecuteInGameThreadWithDelay(handle, delayMs, callback)
                // Like UE's Delay - only creates if handle doesn't already exist
                auto handle = lua_tointeger(L, 1);
                auto delay_ms = lua_tointeger(L, 2);

                // Acquire mutex before ANY Lua state operations to prevent race conditions
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};

                // Check if handle already exists (check both main and pending queues)
                for (const auto& action : LuaMod::m_delayed_game_thread_actions)
                {
                    if (action.handle == handle && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                    {
                        // Handle exists, do nothing (like UE's Delay node)
                        return 0;
                    }
                }
                for (const auto& action : LuaMod::m_pending_delayed_game_thread_actions)
                {
                    if (action.handle == handle && action.status != LuaMod::DelayedActionStatus::PendingRemoval)
                    {
                        // Handle exists in pending queue, do nothing
                        return 0;
                    }
                }

                // Handle doesn't exist, create new action
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
            }
            else
            {
                // Overload #1: ExecuteInGameThreadWithDelay(delayMs, callback) -> handle
                auto delay_ms = lua_tointeger(L, 1);

                // Acquire mutex before ANY Lua state operations to prevent race conditions
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};

                auto [hook_lua, lua_thread_registry_index] = make_hook_state(mod);

                lua_pushvalue(L, 2);
                lua_xmove(L, hook_lua->get_lua_state(), 1);
                const auto func_ref = luaL_ref(hook_lua->get_lua_state(), LUA_REGISTRYINDEX);

                DelayedGameThreadAction action{};
                action.lua = hook_lua;
                action.lua_action_function_ref = func_ref;
                action.lua_action_thread_ref = lua_thread_registry_index;
                action.method = method;
                action.delay_ms = delay_ms;
                action.execute_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
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
            }
        });
