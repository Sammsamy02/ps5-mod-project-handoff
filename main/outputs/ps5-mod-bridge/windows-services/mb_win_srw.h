#ifndef MB_WIN_SRW_H
#define MB_WIN_SRW_H
#include "mb_win_threads.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mb_win_srwlock { uint64_t opaque; } mb_win_srwlock;
typedef struct mb_win_condition { uint64_t opaque; } mb_win_condition;
typedef struct mb_win_srw { void *state; } mb_win_srw;
typedef struct mb_win_srw_pin { mb_win_srw *owner; struct mb_win_srw_pin *next; } mb_win_srw_pin;
typedef struct mb_win_srw_config { mb_win_thread_domain *domain; size_t capacity; } mb_win_srw_config;
typedef enum mb_win_srw_status {
    MB_WIN_SRW_OK=0, MB_WIN_SRW_INVALID_ARGUMENT, MB_WIN_SRW_BAD_STATE,
    MB_WIN_SRW_BUSY, MB_WIN_SRW_NATIVE_ERROR
} mb_win_srw_status;
/* Zeroed stationary owner; capacity bounds simultaneously active SRW/CV
 * sidecars (not the number of public objects ever initialized). Controls and
 * final close require externally quiescent API calls. Ordinary operations
 * synchronize internally. Domain/code outlive owner, sessions and calls.
 * Owner pins the domain; close refuses service pins, holders or waiters.
 * Every calling native thread must already be admitted to the domain.
 * Public objects are exactly eight bytes/aligned eight, initially all zero;
 * no native pthread object is embedded in Windows storage. Objects and their
 * storage remain stationary/alive through all holders, sleepers and reacquire.
 * Idle objects return to zero and can be forgotten/copied/reused. Initialize
 * and external writes/copies require complete object quiescence. Active-token
 * copies, forged tokens, invalid pointers, wrong-thread/mode release, recursion
 * and upgrades are unsupported contract violations. No FIFO/fairness promise.
 * No cancellation/pthread_exit, longjmp or exception may escape an operation.
 * Void/try APIs fail-stop on contract, capacity, allocation or native failures;
 * try contention alone returns zero. Condition sleep reports unsupported flags
 * as 87, resource failure as 8, native failure as 31; these are adapter policy.
 * Timeout returns zero with Windows error 1460, after reacquiring the same
 * mode. Flags must be 0 exclusive or 1 shared; UINT32_MAX means infinite.
 * Success/try contention preserve LastError. Spurious/stolen wakes require
 * caller predicate loops. The caller owns the lock before sleep, including
 * zero timeout; failure never leaves a previously held lock released.
 * Idle sidecars retire during the final operation, not owner close, so close
 * never touches forgotten public storage. Not process-shared synchronization.
 */
mb_win_srw_status mb_win_srw_open(mb_win_srw *,const mb_win_srw_config *,int *);
mb_win_srw_status mb_win_srw_close(mb_win_srw *,int *);
mb_win_srw_status mb_win_srw_acquire_pin(mb_win_srw *,mb_win_srw_pin *);
mb_win_srw_status mb_win_srw_release_pin(mb_win_srw_pin *);
mb_win_thread_domain *mb_win_srw_domain(mb_win_srw *);
void mb_win_srw_initialize(mb_win_srw *,mb_win_srwlock *);
void mb_win_srw_acquire(mb_win_srw *,mb_win_srwlock *,uint32_t shared);
uint8_t mb_win_srw_try_acquire(mb_win_srw *,mb_win_srwlock *,uint32_t shared);
void mb_win_srw_release(mb_win_srw *,mb_win_srwlock *,uint32_t shared);
void mb_win_srw_condition_initialize(mb_win_srw *,mb_win_condition *);
int32_t mb_win_srw_sleep(mb_win_srw *,mb_win_condition *,mb_win_srwlock *,uint32_t milliseconds,uint32_t flags);
void mb_win_srw_wake(mb_win_srw *,mb_win_condition *,uint32_t all);
#ifdef __cplusplus
}
#endif
#endif
