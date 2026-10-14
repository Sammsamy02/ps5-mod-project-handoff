        m_lua.register_function("ExecuteInGameThread", [](const LuaMadeSimple::Lua& lua) -> int {
            std::string error_overload_not_found{R"(
No overload found for function 'ExecuteInGameThread'.
Overloads:
#1: ExecuteInGameThread(LuaFunction callback)
#2: ExecuteInGameThread(LuaFunction callback, EGameThreadMethod method)
    method: EGameThreadMethod.EngineTick or EGameThreadMethod.ProcessEvent)"};

            lua_State* L = lua.get_lua_state();
            GameThreadExecutionMethod method = LuaMod::m_default_game_thread_method;
            int callback_idx = 1;

            if (lua_isfunction(L, 1) && lua_isinteger(L, 2))
            {
                // Overload #2: callback, method
                method = static_cast<GameThreadExecutionMethod>(lua_tointeger(L, 2));
            }
            else if (!lua_isfunction(L, 1))
            {
                lua.throw_error(error_overload_not_found);
            }

            const auto mod = get_mod_ref(lua);

#if defined(__PROSPERO__)
            // [claude-cloud event-carrier] In the marshal world (pump_pending) the marshal
            // is the only pump: queue onto it even before a carrier arms, never onto the
            // EngineTick/ProcessEvent queues whose detours are refused (stranded closures).
            if (funcptr_marshal_usable())
            {
                method = kFuncPtrMarshalExecutor;
            }
#endif
            // Check hook availability before registering
            if (method == GameThreadExecutionMethod::EngineTick)
            {
                if (!is_engine_tick_hook_available())
                {
#if defined(__PROSPERO__)
                    // [R3] three-state ladder (DIFF 1; R3D 4.5):
                    // EngineTick -> ProcessEvent -> FuncPtrMarshal -> throw
                    if (is_process_event_hook_available())
                    {
                        // [R3-L11EL] method selection only; the lazy detour arm
                        // moved AFTER the enqueue (below), so a refused
                        // registration can never discard the closure.
                        method = GameThreadExecutionMethod::ProcessEvent;
                    }
                    else if (is_funcptr_marshal_armed())
                    {
                        method = kFuncPtrMarshalExecutor;
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
#if defined(__PROSPERO__)
                    // [R3-L11EL] (worker-l11-enqueue-ladder, RUNBOOK 5.1 item 1):
                    // address-availability is NOT registrability (R3D 4.5 trap).
                    // In W_FAILS_R_WORKS the EngineTick address IS assigned
                    // (vtable fallback) and the lazy arm below used to run here,
                    // THROW before anything was enqueued, and bypass the armed
                    // FuncPtr marshal entirely (main.lua:695 default method).
                    // Marshal-first when armed: the marshal is the game-thread
                    // pump in this world; no second EngineTick pump may be
                    // attempted (the initializer deferred those registrations).
                    if (is_funcptr_marshal_armed())
                    {
                        method = kFuncPtrMarshalExecutor;
                    }
                    // else: keep EngineTick; the lazy arm is attempted only
                    // AFTER the closure is enqueued (order guarantee below).
#else
                    LuaMod::ensure_engine_tick_hooked();
#endif
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
                        // [R3-L11EL] method selection only; the lazy detour arm
                        // moved AFTER the enqueue (below).
                        method = GameThreadExecutionMethod::EngineTick;
                    }
                    else if (is_funcptr_marshal_armed())
                    {
                        method = kFuncPtrMarshalExecutor;
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
#if defined(__PROSPERO__)
                    // [R3-L11EL] mirror of the EngineTick branch: when the marshal
                    // is armed it outranks the address check (R3D 4.5), and the
                    // lazy arm runs only AFTER the closure is enqueued.
                    if (is_funcptr_marshal_armed())
                    {
                        method = kFuncPtrMarshalExecutor;
                    }
                    // else: keep ProcessEvent; armed after the enqueue below.
#else
                    LuaMod::ensure_process_event_hooked(mod);
#endif
                }
            }
            else if (method == kFuncPtrMarshalExecutor)
            {
                // [R3] availability = carrier armed (module state), NOT an address
                // is_ready() (R3D 3 trap / 4.5). Enqueueing before arming is safe and
                // lazy (R3D 4.4a): the closure simply awaits the marshal drain.
                if (!is_funcptr_marshal_armed())
                {
                    Output::send<LogLevel::Warning>(STR("ExecuteInGameThread: FuncPtrMarshal requested but the carrier is not armed yet; the closure will await arming\n"));
                }
            }

            // Acquire mutex before ANY Lua state operations to prevent race conditions
            // when async thread and game thread both access the same hook_lua state
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};

            auto [hook_lua, lua_thread_registry_index] = make_hook_state(mod);

            lua_pushvalue(L, callback_idx);
            lua_xmove(L, hook_lua->get_lua_state(), 1);
            const auto func_ref = luaL_ref(hook_lua->get_lua_state(), LUA_REGISTRYINDEX);

            SimpleLuaAction simpleAction{hook_lua, func_ref, lua_thread_registry_index};
            if (method == GameThreadExecutionMethod::EngineTick)
            {
                // Use pending queue if we're currently iterating to prevent iterator invalidation
                if (LuaMod::m_is_processing_actions)
                {
                    LuaMod::m_pending_engine_tick_actions.emplace_back(simpleAction);
                }
                else
                {
                    LuaMod::m_engine_tick_actions.emplace_back(simpleAction);
                }
            }
            else
            {
                // Use pending queue if we're currently iterating to prevent iterator invalidation
                if (LuaMod::m_is_processing_actions)
                {
                    LuaMod::m_pending_game_thread_actions.emplace_back(simpleAction);
                }
                else
                {
                    LuaMod::m_game_thread_actions.emplace_back(simpleAction);
                }
            }

#if defined(__PROSPERO__)
            // [R3-L11EL] THE ORDER GUARANTEE (worker-l11-enqueue-ladder; RUNBOOK
            // 5.1 item 1): the lazy detour arm runs only AFTER the closure has
            // been enqueued, so a refused registration can never discard enqueued
            // work. The queue survives; drain happens via whatever pump exists.
            // Marshal-selected paths arm nothing here: the marshal is armed by the
            // calibration consumer and its pump drains the game-thread queue; a
            // second EngineTick/ProcessEvent pump is exactly what DIFF-2's
            // deferred initializer registrations must not become. The
            // recursive_mutex is already held by this thread (recursive re-entry,
            // same as the RegisterHook path); the pump queues are guarded by it.
            if (method == GameThreadExecutionMethod::EngineTick || method == GameThreadExecutionMethod::ProcessEvent)
            {
                TRY([&]() {
                    if (method == GameThreadExecutionMethod::EngineTick)
                    {
                        LuaMod::ensure_engine_tick_hooked();
                    }
                    else
                    {
                        LuaMod::ensure_process_event_hooked(mod);
                    }
                });
                // Loud stall honesty: a refused arm leaves the closure queued with
                // no drainer. The closure is NOT discarded (enqueue-first); say so
                // every time so "queued forever" is never silent.
                if (method == GameThreadExecutionMethod::EngineTick && !LuaMod::m_is_engine_tick_hooked)
                {
                    Output::send<LogLevel::Error>(STR("[R3] ExecuteInGameThread: closure enqueued but the EngineTick pump is not registered (detour registration refused); it stays queued until a pump exists\n"));
                }
                else if (method == GameThreadExecutionMethod::ProcessEvent && !mod->m_is_process_event_hooked)
                {
                    Output::send<LogLevel::Error>(STR("[R3] ExecuteInGameThread: closure enqueued but the ProcessEvent pump is not registered (detour registration refused); it stays queued until a pump exists\n"));
                }
            }
#endif

            return 0;
        });
