// String/StringType.hpp -- PS5 Mod Bridge native mod SDK.
// Same spelling as UE4SS on Windows: StringType is a wide string, STR("x") is
// a wide literal. wchar_t is 32-bit (UTF-32) on the PS5 toolchain; the SDK
// converts to UTF-8 at the ABI boundary (mbm/Sdk.hpp).
#pragma once
#include <string>
#include <string_view>

#ifndef STR
#define STR(str) L##str
#endif

namespace RC
{
    using CharType = wchar_t;
    using StringType = std::wstring;
    using StringViewType = std::wstring_view;
} // namespace RC
