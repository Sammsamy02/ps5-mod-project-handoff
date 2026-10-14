#pragma once
// HOST-FIXTURE SHIM — Helpers/String.hpp (worker-menu-chain).
// Copied from the accepted worker-r3-ladder host-test shim
// (worker-l11-enqueue-ladder origin). ONE additive alias for this fixture:
// CharType, required by the REAL Input/KeyDef.cpp (string_to_key's towlower
// lambda). Matches the REAL String/StringType.hpp (deps/first) CharType on
// non-FORCE_U16 builds; redeclaring an alias to the same type is legal.
// The real upstream header drags the Unreal port in; the host harness only
// needs the StringType alias, STR, ensure_str and to_string that the REAL
// port ExceptionHandling.hpp (TRY) and the REAL LuaMadeSimple.cpp use.

#include <cstring>
#include <string>
#include <string_view>

namespace RC
{
    using CharType = wchar_t;

    using StringType = std::wstring;
    using StringViewType = std::wstring_view;

    inline auto ensure_str(const std::wstring& input) -> std::wstring
    {
        return input;
    }

    inline auto ensure_str(const wchar_t* input) -> std::wstring
    {
        return input != nullptr ? std::wstring{input} : std::wstring{};
    }

    inline auto ensure_str(const std::string& input) -> std::wstring
    {
        return std::wstring{input.begin(), input.end()};
    }

    inline auto ensure_str(const char* input) -> std::wstring
    {
        return input != nullptr ? std::wstring{input, input + std::strlen(input)} : std::wstring{};
    }

    inline auto to_string(const std::wstring& input) -> std::string
    {
        std::string out;
        out.reserve(input.size());
        for (wchar_t c : input)
        {
            out.push_back(static_cast<char>(c));
        }
        return out;
    }

    inline auto to_string(std::wstring_view input) -> std::string
    {
        return to_string(std::wstring{input});
    }

    inline auto to_string(const std::u16string& input) -> std::string
    {
        return std::string{input.begin(), input.end()};
    }

    inline auto to_string(std::u16string_view input) -> std::string
    {
        return to_string(std::u16string{input});
    }

    inline auto to_string(const std::string& input) -> std::string
    {
        return input;
    }
} // namespace RC

#define STR(text) L##text
