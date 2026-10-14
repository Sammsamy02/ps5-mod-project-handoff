    static auto process_simple_actions(std::vector<LuaMod::SimpleLuaAction>& actions) -> void
    {
        std::vector<LuaMod::SimpleLuaAction> to_run{};
        {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            if (actions.empty())
            {
                return;
            }
            to_run = std::move(actions);
            actions.clear();
        }

        std::vector<LuaMod::SimpleLuaAction> deferred{};
        for (auto& lua_data : to_run)
        {
            if (LuaMod::m_is_currently_executing_game_action)
            {
                deferred.emplace_back(std::move(lua_data));
                continue;
            }

            LuaMod::m_is_currently_executing_game_action = true;
            TRY([&]() {
                lua_data.lua->registry().get_function_ref(lua_data.lua_action_function_ref);
                lua_data.lua->call_function(0, 0);
            });
            luaL_unref(lua_data.lua->get_lua_state(), LUA_REGISTRYINDEX, lua_data.lua_action_function_ref);
            LuaMod::m_is_currently_executing_game_action = false;
        }

        if (!deferred.empty())
        {
            std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
            actions.insert(
                actions.end(),
                std::make_move_iterator(deferred.begin()),
                std::make_move_iterator(deferred.end()));
        }
    }
