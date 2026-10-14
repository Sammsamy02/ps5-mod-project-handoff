#pragma once
#include <cstdint>
#include <mutex>

namespace RC::L23GameThreadProbe {
enum class Status { Idle, Submitting, Queued, TimedOut, Ran, Failed, Cancelled, WrongThread };
struct Result { bool accepted{}; bool late{}; uint64_t elapsed{}; int32_t thread{}; };
// One job per runtime lifetime. Owner is an identity token, never dereferenced.
// A timed-out job remains owned by the normal Lua action queue until drain or
// uninstall; timing out does not free Lua state or create an alternate pump.
class State {
    mutable std::mutex mutex;
    Status current = Status::Idle;
    const void* owner = nullptr;
    uint64_t started = 0;
public:
    bool begin(const void* token, uint64_t now, int32_t thread) {
        std::lock_guard lock(mutex);
        if(current != Status::Idle || !token || thread <= 0) return false;
        owner = token; started = now; current = Status::Submitting; return true;
    }
    void queued() { std::lock_guard lock(mutex); if(current==Status::Submitting) current=Status::Queued; }
    void failed() { std::lock_guard lock(mutex); if(current==Status::Submitting) current=Status::Failed; }
    bool timeout(uint64_t now) {
        std::lock_guard lock(mutex);
        if(current!=Status::Queued || now<started || now-started<30000) return false;
        current=Status::TimedOut; return true;
    }
    Result ran(uint64_t now, int32_t thread, bool on_game_thread) {
        std::lock_guard lock(mutex);
        if(current!=Status::Queued && current!=Status::TimedOut) return {};
        if(!on_game_thread || thread<=0 || now<started) {current=Status::WrongThread; return {};}
        Result result{true,current==Status::TimedOut || now-started>=30000,now-started,thread};
        current=Status::Ran;return result;
    }
    bool cancel(const void* token) {
        std::lock_guard lock(mutex);
        if(owner!=token || (current!=Status::Submitting && current!=Status::Queued && current!=Status::TimedOut)) return false;
        current=Status::Cancelled; return true;
    }
    Status status() const {std::lock_guard lock(mutex);return current;}
};
// Implemented in the existing LuaMod unit; Program only polls a monotonic
// diagnostic deadline. It never drains or runs the callback on its own thread.
void Poll() noexcept;
}
