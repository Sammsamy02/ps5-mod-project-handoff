#ifndef CAP13_VERIFIED_GS_SEGMENT_SWITCH_H
#define CAP13_VERIFIED_GS_SEGMENT_SWITCH_H

#include "guest_teb_lifecycle.h"

namespace cap13 {

struct GsBaseOperations
{
    bool (*read)(void* context, void** value) noexcept = nullptr;
    bool (*write)(void* context, void* value) noexcept = nullptr;
    void* context = nullptr;
};

// A physical GS transition. Pair each successful enter with leave on the
// same thread. Use ReentrantGuestSegmentSwitch around this object when guest
// DLL entry can nest. The caller must keep the backend and operations alive.
// If restoring host GS cannot be verified, the process stops before host code
// can continue in an unknown segment state.
class VerifiedGsSegmentSwitch final : public GuestSegmentSwitch
{
  public:
    explicit VerifiedGsSegmentSwitch(GsBaseOperations operations)
        : operations_(operations) {}
    bool enter(void* guest_teb) override;
    void leave() noexcept override;

  private:
    void restore_or_abort(void* host_gs) noexcept;
    GsBaseOperations operations_;
};

#if defined(__PROSPERO__)
// Compile-time PS5 binding. Its runtime behavior still requires a console
// round trip before a caller can admit real DLL entry.
GsBaseOperations ps5_sysarch_gs_operations() noexcept;
#endif

} // namespace cap13

#endif
