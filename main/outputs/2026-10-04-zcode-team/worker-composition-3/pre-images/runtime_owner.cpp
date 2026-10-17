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
RuntimeOwner::RuntimeOwner(std::unique_ptr<RuntimeBackend> backend)
    : backend_(std::move(backend)), signals_(std::make_shared<Signals>())
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
            signals_->status = {RuntimeState::Failed, "Native runtime init returned without a stop request"};
    }
    catch (const std::exception& error)
    {
        std::lock_guard lock(signals_->mutex);
        signals_->status = {RuntimeState::Failed, error.what()};
    }
    catch (...)
    {
        std::lock_guard lock(signals_->mutex);
        signals_->status = {RuntimeState::Failed, "Native runtime initialization failed with an unknown exception"};
    }
    // Completed thread IDs may later be reused by an unrelated caller.
    std::lock_guard lock(signals_->mutex);
    signals_->initializer = {};
}
}
