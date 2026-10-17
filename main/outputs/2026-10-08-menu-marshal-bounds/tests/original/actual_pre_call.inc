static auto call_ufunction_from_lua_impl(const LuaMadeSimple::Lua& lua, FNativeUFunctionCall& Invocation) -> int
    {
        if (!lua.is_userdata())
        {
            lua.throw_error("[UFunction::setup_metamethods -> __call] Attempted to call a UFunction without UFunction userdata attached");
        }

        auto& lua_object = lua.get_userdata<LuaType::UFunction>();

        Unreal::UFunction* func{};
        Unreal::UObject* calling_context{};
        bool is_first_userdata_function = lua_object.get_remote_cpp_object()->IsA<Unreal::UFunction>();

        if (is_first_userdata_function)
        {
            func = lua_object.get_remote_cpp_object();
            calling_context = lua_object.get_base();
        }
        else
        {
            calling_context = lua_object.get_remote_cpp_object();
        }

        if (lua.is_userdata())
        {
            auto& lua_object2 = lua.get_userdata<LuaType::UE4SSBaseObject>(1, true);
            if (!is_first_userdata_function && lua_object2.derives_from_function())
            {
                func = lua.get_userdata<LuaType::UFunction>(1).get_remote_cpp_object();
            }
            else if (func && !calling_context)
            {
                calling_context = lua.get_userdata<LuaType::UObject>(1).get_remote_cpp_object();
            }
            else
            {
                lua.discard_value();
            }
        }

        if (!func || !calling_context)
        {
            lua.throw_error("[UFunction::call_ufunction_from_lua] Tried calling function without both UFunction and calling context");
        }

        uint8_t num_ufunc_params = func->GetNumParms();
        uint16_t return_value_offset = func->GetReturnValueOffset();

        // Unreal::FProperty* param_next = func->get_child_properties<Unreal::FProperty*>();
        Unreal::FProperty* return_value_property{};
        Unreal::FName return_value_property_type{0u, 0u};
        int32_t return_value_property_offset_internal{};

        DynamicUnrealFunctionData dynamic_unreal_function_data{};
#ifdef __PROSPERO__
        auto& dynamic_unreal_function_out_parameters = Invocation.OutParameters;
#else
        DynamicUnrealFunctionOutParameters dynamic_unreal_function_out_parameters{};
#endif
        bool has_out_params{false};

        // if (param_next)
        if (func->HasChildren())
        {
            uint8_t num_supplied_params = Helper::Integer::to<uint8_t>(lua.get_stack_size());

            // When return_value_offset is 0xFFFF it means that there is no return value so num_ufunc_params is accurate
            // Otherwise you must subtract 1 to account for the return value (stored in the same struct and counts as a param)
            // The ternary makes sure that we never have a negative number of params
            bool has_return_value = return_value_offset != 0xFFFF;
            uint8_t num_expected_params = num_ufunc_params;
            uint8_t num_expected_params_with_return_value = has_return_value ? (num_expected_params - 1 < 0 ? 0 : num_expected_params - 1) : num_expected_params;

            if (num_supplied_params != num_expected_params_with_return_value)
            {
                lua.throw_error(fmt::format("[UFunction::setup_metamethods -> __call] UFunction expected {} parameters, received {}",
                                            num_expected_params_with_return_value,
                                            num_supplied_params));
            }

            // for (uint8_t i = 0; i < num_expected_params; ++i)
            // uint8_t i = 0;
            for (Unreal::FProperty* param_next : Unreal::TFieldRange<Unreal::FProperty>(func, Unreal::EFieldIterationFlags::IncludeDeprecated))
            {
                // if (i > 0)
                //{
                //     // If not the first iteration then get the next param
                //     // If it's the first iteration then paramNext was already set earlier
                //     param_next = param_next->get_next<Unreal::XProperty*>();
                // }

                // if (!param_next)
                //{
                //     break;
                // }

                if (!param_next->HasAnyPropertyFlags(Unreal::CPF_Parm))
                {
                    continue;
                }

                int32_t offset_internal = param_next->GetOffset_Internal();
                Unreal::FName property_type_fname = param_next->GetClass().GetFName();

                // Is the offset of this parameter is the same as the ReturnValueOffset in the UFunction ?
                // If yes, then this parameter should be treated as the return value
                // If no, then treat this as just another parameter
                if (offset_internal == return_value_offset)
                {
                    return_value_property = param_next;
                    return_value_property_type = property_type_fname;
                    return_value_property_offset_internal = offset_internal;
                }
                else
                {
                    if (param_next->HasAnyPropertyFlags(Unreal::CPF_OutParm) && !param_next->HasAnyPropertyFlags(Unreal::CPF_ReturnParm) &&
                        !param_next->HasAnyPropertyFlags(Unreal::CPF_ConstParm))
                    {
                        has_out_params = true;

                        // Store pointer to params property, and a Lua ref to the corresponding table

                        if (!lua.is_table())
                        {
                            lua.throw_error(
                                    "Tried storing reference to a Lua table for an 'Out' parameter when calling a UFunction but no table was on the stack");
                        }

#ifdef __PROSPERO__
                        Invocation.StoreOutTableReference(param_next);
#else
                        // Duplicate the Lua function to the top of the stack for luaL_ref
                        lua_pushvalue(lua.get_lua_state(), 1);

                        // Take a reference to the Lua function (it also pops it of the stack)
                        dynamic_unreal_function_out_parameters.add({.property = param_next, .lua_ref = lua.registry().make_ref()});
#endif

                        if (!param_next->IsA<Unreal::FStructProperty>() && !param_next->IsA<Unreal::FArrayProperty>())
                        {
                            continue;
                        }
                    }

                    int32_t name_comparison_index = property_type_fname.GetComparisonIndex();

                    if (StaticState::m_property_value_pushers.contains(name_comparison_index))
                    {
                        void* data = &dynamic_unreal_function_data.data[offset_internal];

                        const PusherParams pusher_params{.operation = Operation::Set,
                                                         .lua = lua,
                                                         .base = static_cast<Unreal::UObject*>(static_cast<void*>(dynamic_unreal_function_data.data)),
                                                         .data = data,
                                                         .property = param_next};
                        StaticState::m_property_value_pushers[name_comparison_index](pusher_params);
                    }
                    else
                    {
                        std::string parameter_type_name = to_string(property_type_fname.ToString());
                        std::string parameter_name = to_string(param_next->GetName());
                        lua.throw_error(
                                fmt::format("Tried calling UFunction without a registered handler for parameter. Parameter '{}' of type '{}' not supported.",
                                            parameter_name,
                                            parameter_type_name));
                    }
                }
            };
        }

        // It's assumed that everything is safe here
        // Every part of the system is designed to throw when stuff goes wrong
        // This code should never be executed in those cases
        calling_context->ProcessEvent(func, dynamic_unreal_function_data.data);
        return 0;
    }
