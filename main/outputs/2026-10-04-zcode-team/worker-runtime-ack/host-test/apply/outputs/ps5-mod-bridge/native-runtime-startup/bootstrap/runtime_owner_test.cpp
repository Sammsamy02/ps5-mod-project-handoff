#include "runtime_owner.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

using namespace modbridge::native;
using namespace std::chrono_literals;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); std::_Exit(1); } } while (false)

struct Control
{
    std::mutex mutex;
    std::condition_variable changed;
    bool entered{}, announce{}, stopping{}, fail{}, exit_without_ready{};
    bool exited{}, destroyed{}, destroyed_early{};
    int starts{};
    std::function<void()> ready;
    std::function<void()> before_wait;
};
class ControlledBackend final : public RuntimeBackend
{
    std::shared_ptr<Control> c_;
public:
    explicit ControlledBackend(std::shared_ptr<Control> c) : c_(std::move(c)) {}
    ~ControlledBackend() override
    {
        std::lock_guard lock(c_->mutex);
        c_->destroyed_early = c_->entered && !c_->exited;
        c_->destroyed = true;
    }
    void initialize(std::function<void()> ready) override
    {
        std::unique_lock lock(c_->mutex);
        ++c_->starts;
        c_->entered = true;
        c_->ready = std::move(ready);
        c_->changed.notify_all();
        if (c_->before_wait)
        {
            lock.unlock();
            c_->before_wait();
            lock.lock();
        }
        c_->changed.wait(lock, [&] { return c_->announce || c_->stopping || c_->fail || c_->exit_without_ready; });
        if (c_->fail) { c_->exited = true; throw std::runtime_error("engine unavailable"); }
        if (c_->exit_without_ready) { c_->exited = true; return; }
        // A late ready callback must not overwrite Stopping/Stopped.
        auto signal = c_->ready;
        lock.unlock();
        signal();
        lock.lock();
        c_->changed.wait(lock, [&] { return c_->stopping; });
        c_->exited = true;
    }
    void request_stop() noexcept override
    {
        std::lock_guard lock(c_->mutex);
        c_->stopping = true;
        c_->changed.notify_all();
    }
};
static void wait_entered(const std::shared_ptr<Control>& c)
{
    std::unique_lock lock(c->mutex);
    CHECK(c->changed.wait_for(lock, 2s, [&] { return c->entered; }));
}
static void wait_state(const RuntimeOwner& owner, RuntimeState expected)
{
    const auto end = std::chrono::steady_clock::now() + 2s;
    while (owner.status().state != expected && std::chrono::steady_clock::now() < end)
        std::this_thread::yield();
    CHECK(owner.status().state == expected);
}
int main()
{
    bool rejected = false;
    try { RuntimeOwner owner(nullptr); } catch (const std::invalid_argument&) { rejected = true; }
    CHECK(rejected);
    {
        auto c = std::make_shared<Control>();
        RuntimeOwner owner(std::make_unique<ControlledBackend>(c));
        CHECK(owner.status().state == RuntimeState::Prepared);
        CHECK(owner.start());
        wait_entered(c);
        CHECK(!owner.start());
        CHECK(owner.status().state == RuntimeState::Starting);
        {
            std::lock_guard lock(c->mutex);
            c->announce = true;
            c->changed.notify_all();
        }
        wait_state(owner, RuntimeState::EngineInitialized);
        CHECK(owner.stop());
        CHECK(owner.stop());
        CHECK(owner.status().state == RuntimeState::Stopped);
        CHECK(c->exited && !c->destroyed);
        c->ready();
        CHECK(owner.status().state == RuntimeState::Stopped);
        CHECK(!owner.start());
        CHECK(c->starts == 1);
    }
    {
        auto c = std::make_shared<Control>();
        RuntimeOwner owner(std::make_unique<ControlledBackend>(c));
        CHECK(owner.stop());
        CHECK(!owner.start());
        CHECK(c->starts == 0);
    }
    {
        auto c = std::make_shared<Control>();
        {
            RuntimeOwner owner(std::make_unique<ControlledBackend>(c));
            CHECK(owner.start());
            wait_entered(c);
            CHECK(owner.stop());
            CHECK(owner.status().state == RuntimeState::Stopped);
            CHECK(c->exited && !c->destroyed);
        }
        CHECK(c->destroyed && !c->destroyed_early);
    }
    for (bool fail : {false, true})
    {
        auto c = std::make_shared<Control>();
        c->fail = fail;
        c->exit_without_ready = !fail;
        RuntimeOwner owner(std::make_unique<ControlledBackend>(c));
        CHECK(owner.start());
        wait_state(owner, RuntimeState::Failed);
        CHECK(!owner.status().error.empty());
        if (fail) CHECK(owner.status().error == "engine unavailable");
        CHECK(owner.stop());
        CHECK(owner.status().state == RuntimeState::Failed);
        CHECK(!owner.start());
    }
    {
        auto c = std::make_shared<Control>();
        RuntimeOwner owner(std::make_unique<ControlledBackend>(c));
        CHECK(owner.start());
        wait_entered(c);
        {
            std::lock_guard lock(c->mutex);
            c->fail = true;
        }
        // Stop wakes initialize, which reports an independent real failure.
        // Cancellation must never clear that failure from the owner status.
        CHECK(owner.stop());
        CHECK(owner.status().state == RuntimeState::Failed);
        CHECK(owner.status().error == "engine unavailable");
    }
    {
        auto c = std::make_shared<Control>();
        {
            RuntimeOwner owner(std::make_unique<ControlledBackend>(c));
            CHECK(owner.start());
            wait_entered(c);
        }
        CHECK(c->exited && c->destroyed && !c->destroyed_early);
        c->ready(); // Callback cannot access a destroyed owner.
    }
    {
        auto c = std::make_shared<Control>();
        RuntimeOwner owner(std::make_unique<ControlledBackend>(c));
        std::atomic<bool> self_join_rejected{false};
        c->before_wait = [&] { self_join_rejected.store(!owner.stop()); };
        c->announce = true;
        CHECK(owner.start());
        wait_state(owner, RuntimeState::EngineInitialized);
        CHECK(self_join_rejected.load());
        bool first{}, second{};
        std::thread stop_one([&] { first = owner.stop(); });
        std::thread stop_two([&] { second = owner.stop(); });
        stop_one.join();
        stop_two.join();
        CHECK(first && second);
        CHECK(owner.status().state == RuntimeState::Stopped);
    }
    std::puts("PASS runtime owner lifecycle and failure controls (host backend only)");
}
