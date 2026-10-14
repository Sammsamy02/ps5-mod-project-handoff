#pragma once
#include <mutex>
#include <thread>
#include <utility>

namespace l13_sink {
// Only the actual RuntimeOwner initializer publishes ownership. Startup and
// acknowledged stop are serialized by the loader; stop joins this owner
// before its fallback release. Foreign callers consult only this gate, never
// the unsynchronized bank, captures, fault ledger or probe telemetry.
class InitializerOwner {
    std::mutex mutex_;
    std::thread::id initializer_{};
public:
    void prepare() {
        std::lock_guard<std::mutex> lock(mutex_);
        initializer_ = {};
    }
    bool enter() {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto caller = std::this_thread::get_id();
        if (initializer_ != std::thread::id{} && initializer_ != caller) return false;
        initializer_ = caller;
        return true;
    }
    bool allows() {
        std::lock_guard<std::mutex> lock(mutex_);
        return initializer_ != std::thread::id{} && initializer_ == std::this_thread::get_id();
    }
    void leave() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initializer_ == std::this_thread::get_id()) initializer_ = {};
    }
};

// Resource completion can be explicit at the final check boundary or happen
// during exception unwinding. Consume the cleanup obligation before invoking
// it, so destruction never retries a close after an error or reused FD.
template <typename Release>
class CompletionCleanup {
    Release release_;
    bool pending_ = true;
public:
    explicit CompletionCleanup(Release release) : release_(std::move(release)) {}
    CompletionCleanup(const CompletionCleanup&) = delete;
    CompletionCleanup& operator=(const CompletionCleanup&) = delete;
    void complete() noexcept {
        if (!pending_) return;
        pending_ = false;
        release_();
    }
    ~CompletionCleanup() noexcept { complete(); }
};

// module_start failures never set runtime_active, so module_stop will not call
// the bridge stop function. Release here unless an owner can still use the fd.
template <typename Owner, typename Release, typename Start>
auto with_startup_cleanup(Owner &owner, Release release, Start start) -> decltype(start())
{
    struct Cleanup {
        Owner &owner;
        Release &release;
        ~Cleanup() { if (!owner) release(); }
    } cleanup{owner, release};
    return start();
}
} // namespace l13_sink
