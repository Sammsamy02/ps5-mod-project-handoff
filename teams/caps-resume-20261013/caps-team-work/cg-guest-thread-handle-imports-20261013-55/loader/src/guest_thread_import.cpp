#include "guest_thread_manager.h"
#include "win_api_shim.h"

namespace cap13 {

bool WinApiShim::set_guest_thread_manager(GuestThreadManager* manager)
{
    if (!manager) return false;
    return set_guest_thread_adapter(
        manager,
        [](void* user, void* attributes, size_t stack_size, void* start,
           void* argument, uint32_t flags, uint32_t* out_id) -> uint64_t {
            const auto result = static_cast<GuestThreadManager*>(user)->create(
                start, argument, attributes, stack_size, flags);
            if (result.status != GuestThreadCreateStatus::Ok) return 0;
            if (out_id) *out_id = result.thread_id;
            return result.handle;
        },
        [](void* user, uint64_t handle, uint32_t ms) -> uint32_t {
            const auto result = static_cast<GuestThreadManager*>(user)->wait(
                handle, ms);
            switch (result)
            {
                case GuestThreadWaitStatus::Object0: return 0;
                case GuestThreadWaitStatus::Timeout: return 0x102;
                case GuestThreadWaitStatus::WouldDeadlock: return 0xFFFFFFFEu;
                case GuestThreadWaitStatus::InvalidHandle: return 0xFFFFFFFFu;
            }
            return 0xFFFFFFFFu;
        },
        [](void* user, uint64_t handle, uint32_t* out_code) -> bool {
            const auto result = static_cast<GuestThreadManager*>(user)->query(
                handle);
            if (!result.valid || !out_code ||
                (result.finished && result.run.status !=
                                        GuestThreadRunStatus::Ok))
                return false;
            *out_code = result.finished ? result.run.exit_code : 259u;
            return true;
        },
        [](void* user, uint64_t handle) -> bool {
            return static_cast<GuestThreadManager*>(user)->close(handle);
        });
}

} // namespace cap13
