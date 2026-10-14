#pragma once
// HOST-TEST SHIM — DynamicOutput/Output.hpp (worker-l11-enqueue-ladder).
// Minimal stand-in for the port's DynamicOutput device so the REAL staged
// binding/queue/drain code (extracted verbatim from the staged LuaMod.cpp
// lineages) and the REAL port ExceptionHandling.hpp (TRY) compile on macOS.
// Every Output::send is captured into a log buffer the test asserts on
// (the tiered oracle) and echoed to stdout for humans.
// This shim exists ONLY in the host harness; the staged diff itself adds no
// includes (its sends use the same port API the TU already uses).

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/xchar.h>

namespace RC
{
    enum class LogLevel
    {
        Verbose,
        Debug,
        Normal,
        Warning,
        Error
    };

    struct OutputCapture
    {
        LogLevel level{};
        std::wstring text{};
    };

    inline std::vector<OutputCapture>& output_capture()
    {
        static std::vector<OutputCapture> instance;
        return instance;
    }

    inline auto narrow(const std::wstring& wide) -> std::string
    {
        std::string out;
        out.reserve(wide.size());
        for (wchar_t c : wide)
        {
            out.push_back(static_cast<char>(c));
        }
        return out;
    }

    namespace Output
    {
        inline auto has_internal_error() -> bool
        {
            return false;
        }

        template <LogLevel L, typename... Args>
        auto send(const std::wstring& format_string, Args&&... args) -> void
        {
            // fmt::runtime: the staged call sites pass the format string as a
            // runtime value (e.g. ensure_str(e.what()) in the port's TRY macro),
            // so compile-time format-string checking does not apply here.
            std::wstring message = fmt::format(fmt::runtime(std::wstring_view{format_string}), std::forward<Args>(args)...);
            output_capture().push_back({L, message});
            std::printf("[OUT][%d] %s", static_cast<int>(L), narrow(message).c_str());
        }

        template <LogLevel L>
        auto send(const std::wstring& format_string) -> void
        {
            output_capture().push_back({L, format_string});
            std::printf("[OUT][%d] %s", static_cast<int>(L), narrow(format_string).c_str());
        }
    } // namespace Output
} // namespace RC
