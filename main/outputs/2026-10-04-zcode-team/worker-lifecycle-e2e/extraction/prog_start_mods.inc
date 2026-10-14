            if constexpr (std::is_same_v<ModType, LuaMod>)
                for (const auto& name : source->startup_mod_names())
                {
                    // P0 (worker-lifecycle-conversion): startup_mod_names() keys
                    // are narrow ASCII; ensure_str(name) garbles and the lookup
                    // silently misses — enabled native mods would never start.
                    auto mod = UE4SSProgram::find_mod_by_name<LuaMod>(widen_ascii_native_key(name), UE4SSProgram::IsInstalled::Yes);
                    if (mod && !mod->is_started()) mod->start_mod();
                }
