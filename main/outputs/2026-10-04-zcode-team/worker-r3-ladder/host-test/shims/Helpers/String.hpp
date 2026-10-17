#pragma once
// HOST-TEST SHIM — Helpers/String.hpp (worker-l11-enqueue-ladder).
// The real upstream header drags the Unreal port in; the host harness only
// needs the StringType alias, STR, ensure_str and to_string that the REAL
// port ExceptionHandling.hpp (TRY) and the REAL LuaMadeSimple.cpp use.

#include <cstring>
#include <string>
#include <string_view>

namespace RC
{
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
