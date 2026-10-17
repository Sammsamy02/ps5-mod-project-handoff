#ifndef MB_WIN_INIT_ONCE_H
#define MB_WIN_INIT_ONCE_H
/* VENDORED ADAPTED COPY (origin: pattern of outputs/ps5-mod-bridge/
 * windows-services/mb_win_srw.h; that original file is unchanged). */
#include "mb_win_threads.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Microsoft documents INIT_ONCE as RTL_RUN_ONCE { PVOID Ptr; } and both
 * InitOnceBeginInitialize/InitOnceComplete as returning 32-bit BOOL with a
 * PBOOL fPending out and pointer-sized context, unlike the 8-bit BOOLEAN
 * TryAcquireSRWLock precedent in this codebase. */
typedef struct mb_win_once { uint64_t opaque; } mb_win_once;
typedef struct mb_win_init_once { void *state; } mb_win_init_once;
typedef struct mb_win_init_once_pin { mb_win_init_once *owner; struct mb_win_init_once_pin *next; } mb_win_init_once_pin;
typedef struct mb_win_init_once_config { mb_win_thread_domain *domain; size_t capacity; } mb_win_init_once_config;
typedef enum mb_win_init_once_status {
    MB_WIN_INIT_ONCE_OK=0, MB_WIN_INIT_ONCE_INVALID_ARGUMENT, MB_WIN_INIT_ONCE_BAD_STATE,
    MB_WIN_INIT_ONCE_BUSY, MB_WIN_INIT_ONCE_NATIVE_ERROR
} mb_win_init_once_status;
/* Microsoft-documented flag values (winuser/winbase headers). */
#define MB_WIN_INIT_ONCE_CHECK_ONLY UINT32_C(0x1)
#define MB_WIN_INIT_ONCE_ASYNC UINT32_C(0x2)
#define MB_WIN_INIT_ONCE_INIT_FAILED UINT32_C(0x4)
/* Documented Windows last-error codes used by this adapter. */
#define MB_WIN_ERROR_IO_PENDING UINT32_C(997)
#define MB_WIN_ERROR_ALREADY_EXISTS UINT32_C(183)
#define MB_WIN_ERROR_GEN_FAILURE UINT32_C(31)
/* Zeroed stationary owner; capacity bounds simultaneously retained sidecars
 * (in-progress AND completed: a completed object must remember its context,
 * so it holds capacity until owner close). Controls and final close require
 * externally quiescent API calls; ordinary begin/complete operations
 * synchronize internally. Domain/code outlive owner, sessions and calls.
 * Owner pins the domain; close refuses service pins, active initialization
 * rights or blocked waiters. Every calling native thread must already be
 * admitted to the domain. Public objects are exactly eight bytes/aligned
 * eight, initially all zero; no native pthread object is embedded in
 * Windows storage. Objects remain stationary/alive through all holders and
 * waiters. A failed initialization with no waiters retires the sidecar and
 * returns public storage to zero for reuse, exactly like an idle SRW object;
 * a completed object retains a nonzero token until the caller re-zeroes it
 * (a Windows application must also re-zero INIT_ONCE to restart it).
 * Forged tokens, invalid pointers and storage reuse without re-zeroing are
 * unsupported contract violations. Begin may block without timeout (there
 * is no timed InitOnce API); waiters are released FIFO by completion or
 * failure handoff. CHECK_ONLY asks whether initialization completed: on a
 * completed object it returns success with pending 0 and the stored
 * context; otherwise it fails with last error 997 ERROR_IO_PENDING as
 * Microsoft documents. Success returns pending 1 and grants exactly one
 * exclusive initialization right, except ASYNC grants every asker
 * immediately. INIT_FAILED hands the right to the first queued waiter or,
 * with none, resets the object to never-begun; its context argument is
 * ignored and it must not be combined with ASYNC. The first ASYNC complete
 * wins and stores the context; later right holders fail with 183
 * ERROR_ALREADY_EXISTS. Complete without the right, duplicate complete and
 * complete on a never-begun object fail; capacity/token/allocation
 * exhaustion and native failures fail explicitly with 8/31/87. Unknown
 * flag bits fail with 87. Success preserves LastError; only failures write
 * it. These numeric failure codes follow Microsoft where documented and
 * are adapter policy elsewhere. Success/try APIs never block the caller
 * out of failure: unimplemented paths fail, never fake success. No
 * cancellation/pthread_exit, longjmp or exception may escape an operation.
 * Not process-shared initialization. Output pointers are optional; failed
 * calls never write them.
 */
mb_win_init_once_status mb_win_init_once_open(mb_win_init_once *,const mb_win_init_once_config *,int *);
mb_win_init_once_status mb_win_init_once_close(mb_win_init_once *,int *);
mb_win_init_once_status mb_win_init_once_acquire_pin(mb_win_init_once *,mb_win_init_once_pin *);
mb_win_init_once_status mb_win_init_once_release_pin(mb_win_init_once_pin *);
mb_win_thread_domain *mb_win_init_once_domain(mb_win_init_once *);
int32_t mb_win_init_once_begin(mb_win_init_once *,mb_win_once *,uint32_t flags,int32_t *pending,void **context);
int32_t mb_win_init_once_complete(mb_win_init_once *,mb_win_once *,uint32_t flags,void *context);
#ifdef __cplusplus
}
#endif
#endif
