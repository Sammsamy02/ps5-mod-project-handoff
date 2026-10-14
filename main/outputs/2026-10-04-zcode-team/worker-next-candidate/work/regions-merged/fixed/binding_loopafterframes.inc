        m_lua.register_function("LoopInGameThreadAfterFrames", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'LoopInGameThreadAfterFrames'.
Overloads:
#1: LoopInGameThreadAfterFrames(integer frames, LuaFunction callback) -> integer handle)"};

            lua_State* L = lua.get_lua_state();
            if (!lua.is_integer() || !lua_isfunction(L, 2))
            {
                lua.throw_error(error_overload_not_found);
            }

            // Frame-based delays require EngineTick - cannot use ProcessEvent
#if defined(__PROSPERO__)
            // [R3-LADDER] (RUNBOOK 5.1 family closure; worker-r3-ladder
            // 2026-10-05): with the marshal the usable pump, no EngineTick
            // detour may be attempted here - the old path fell through to
            // ensure_engine_tick_hooked(), whose refused registration threw and
            // discarded the closure while the marshal sat armed. The marshal
            // drain deliberately skips frame-based actions
            // (process_delayed_actions: frame delays remain EngineTick-only),
            // so a frame-based request is refused LOUDLY and precisely instead
            // of being silently stalled on a pump that can never count it.
            if (funcptr_marshal_usable())
            {
                lua.throw_error("LoopInGameThreadAfterFrames: frame-based delays need an EngineTick pump; only the R3 FuncPtr-marshal pump exists in this world and its drain cannot count frames");
            }
#endif
            // Frame-based delays require EngineTick - cannot use ProcessEvent
            if (!is_engine_tick_hook_available())
            {
                lua.throw_error("LoopInGameThreadAfterFrames: EngineTick hook is not available (AOB scan failed). Frame-based delays require EngineTick.");
            }

#if defined(__PROSPERO__)
            LuaMod::ensure_engine_tick_hooked();
#endif
            auto frames = lua.get_integer();
            auto mod = get_mod_ref(lua);

            // Acquire mutex before ANY Lua state operations to prevent race conditions
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};

            auto [hook_lua, lua_thread_registry_index] = make_hook_state(mod);

            // After get_integer() pops the first arg, the function is now at index 1
            lua_pushvalue(L, 1);
            lua_xmove(L, hook_lua->get_lua_state(), 1);
            const auto func_ref = luaL_ref(hook_lua->get_lua_state(), LUA_REGISTRYINDEX);

            DelayedGameThreadAction action{};
            action.lua = hook_lua;
            action.lua_action_function_ref = func_ref;
            action.lua_action_thread_ref = lua_thread_registry_index;
            action.delay_frames = frames;
            action.frames_remaining = frames;
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
#if !defined(__PROSPERO__)
            LuaMod::ensure_engine_tick_hooked();
#endif

            lua.set_integer(action.handle);
            return 1;
        });
