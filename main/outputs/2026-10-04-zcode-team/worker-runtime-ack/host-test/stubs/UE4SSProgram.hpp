// Host-test stub of the port's UE4SSProgram surface consumed by the patched
// ue4ss_backend.cpp. init() replicates the Prospero ordering contract the
// emission points rely on: on_engine_initialized fires (only when no shutdown
// was requested) after "engine setup", then init blocks until a cooperative
// shutdown request. Failure knobs let each test drive one program moment.
#pragma once

#include <Platform/NativeProgramContext.hpp>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <initializer_list>

namespace RC
{
    struct MProgram { struct BinaryOptions {}; };

    class UE4SSProgram final
    {
    public:
        // Failure injection knobs (per-process; set before Prepare*).
        static inline std::atomic<bool> reject_construction{false};
        static inline std::atomic<bool> reject_inventory{false};
        static inline std::atomic<bool> fail_after_engine_ready{false};

        UE4SSProgram(Platform::NativeProgramContext context,
                     std::initializer_list<MProgram::BinaryOptions>)
        {
            if (reject_construction.load()) error_ = "stub construction error";
            context_ = std::move(context);
        }
        auto native_startup_error_message() noexcept -> const char*
        {
            return error_.empty() ? nullptr : error_.c_str();
        }
        auto start_native_mods() -> void
        {
            mods_started_ = true;
            if (reject_inventory.load()) error_ = "stub native mods rejected";
        }
        auto init() -> void
        {
            if (fail_after_engine_ready.load())
            {
                // "Engine setup" completed and the callback fired, then the
                // program recorded a startup error before the update loop.
                fire_ready();
                error_ = "stub engine startup deadline exceeded";
                return;
            }
            fire_ready();
            std::unique_lock lock(wait_mutex_);
            wait_.wait(lock, [&] { return shutdown_requested_.load(); });
        }
        auto request_shutdown() noexcept -> void
        {
            shutdown_requested_.store(true);
            std::lock_guard lock(wait_mutex_);
            wait_.notify_all();
        }
    private:
        auto fire_ready() -> void
        {
            if (!shutdown_requested_.load(std::memory_order_acquire) && context_.on_engine_initialized)
                context_.on_engine_initialized();
        }
        Platform::NativeProgramContext context_;
        std::string error_;
        std::atomic<bool> mods_started_{false};
        std::atomic<bool> shutdown_requested_{false};
        std::mutex wait_mutex_;
        std::condition_variable wait_;
    };
}
