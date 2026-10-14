    // Private helper: Ensures hook thread exists and returns the registry reference (or LUA_REFNIL if already exists)
    static int ensure_hook_thread_exists(LuaMod* mod)
    {
        if (mod->m_hook_lua == nullptr)
        {
            // First use - create new thread and anchor it in the registry
            mod->m_hook_lua = &mod->lua().new_thread();
            int thread_ref = luaL_ref(mod->lua().get_lua_state(), LUA_REGISTRYINDEX);
            return thread_ref;
        }

        // Thread already exists and is already anchored
        return LUA_REFNIL;
    }

    // Returns the hook lua thread for immediate use (doesn't need registry reference management)
    auto static get_hook_lua(LuaMod* mod) -> LuaMadeSimple::Lua*
    {
        ensure_hook_thread_exists(mod);
        return mod->m_hook_lua;
    }

    // Returns hook state with registry reference (for persistent hooks that need cleanup)
    // auto static make_hook_state(Mod* mod, const LuaMadeSimple::Lua& lua)->std::shared_ptr<LuaMadeSimple::Lua>
    auto static make_hook_state(LuaMod* mod) -> std::pair<LuaMadeSimple::Lua*, int>
    {
        int thread_ref = ensure_hook_thread_exists(mod);
        return {mod->m_hook_lua, thread_ref};

        // Make the hook thread (which is just a separate Lua stack) be a global in its parent.
        // This is needed because otherwise it will be GCd when we don't want it to.
        // lua_setglobal(lua.get_lua_state(), "HookThread");

        // Commenting out until we switch to lua_newstate instead of lua_newthread.
        // For the switch to happen, we need to be able to move or copy Lua types across lua_states which we can't do yet.
        /*
        mod->m_hook_lua->register_function("RegisterHook", [](const LuaMadeSimple::Lua& lua) -> int {
            lua.throw_error("'RegisterHook' is not allowed from the game thread");
            return 0;
        });

        mod->m_hook_lua->register_function("NotifyOnNewObject", [](const LuaMadeSimple::Lua& lua) -> int {
            lua.throw_error("'NotifyOnNewObject' is not allowed in the game thread");
            return 0;
        });
        //*/
        //}
    }
