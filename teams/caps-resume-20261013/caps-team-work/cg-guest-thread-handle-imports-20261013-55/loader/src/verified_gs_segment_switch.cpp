#include "verified_gs_segment_switch.h"

#include <cstdlib>
#include <unordered_map>

#if defined(__PROSPERO__)
#include <machine/sysarch.h>
#endif

namespace cap13 {

namespace {
// The same backend may be used by several host threads, each with its own GS.
// Insert before changing GS so no allocation occurs after the transition.
thread_local std::unordered_map<const VerifiedGsSegmentSwitch*, void*> saved_gs;

#if defined(__PROSPERO__)
bool ps5_read_gs(void*, void** value) noexcept
{
    return value && sysarch(AMD64_GET_GSBASE, value) == 0;
}

bool ps5_write_gs(void*, void* value) noexcept
{
    return sysarch(AMD64_SET_GSBASE, &value) == 0;
}
#endif
} // namespace

void VerifiedGsSegmentSwitch::restore_or_abort(void* host_gs) noexcept
{
    if (!operations_.write(operations_.context, host_gs)) std::abort();
    void* observed = nullptr;
    if (!operations_.read(operations_.context, &observed) || observed != host_gs)
        std::abort();
}

bool VerifiedGsSegmentSwitch::enter(void* guest_teb)
{
    if (!guest_teb || !operations_.read || !operations_.write ||
        saved_gs.find(this) != saved_gs.end())
        return false;

    void* host_gs = nullptr;
    if (!operations_.read(operations_.context, &host_gs)) return false;
    try { saved_gs.emplace(this, host_gs); }
    catch (...) { return false; }

    const bool wrote = operations_.write(operations_.context, guest_teb);
    void* observed = nullptr;
    const bool read_back = operations_.read(operations_.context, &observed);
    if (!read_back || !wrote || observed != guest_teb)
    {
        if (!read_back || observed != host_gs) restore_or_abort(host_gs);
        saved_gs.erase(this);
        return false;
    }
    return true;
}

void VerifiedGsSegmentSwitch::leave() noexcept
{
    const auto frame = saved_gs.find(this);
    if (frame == saved_gs.end()) return;
    restore_or_abort(frame->second);
    saved_gs.erase(frame);
}

#if defined(__PROSPERO__)
GsBaseOperations ps5_sysarch_gs_operations() noexcept
{
    return {ps5_read_gs, ps5_write_gs, nullptr};
}
#endif

} // namespace cap13
