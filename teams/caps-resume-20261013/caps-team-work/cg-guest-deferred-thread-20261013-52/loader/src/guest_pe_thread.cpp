#include "guest_pe_thread.h"

namespace cap13 {

GuestThreadRunResult pe_run_guest_thread(GuestPeBatchResult& batch,
                                         StaticTlsBank& bank, void* start,
                                         void* argument)
{
    GuestThreadRunResult result{};
    if (!batch.ok || batch.modules.empty()) return result;
    GuestPeModule* owner = nullptr;
    for (const auto& module : batch.modules)
    {
        if (!module || !module->mapped_handle()) return result;
        if (module->owns_executable_address(start))
        {
            if (owner)
            {
                result.status = GuestThreadRunStatus::InvalidStart;
                return result;
            }
            owner = module.get();
        }
    }
    if (!owner)
    {
        result.status = GuestThreadRunStatus::InvalidStart;
        return result;
    }

    GuestTebView teb(bank);
    size_t notified = 0;
    result.status = GuestThreadRunStatus::Ok;
    for (const auto& module : batch.modules)
    {
        GuestNotifyStatus status = GuestNotifyStatus::WrongState;
        try { status = module->thread_attach(teb); }
        catch (...) { status = GuestNotifyStatus::WrongState; }
        if (status != GuestNotifyStatus::Ok)
        {
            result.status = GuestThreadRunStatus::AttachFailed;
            break;
        }
        ++notified;
    }

    if (result.status == GuestThreadRunStatus::Ok)
    {
        GuestCallStatus status = GuestCallStatus::WrongState;
        try
        {
            status = owner->invoke_thread_start(start, argument, teb,
                                                 &result.exit_code);
        }
        catch (...) { status = GuestCallStatus::WrongState; }
        if (status != GuestCallStatus::Ok)
            result.status = GuestThreadRunStatus::StartFailed;
    }

    bool detach_failed = false;
    while (notified)
    {
        GuestNotifyStatus status = GuestNotifyStatus::WrongState;
        try { status = batch.modules[--notified]->thread_detach(teb); }
        catch (...) { status = GuestNotifyStatus::WrongState; }
        if (status != GuestNotifyStatus::Ok) detach_failed = true;
    }
    if (detach_failed) result.status = GuestThreadRunStatus::DetachFailed;
    return result;
}

} // namespace cap13
