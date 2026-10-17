    // Helper to ensure engine tick hook is registered
    auto LuaMod::ensure_engine_tick_hooked() -> void
    {
#ifdef __PROSPERO__
        std::lock_guard<std::recursive_mutex> guard{m_thread_actions_mutex};
#endif
        if (!m_is_engine_tick_hooked)
        {
#if defined(__PROSPERO__)
            const auto id = Unreal::Hook::RegisterEngineTickPreCallback(engine_tick_hook, {false, false, STR("UE4SS"), STR("LuaModImpl")});
            if (id == Unreal::Hook::ERROR_ID)
                throw std::runtime_error("EngineTick callback registration failed");
#else
            Unreal::Hook::RegisterEngineTickPreCallback(engine_tick_hook, {false, false, STR("UE4SS"), STR("LuaModImpl")});
#endif
            m_is_engine_tick_hooked = true;
        }
    }

    // Helper to ensure process event hook is registered
    auto LuaMod::ensure_process_event_hooked(LuaMod* mod) -> void
    {
#ifdef __PROSPERO__
        std::lock_guard<std::recursive_mutex> guard{m_thread_actions_mutex};
        if (NativeProcessEventHookId == Unreal::Hook::ERROR_ID)
        {
            const auto id = Unreal::Hook::RegisterProcessEventPreCallback(process_event_hook, {false, false, STR("UE4SS"), STR("LuaModImpl")});
            if (id == Unreal::Hook::ERROR_ID)
                throw std::runtime_error("ProcessEvent callback registration failed");
            NativeProcessEventHookId = id;
        }
        mod->m_is_process_event_hooked = true;
#else
        if (!mod->m_is_process_event_hooked)
        {
#if defined(__PROSPERO__)
            const auto id = Unreal::Hook::RegisterProcessEventPreCallback(process_event_hook, {false, false, STR("UE4SS"), STR("LuaModImpl")});
            if (id == Unreal::Hook::ERROR_ID)
                throw std::runtime_error("ProcessEvent callback registration failed");
#else
            Unreal::Hook::RegisterProcessEventPreCallback(process_event_hook, {false, false, STR("UE4SS"), STR("LuaModImpl")});
#endif
            mod->m_is_process_event_hooked = true;
        }
#endif
    }
