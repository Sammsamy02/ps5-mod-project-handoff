// [LFB] fixture stub (agent-EY): shadows <DynamicOutput/Output.hpp> for the
// LuaFNameBridge harness. Same namespace/enum shape as the real header
// (RC::LogLevel enum, RC::Output::send). Captures every send line (wide)
// into a vector the fixture asserts on. Mini-fmt: '{}' and '{:X}' for
// integral args, '{}' for string args (all the bridge uses).
//
// [EV string-seam discipline]: StringType is built with resize + indexed
// writes ONLY — never push_back/append — because on short-wchar builds the
// prebuilt library's 4-byte-wchar extern-template basic_string<wchar_t>
// members interleave a NUL after every unit (host-reproduced under
// -fshort-wchar; the exact bug class this converter exists to bypass).
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

#include <String/StringType.hpp>

namespace RC
{
    namespace LogLevel
    {
        enum LogLevel
        {
            Default = 0,
            Normal = 1,
            Verbose = 2,
            Warning = 3,
            Error = 4,
        };
    } // namespace LogLevel
}

namespace RC::Output
{
    inline std::mutex g_lfb_log_mutex{};
    inline std::vector<StringType> g_lfb_log{};
    inline std::vector<int> g_lfb_level{};

    // Scratch buffer: std::vector<CharType> is instantiated in-TU (header
    // only); only basic_string<wchar_t> carries the prebuilt extern-template
    // stride bug. Final copy into StringType uses resize + operator[].
    inline void lfb_store(StringType& out, const std::vector<CharType>& units)
    {
        out.resize(units.size());
        for (size_t i = 0; i < units.size(); ++i) { out[i] = units[i]; }
    }

    inline void lfb_append_uint(std::vector<CharType>& out, uint64_t v, bool hex)
    {
        CharType buf[24];
        const CharType* digits = hex ? STR("0123456789ABCDEF") : STR("0123456789");
        int p = 0;
        if (v == 0) { buf[p++] = CharType('0'); }
        while (v)
        {
            buf[p++] = digits[hex ? (v & 0xF) : (v % 10)];
            v = hex ? (v >> 4) : (v / 10);
        }
        while (p) { out.push_back(buf[--p]); }
    }

    inline void lfb_fmt(std::vector<CharType>& out, const CharType* f)
    {
        for (; *f; ++f) { out.push_back(*f); }
    }

    template <class A, class... Rest>
    inline void lfb_fmt(std::vector<CharType>& out, const CharType* f, A&& arg, Rest&&... rest)
    {
        for (; *f; ++f)
        {
            if (f[0] == CharType('{') && f[1])
            {
                bool hex = false;
                const CharType* close = f + 1;
                while (*close && *close != CharType('}'))
                {
                    if (*close == CharType('X') || *close == CharType('x')) { hex = true; }
                    ++close;
                }
                if (!*close) { out.push_back(*f); continue; }
                using Decay = std::decay_t<A>;
                if constexpr (std::is_integral_v<Decay> || std::is_enum_v<Decay>)
                {
                    lfb_append_uint(out, static_cast<uint64_t>(arg), hex);
                }
                else
                {
                    // String-like arg with data()/size().
                    for (size_t i = 0; i < arg.size(); ++i) { out.push_back(arg.data()[i]); }
                }
                lfb_fmt(out, close + 1, std::forward<Rest>(rest)...);
                return;
            }
            out.push_back(*f);
        }
    }

    // First parameter by pointer: constructing StringViewType from a literal
    // would run the prebuilt (4-byte-wchar) traits::length on 2-byte data.
    template <LogLevel::LogLevel Level = LogLevel::Default, typename... Args>
    inline void send(const CharType* f, Args&&... args)
    {
        std::vector<CharType> units;
        lfb_fmt(units, f, std::forward<Args>(args)...);
        StringType line;
        lfb_store(line, units);
        std::lock_guard<std::mutex> lock(g_lfb_log_mutex);
        g_lfb_log.push_back(std::move(line)); // vector push_back of StringType: move ctor, inline/stride-correct
        g_lfb_level.push_back(static_cast<int>(Level));
    }
} // namespace RC::Output
