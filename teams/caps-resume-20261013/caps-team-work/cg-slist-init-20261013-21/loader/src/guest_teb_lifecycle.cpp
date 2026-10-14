#include "guest_teb_lifecycle.h"
#include "win32_last_error_state.h"

#include "win64_bridge.h"

#include <cstring>
#include <new>
#include <stdexcept>

namespace cap13 {

namespace {
constexpr uint64_t DLL_PROCESS_DETACH = 0;
constexpr uint64_t DLL_PROCESS_ATTACH = 1;
constexpr uint64_t DLL_THREAD_ATTACH = 2;
constexpr uint64_t DLL_THREAD_DETACH = 3;
constexpr size_t TEB_SELF_OFFSET = 0x30;
constexpr size_t TEB_TLS_ARRAY_OFFSET = 0x58;

class LeaveOnExit
{
  public:
    explicit LeaveOnExit(GuestSegmentSwitch& segment) : segment_(segment) {}
    ~LeaveOnExit() { segment_.leave(); }
    LeaveOnExit(const LeaveOnExit&) = delete;
    LeaveOnExit& operator=(const LeaveOnExit&) = delete;

  private:
    GuestSegmentSwitch& segment_;
};
} // namespace

GuestTebView::GuestTebView(StaticTlsBank& bank)
    : bank_(bank), owner_(std::this_thread::get_id()) {}

const StaticTlsBank::ThreadArray& GuestTebView::refresh()
{
    if (!on_owner_thread())
        throw std::logic_error("guest TEB refreshed from the wrong host thread");
    array_ = bank_.attach_current_thread();
    const uint64_t self_pointer = reinterpret_cast<uint64_t>(prefix_.data());
    const uint64_t tls_pointer = reinterpret_cast<uint64_t>(array_.slots.data());
    std::memcpy(prefix_.data() + TEB_SELF_OFFSET, &self_pointer, sizeof(self_pointer));
    std::memcpy(prefix_.data() + TEB_TLS_ARRAY_OFFSET, &tls_pointer, sizeof(tls_pointer));
    const uint32_t last_error = win32_get_last_error();
    std::memcpy(prefix_.data() + WIN32_TEB_LAST_ERROR_OFFSET,
                &last_error, sizeof(last_error));
    return array_;
}

void* GuestTebView::address() noexcept { return prefix_.data(); }
void* const* GuestTebView::tls_array() const noexcept { return array_.slots.data(); }
void* GuestTebView::self() noexcept { return prefix_.data(); }
uint8_t* GuestTebView::block(uint32_t index) const
{
    if (!on_owner_thread()) return nullptr;
    return bank_.current_block(index);
}
bool GuestTebView::belongs_to(const StaticTlsBank& bank) const noexcept
{
    return &bank_ == &bank;
}
bool GuestTebView::on_owner_thread() const noexcept
{
    return owner_ == std::this_thread::get_id();
}

GuestModuleLifecycle::GuestModuleLifecycle(StaticTlsBank& bank,
                                           GuestSegmentSwitch& segment,
                                           uint64_t image_base, uint64_t entry,
                                           std::vector<uint64_t> tls_callbacks)
    : bank_(bank), segment_(segment), image_base_(image_base), entry_(entry),
      callbacks_(std::move(tls_callbacks)) {}

GuestNotifyStatus GuestModuleLifecycle::notify(GuestTebView& teb, uint64_t reason)
{
    if (!teb.belongs_to(bank_) || !teb.on_owner_thread())
        return GuestNotifyStatus::WrongThreadOrBank;
    try { (void)teb.refresh(); }
    catch (const std::bad_alloc&) { return GuestNotifyStatus::TlsPreparationFailed; }
    if (!segment_.enter(teb.address())) return GuestNotifyStatus::BackendRefused;
    LeaveOnExit guard(segment_);
    ScopedGuestLastErrorBinding error_binding(teb.address());
    for (uint64_t callback : callbacks_)
        (void)cap13_win64_call3(reinterpret_cast<void*>(callback), image_base_, reason, 0);
    if (entry_)
    {
        const uint64_t result = cap13_win64_call3(
            reinterpret_cast<void*>(entry_), image_base_, reason, 0);
        if (reason == DLL_PROCESS_ATTACH && result == 0)
            return GuestNotifyStatus::EntryReturnedFalse;
    }
    return GuestNotifyStatus::Ok;
}

GuestNotifyStatus GuestModuleLifecycle::process_attach(GuestTebView& teb)
{
    if (attached_) return GuestNotifyStatus::WrongState;
    const GuestNotifyStatus status = notify(teb, DLL_PROCESS_ATTACH);
    if (status == GuestNotifyStatus::Ok) attached_ = true;
    return status;
}

GuestNotifyStatus GuestModuleLifecycle::thread_attach(GuestTebView& teb)
{
    if (!attached_) return GuestNotifyStatus::WrongState;
    return notify(teb, DLL_THREAD_ATTACH);
}

GuestNotifyStatus GuestModuleLifecycle::thread_detach(GuestTebView& teb)
{
    if (!attached_) return GuestNotifyStatus::WrongState;
    return notify(teb, DLL_THREAD_DETACH);
}

GuestNotifyStatus GuestModuleLifecycle::process_detach(GuestTebView& teb)
{
    if (!attached_) return GuestNotifyStatus::WrongState;
    const GuestNotifyStatus status = notify(teb, DLL_PROCESS_DETACH);
    if (status == GuestNotifyStatus::Ok) attached_ = false;
    return status;
}

} // namespace cap13
