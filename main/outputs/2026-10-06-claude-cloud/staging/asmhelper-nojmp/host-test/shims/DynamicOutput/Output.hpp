#pragma once
// host-test shim for the port's DynamicOutput (records every send)
#include <cstdio>
#include <string>
#include <vector>
#include <fmt/xchar.h>
#ifndef STR
#define STR(text) L##text
#endif
namespace RC
{
    enum class LogLevel { Verbose, Default, Normal, Warning, Error };
    inline std::vector<std::wstring>& output_log() { static std::vector<std::wstring> v; return v; }
    namespace Output
    {
        template <LogLevel L = LogLevel::Default, typename... Args>
        auto send(const wchar_t* f, Args&&... args) -> void
        {
            auto m = fmt::format(fmt::runtime(std::wstring_view{f}), std::forward<Args>(args)...);
            output_log().push_back(m);
            std::printf("[OUT] %ls", m.c_str());
        }
    }
}
