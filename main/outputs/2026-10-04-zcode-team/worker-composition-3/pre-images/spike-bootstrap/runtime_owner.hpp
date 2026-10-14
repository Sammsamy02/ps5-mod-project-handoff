#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace modbridge::native
{
enum class RuntimeState { Prepared, Starting, EngineInitialized, Stopping, Stopped, Failed };
struct RuntimeStatus
{
    RuntimeState state{RuntimeState::Prepared};
    std::string error;
};

// initialize blocks until shutdown. ready is called only after actual engine
// initialization. request_stop must be thread safe and nonthrowing. Backend
// destruction requires engine-hook quiescence separately established by loader.
class RuntimeBackend
{
public:
    virtual ~RuntimeBackend() = default;
    virtual void initialize(std::function<void()> ready) = 0;
    virtual void request_stop() noexcept = 0;
};

// Same-toolchain internal C++ interface, never an exported PRX/STL ABI.
// Retains backend/context through the complete init-thread lifetime. stop()
// joins that thread; it does NOT certify engine-hook quiescence or safe unload.
class RuntimeOwner final
{
public:
    explicit RuntimeOwner(std::unique_ptr<RuntimeBackend> backend);
    ~RuntimeOwner();
    RuntimeOwner(const RuntimeOwner&) = delete;
    RuntimeOwner& operator=(const RuntimeOwner&) = delete;
    bool start();
    // Returns false on the initialization thread to prevent self-join. A
    // loader control thread must stop/destroy the owner instead.
    bool stop();
    RuntimeStatus status() const;

private:
    struct Signals;
    std::unique_ptr<RuntimeBackend> backend_;
    std::shared_ptr<Signals> signals_;
    std::mutex operations_;
    std::thread thread_;
    void initialize() noexcept;
};
}
