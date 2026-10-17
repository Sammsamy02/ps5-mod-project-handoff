#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <modforge_activation.h>
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

// Configuration for the runtime acknowledgment file (RUNTIME-ACK.md): the mod
// root that owns <root>/runtime-status.txt plus the session facts recorded at
// open. Supplied by the module_start composition through the same
// caller-owned injection pattern as NativeProgramContext::mod_source; an empty
// status_root leaves every acknowledgment a no-op. Never a global.
struct RuntimeAckConfig
{
    std::filesystem::path status_root;
    long pid{0};
    std::uint64_t now_ms{0};
};

// One session's append-only runtime acknowledgment channel. mfa_status_open
// replaces the status file (fresh-session semantics) during module_start while
// /data is still reachable; mfa_status_write then appends whole state lines
// through the retained descriptor for the whole process lifetime. Every
// method is nonthrowing and records write failures internally: an
// acknowledgment that cannot be written must never fail or delay engine
// startup (record, continue).
class RuntimeAckSink final
{
public:
    // Default-constructed sinks are inert (no descriptor); prefer Open().
    RuntimeAckSink() = default;
    RuntimeAckSink(const RuntimeAckSink&) = delete;
    RuntimeAckSink& operator=(const RuntimeAckSink&) = delete;
    // Opens (or fails to open) the status file; never throws. An open failure
    // yields an inert sink with the reason in last_error(), so startup
    // continues unacknowledged rather than failing. Returns null only when
    // allocation itself failed.
    static std::shared_ptr<RuntimeAckSink> Open(const RuntimeAckConfig& config) noexcept;
    void activated() noexcept;  // RUNTIME-ACK point 1: activation consumed, no engine work yet
    void engine_ready() noexcept; // point 2: real on_engine_initialized callback fired
    void mods_started() noexcept; // point 3: deferred mod startup from the activation views
    void failed(int code, const char* detail) noexcept; // point 4: startup failure (code != 0)
    void stopped() noexcept;    // point 5: stop() joined the init thread
    void close() noexcept;      // point 5: release the retained descriptor
    const std::string& last_error() const noexcept { return last_error_; }
private:
    void emit(const char* state, int code, const char* detail) noexcept;
    mfa_status status_{-1, 0};
    std::mutex mutex_;
    std::string last_error_;
    bool stopped_written_{false};
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
    // ack is the optional RUNTIME-ACK emission sink; absent (null) leaves the
    // owner's behavior byte-for-byte unchanged.
    explicit RuntimeOwner(std::unique_ptr<RuntimeBackend> backend,
                          std::shared_ptr<RuntimeAckSink> ack = nullptr);
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
    std::shared_ptr<RuntimeAckSink> ack_;
    std::mutex operations_;
    std::thread thread_;
    void initialize() noexcept;
};
}
