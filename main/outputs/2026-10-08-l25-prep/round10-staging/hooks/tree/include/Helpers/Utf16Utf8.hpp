#pragma once

#include <String/StringType.hpp>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace RC::Unicode {
inline constexpr std::size_t MaxUnits = 65536;
inline constexpr std::size_t MaxBytes = MaxUnits * 3;
static_assert(sizeof(RC::CharType) == 2, "WriterDevice requires the actual native UTF-16 contract or the owned FORCE_U16 host profile");

inline std::uint32_t NextUtf16(RC::StringViewType Input, std::size_t& Index) {
    std::uint32_t Code = static_cast<std::uint16_t>(Input[Index++]);
    if (Code >= 0xD800 && Code <= 0xDBFF) {
        if (Index == Input.size()) throw std::invalid_argument("Output contains incomplete UTF-16");
        const std::uint32_t Low = static_cast<std::uint16_t>(Input[Index++]);
        if (Low < 0xDC00 || Low > 0xDFFF) throw std::invalid_argument("Output contains invalid UTF-16");
        Code = 0x10000 + ((Code - 0xD800) << 10) + (Low - 0xDC00);
    } else if (Code >= 0xDC00 && Code <= 0xDFFF) {
        throw std::invalid_argument("Output contains unmatched UTF-16 surrogate");
    }
    return Code;
}
inline std::size_t Utf8Size(RC::StringViewType Input) {
    if (Input.size() > MaxUnits) throw std::length_error("Output exceeds the UTF-16 unit limit");
    std::size_t Bytes = 0;
    for (std::size_t Index = 0; Index < Input.size();) {
        const auto Code = NextUtf16(Input, Index);
        Bytes += Code <= 0x7F ? 1 : Code <= 0x7FF ? 2 : Code <= 0xFFFF ? 3 : 4;
    }
    if (Bytes > MaxBytes) throw std::length_error("Output exceeds the UTF-8 byte limit");
    return Bytes;
}
inline std::string EncodeUtf8(RC::StringViewType Input) {
    std::string Result;
    Result.reserve(Utf8Size(Input));
    for (std::size_t Index = 0; Index < Input.size();) {
        const auto Code = NextUtf16(Input, Index);
        if (Code <= 0x7F) Result.push_back(static_cast<char>(Code));
        else if (Code <= 0x7FF) {
            Result.push_back(static_cast<char>(0xC0 | (Code >> 6)));
            Result.push_back(static_cast<char>(0x80 | (Code & 0x3F)));
        } else if (Code <= 0xFFFF) {
            Result.push_back(static_cast<char>(0xE0 | (Code >> 12)));
            Result.push_back(static_cast<char>(0x80 | ((Code >> 6) & 0x3F)));
            Result.push_back(static_cast<char>(0x80 | (Code & 0x3F)));
        } else {
            Result.push_back(static_cast<char>(0xF0 | (Code >> 18)));
            Result.push_back(static_cast<char>(0x80 | ((Code >> 12) & 0x3F)));
            Result.push_back(static_cast<char>(0x80 | ((Code >> 6) & 0x3F)));
            Result.push_back(static_cast<char>(0x80 | (Code & 0x3F)));
        }
    }
    return Result;
}
inline RC::StringType DecodeUtf8(std::string_view Input) {
    if (Input.size() > MaxBytes) throw std::length_error("Text exceeds the UTF-8 byte limit");
    // [EV seam fix 2026-10-09] NO push_back: on short-wchar targets every
    // basic_string<wchar_t> member that resolves to the prebuilt library's
    // extern-template instantiation was compiled for a 4-byte wchar_t and
    // writes 4-byte strides, interleaving a NUL after every character
    // ('P',0,'l',0,... — host-reproduced under -fshort-wchar; the PS5 field
    // shape '[FFO] query-fallback ts:P' and '/Script/Engine' Find misses).
    // resize + operator[] + assign(ptr,count) are inline everywhere and
    // stride-correct (host-audited at SSO and heap sizes). One UTF-8 byte
    // yields at most one UTF-16 unit, so Input.size() is a safe capacity
    // upper bound; the exact size is set by the final shrink resize.
    RC::StringType Result;
    Result.resize(Input.size());
    std::size_t Used = 0;
    for (std::size_t Index = 0; Index < Input.size();) {
        const auto Lead = static_cast<unsigned char>(Input[Index++]);
        std::uint32_t Code = Lead;
        unsigned Tail = 0;
        std::uint32_t Minimum = 0;
        if (Lead <= 0x7F) {}
        else if (Lead >= 0xC2 && Lead <= 0xDF) { Code &= 0x1F; Tail = 1; Minimum = 0x80; }
        else if (Lead >= 0xE0 && Lead <= 0xEF) { Code &= 0x0F; Tail = 2; Minimum = 0x800; }
        else if (Lead >= 0xF0 && Lead <= 0xF4) { Code &= 7; Tail = 3; Minimum = 0x10000; }
        else throw std::invalid_argument("Text contains invalid UTF-8");
        if (Tail > Input.size() - Index) throw std::invalid_argument("Text contains incomplete UTF-8");
        for (unsigned Count = 0; Count < Tail; ++Count) {
            const auto Byte = static_cast<unsigned char>(Input[Index++]);
            if ((Byte & 0xC0) != 0x80) throw std::invalid_argument("Text contains invalid UTF-8 continuation");
            Code = (Code << 6) | (Byte & 0x3F);
        }
        if (Code < Minimum || Code > 0x10FFFF || (Code >= 0xD800 && Code <= 0xDFFF))
            throw std::invalid_argument("Text contains a non-scalar UTF-8 value");
        const std::size_t Units = Code <= 0xFFFF ? 1 : 2;
        if (Units > MaxUnits - Used) throw std::length_error("Text exceeds the UTF-16 unit limit");
        if (Units == 1) Result[Used++] = static_cast<RC::CharType>(Code);
        else {
            Code -= 0x10000;
            Result[Used++] = static_cast<RC::CharType>(0xD800 + (Code >> 10));
            Result[Used++] = static_cast<RC::CharType>(0xDC00 + (Code & 0x3FF));
        }
    }
    Result.resize(Used);
    return Result;
}
}
