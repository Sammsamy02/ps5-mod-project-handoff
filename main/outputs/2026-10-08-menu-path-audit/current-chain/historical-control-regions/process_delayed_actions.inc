    template <GameThreadExecutionMethod Executor>
    static auto process_delayed_actions(std::vector<LuaMod::DelayedGameThreadAction>& actions) -> void
    {
        struct ReadyDelayedExec
        {
            size_t index{};
            const LuaMadeSimple::Lua* lua{};
            int32_t lua_action_function_ref{};
        };

        std::vector<ReadyDelayedExec> ready{};
        {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            const auto now = std::chrono::steady_clock::now();
            for (size_t i = 0; i < actions.size(); ++i)
            {
                auto& action = actions[i];

                if (action.status == LuaMod::DelayedActionStatus::PendingRemoval)
                {
                    continue;
                }

                if constexpr (Executor == GameThreadExecutionMethod::ProcessEvent)
                {
                    if (action.method != GameThreadExecutionMethod::ProcessEvent)
                    {
                        continue;
                    }
                }
                else if constexpr (Executor == GameThreadExecutionMethod::EngineTick)
                {
                    if (action.method != GameThreadExecutionMethod::EngineTick)
                    {
                        continue;
                    }
                }
                else if constexpr (Executor == kFuncPtrMarshalExecutor)
                {
                    // [R3] the marshal drains only marshal-tagged delayed actions
                    // (time-based; frame-based delays remain EngineTick-only, so the
                    // readiness block below leaves them untouched for this executor).
                    if (action.method != kFuncPtrMarshalExecutor)
                    {
                        continue;
                    }
                }

                if (action.status == LuaMod::DelayedActionStatus::Paused || action.status == LuaMod::DelayedActionStatus::Executing)
                {
                    continue;
                }

                bool ready_to_execute = false;
                if (action.method == GameThreadExecutionMethod::EngineTick && action.delay_frames > 0)
                {
                    action.frames_remaining--;
                    ready_to_execute = action.frames_remaining <= 0;
                }
                else if (action.method != GameThreadExecutionMethod::EngineTick && action.delay_frames > 0)
                {
                    Output::send<LogLevel::Warning>(STR("ProcessEvent hook received frame-based delayed action - this should not happen\n"));
                    continue;
                }
                else
                {
                    ready_to_execute = now >= action.execute_at;
                }

                if (!ready_to_execute || LuaMod::m_is_currently_executing_game_action)
                {
                    continue;
                }

                action.status = LuaMod::DelayedActionStatus::Executing;
                ready.push_back(ReadyDelayedExec{i, action.lua, action.lua_action_function_ref});
            }
        }

        for (auto& exec : ready)
        {
            if (LuaMod::m_is_currently_executing_game_action)
            {
                continue;
            }

            LuaMod::m_is_currently_executing_game_action = true;
            TRY([&]() {
                exec.lua->registry().get_function_ref(exec.lua_action_function_ref);
                exec.lua->call_function(0, 0);
            });
            LuaMod::m_is_currently_executing_game_action = false;
        }

        {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            const auto now = std::chrono::steady_clock::now();
            for (auto& exec : ready)
            {
                if (exec.index >= actions.size())
                {
                    continue;
                }
                auto& action = actions[exec.index];

                if (action.status == LuaMod::DelayedActionStatus::PendingRemoval)
                {
                    continue;
                }

                if (action.pause_after_execution)
                {
                    action.pause_after_execution = false;
                    if (action.method == GameThreadExecutionMethod::EngineTick && action.delay_frames > 0)
                    {
                        action.frames_remaining = action.delay_frames;
                    }
                    else
                    {
                        action.time_remaining_ms = action.delay_ms;
                        action.execute_at = now + std::chrono::milliseconds(action.delay_ms);
                    }
                    action.status = LuaMod::DelayedActionStatus::Paused;
                    continue;
                }

                if (action.is_looping)
                {
                    if (action.method == GameThreadExecutionMethod::EngineTick && action.delay_frames > 0)
                    {
                        action.frames_remaining = action.delay_frames;
                    }
                    else
                    {
                        action.execute_at = now + std::chrono::milliseconds(action.delay_ms);
                    }
                    action.status = LuaMod::DelayedActionStatus::Active;
                }
                else
                {
                    action.status = LuaMod::DelayedActionStatus::PendingRemoval;
                }
            }

            std::erase_if(actions, [&](LuaMod::DelayedGameThreadAction& action) -> bool {
                if (action.status == LuaMod::DelayedActionStatus::PendingRemoval)
                {
                    luaL_unref(action.lua->get_lua_state(), LUA_REGISTRYINDEX, action.lua_action_function_ref);
                    return true;
                }
                return false;
            });
        }
    }
