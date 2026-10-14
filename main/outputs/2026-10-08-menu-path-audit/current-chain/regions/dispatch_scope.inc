#ifdef __PROSPERO__
    namespace
    {
        // This protects Lua action state, not arbitrary engine code or memory.
        // Uninstall already uses the same mutex through lua_close.
        thread_local bool NativeLuaActionDispatching{};
        Unreal::Hook::GlobalCallbackId NativeProcessEventHookId{Unreal::Hook::ERROR_ID};

        class NativeLuaActionDispatchScope
        {
            std::unique_lock<std::recursive_mutex> Lock{LuaMod::m_thread_actions_mutex};
            bool PreviousProcessing{LuaMod::m_is_processing_actions};
            bool OwnsDispatch{!PreviousProcessing && !NativeLuaActionDispatching};
        public:
            NativeLuaActionDispatchScope()
            {
                if (OwnsDispatch)
                {
                    LuaMod::m_is_processing_actions = true;
                    NativeLuaActionDispatching = true;
                }
            }
            ~NativeLuaActionDispatchScope()
            {
                if (OwnsDispatch)
                {
                    LuaMod::m_is_processing_actions = PreviousProcessing;
                    NativeLuaActionDispatching = false;
                }
            }
            NativeLuaActionDispatchScope(const NativeLuaActionDispatchScope&) = delete;
            auto owns_dispatch() const noexcept -> bool { return OwnsDispatch; }
        };
    }
#endif
