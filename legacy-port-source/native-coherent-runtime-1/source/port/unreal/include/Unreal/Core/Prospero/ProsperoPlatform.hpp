#pragma once

#if !defined(__PROSPERO__) || !defined(__SCE__) || !defined(__x86_64__)
#error This platform profile requires the actual x86_64 Prospero SDK target.
#endif
#include <Unreal/Core/Clang/ClangPlatform.hpp>
#include <Unreal/Core/GenericPlatform/GenericPlatform.hpp>

struct FProsperoPlatformTypes : FGenericPlatformTypes {};
using FPlatformTypes = FProsperoPlatformTypes;

static_assert(sizeof(wchar_t) == 2 && sizeof(void*) == 8 && sizeof(long) == 8);
static_assert(sizeof(FPlatformTypes::TCHAR) == 2 && sizeof(FPlatformTypes::WIDECHAR) == 2);
static_assert(sizeof(FPlatformTypes::int32) == 4 && sizeof(FPlatformTypes::int64) == 8);

#define PLATFORM_PROSPERO 1
#define PLATFORM_CONSOLE 1
#define PLATFORM_DESKTOP 0
#define PLATFORM_64BITS 1
#define PLATFORM_LITTLE_ENDIAN 1
#define PLATFORM_COMPILER_CLANG 1
#define PLATFORM_EXCEPTIONS_DISABLED 0
#define PLATFORM_SEH_EXCEPTIONS_DISABLED 1
#define PLATFORM_TCHAR_IS_4_BYTES 0
#define PLATFORM_WCHAR_IS_4_BYTES 0
#define PLATFORM_TCHAR_IS_CHAR16 0
#define PLATFORM_TCHAR_IS_UTF8CHAR 0
#define PLATFORM_USES_MICROSOFT_LIBC_FUNCTIONS 0
#define PLATFORM_SUPPORTS_PRAGMA_PACK 1
#define PLATFORM_HAS_128BIT_ATOMICS 0
#define PLATFORM_USE_LS_SPEC_FOR_WIDECHAR 1
#define PLATFORM_BREAK() __builtin_trap()
#define PLATFORM_BREAK_IS_EXPRESSION 1
#define LINE_TERMINATOR TEXT("\n")
#define LINE_TERMINATOR_ANSI "\n"

#ifndef FORCEINLINE
#define FORCEINLINE inline __attribute__((always_inline))
#endif
#ifndef FORCENOINLINE
#define FORCENOINLINE __attribute__((noinline))
#endif
#define GCC_PACK(n) __attribute__((packed, aligned(n)))
#define GCC_ALIGN(n) __attribute__((aligned(n)))
#ifndef MS_ALIGN
#define MS_ALIGN(n) // Alignment is supplied by the paired GCC_ALIGN suffix on Clang.
#endif
#define DLLEXPORT __attribute__((visibility("default")))
#define DLLIMPORT
