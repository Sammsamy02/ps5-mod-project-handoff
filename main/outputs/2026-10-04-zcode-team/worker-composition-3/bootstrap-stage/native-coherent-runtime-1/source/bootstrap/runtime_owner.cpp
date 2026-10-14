#include "runtime_owner.hpp"
#include <stdexcept>

namespace modbridge::native
{
struct RuntimeOwner::Signals
{
    mutable std::mutex mutex;
    RuntimeStatus status;
    std::thread::id initializer;
};
RuntimeOwner::RuntimeOwner(std::unique_ptr<RuntimeBackend> backend, std::shared_ptr<RuntimeAckSink> ack)
    : backend_(std::move(backend)), signals_(std::make_shared<Signals>()), ack_(std::move(ack))
{
    if (!backend_) throw std::invalid_argument("Native runtime backend is required");
}
RuntimeOwner::~RuntimeOwner()
{
    // Destroying the object on its own worker would free live program state.
    // There is deliberately no detached-thread fallback.
    if (!stop()) std::terminate();
}
bool RuntimeOwner::start()
{
    std::lock_guard operation(operations_);
    {
        std::lock_guard lock(signals_->mutex);
        if (signals_->status.state != RuntimeState::Prepared) return false;
        signals_->status.state = RuntimeState::Starting;
    }
    try { thread_ = std::thread(&RuntimeOwner::initialize, this); }
    catch (const std::exception& error)
    {
        std::lock_guard lock(signals_->mutex);
        signals_->status = {RuntimeState::Failed, error.what()};
        // RUNTIME-ACK point 4: the session failed before any engine work;
        // record and continue, never propagate from the acknowledgment.
        if (ack_) ack_->failed(1, error.what());
        return false;
    }
    return true;
}
bool RuntimeOwner::stop()
{
    // Check before operations_: another caller may already hold it while
    // joining us. The worker must never wait behind its own joiner.
    {
        std::lock_guard lock(signals_->mutex);
        if (signals_->initializer == std::this_thread::get_id()) return false;
    }
    std::lock_guard operation(operations_);
    {
        std::lock_guard lock(signals_->mutex);
        if (signals_->status.state != RuntimeState::Failed &&
            signals_->status.state != RuntimeState::Stopped)
            signals_->status.state = RuntimeState::Stopping;
    }
    backend_->request_stop();
    if (thread_.joinable()) thread_.join();
    {
        std::lock_guard lock(signals_->mutex);
        signals_->initializer = {};
        if (signals_->status.state != RuntimeState::Failed)
            signals_->status.state = RuntimeState::Stopped;
        // RUNTIME-ACK point 5: after stop() has joined the init thread, the
        // cooperative stop is the session's final acknowledged state; only
        // then is the retained descriptor released. The sink emits stopped
        // at most once per session (the init thread may complete the
        // Stopping->Stopped transition first; repeated stop adds nothing),
        // and a failed session keeps its failed line last.
        if (ack_ && signals_->status.state == RuntimeState::Stopped)
        {
            ack_->stopped();
            ack_->close();
        }
    }
    return true;
}
RuntimeStatus RuntimeOwner::status() const
{
    std::lock_guard lock(signals_->mutex);
    return signals_->status;
}
void RuntimeOwner::initialize() noexcept
{
    {
        std::lock_guard lock(signals_->mutex);
        signals_->initializer = std::this_thread::get_id();
    }
    try
    {
        std::weak_ptr<Signals> weak = signals_;
        backend_->initialize([weak] {
            if (const auto signals = weak.lock())
            {
                std::lock_guard lock(signals->mutex);
                if (signals->status.state == RuntimeState::Starting)
                    signals->status.state = RuntimeState::EngineInitialized;
            }
        });
        std::lock_guard lock(signals_->mutex);
        if (signals_->status.state == RuntimeState::Stopping)
            signals_->status.state = RuntimeState::Stopped;
        else
        {
            signals_->status = {RuntimeState::Failed, "Native runtime init returned without a stop request"};
            // RUNTIME-ACK point 4: init returned on its own without readiness.
            if (ack_) ack_->failed(1, "Native runtime init returned without a stop request");
        }
    }
    catch (const std::exception& error)
    {
        std::lock_guard lock(signals_->mutex);
        signals_->status = {RuntimeState::Failed, error.what()};
        // RUNTIME-ACK point 4: genuine startup error, including a typed
        // deadline/cancellation converted to an error by the backend.
        if (ack_) ack_->failed(1, error.what());
    }
    catch (...)
    {
        std::lock_guard lock(signals_->mutex);
        signals_->status = {RuntimeState::Failed, "Native runtime initialization failed with an unknown exception"};
        if (ack_) ack_->failed(1, "Native runtime initialization failed with an unknown exception");
    }
    // Completed thread IDs may later be reused by an unrelated caller.
    std::lock_guard lock(signals_->mutex);
    signals_->initializer = {};
}

std::shared_ptr<RuntimeAckSink> RuntimeAckSink::Open(const RuntimeAckConfig& config) noexcept
{
    try
    {
        auto sink = std::make_shared<RuntimeAckSink>();
        if (config.status_root.empty())
        {
            sink->last_error_ = "runtime ack root is not configured";
            return sink;
        }
        const std::string root = config.status_root.string();
        if (const int rc = mfa_status_open(&sink->status_, root.c_str(), config.pid, config.now_ms))
        {
            sink->status_.fd = -1;
            sink->last_error_ = "mfa_status_open failed: " + std::to_string(-rc);
            return sink; // Inert: fd < 0, every emit becomes a recorded no-op.
        }
        return sink;
    }
    catch (...) // Allocation failure: run unacknowledged, never fail startup.
    {
        return nullptr;
    }
}
void RuntimeAckSink::emit(const char* state, int code, const char* detail) noexcept
{
    try
    {
        std::lock_guard lock(mutex_);
        if (status_.fd < 0)
        {
            if (last_error_.empty()) last_error_ = "ack sink has no open status file";
            return;
        }
        if (const int rc = mfa_status_write(&status_, state, code, detail ? detail : ""))
            last_error_ = std::string("mfa_status_write(") + state + ") failed: " + std::to_string(-rc);
    }
    catch (...) // Recording a diagnostic must never take down startup.
    {
    }
}
void RuntimeAckSink::activated() noexcept { emit("activated", 0, ""); }
void RuntimeAckSink::engine_ready() noexcept { emit("engine-ready", 0, ""); }
void RuntimeAckSink::mods_started() noexcept { emit("mods-started", 0, ""); }
void RuntimeAckSink::failed(int code, const char* detail) noexcept
{
    emit("failed", code != 0 ? code : 1, detail ? detail : "");
}
void RuntimeAckSink::stopped() noexcept
{
    try
    {
        {
            std::lock_guard lock(mutex_);
            if (stopped_written_) return; // one stopped line per session
            stopped_written_ = true;
        }
        emit("stopped", 0, "");
    }
    catch (...)
    {
    }
}
void RuntimeAckSink::close() noexcept
{
    try
    {
        std::lock_guard lock(mutex_);
        mfa_status_close(&status_);
    }
    catch (...)
    {
    }
}
}
