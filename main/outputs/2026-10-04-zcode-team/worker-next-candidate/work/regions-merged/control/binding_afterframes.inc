        m_lua.register_function("ExecuteInGameThreadAfterFrames", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'ExecuteInGameThreadAfterFrames'.
Overloads:
#1: ExecuteInGameThreadAfterFrames(integer frames, LuaFunction callback) -> integer handle)"};

            lua_State* L = lua.get_lua_state();
            if (!lua.is_integer() || !lua_isfunction(L, 2))
            {
                lua.throw_error(error_overload_not_found);
            }

            // Frame-based delays require EngineTick - cannot use ProcessEvent
            if (!is_engine_tick_hook_available())
            {
                lua.throw_error("ExecuteInGameThreadAfterFrames: EngineTick hook is not available (AOB scan failed). Frame-based delays require EngineTick.");
            }

#if defined(__PROSPERO__)
            LuaMod::ensure_engine_tick_hooked();
#endif
            auto frames = lua.get_integer();
            auto mod = get_mod_ref(lua);

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
            action.delay_frames = frames;
            action.frames_remaining = frames;
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
#if !defined(__PROSPERO__)
            LuaMod::ensure_engine_tick_hooked();
#endif

            lua.set_integer(action.handle);
            return 1;
        });
