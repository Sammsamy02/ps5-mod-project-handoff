#include <limits>

#include <DynamicOutput/Output.hpp>
#include <Helpers/String.hpp>
#include <LuaFNameBridge.hpp> // [LFB] ONE hand Lua-string -> engine-name converter (agent-EY)
#include <LuaType/LuaFName.hpp>

namespace RC::LuaType
{
    FName::FName(Unreal::FName object) : LocalObjectBase<Unreal::FName, FNameName>(std::move(object))
    {
    }

    auto FName::construct(const LuaMadeSimple::Lua& lua, Unreal::FName unreal_object) -> const LuaMadeSimple::Lua::Table
    {
        LuaType::FName lua_object{unreal_object};

        auto metatable_name = "FNameUserdata";

        LuaMadeSimple::Lua::Table table = lua.get_metatable(metatable_name);
        if (lua.is_nil(-1))
        {
            lua.discard_value(-1);
            lua.prepare_new_table();
            setup_metamethods(lua_object);
            setup_member_functions<LuaMadeSimple::Type::IsFinal::Yes>(table);
            lua.new_metatable<LuaType::FName>(metatable_name, lua_object.get_metamethods());
#ifdef __PROSPERO__
            // [LFB-M] agent-FL (2026-10-10, Claude reverse-routing directive):
            // __tostring for the FName userdata. The LuaMadeSimple MetaMethod
            // enum has no Tostring entry, so the metamethod is attached to
            // the registry-named metatable through the raw Lua C API —
            // luaL_getmetatable("FNameUserdata") + lua_setfield. This is THE
            // route the mod's str()/tostring() takes on FName userdata
            // (explicitly constructed FName("name") values, FName returns
            // like UObject:GetName()): pool decoder (FName::ToString) ->
            // bridge hand UTF-16 -> UTF-8 -> lua_pushlstring, logging the
            // 'fname-rev' family (first 10 uses). Identical payload to the
            // :ToString member above. Upstream (host) keeps Lua's default
            // userdata stringification.
            if (lua_State* lps = lua.get_lua_state())
            {
                luaL_getmetatable(lps, metatable_name);
                if (!lua_isnil(lps, -1))
                {
                    lua_pushcfunction(lps, [](lua_State* l) -> int {
                        auto* self = static_cast<LuaType::FName*>(lua_touserdata(l, 1));
                        if (!self)
                        {
                            lua_pushlstring(l, "", 0);
                            return 1;
                        }
                        LuaFNameBridge::push_fname_to_lua(l, self->get_local_cpp_object());
                        return 1;
                    });
                    lua_setfield(lps, -2, "__tostring");
                }
                lua_pop(lps, 1);
            }
#endif
        }

        // Create object & surrender ownership to Lua
        lua.transfer_stack_object(std::move(lua_object), metatable_name, lua_object.get_metamethods());

        return table;
    }

    auto FName::construct(const LuaMadeSimple::Lua& lua, BaseObject& construct_to) -> const LuaMadeSimple::Lua::Table
    {
        LuaMadeSimple::Lua::Table table = lua.prepare_new_table();

        setup_member_functions<LuaMadeSimple::Type::IsFinal::No>(table);

        setup_metamethods(construct_to);

        return table;
    }

    auto FName::setup_metamethods(BaseObject& base_object) -> void
    {
        base_object.get_metamethods().create(LuaMadeSimple::Lua::MetaMethod::Call, []([[maybe_unused]] const LuaMadeSimple::Lua& lua) -> int {
            // Acts as a "constructor" for FName
            std::string error_overload_not_found{R"(
No overload found for function 'FName'.
Overloads:
#1: FName(string Name, EFindName = FNAME_Add)
#2: FName(integer ComparisonIndex, EFindName = FNAME_Add))"};

            if (lua.is_userdata())
            {
                // Discard the userdata since it's unwanted data
                lua.discard_value();
            }

            // [LFB] Claude items 1-2 (the FName() constructor exposed to
            // Lua): the ONE hand converter owns the Lua name string —
            // lua_tolstring + hand UTF-8->UTF-16 into a bounded NUL-
            // terminated buffer, then the slot-2 native ctor DIRECTLY
            // (FName::ConstructorInternal, image 0x23D590) with the
            // caller's EFindName (upstream default FNAME_Add kept). Was
            // ensure_str(lua.get_string()) + the StringType ctor seam; no
            // std::wstring, no Unicode::DecodeUtf8, no wide libc. The
            // is_string() test happens BEFORE the bridge construction so
            // the stack shape matches the original get_string() flow; the
            // bridge is a no-op (no removal) for the integer overload.
            const bool arg_is_string = lua.is_string();
            const LuaFNameBridge::Bridge name{lua.get_lua_state()};

            int64_t name_comparison_index{};
            bool have_name_string{};
            if (arg_is_string)
            {
                // Empty-string inputs keep the upstream semantics: the
                // integer branch with index 0 (NAME_None).
                have_name_string = name.ok() && name.unit_count() != 0;
            }
            else if (lua.is_integer())
            {
                name_comparison_index = lua.get_integer();
            }
            else
            {
                lua.throw_error(error_overload_not_found);
            }

            Unreal::EFindName find_type{Unreal::EFindName::FNAME_Add};
            if (lua.is_integer())
            {
                find_type = static_cast<Unreal::EFindName>(lua.get_integer());
            }

            if (!have_name_string)
            {
                if (name_comparison_index < 0)
                {
                    lua.throw_error("FName constructor cannot take an integer smaller than uint32.");
                }
                if (name_comparison_index > std::numeric_limits<uint32_t>::max())
                {
                    lua.throw_error("FName constructor cannot take an integer larger than uint32.");
                }
                LuaType::FName::construct(lua, Unreal::FName(static_cast<uint32_t>(name_comparison_index), find_type));
            }
            else
            {
                LuaType::FName::construct(lua, name.to_fname(find_type));
            }

            return 1;
        });

        base_object.get_metamethods().create(LuaMadeSimple::Lua::MetaMethod::Equal, []([[maybe_unused]] const LuaMadeSimple::Lua& lua) -> int {
            if (!lua.is_userdata(1) || !lua.is_userdata(2))
            {
                lua.throw_error("FName __eq metamethod called but there was not two userdata to compare");
            }

            auto name_a = lua.get_userdata<LuaType::FName>();
            auto name_b = lua.get_userdata<LuaType::FName>();

            return name_a.get_local_cpp_object() == name_b.get_local_cpp_object();
        });
    }

    template <LuaMadeSimple::Type::IsFinal is_final>
    auto FName::setup_member_functions(LuaMadeSimple::Lua::Table& table) -> void
    {
        table.add_pair("ToString", [](const LuaMadeSimple::Lua& lua) -> int {
            auto& lua_object = lua.get_userdata<FName>();

            // [LFB] Claude item 4 (reverse direction; the mod reads ItemId
            // via str() -> :ToString()): pool decoder (FName::ToString,
            // agent ER) -> hand UTF-16 -> UTF-8 -> lua_pushlstring. Was
            // to_string(...) — no wide-libc conversion on this path.
            // [LFB-M] agent-FL: push_fname_to_lua now logs the 'fname-rev'
            // family (first-10 markers + first-10 refusals with call-site
            // tag + hex units), same as the __tostring metamethod above.
            LuaFNameBridge::push_fname_to_lua(lua.get_lua_state(), lua_object.get_local_cpp_object());

            return 1;
        });

        table.add_pair("GetComparisonIndex", [](const LuaMadeSimple::Lua& lua) -> int {
            auto& lua_object = lua.get_userdata<FName>();

            lua.set_integer(lua_object.get_local_cpp_object().GetComparisonIndex());

            return 1;
        });

        table.add_pair("Equals", [](const LuaMadeSimple::Lua& lua) -> int {
            if (!lua.is_userdata(1) || !lua.is_userdata(2))
            {
                lua.throw_error("FName.Equals called but there was not two userdata to compare (use ':' to call, not '.')");
            }

            auto name_a = lua.get_userdata<LuaType::FName>();
            auto name_b = lua.get_userdata<LuaType::FName>();

            return name_a.get_local_cpp_object().Equals(name_b.get_local_cpp_object());
        });

        if constexpr (is_final == LuaMadeSimple::Type::IsFinal::Yes)
        {
            table.add_pair("type", [](const LuaMadeSimple::Lua& lua) -> int {
                lua.set_string(ClassName::ToString());
                return 1;
            });

            // If this is the final object then we also want to finalize creating the table
            // If not then it's the responsibility of the overriding object to call 'make_global()'
            // table.make_global("FNameUserdata");
        }
    }
} // namespace RC::LuaType
