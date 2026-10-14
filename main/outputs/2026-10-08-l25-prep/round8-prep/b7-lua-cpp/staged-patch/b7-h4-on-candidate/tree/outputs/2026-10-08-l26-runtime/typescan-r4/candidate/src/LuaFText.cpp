// [LFB-M] agent-FD candidate (2026-10-10, Claude Q1 items 3-4).
// Original identity: the lua-luaftext unit source (l22-runtime diagnostics
// copy of ue4ss/src/LuaType/LuaFText.cpp, L22Diagnostics logging preserved
// verbatim). Prospero-only changes, all guarded by __PROSPERO__ so host
// behavior stays upstream:
//   - FText("string") constructor: ensure_str + Unreal::FText(StringType) ->
//     LuaFNameBridge::Bridge::to_ftext (hand UTF-8 -> UTF-16 ->
//     FString::AppendChars -> engine Conv_StringToText; never the
//     FText(const CharType*) FString(TCHAR*) Strlen seam).
//   - FText:ToString member: to_string(*ToFString()) ->
//     LuaFNameBridge::push_ftext_to_lua (ToFString/Conv_TextToString ->
//     hand UTF-16 -> UTF-8 -> lua_pushlstring; never FText::ToString's
//     StringType ctor seam).
//   - FText.__eq: FText::ToString() (StringType ctor seam) -> ToFString on
//     both sides + hand unit-loop compare.
//   - FText.__tostring (agent-FL, Claude reverse-routing round): Lua's
//     str()/tostring() on FText userdata now routes through the same bridge
//     reverse path via a raw-Lua-API metamethod on the "FText" metatable
//     (the LuaMadeSimple MetaMethod enum has no Tostring entry).
// Markers: [LFB-M] ftext-fwd / ftext-rev (first 10 uses each, from the
// bridge). Refusals: first-10 per family with call-site tag + hex units,
// then the empty value.
#include <L22Diagnostics/Logging.hpp>
#include <DynamicOutput/Output.hpp>
#include <LuaFNameBridge.hpp> // [LFB] ONE hand converter (agent-EY; FString/FText families agent-FD)
#include <LuaType/LuaFText.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/FText.hpp>

namespace RC::LuaType
{
    FText::FText(Unreal::FText object) : LocalObjectBase<Unreal::FText, FTextName>(std::move(object))
    {
    }

    auto FText::construct(const LuaMadeSimple::Lua& lua, Unreal::FText unreal_object) -> const LuaMadeSimple::Lua::Table
    {
        LuaType::FText lua_object{unreal_object};

        auto metatable_name = ClassName::ToString();

        LuaMadeSimple::Lua::Table table = lua.get_metatable(metatable_name);
        if (lua.is_nil(-1))
        {
            lua.discard_value(-1);
            SelfType::construct(lua, lua_object);
            setup_metamethods(lua_object);
            setup_member_functions<LuaMadeSimple::Type::IsFinal::Yes>(table);
            lua.new_metatable<LuaType::FText>(metatable_name, lua_object.get_metamethods());
#ifdef __PROSPERO__
            // [LFB-M] agent-FL (2026-10-10, Claude reverse-routing directive):
            // __tostring for the FText userdata. The LuaMadeSimple MetaMethod
            // enum has no Tostring entry, so the metamethod is attached to
            // the registry-named metatable through the raw Lua C API —
            // luaL_getmetatable("FText") + lua_setfield. str()/tostring() on
            // FText userdata now routes through the bridge reverse path
            // (ToFString/Conv_TextToString -> hand UTF-16 -> UTF-8 ->
            // lua_pushlstring; 'ftext-rev' family, first 10 uses), identical
            // payload to the :ToString member. Upstream (host) keeps Lua's
            // default userdata stringification.
            if (lua_State* lps = lua.get_lua_state())
            {
                luaL_getmetatable(lps, metatable_name);
                if (!lua_isnil(lps, -1))
                {
                    lua_pushcfunction(lps, [](lua_State* l) -> int {
                        auto* self = static_cast<LuaType::FText*>(lua_touserdata(l, 1));
                        if (!self)
                        {
                            lua_pushlstring(l, "", 0);
                            return 1;
                        }
                        LuaFNameBridge::push_ftext_to_lua(l, self->get_local_cpp_object());
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

    auto FText::construct(const LuaMadeSimple::Lua& lua, BaseObject& construct_to) -> const LuaMadeSimple::Lua::Table
    {
        LuaMadeSimple::Lua::Table table = SelfType::construct(lua, construct_to);

        setup_member_functions<LuaMadeSimple::Type::IsFinal::No>(table);

        setup_metamethods(construct_to);

        return table;
    }

    auto FText::setup_metamethods(BaseObject& base_object) -> void
    {
        base_object.get_metamethods().create(LuaMadeSimple::Lua::MetaMethod::Call, []([[maybe_unused]] const LuaMadeSimple::Lua& lua) -> int {
            // Acts as a "constructor" for FText
            std::string error_overload_not_found{R"(
No overload found for function 'FText'.
Overloads:
#1: FText(string Text)
)"};

            if (lua.is_userdata())
            {
                // Discard the userdata since it's unwanted data
                lua.discard_value();
            }

#ifdef __PROSPERO__
            // [LFB-M] agent-FD (Claude Q1 item 3): the FText() constructor
            // exposed to Lua (the mod menu's SetText(FText("test")) shape).
            // The ONE hand converter owns the Lua string: lua_tolstring ->
            // hand UTF-8 -> UTF-16 -> FString::AppendChars -> the engine's
            // Conv_StringToText (Bridge::to_ftext). Was
            // ensure_str(lua.get_string()) + Unreal::FText(StringType) ->
            // FText(const CharType*) -> FString(TCHAR*) Strlen — the wide
            // conversion stack is entirely off this path. Refused input
            // constructs an empty FText after the loud family notice.
            if (!lua.is_string())
            {
                L22Diagnostics::throw_lua_error(lua, error_overload_not_found, "FText");
            }
            const LuaFNameBridge::Bridge text_bridge{lua.get_lua_state()};
            try
            {
                LuaType::FText::construct(lua, text_bridge.to_ftext());
            }
            catch (const std::exception& error)
            {
                L22Diagnostics::error("FText Lua construction", error.what());
                throw;
            }
            catch (void*)
            {
                // [B7] With Lua built as C++, a Lua error is a C++ throw of a pointer. It must reach
                // lua_pcall's own handler (rethrow); swallowing it here leaves Lua's lock held.
                throw;
            }
            catch (...)
            {
                L22Diagnostics::error("FText Lua construction", "non-standard C++ exception");
                throw;
            }

            return 1;
#else
            StringType text_string{};
            if (lua.is_string())
            {
                text_string = ensure_str(lua.get_string());
            }
            else
            {
                L22Diagnostics::throw_lua_error(lua, error_overload_not_found, "FText");
            }

            try
            {
                LuaType::FText::construct(lua, Unreal::FText(text_string));
            }
            catch (const std::exception& error)
            {
                L22Diagnostics::error("FText Lua construction", error.what());
                throw;
            }
            catch (void*)
            {
                // [B7] With Lua built as C++, a Lua error is a C++ throw of a pointer. It must reach
                // lua_pcall's own handler (rethrow); swallowing it here leaves Lua's lock held.
                throw;
            }
            catch (...)
            {
                L22Diagnostics::error("FText Lua construction", "non-standard C++ exception");
                throw;
            }

            return 1;
#endif
        });

        base_object.get_metamethods().create(LuaMadeSimple::Lua::MetaMethod::Equal, []([[maybe_unused]] const LuaMadeSimple::Lua& lua) -> int {
            if (!lua.is_userdata(1) || !lua.is_userdata(2))
            {
                L22Diagnostics::throw_lua_error(lua, "FText __eq metamethod called but there was not two userdata to compare", "FText");
            }

            auto text_a = lua.get_userdata<LuaType::FText>();
            auto text_b = lua.get_userdata<LuaType::FText>();
#ifdef __PROSPERO__
            // [LFB-M] agent-FD: equality without FText::ToString() — its
            // StringType(const CharType*) return resolves to the prebuilt
            // 4-byte-wchar libc++ ctor on the short-wchar target. Both
            // sides go through ToFString (engine Conv_TextToString) and a
            // hand unit loop.
            const Unreal::FString string_a = text_a.get_local_cpp_object().ToFString();
            const Unreal::FString string_b = text_b.get_local_cpp_object().ToFString();
            const int32_t len_a = string_a.Len();
            const int32_t len_b = string_b.Len();
            if (len_a != len_b)
            {
                return false;
            }
            const Unreal::TCHAR* units_a = string_a.GetCharArray().GetData();
            const Unreal::TCHAR* units_b = string_b.GetCharArray().GetData();
            if (!units_a || !units_b)
            {
                return len_a == 0 && len_b == 0;
            }
            for (int32_t i = 0; i < len_a; ++i)
            {
                if (units_a[i] != units_b[i])
                {
                    return false;
                }
            }
            return true;
#else
            // FText objects are equal if their string representations are equal
            return text_a.get_local_cpp_object().ToString() == text_b.get_local_cpp_object().ToString();
#endif
        });
    }

    template <LuaMadeSimple::Type::IsFinal is_final>
    auto FText::setup_member_functions(const LuaMadeSimple::Lua::Table& table) -> void
    {
        table.add_pair("ToString", [](const LuaMadeSimple::Lua& lua) -> int {
            auto& lua_object = lua.get_userdata<FText>();

#ifdef __PROSPERO__
            // [LFB-M] agent-FD (Claude Q1 item 4): engine FText -> Lua
            // string return read. Bridge reverse path: ToFString (engine
            // Conv_TextToString) -> hand UTF-16 -> UTF-8 ->
            // lua_pushlstring. Was to_string(*fstring) (Helpers codec);
            // FText::ToString is never called (StringType ctor seam).
            LuaFNameBridge::push_ftext_to_lua(lua.get_lua_state(), lua_object.get_local_cpp_object());
#else
            const auto& fstring = lua_object.get_local_cpp_object().ToFString();
            lua.set_string(to_string(*fstring));
#endif

            return 1;
        });

        if constexpr (is_final == LuaMadeSimple::Type::IsFinal::Yes)
        {
            table.add_pair("type", [](const LuaMadeSimple::Lua& lua) -> int {
                lua.set_string(ClassName::ToString());
                return 1;
            });

            // If this is the final object then we also want to finalize creating the table
            // If not then it's the responsibility of the overriding object to call 'make_global()'
            // table.make_global(ClassName::ToString());
        }
    }
} // namespace RC::LuaType
