#pragma once

// =====================================================================================
// Toolchain self-test declaration (agent-FF, l26 build-discipline; Claude
// directive: "settle the toolchain question" with two in-binary lines).
//
// The question: this runtime's TUs see a 2-byte wchar_t (Prospero native; the
// tree's UTF-16 contract static_asserts sizeof(RC::CharType) == 2), while the
// PREBUILT libc++ binary was compiled assuming a 4-byte wchar_t. Belief (host
// evidence, Helpers/Utf16Utf8.hpp seam fix): wcslen and the out-of-line
// std::wstring members stride 4 bytes over 2-byte data. RC_toolchain_self_test
// proves or refutes that ON CONSOLE, inside the shipped binary, via:
//
//     [ST] wcslen-16units expect=16 got=<n> stride=<sizeof(wchar_t)>
//     [ST] wstring-rt size=<n> content-match=<yes/no> raw-first8=<hex bytes>
//
// extern "C" so UnrealInitializer.cpp can also declare it TU-locally without
// header coupling if ever needed (it includes this header today — it's cheap).
// Crash-safe: never throws, nothing escapes. Called from UnrealInitializer::
// Initialize immediately after the [BUILD] stamp line.
//
// The wide-libc calls inside the definition are the EXPERIMENT itself;
// everywhere else the tree avoids wide libc.
// =====================================================================================

extern "C" auto RC_toolchain_self_test() -> void;
