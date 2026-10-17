// Host-owned thread objects for attached guest PE batches. Import publication
// is a separate shim decision after guest pointer and loader-lock gates.
#ifndef CAP13_GUEST_THREAD_MANAGER_H
#define CAP13_GUEST_THREAD_MANAGER_H

#include "guest_pe_thread.h"

#include <cstdint>
#include <memory>

namespace cap13 {

enum class GuestThreadCreateStatus
{
    Ok, WrongState, InvalidStart, UnsupportedOptions, Exhausted, StartFailed
};
struct GuestThreadCreateResult
{
    GuestThreadCreateStatus status = GuestThreadCreateStatus::WrongState;
    uint64_t handle = 0;
    uint32_t thread_id = 0;
};
enum class GuestThreadWaitStatus { Object0, Timeout, InvalidHandle, WouldDeadlock };
struct GuestThreadQueryResult
{
    bool valid = false;
    bool finished = false;
    GuestThreadRunResult run{};
};

class GuestThreadManager
{
  public:
    using Runner = GuestThreadRunResult (*)(GuestPeBatchResult&, StaticTlsBank&,
                                             void*, void*);
    explicit GuestThreadManager(GuestPeBatchResult& batch, StaticTlsBank& bank,
                                Runner runner = pe_run_guest_thread);
    // Construct before process attach so DllMain-created workers can hold
    // handles but cannot enter guest code until activate() sees a stable,
    // fully attached batch. The batch must outlive all workers and quiesce.
    explicit GuestThreadManager(StaticTlsBank& bank,
                                Runner runner = pe_run_guest_thread);
    ~GuestThreadManager();
    GuestThreadManager(const GuestThreadManager&) = delete;
    GuestThreadManager& operator=(const GuestThreadManager&) = delete;

    // This stage supports an immediate start with default attributes and
    // stack. Other options are refused before any worker is created.
    GuestThreadCreateResult create(void* start, void* argument,
                                   void* attributes = nullptr,
                                   uint64_t stack_size = 0,
                                   uint32_t flags = 0);
    // Call after the attached batch has reached its stable output object.
    // A failed attach should call abort_pending() instead. Activation is
    // one-shot; invalid queued starts complete with InvalidStart.
    bool activate(GuestPeBatchResult& batch);
    bool abort_pending();
    GuestThreadWaitStatus wait(uint64_t handle, uint32_t milliseconds);
    GuestThreadQueryResult query(uint64_t handle);
    bool close(uint64_t handle);
    // Stops admission and joins every worker, including closed handles.
    // Must precede batch detach. Calling from an owned worker is refused.
    bool quiesce_all();
    // Includes failures from workers whose handles were already closed.
    // Check after quiesce before deciding whether the batch can be detached.
    bool had_worker_failure();

  private:
    struct Impl;
    std::unique_ptr<Impl> m_;
};

} // namespace cap13
#endif
