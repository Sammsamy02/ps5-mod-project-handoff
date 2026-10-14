#ifndef CAP13_GUEST_CRITICAL_SECTIONS_H
#define CAP13_GUEST_CRITICAL_SECTIONS_H

#include "guest_read_map.h"
#include "guest_write_map.h"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <thread>
#include <unordered_map>

namespace cap13 {

// Per-shim host state for guest-owned x64 CRITICAL_SECTION objects. The guest
// object is logically opaque, as documented by Win32. The guest must keep its
// 40-byte storage alive, unmoved, and registered until DeleteCriticalSection;
// all guest calls must be quiescent before destroying this manager.
class GuestCriticalSections
{
  public:
    bool bind(GuestReadMap* reads, GuestWriteMap* writes);
    bool ready() const;
    bool initialize(void* guest, uint32_t spin_count, uint32_t flags);
    bool enter(void* guest);
    bool leave(void* guest);
    bool erase(void* guest);

  private:
    struct State
    {
        std::mutex mu;
        std::condition_variable cv;
        std::thread::id owner{};
        uint32_t depth = 0;
        uint32_t waiters = 0;
        bool deleted = false;
    };
    mutable std::mutex mu_;
    GuestReadMap* reads_ = nullptr;
    GuestWriteMap* writes_ = nullptr;
    std::unordered_map<void*, std::shared_ptr<State>> entries_;

    bool valid_guest(void* guest) const;
    std::shared_ptr<State> find(void* guest) const;
};

namespace critical_section_detail {
constexpr uint32_t kNoDebugInfo = 0x01000000u;
struct GuestLayout
{
    uint64_t debug_info = 0;
    int32_t lock_count = -1;
    int32_t recursion_count = 0;
    uint64_t owning_thread = 0;
    uint64_t lock_semaphore = 0;
    uint64_t spin_count = 0;
};
static_assert(sizeof(GuestLayout) == 40, "x64 CRITICAL_SECTION must be 40 bytes");
}

inline bool GuestCriticalSections::bind(GuestReadMap* reads,
                                        GuestWriteMap* writes)
{
    if (!reads || !writes) return false;
    std::lock_guard guard(mu_);
    if (reads_ || writes_) return reads_ == reads && writes_ == writes;
    reads_ = reads;
    writes_ = writes;
    return true;
}

inline bool GuestCriticalSections::ready() const
{
    std::lock_guard guard(mu_);
    return reads_ && writes_;
}

inline bool GuestCriticalSections::valid_guest(void* guest) const
{
    if (!guest || (reinterpret_cast<uintptr_t>(guest) & 7u) || !reads_)
        return false;
    critical_section_detail::GuestLayout sample{};
    return reads_->read(guest, &sample, sizeof(sample));
}

inline std::shared_ptr<GuestCriticalSections::State>
GuestCriticalSections::find(void* guest) const
{
    std::lock_guard guard(mu_);
    auto row = entries_.find(guest);
    return row == entries_.end() ? std::shared_ptr<State>{} : row->second;
}

inline bool GuestCriticalSections::initialize(void* guest,
                                               uint32_t spin_count,
                                               uint32_t flags)
{
    if (flags && flags != critical_section_detail::kNoDebugInfo) return false;
    std::lock_guard guard(mu_);
    if (!reads_ || !writes_ || !valid_guest(guest) || entries_.count(guest))
        return false;
    std::shared_ptr<State> state;
    try
    {
        state = std::make_shared<State>();
        entries_.emplace(guest, state);
    }
    catch (const std::bad_alloc&) { return false; }
    critical_section_detail::GuestLayout layout{};
    layout.spin_count = spin_count;
    if (!writes_->write(guest, &layout, sizeof(layout)))
    {
        entries_.erase(guest);
        return false;
    }
    return true;
}

inline bool GuestCriticalSections::enter(void* guest)
{
    auto state = find(guest);
    if (!state || !valid_guest(guest)) return false;
    std::unique_lock guard(state->mu);
    if (state->deleted) return false;
    const auto self = std::this_thread::get_id();
    if (state->owner != self && state->depth)
    {
        if (state->waiters == std::numeric_limits<uint32_t>::max())
            return false;
        ++state->waiters;
        try
        {
            state->cv.wait(guard, [&] {
                return state->deleted || state->depth == 0;
            });
        }
        catch (...)
        {
            --state->waiters;
            throw;
        }
        --state->waiters;
        if (state->deleted) return false;
    }
    if (state->depth == std::numeric_limits<uint32_t>::max())
        return false;
    state->owner = self;
    ++state->depth;
    return true;
}

inline bool GuestCriticalSections::leave(void* guest)
{
    auto state = find(guest);
    if (!state || !valid_guest(guest)) return false;
    std::unique_lock guard(state->mu);
    if (state->deleted || !state->depth ||
        state->owner != std::this_thread::get_id())
        return false;
    if (--state->depth == 0)
    {
        state->owner = {};
        guard.unlock();
        state->cv.notify_one();
    }
    return true;
}

inline bool GuestCriticalSections::erase(void* guest)
{
    std::lock_guard registry_guard(mu_);
    auto row = entries_.find(guest);
    if (row == entries_.end() || !valid_guest(guest)) return false;
    std::lock_guard state_guard(row->second->mu);
    if (row->second->depth || row->second->waiters) return false;
    row->second->deleted = true;
    entries_.erase(row);
    return true;
}

} // namespace cap13
#endif
