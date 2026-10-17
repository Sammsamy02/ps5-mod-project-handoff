// mbm/Sdk.hpp -- PS5 Mod Bridge native mod SDK runtime (lives inside the mod
// module; one copy per mod). Holds the host table handed to mbm_mod_entry_v1,
// UTF-8 <-> wide conversion and small helpers the compatibility headers use.
// Nothing here is shared with the host except the C ABI in NativeModAbi.h.
#pragma once
#include <mbm/NativeModAbi.h>
#include <String/StringType.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace RC::Sdk
{
    struct Globals
    {
        const mbm_host_v1* host{};
        uint64_t token{};
    };
    // Defined in mbm_sdk_entry.cpp (one per mod module).
    auto globals() -> Globals&;

    inline auto host() -> const mbm_host_v1*
    {
        return globals().host;
    }
    inline auto token() -> uint64_t
    {
        return globals().token;
    }

    // ---- UTF-8 <-> wide (UTF-32 when wchar_t is 4 bytes, UTF-16 when 2) ----
    inline auto to_utf8(std::wstring_view w) -> std::string
    {
        std::string out;
        out.reserve(w.size());
        for (size_t i = 0; i < w.size(); ++i)
        {
            uint32_t cp = static_cast<uint32_t>(w[i]);
            if constexpr (sizeof(wchar_t) == 2)
            {
                if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < w.size())
                {
                    uint32_t lo = static_cast<uint32_t>(w[i + 1]);
                    if (lo >= 0xDC00 && lo <= 0xDFFF)
                    {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        ++i;
                    }
                }
            }
            if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) cp = 0xFFFD;
            if (cp < 0x80) out.push_back(static_cast<char>(cp));
            else if (cp < 0x800) { out.push_back(static_cast<char>(0xC0 | (cp >> 6))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
            else if (cp < 0x10000) { out.push_back(static_cast<char>(0xE0 | (cp >> 12))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
            else { out.push_back(static_cast<char>(0xF0 | (cp >> 18))); out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
        }
        return out;
    }

    inline auto to_wide(std::string_view s) -> std::wstring
    {
        std::wstring out;
        out.reserve(s.size());
        size_t i = 0;
        while (i < s.size())
        {
            const auto c = static_cast<unsigned char>(s[i]);
            uint32_t cp = 0xFFFD;
            size_t n = 1;
            if (c < 0x80) cp = c;
            else if ((c >> 5) == 0x6 && i + 1 < s.size()) { cp = ((c & 0x1F) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3F); n = 2; }
            else if ((c >> 4) == 0xE && i + 2 < s.size()) { cp = ((c & 0x0F) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) | (static_cast<unsigned char>(s[i + 2]) & 0x3F); n = 3; }
            else if ((c >> 3) == 0x1E && i + 3 < s.size()) { cp = ((c & 0x07) << 18) | ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) | ((static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) | (static_cast<unsigned char>(s[i + 3]) & 0x3F); n = 4; }
            i += n;
            if constexpr (sizeof(wchar_t) == 2)
            {
                if (cp >= 0x10000) { cp -= 0x10000; out.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10))); out.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF))); continue; }
            }
            out.push_back(static_cast<wchar_t>(cp));
        }
        return out;
    }

    inline void log(int32_t level, std::string_view utf8)
    {
        if (auto* h = host(); h && h->log)
        {
            std::string s(utf8);
            h->log(h->ctx, token(), level, s.c_str());
        }
    }

    // Read a host string through the caller-buffer protocol.
    template <typename F>
    inline auto read_string(F&& f) -> std::string
    {
        std::string buf(128, '\0');
        int32_t len = 0;
        mbm_status s = f(buf.data(), static_cast<int32_t>(buf.size()), &len);
        if (s == MBM_E_TRUNCATED && len > 0)
        {
            buf.assign(static_cast<size_t>(len) + 1, '\0');
            s = f(buf.data(), static_cast<int32_t>(buf.size()), &len);
        }
        if (s != MBM_OK) return {};
        buf.resize(static_cast<size_t>(len));
        return buf;
    }

    // Mod-owned storage for std::function callbacks handed to the host as
    // (C function pointer, void* user). Entries live until the mod is
    // destroyed; the host revokes before destroy, so no dangling call.
    template <typename Fn>
    struct CallbackStore
    {
        std::mutex mutex;
        std::vector<std::unique_ptr<Fn>> items;
        auto keep(Fn fn) -> Fn*
        {
            std::lock_guard g{mutex};
            items.push_back(std::make_unique<Fn>(std::move(fn)));
            return items.back().get();
        }
    };

    // Text of the last exception caught by an SDK trampoline (per module).
    auto last_error_slot() -> std::string&;
    inline auto record_fault(const char* what) -> mbm_status
    {
        try { last_error_slot() = what ? what : "unknown exception"; } catch (...) {}
        return MBM_E_MOD_FAULT;
    }

    // Run a mod callable, converting any exception into MBM_E_MOD_FAULT.
    template <typename F>
    inline auto contain(F&& f) -> mbm_status
    {
        try
        {
            f();
            return MBM_OK;
        }
        catch (const std::exception& e)
        {
            return record_fault(e.what());
        }
        catch (...)
        {
            return record_fault("non-standard exception");
        }
    }

    // Upstream-compatible game-thread helper (C++ equivalent of Lua
    // ExecuteInGameThread). Returns false when the host refused.
    auto execute_in_game_thread(std::function<void()> fn) -> bool;

    // Hook admission report (why a family is or is not available).
    inline auto admission(uint32_t family) -> mbm_admission
    {
        mbm_admission a{};
        if (auto* h = host(); h && h->hook_admission) h->hook_admission(h->ctx, family, &a);
        return a;
    }
} // namespace RC::Sdk
