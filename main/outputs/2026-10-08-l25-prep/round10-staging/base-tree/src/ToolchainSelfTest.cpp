// =====================================================================================
// Toolchain self-test definition (agent-FF, l26 build-discipline). See
// include/ToolchainSelfTest.hpp for the contract and the two console markers.
//
// Probes (both Prospero-only; a non-Prospero compile of this TU logs one skip
// line and returns, so hypothetical host/Windows builds of the tree stay
// buildable):
//   1. wcslen over a hand-filled 16-unit 2-byte buffer ("PlayerController"):
//      expect 16. A 4-byte-stride wcslen pairs units and returns 8; the
//      trailing run of L'\0' sentinels (48 units) keeps EITHER stride
//      terminating inside the 64-unit buffer, so the probe cannot overrun.
//   2. std::wstring assign/push_back/append round trip, then a hand-walk of
//      w.data()/w.size() as char16 against a hand-built expected image:
//      expect size=19, content-match=yes ("PlayerControllerXYZ"). The first 8
//      raw payload bytes are printed as hex so a 4-byte-stride corruption is
//      NAMED by the log, not just flagged as a mismatch.
//
// The wcslen/wstring calls here are deliberate (this IS the experiment).
// Everywhere else the tree avoids wide libc.
// =====================================================================================

#include "../include/ToolchainSelfTest.hpp"

#include <cwchar>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

#include <fmt/core.h>
#include <DynamicOutput/Output.hpp>
#include <Helpers/Utf16Utf8.hpp>

namespace
{
    // Narrow-first logging doctrine (same as send_narrow_diag / VmtPump::st_log
    // in UnrealInitializer.cpp): format narrow, widen ONCE via
    // Unicode::DecodeUtf8 (the stride-safe inline decoder), swallow everything;
    // stderr fwrite as the last-resort sink. Takes string_view so the
    // exception-fallback call sites never construct/allocate before the try.
    auto st_send(std::string_view narrow) noexcept -> void
    {
        try
        {
            RC::Output::send(RC::Unicode::DecodeUtf8(narrow));
        }
        catch (...)
        {
            try { std::fwrite(narrow.data(), 1, narrow.size(), stderr); } catch (...) {}
        }
    }
}

extern "C" auto RC_toolchain_self_test() -> void
{
#if defined(__PROSPERO__)
    static_assert(sizeof(wchar_t) == 2, "this runtime's wchar_t contract is 2 bytes (see Helpers/Utf16Utf8.hpp)");

    // ---- Probe 1: wcslen on a 16-unit 2-byte literal (expect 16) ----
    try
    {
        static const char16_t kLit16[17] = u"PlayerController"; // exactly 16 units + NUL

        wchar_t buf[64]; // larger than the literal: 16 payload units + 48 zero sentinels
        for (int i = 0; i < 16; ++i) { buf[i] = static_cast<wchar_t>(kLit16[i]); }
        for (int i = 16; i < 64; ++i) { buf[i] = L'\0'; }

        // Force a RUNTIME call (double fold barrier): the volatile POINTER read
        // hides the buffer identity from the optimizer, the volatile RESULT
        // sink (the directive's `volatile size_t got`) blocks folding the
        // return value.
        wchar_t* volatile p = buf;
        volatile size_t got = std::wcslen(p);

        st_send(fmt::format("[ST] wcslen-16units expect=16 got={} stride={}\n",
                            static_cast<size_t>(got), sizeof(wchar_t)));
    }
    catch (...)
    {
        st_send("[ST] wcslen-16units FAIL exception\n");
    }

    // ---- Probe 2: std::wstring append/push_back round trip ----
    try
    {
        static const char16_t kLit16[17] = u"PlayerController";

        std::wstring w;
        w.assign(L"PlayerController"); // may bind the prebuilt out-of-line members
        w.push_back(L'X');
        w.append(L"YZ");

        const size_t n = w.size();

        // Hand-built expected image, hand-loop discipline (no wide libc):
        // "PlayerControllerXYZ" = 19 units.
        char16_t expected[19];
        for (size_t i = 0; i < 16; ++i) { expected[i] = kLit16[i]; }
        expected[16] = u'X';
        expected[17] = u'Y';
        expected[18] = u'Z';

        bool match = (n == 19);
        if (match)
        {
            const char16_t* data = reinterpret_cast<const char16_t*>(w.data());
            for (size_t i = 0; i < 19; ++i)
            {
                if (data[i] != expected[i]) { match = false; break; }
            }
        }

        // First 8 raw payload bytes as hex, sanity-gated (null data or an
        // insane size never gets dereferenced — the probe cannot fault here).
        std::string raw{"skipped"};
        const wchar_t* payload = w.data();
        if (payload != nullptr && n >= 4 && n <= 4096)
        {
            const auto* bytes = reinterpret_cast<const unsigned char*>(payload);
            raw.clear();
            for (size_t i = 0; i < 8; ++i) { raw += fmt::format("{:02x}", static_cast<unsigned>(bytes[i])); }
        }

        st_send(fmt::format("[ST] wstring-rt size={} content-match={} raw-first8={}\n",
                            n, match ? "yes" : "no", raw));
    }
    catch (...)
    {
        st_send("[ST] wstring-rt FAIL exception\n");
    }
#else
    st_send("[ST] toolchain self-test skipped (non-Prospero TU)\n");
#endif
}
