// time_shim.cpp — host stand-in for deps/first/Helpers/src/Time.cpp in the
// logwriter harness (agent CL, 2026-10-08).
//
// The real Time.cpp formats via fmt::chrono's localized %X path, which
// instantiates std::time_put<CharType>; host libc++ provides no char16_t
// locale facets (the target's 2-byte wchar_t does). The timestamp is NOT a
// unit under test (F1-F4 touch atomicity and encoding only), so this shim
// reproduces the exact "[YYYY-MM-DD HH:MM:SS]" text the real
// OutputDevice::default_format_string expects, keeping the log line shape
// identical for the analyzer. Everything else in the harness links the REAL
// staged units (Output.cpp / PosixFile.cpp / FileDevice.hpp variants).
#include <cstring>
#include <ctime>

#include <Helpers/Time.hpp>

namespace RC
{
    auto get_now_as_string(StringViewType format) -> StringType
    {
        (void)format; // harness only uses the default "%Y-%m-%d %X" shape
        const std::time_t now = std::time(nullptr);
        std::tm tmv{};
        localtime_r(&now, &tmv);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tmv);
        return StringType(buf, buf + std::strlen(buf));
    }
} // namespace RC
