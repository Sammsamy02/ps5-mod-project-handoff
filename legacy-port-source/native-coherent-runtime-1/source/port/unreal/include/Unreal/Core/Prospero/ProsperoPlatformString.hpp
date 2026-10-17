#pragma once

#include <Unreal/Core/GenericPlatform/GenericPlatformString.hpp>
#include <Unreal/Core/Misc/Char.hpp>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <type_traits>

namespace RC::Unreal {
struct FProsperoPlatformString : FGenericPlatformString {
    using FGenericPlatformString::Stricmp;
    using FGenericPlatformString::Strnicmp;
    using FGenericPlatformString::Strncmp;

private:
    template <typename Char> static uint32 CodeUnit(Char Value) {
        return static_cast<uint32>(static_cast<std::make_unsigned_t<Char>>(Value));
    }
    template <typename Char> static constexpr void CheckNumericType() {
        static_assert(std::is_same_v<Char, ANSICHAR> || std::is_same_v<Char, UTF8CHAR> || std::is_same_v<Char, WIDECHAR>);
    }
    template <typename Char> static int64 Signed64(const Char* Start, Char** End, int32 Base) {
        CheckNumericType<Char>();
        if constexpr (std::is_same_v<Char, WIDECHAR>) {
            return static_cast<int64>(::wcstoll(Start, End, Base));
        } else {
            char* End8 = nullptr;
            const auto Value = ::strtoll(reinterpret_cast<const char*>(Start), End ? &End8 : nullptr, Base);
            if (End) *End = reinterpret_cast<Char*>(End8);
            return static_cast<int64>(Value);
        }
    }
    template <typename Char> static uint64 Unsigned64(const Char* Start, Char** End, int32 Base) {
        CheckNumericType<Char>();
        if constexpr (std::is_same_v<Char, WIDECHAR>) {
            return static_cast<uint64>(::wcstoull(Start, End, Base));
        } else {
            char* End8 = nullptr;
            const auto Value = ::strtoull(reinterpret_cast<const char*>(Start), End ? &End8 : nullptr, Base);
            if (End) *End = reinterpret_cast<Char*>(End8);
            return static_cast<uint64>(Value);
        }
    }

public:
    // Counts are code units; bounded copies reserve and always write a terminator when capacity is nonzero.
    template <typename Char> static Char* Strncpy(Char* Dest, const Char* Src, SIZE_T MaxLen) {
        if (!MaxLen) return Dest;
        SIZE_T Index = 0;
        while (Index + 1 < MaxLen && Src[Index]) {
            Dest[Index] = Src[Index];
            ++Index;
        }
        Dest[Index] = Char(0);
        return Dest;
    }
    template <typename Char> static Char* Strcpy(Char* Dest, SIZE_T DestCount, const Char* Src) {
        return Strncpy(Dest, Src, DestCount);
    }
    template <typename Char> static Char* Strcat(Char* Dest, SIZE_T DestCount, const Char* Src) {
        if (!DestCount) return Dest;
        SIZE_T Length = 0;
        while (Length < DestCount && Dest[Length]) ++Length;
        if (Length == DestCount) {
            Dest[DestCount - 1] = Char(0);
            return Dest;
        }
        Strncpy(Dest + Length, Src, DestCount - Length);
        return Dest;
    }
    template <typename Char> static Char* Strupr(Char* Dest, SIZE_T DestCount) {
        for (SIZE_T Index = 0; Index < DestCount && Dest[Index]; ++Index)
            Dest[Index] = TChar<Char>::ToUpper(Dest[Index]);
        return Dest;
    }
    template <typename Char> static int32 Strcmp(const Char* A, const Char* B) {
        while (*A && *A == *B) { ++A; ++B; }
        const uint32 Left = CodeUnit(*A), Right = CodeUnit(*B);
        return Left < Right ? -1 : (Left > Right ? 1 : 0);
    }
    template <typename Char> static int32 Strlen(const Char* String) {
        SIZE_T Length = 0;
        while (String[Length]) ++Length;
        return static_cast<int32>(Length);
    }
    template <typename Char> static int32 Strnlen(const Char* String, SIZE_T MaxLen) {
        SIZE_T Length = 0;
        while (Length < MaxLen && String[Length]) ++Length;
        return static_cast<int32>(Length);
    }
    template <typename Char> static const Char* Strstr(const Char* String, const Char* Find) {
        if (!*Find) return String;
        for (; *String; ++String) {
            const Char* Candidate = String;
            const Char* Needle = Find;
            while (*Needle && *Candidate == *Needle) { ++Candidate; ++Needle; }
            if (!*Needle) return String;
        }
        return nullptr;
    }
    template <typename Char> static const Char* Strchr(const Char* String, Char Value) {
        do { if (*String == Value) return String; } while (*String++);
        return nullptr;
    }
    template <typename Char> static const Char* Strrchr(const Char* String, Char Value) {
        const Char* Found = nullptr;
        do { if (*String == Value) Found = String; } while (*String++);
        return Found;
    }
    template <typename Char> static int32 Strtoi(const Char* Start, Char** End, int32 Base) {
        // Native long is 64-bit. Preserve the explicitly 32-bit return range.
        const int64 Value = Signed64(Start, End, Base);
        if (Value > INT_MAX) { errno = ERANGE; return INT_MAX; }
        if (Value < INT_MIN) { errno = ERANGE; return INT_MIN; }
        return static_cast<int32>(Value);
    }
    template <typename Char> static int64 Strtoi64(const Char* Start, Char** End, int32 Base) {
        return Signed64(Start, End, Base);
    }
    template <typename Char> static uint64 Strtoui64(const Char* Start, Char** End, int32 Base) {
        return Unsigned64(Start, End, Base);
    }
    template <typename Char> static int32 Atoi(const Char* String) { return Strtoi(String, static_cast<Char**>(nullptr), 10); }
    template <typename Char> static int64 Atoi64(const Char* String) { return Signed64(String, static_cast<Char**>(nullptr), 10); }
    template <typename Char> static double Atod(const Char* String) {
        CheckNumericType<Char>();
        if constexpr (std::is_same_v<Char, WIDECHAR>) return ::wcstod(String, nullptr);
        else return ::strtod(reinterpret_cast<const char*>(String), nullptr);
    }
    template <typename Char> static float Atof(const Char* String) { return static_cast<float>(Atod(String)); }
    template <typename Char> static Char* Strtok(Char* String, const Char* Delim, Char** Context) {
        Char* Current = String ? String : *Context;
        if (!Current) return nullptr;
        while (*Current && Strchr(Delim, *Current)) ++Current;
        if (!*Current) { *Context = Current; return nullptr; }
        Char* Token = Current;
        while (*Current && !Strchr(Delim, *Current)) ++Current;
        if (*Current) *Current++ = Char(0);
        *Context = Current;
        return Token;
    }
    static int32 GetVarArgs(ANSICHAR* Dest, SIZE_T DestSize, const ANSICHAR*& Fmt, va_list Args) {
        const int Result = ::vsnprintf(Dest, DestSize, Fmt, Args);
        return Result >= 0 && static_cast<SIZE_T>(Result) < DestSize ? static_cast<int32>(Result) : -1;
    }
    static int32 GetVarArgs(UTF8CHAR* Dest, SIZE_T DestSize, const UTF8CHAR*& Fmt, va_list Args) {
        const char* Format = reinterpret_cast<const char*>(Fmt);
        return GetVarArgs(reinterpret_cast<char*>(Dest), DestSize, Format, Args);
    }
    static int32 GetVarArgs(WIDECHAR* Dest, SIZE_T DestSize, const WIDECHAR*& Fmt, va_list Args) {
        const int Result = ::vswprintf(Dest, DestSize, Fmt, Args);
        return Result >= 0 && static_cast<SIZE_T>(Result) < DestSize ? static_cast<int32>(Result) : -1;
    }
};
using FPlatformString = FProsperoPlatformString;
}
