#pragma once
#include <mutex>
#include <stdexcept>
#include <thread>

namespace RC::Unreal::NativeThreadDetail {
enum class Publication { Published, AlreadyCurrent, DifferentThread, Unavailable };

// Private native state, used by the genuine Tick publisher and existing getters.
// Publication does not itself prove engine identity or make worker callbacks safe.
class Identity final {
    mutable std::mutex mutex_;
    std::thread::id id_{};
public:
    Identity() = default;
    Identity(const Identity&) = delete;
    auto operator=(const Identity&) -> Identity& = delete;

    auto publish_current() noexcept -> Publication {
        const auto current=std::this_thread::get_id();
        if(current==std::thread::id{}) return Publication::Unavailable;
        try {
            std::lock_guard lock{mutex_};
            if(id_==std::thread::id{}) {
                id_=current;
                return Publication::Published;
            }
            return id_==current ? Publication::AlreadyCurrent : Publication::DifferentThread;
        } catch(...) {
            return Publication::Unavailable;
        }
    }
    auto id() const -> std::thread::id {
        std::lock_guard lock{mutex_};
        if(id_==std::thread::id{})
            throw std::runtime_error{"GetGameThreadId called too early, not yet initialized by UGameEngine::Tick"};
        return id_;
    }
    auto in_current() const -> bool {
        return std::this_thread::get_id()==id();
    }
    auto id_raw() const noexcept -> std::thread::id {
        try {
            std::lock_guard lock{mutex_};
            return id_;
        } catch(...) {
            // The raw contract must remain nonthrowing and refuse admission
            // if synchronization cannot provide a known identity.
            return {};
        }
    }
    auto initialized() const noexcept -> bool {
        return id_raw()!=std::thread::id{};
    }
    auto in_current_raw() const noexcept -> bool {
        const auto known=id_raw();
        return known!=std::thread::id{} && known==std::this_thread::get_id();
    }
};
}
