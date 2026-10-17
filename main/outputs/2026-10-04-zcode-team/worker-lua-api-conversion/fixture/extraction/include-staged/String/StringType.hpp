#pragma once
// worker-lua-api-conversion fixture SHIM (disclosed GLUE, require-path pattern).
// Replicates deps/first/String/include/String/StringType.hpp contract with the
// 2-byte CharType host profile (the "owned FORCE_U16 host profile" the real
// header's static_assert names). On PROSPERO, CharType is wchar_t (2 bytes);
// macOS host wchar_t is 4 bytes, so the fixture pins char16_t — same 2-byte
// UTF-16 code-unit width and values as the target. Byte-exact sources under
// test are Helpers/String.hpp + Helpers/Utf16Utf8.hpp (symlinks, unmodified).
#include <string>
#include <string_view>

namespace RC
{
    using CharType = char16_t;
#define STR(str) u##str

    using StringType = std::basic_string<CharType>;
    using StringViewType = std::basic_string_view<CharType>;
    using StreamIType = std::basic_ifstream<CharType>;
    using StreamOType = std::basic_ofstream<CharType>;

    template <class T>
    auto static ToCharTypePtr(T* ptr)
    {
        static_assert(sizeof(T) == sizeof(CharType), "Sizes of T and CharType must be the same");
        using CharPtrType = std::conditional_t<std::is_const_v<T>, const CharType, CharType>*;
        return reinterpret_cast<CharPtrType>(ptr);
    }

    template <class T, class CharT>
    auto static FromCharTypePtr(CharT* ptr)
    {
        static_assert(sizeof(T) == sizeof(CharType), "Sizes of T and CharType must be the same");
        static_assert(std::is_same_v<std::decay_t<CharT>, CharType>, "Must be a CharType");
        using TargetPtrType = std::conditional_t<std::is_const_v<CharT>, const T, T>*;
        return reinterpret_cast<TargetPtrType>(ptr);
    }
} // namespace RC
