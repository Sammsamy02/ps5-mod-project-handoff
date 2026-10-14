#pragma once
// RED CONTROL ONLY: source-extracted original publication/getters. Not a provider.
#include <thread>
#include <stdexcept>
#include <cstdio>
namespace RC::Unreal::NativeThreadDetail {
enum class Publication { Published, AlreadyCurrent, DifferentThread, Unavailable };
class Identity final {
std::thread::id GGameThreadId{};
bool GGameThreadIdInitialized{};
public:
auto publish_current() -> Publication {
const bool was_initialized=GGameThreadIdInitialized;
        if (GGameThreadId == std::thread::id{})
        {
            GGameThreadId = std::this_thread::get_id();
            GGameThreadIdInitialized = GGameThreadId != std::thread::id{};
            if (!GGameThreadIdInitialized)
            {
                std::fputs("Unable to retrieve ID of game thread\n", stderr);
            }
        }
if (!GGameThreadIdInitialized) return Publication::Unavailable;
if (GGameThreadId != std::this_thread::get_id()) return Publication::DifferentThread;
return was_initialized ? Publication::AlreadyCurrent : Publication::Published;
}
    auto id() -> std::thread::id
    {
        if (!GGameThreadIdInitialized)
        {
            throw std::runtime_error{"id called too early, not yet initialized by UGameEngine::Tick"};
        }
        return GGameThreadId;
    }

    auto in_current() -> bool
    {
        return std::this_thread::get_id() == id();
    }

    auto initialized() noexcept-> bool
    {
        return GGameThreadIdInitialized;
    }

    auto id_raw() noexcept -> std::thread::id
    {
        if (!GGameThreadIdInitialized)
        {
            return {};
        }
        return GGameThreadId;
    }

    auto in_current_raw() noexcept -> bool
    {
        return initialized() && std::this_thread::get_id() == id();
    }
};
}
