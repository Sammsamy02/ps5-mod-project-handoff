        if (m_native_source)
        {
            const auto path = modbridge::normalize_path_for_lua(m_mod_path);
            const auto& mods = m_native_source->lua_mods();
            const auto found = std::find_if(mods.begin(), mods.end(), [&](const auto& mod) { return mod.path == path; });
            if (found == mods.end())
            {
                Output::send<LogLevel::Error>(STR("Mod is absent from native Lua source: {}\n"), ensure_str(m_mod_path));
                set_installable(false);
                return;
            }
            m_scripts_path = std::filesystem::u8path(found->scripts_path);
            return;
        }
