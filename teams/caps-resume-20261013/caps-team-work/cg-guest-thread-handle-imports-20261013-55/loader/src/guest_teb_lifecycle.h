// Offline guest-TEB layout and Win64 DLL notification seam.
// The segment backend must establish guest GS before any guest instruction.
#ifndef CAP13_GUEST_TEB_LIFECYCLE_H
#define CAP13_GUEST_TEB_LIFECYCLE_H

#include "static_tls_bank.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

namespace cap13 {

class GuestTebView
{
  public:
    explicit GuestTebView(StaticTlsBank& bank);
    GuestTebView(const GuestTebView&) = delete;
    GuestTebView& operator=(const GuestTebView&) = delete;

    // Refresh before guest entry, including a nested DLL attach. An old
    // array view remains alive if guest code cached its address; a new TLS
    // registration publishes a new view through the same TEB prefix.
    const StaticTlsBank::ThreadArray& refresh();
    void* address() noexcept;
    void* const* tls_array() const noexcept;
    void* self() noexcept;
    uint8_t* block(uint32_t index) const;
    bool belongs_to(const StaticTlsBank& bank) const noexcept;
    bool on_owner_thread() const noexcept;

  private:
    StaticTlsBank& bank_;
    std::thread::id owner_;
    StaticTlsBank::ThreadArray array_;
    std::vector<StaticTlsBank::ThreadArray> retired_arrays_;
    bool array_initialized_ = false;
    alignas(16) std::array<uint8_t, 0x70> prefix_{};
};

// This interface deliberately has no PS5 implementation. Refusing enter
// prevents lifecycle code from invoking a PE with an unpublished GS view.
class GuestSegmentSwitch
{
  public:
    virtual ~GuestSegmentSwitch() = default;
    virtual bool enter(void* guest_teb) = 0;
    virtual void leave() noexcept = 0;
};

class UnavailableSegmentSwitch final : public GuestSegmentSwitch
{
  public:
    bool enter(void*) override { return false; }
    void leave() noexcept override {}
};

// Share one instance across a root PE and its dynamic dependencies. The
// backend sees one transition for a same-TEB nested guest call. A backend
// returning false from enter must leave the segment unchanged; a throwing
// backend is asked to restore it. The wrapper and backend must outlive all
// balanced entries on every participating thread.
class ReentrantGuestSegmentSwitch final : public GuestSegmentSwitch
{
  public:
    explicit ReentrantGuestSegmentSwitch(GuestSegmentSwitch& backend)
        : backend_(backend) {}
    bool enter(void* guest_teb) override;
    void leave() noexcept override;

  private:
    GuestSegmentSwitch& backend_;
};

enum class GuestNotifyStatus
{
    Ok,
    WrongState,
    WrongThreadOrBank,
    TlsPreparationFailed,
    BackendRefused,
    PolicyRegistrationFailed,
    ModuleRegistrationFailed,
    EntryReturnedFalse
};

class GuestModuleLifecycle
{
  public:
    // image_base and executable addresses must come from a validated,
    // retained mapped image. The caller serializes module notifications.
    GuestModuleLifecycle(StaticTlsBank& bank, GuestSegmentSwitch& segment,
                         uint64_t image_base, uint64_t entry,
                         std::vector<uint64_t> tls_callbacks);

    GuestNotifyStatus process_attach(GuestTebView& teb);
    GuestNotifyStatus thread_attach(GuestTebView& teb);
    GuestNotifyStatus thread_detach(GuestTebView& teb);
    GuestNotifyStatus process_detach(GuestTebView& teb);
    bool disable_thread_notifications() noexcept;

  private:
    GuestNotifyStatus notify(GuestTebView& teb, uint64_t reason);
    StaticTlsBank& bank_;
    GuestSegmentSwitch& segment_;
    uint64_t image_base_;
    uint64_t entry_;
    std::vector<uint64_t> callbacks_;
    std::atomic<bool> attached_{false};
    std::atomic<bool> thread_notifications_disabled_{false};
};

} // namespace cap13
#endif
