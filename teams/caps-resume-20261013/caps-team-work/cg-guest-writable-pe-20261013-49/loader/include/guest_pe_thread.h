// Owned guest worker entry on a caller-created host thread. CreateThread
// publication and Windows thread-handle semantics are separate gates.
#ifndef CAP13_GUEST_PE_THREAD_H
#define CAP13_GUEST_PE_THREAD_H

#include "guest_pe_batch.h"

#include <cstdint>

namespace cap13 {

enum class GuestThreadRunStatus
{
    Ok,
    WrongState,
    InvalidStart,
    AttachFailed,
    StartFailed,
    DetachFailed
};

struct GuestThreadRunResult
{
    GuestThreadRunStatus status = GuestThreadRunStatus::WrongState;
    uint32_t exit_code = 0;
};

// Call on the new host worker thread. The caller retains and serializes the
// attached batch, bank, and segment backends until this returns and joins
// the worker before unloading any module. A start address must be inside an
// attached module's validated executable range. Notifications follow DLL
// attach order and reverse detach order. DetachFailed means cleanup did not
// complete; the caller must treat the batch as dirty and retain it.
GuestThreadRunResult pe_run_guest_thread(GuestPeBatchResult& batch,
                                         StaticTlsBank& bank, void* start,
                                         void* argument);

} // namespace cap13

#endif
