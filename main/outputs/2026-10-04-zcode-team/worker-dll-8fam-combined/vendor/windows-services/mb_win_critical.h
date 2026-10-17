#ifndef MB_WIN_CRITICAL_H
#define MB_WIN_CRITICAL_H
#include "mb_win_threads.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mb_win_critical_storage { uint64_t opaque[5]; } mb_win_critical_storage;
typedef struct mb_win_critical { void *state; } mb_win_critical;
typedef struct mb_win_critical_pin {mb_win_critical *owner;struct mb_win_critical_pin *next;} mb_win_critical_pin;
typedef enum mb_win_critical_status {
    MB_WIN_CRITICAL_OK=0,MB_WIN_CRITICAL_INVALID_ARGUMENT,MB_WIN_CRITICAL_BAD_STATE,
    MB_WIN_CRITICAL_BUSY,MB_WIN_CRITICAL_LIMIT,MB_WIN_CRITICAL_NATIVE_ERROR
} mb_win_critical_status;
/* Native sidecar service; public Windows storage is exactly40 bytes/alignment8.
 * No native pthread representation or Windows internal field values are exposed.
 * Stationary zeroed owner. Open preallocates bounded capacity and pins the domain;
 * native sysconf determines single-processor spin-zero policy, with no fallback.
 * Ordinary methods synchronize internally. Close requires no new external calls;
 * it refuses service pins, holders, spinning/waiting calls and retains native
 * resources on cleanup failure. It may retire idle sidecars without dereferencing
 * their stored Windows addresses, even after their data mapping has retired.
 * Do not copy/move/modify initialized objects; delete before reinitialization.
 * Object storage must remain writable/alive until deleted and all users quiesce.
 * Deleting a held/waited object, wrong-thread Leave, thread exit/cancellation
 * while held, native cancellation, exception or longjmp across calls is invalid.
 * Native checked methods return errors without pretending to synchronize.
 * The Windows void APIs fail-stop on these errors; no SEH emulation is supplied.
 * Actual spin/recheck iterations precede a native condition wait. There is no
 * FIFO or fairness guarantee. TryEnter never waits for critical-section ownership
 * (brief native metadata serialization is still required).
 */
mb_win_critical_status mb_win_critical_open(mb_win_critical *,mb_win_thread_domain *,size_t capacity,int *native_error);
mb_win_critical_status mb_win_critical_close(mb_win_critical *,int *native_error);
mb_win_critical_status mb_win_critical_acquire(mb_win_critical *,mb_win_critical_pin *);
mb_win_critical_status mb_win_critical_release_pin(mb_win_critical_pin *);
mb_win_thread_domain *mb_win_critical_domain(mb_win_critical *);
mb_win_critical_status mb_win_critical_initialize(mb_win_critical *,void *object,uint32_t spin);
mb_win_critical_status mb_win_critical_enter(mb_win_critical *,void *object);
mb_win_critical_status mb_win_critical_try_enter(mb_win_critical *,void *object,int32_t *entered);
mb_win_critical_status mb_win_critical_leave(mb_win_critical *,void *object);
mb_win_critical_status mb_win_critical_delete(mb_win_critical *,void *object);
mb_win_critical_status mb_win_critical_set_spin(mb_win_critical *,void *object,uint32_t spin,uint32_t *previous);
#ifdef __cplusplus
}
#endif
#endif
