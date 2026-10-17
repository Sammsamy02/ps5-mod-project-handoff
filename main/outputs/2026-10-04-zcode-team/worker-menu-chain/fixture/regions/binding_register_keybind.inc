            lua.register_function("RegisterKeyBind", [](const LuaMadeSimple::Lua& lua) -> int {
                std::string error_overload_not_found{R"(
No overload found for function 'RegisterKeyBind'.
Overloads:
#1: RegisterKeyBind(integer key)
#2: RegisterKeyBind(integer key, table modifier_key_integers))"};

                Mod* mod = get_mod_ref(lua);

                if (!lua.is_integer())
                {
                    lua.throw_error(error_overload_not_found);
                }

                int64_t key_from_lua = lua.get_integer();
                if (key_from_lua < std::numeric_limits<uint8_t>::min() || key_from_lua > std::numeric_limits<uint8_t>::max())
                {
                    lua.throw_error("Parameter #1 for function 'RegisterKeyBind' must be an integer between 0 and 255");
                }

                Input::Key key_to_register = static_cast<Input::Key>(key_from_lua);

                const auto lua_keybind_callback_lambda = [](const LuaMadeSimple::Lua& lua, const int callback_register_index) -> void {
                    std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                    try
                    {
                        lua.registry().get_function_ref(callback_register_index);
                        lua.call_function(0, 0);
                    }
                    catch (std::runtime_error& e)
                    {
                        Output::send<LogLevel::Error>(STR("{}\n"), ensure_str(lua.handle_error(e.what())));
                    }
                };

                if (lua.is_function())
                {
                    // Overload #1
                    // P1: Key to register
                    // P2: Callback

                    // Duplicate the Lua function to the top of the stack for luaL_ref
                    lua_pushvalue(lua.get_lua_state(), 1);

                    // Take a reference to the Lua function (it also pops it of the stack)
                    const int32_t lua_callback_registry_index = lua.registry().make_ref();

                    // Taking 'lua_callback_registry_index' by copy here to ensure its survival
                    // Using a 'custom_data' of 1 to signify that this keydown event was created by a mod
                    mod->m_program.register_keydown_event(
                            key_to_register,
                            [&lua, lua_callback_registry_index, lua_keybind_callback_lambda]() {
                                lua_keybind_callback_lambda(lua, lua_callback_registry_index);
                            },
                            1, mod);
                }
                else if (lua.is_table())
                {
                    // Overload #2
                    // P1: Key to register
                    // P2: Table of modifier keys
                    // P3: Callback

                    Input::Handler::ModifierKeyArray modifier_keys{};

                    uint8_t table_counter{};
                    lua.for_each_in_table([&](LuaMadeSimple::LuaTableReference table) -> bool {
                        if (!table.value.is_integer())
                        {
                            lua.throw_error("Lua function 'RegisterKeyBind', overload #2, requires a table of 1-byte large integers as the second parameter");
                        }

                        int64_t full_integer = table.value.get_integer();
                        if (full_integer < std::numeric_limits<uint8_t>::min() || full_integer > std::numeric_limits<uint8_t>::max())
                        {
                            lua.throw_error("Lua function 'RegisterKeyBind', overload #2, requires a table of 1-byte large integers as the second parameter");
                        }

                        modifier_keys[table_counter++] = static_cast<Input::ModifierKey>(table.value.get_integer());

                        return false;
                    });

                    // Duplicate the Lua function to the top of the stack for luaL_ref
                    lua_pushvalue(lua.get_lua_state(), 1);

                    // Take a reference to the Lua function (it also pops it of the stack)
                    const auto lua_callback_registry_index = lua.registry().make_ref();

                    if (table_counter > 0)
                    {
                        mod->m_program.register_keydown_event(
                                key_to_register,
                                modifier_keys,
                                [&lua, lua_callback_registry_index, lua_keybind_callback_lambda]() {
                                    lua_keybind_callback_lambda(lua, lua_callback_registry_index);
                                },
                                1, mod);
                    }
                    else
                    {
                        mod->m_program.register_keydown_event(
                                key_to_register,
                                [&lua, lua_callback_registry_index, lua_keybind_callback_lambda]() {
                                    lua_keybind_callback_lambda(lua, lua_callback_registry_index);
                                },
                                1, mod);
                    }
                }
                else
                {
                    lua.throw_error(error_overload_not_found);
                }

                return 0;
            });
