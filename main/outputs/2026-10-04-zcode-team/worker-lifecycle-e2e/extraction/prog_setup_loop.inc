            auto widen_ascii = [](const std::string& narrow) -> StringType {
                return widen_ascii_native_key(narrow);
            };
            for (const auto& mod : m_native_context.mod_source->lua_mods())
            {
                auto name = widen_ascii(mod.name);
                if (!find_mod_by_name<LuaMod>(name))
                    m_mods.emplace_back(std::make_unique<LuaMod>(*this, std::move(name), widen_ascii(mod.path)));
            }
