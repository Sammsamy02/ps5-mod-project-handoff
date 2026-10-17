// DynamicOutput/DynamicOutput.hpp -- PS5 Mod Bridge native mod SDK.
// Source-compatible subset of UE4SS Output::send: std::format-style "{}"
// placeholders on a wide format string, optional LogLevel template argument.
// The text goes to the host log (ue4ss.log on the console) as UTF-8.
#pragma once
#include <mbm/Sdk.hpp>

#include <format>
#include <string>
#include <utility>

namespace RC
{
    namespace LogLevel
    {
        enum LogLevel : int
        {
            Default,
            Normal,
            Verbose,
            Warning,
            Error,
        };
    }

    namespace Output
    {
        inline auto to_abi_level(LogLevel::LogLevel l) -> int32_t
        {
            switch (l)
            {
            case LogLevel::Verbose: return MBM_LOG_VERBOSE;
            case LogLevel::Warning: return MBM_LOG_WARNING;
            case LogLevel::Error: return MBM_LOG_ERROR;
            default: return MBM_LOG_NORMAL;
            }
        }

        template <LogLevel::LogLevel Level = LogLevel::Default, typename... Args>
        inline auto send(std::wstring_view format, Args&&... args) -> void
        {
            std::wstring text;
            if constexpr (sizeof...(Args) == 0) text.assign(format);
            else text = std::vformat(format, std::make_wformat_args(args...));
            Sdk::log(to_abi_level(Level), Sdk::to_utf8(text));
        }

        template <LogLevel::LogLevel Level = LogLevel::Default, typename... Args>
        inline auto send(std::string_view format, Args&&... args) -> void
        {
            std::string text;
            if constexpr (sizeof...(Args) == 0) text.assign(format);
            else text = std::vformat(format, std::make_format_args(args...));
            Sdk::log(to_abi_level(Level), text);
        }
    } // namespace Output
} // namespace RC
