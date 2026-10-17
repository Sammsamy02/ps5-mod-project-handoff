/* Host-only dyld interposition. Every operation forwards to real Darwin APIs.
 * Recording uses fixed storage and atomics; symbol lookup/printing happen only
 * after tracing stops. No generated code, protection substitution or Mono API
 * replacement is present.
 */
#include "ExecTrace.h"
#include <dlfcn.h>
#include <errno.h>
#include <sched.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>

enum { MAX_EVENTS = 8192 };
struct Event
{
    uintptr_t caller, address, result;
    size_t length;
    int operation, protection, flags, error, failed;
};
static struct Event events[MAX_EVENTS];
static atomic_int active;
static atomic_uint_fast64_t inflight, count, mmapCount, mprotectCount, execCount, failCount;

static void record(int operation, void* address, size_t length, int protection,
                   int flags, uintptr_t result, int failed, int error, void* caller)
{
    if (operation == 1)
        atomic_fetch_add_explicit(&mmapCount, 1, memory_order_relaxed);
    else
        atomic_fetch_add_explicit(&mprotectCount, 1, memory_order_relaxed);
    if (protection & PROT_EXEC)
        atomic_fetch_add_explicit(&execCount, 1, memory_order_relaxed);
    if (failed)
        atomic_fetch_add_explicit(&failCount, 1, memory_order_relaxed);
    uint64_t index = atomic_fetch_add_explicit(&count, 1, memory_order_relaxed);
    if (index < MAX_EVENTS)
        events[index] = (struct Event){(uintptr_t)caller, (uintptr_t)address, result, length,
                                      operation, protection, flags, error, failed};
}

static void* traced_mmap(void* address, size_t length, int protection, int flags, int fd, off_t offset)
{
    atomic_fetch_add_explicit(&inflight, 1, memory_order_acquire);
    int enabled = atomic_load_explicit(&active, memory_order_acquire);
    void* caller = __builtin_return_address(0);
    // dyld does not apply an interposer to its own original-function imports.
    void* result = mmap(address, length, protection, flags, fd, offset);
    int saved = errno;
    if (enabled)
        record(1, address, length, protection, flags, (uintptr_t)result, result == MAP_FAILED, saved, caller);
    atomic_fetch_sub_explicit(&inflight, 1, memory_order_release);
    errno = saved;
    return result;
}

static int traced_mprotect(void* address, size_t length, int protection)
{
    atomic_fetch_add_explicit(&inflight, 1, memory_order_acquire);
    int enabled = atomic_load_explicit(&active, memory_order_acquire);
    void* caller = __builtin_return_address(0);
    int result = mprotect(address, length, protection);
    int saved = errno;
    if (enabled)
        record(2, address, length, protection, 0, (uintptr_t)(intptr_t)result, result != 0, saved, caller);
    atomic_fetch_sub_explicit(&inflight, 1, memory_order_release);
    errno = saved;
    return result;
}

#define INTERPOSE(replacement, original) \
    __attribute__((used)) static const struct { const void* replacement; const void* original; } \
    pair_##original __attribute__((section("__DATA,__interpose"))) = {(const void*)replacement, (const void*)original}
INTERPOSE(traced_mmap, mmap);
INTERPOSE(traced_mprotect, mprotect);

void cap10_exec_stop(void)
{
    atomic_store_explicit(&active, 0, memory_order_release);
    while (atomic_load_explicit(&inflight, memory_order_acquire))
        sched_yield();
}

void cap10_exec_reset(void)
{
    cap10_exec_stop();
    atomic_store(&count, 0);
    atomic_store(&mmapCount, 0);
    atomic_store(&mprotectCount, 0);
    atomic_store(&execCount, 0);
    atomic_store(&failCount, 0);
}

void cap10_exec_start(void)
{
    atomic_store_explicit(&active, 1, memory_order_release);
}

struct Cap10ExecStats cap10_exec_stats(void)
{
    uint64_t total = atomic_load(&count);
    return (struct Cap10ExecStats){atomic_load(&mmapCount), atomic_load(&mprotectCount),
        atomic_load(&execCount), atomic_load(&failCount), total < MAX_EVENTS ? total : MAX_EVENTS,
        total > MAX_EVENTS ? total - MAX_EVENTS : 0};
}

void cap10_exec_report(void)
{
    struct Cap10ExecStats stats = cap10_exec_stats();
    for (uint64_t index = 0; index < stats.recorded_events; ++index)
    {
        struct Event* event = &events[index];
        Dl_info info = {0};
        int found = dladdr((void*)event->caller, &info);
        printf("EXEC_EVENT index=%llu operation=%s length=%zu protection=%d flags=%d failed=%d errno=%d caller_offset=0x%llx image=%s symbol=%s\n",
               (unsigned long long)index, event->operation == 1 ? "mmap" : "mprotect", event->length,
               event->protection, event->flags, event->failed, event->error,
               (unsigned long long)(found ? event->caller - (uintptr_t)info.dli_fbase : event->caller),
               found && info.dli_fname ? info.dli_fname : "unknown", found && info.dli_sname ? info.dli_sname : "unknown");
    }
    printf("{\"exec_trace\":true,\"mmap_calls\":%llu,\"mprotect_calls\":%llu,\"executable_requests\":%llu,\"failed_requests\":%llu,\"recorded_events\":%llu,\"dropped_events\":%llu}\n",
           (unsigned long long)stats.mmap_calls, (unsigned long long)stats.mprotect_calls,
           (unsigned long long)stats.executable_requests, (unsigned long long)stats.failed_requests,
           (unsigned long long)stats.recorded_events, (unsigned long long)stats.dropped_events);
}
