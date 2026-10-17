#ifndef MB_WIN_ADDRESS_WAIT_H
#define MB_WIN_ADDRESS_WAIT_H
#include "mb_win_threads.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MB_WIN_ADDRESS_WAIT_INFINITE UINT32_MAX
#define MB_WIN_ADDRESS_WAIT_TIMEOUT UINT32_C(1460)
typedef struct mb_win_address_wait {void *state;} mb_win_address_wait;
typedef struct mb_win_address_wait_pin {
    mb_win_address_wait *owner;
    struct mb_win_address_wait_pin *next;
} mb_win_address_wait_pin;
typedef enum mb_win_address_wait_status {
    MB_WIN_ADDRESS_WAIT_OK=0, MB_WIN_ADDRESS_WAIT_INVALID_ARGUMENT,
    MB_WIN_ADDRESS_WAIT_BAD_STATE, MB_WIN_ADDRESS_WAIT_BUSY,
    MB_WIN_ADDRESS_WAIT_NATIVE_ERROR, MB_WIN_ADDRESS_WAIT_OWNER_ERROR
} mb_win_address_wait_status;

/* One process address-wait domain, backed by a native pthread mutex and FIFO
 * address-keyed wait records. Zero-init stationary owner and pins; state is
 * private. Open/close and final pin retirement require externally serialized
 * controls and no new calls entering concurrently. Existing waiters keep close
 * BUSY. All ordinary calls synchronize internally; never hold the thread-domain
 * mutex around them. The owner pins its existing mb_win_thread_domain until
 * successful close, including failed native cleanup retries. Pin copies cannot
 * be released. Every calling thread must already be admitted to that domain.
 * Calls/pins and their code must retire before the owner/domain storage does.
 * Cancellation, pthread_exit, exceptions and longjmp through a wait are outside
 * this contract. Calls may block; do not invoke them while a required waker is
 * blocked by the caller's loader or application lock.
 *
 * Address is a caller-owned naturally aligned unsigned 1/2/4/8-byte scalar.
 * Every concurrent access must use matching-width atomic operations: publish a
 * change with an atomic release store, then wake; readers use atomic acquire
 * loads. This adapter uses matching-width __atomic_load_n, not racing memcpy or
 * volatile C reads. CompareAddress contains immutable bytes for the duration of
 * the call and may be unaligned. Both buffers and the address identity must stay
 * alive until all calls return; do not recycle an address with waiters present.
 * This admitted atomic protocol is narrower than arbitrary Windows machine
 * code and does not validate inaccessible pointers or supply an object owner.
 *
 * Compare, queue insertion and native condition waiting share the owner mutex.
 * WakeSingle marks the oldest unsignaled waiter for the exact pointer; WakeAll
 * marks all of them. A wake may succeed without a value change, so callers must
 * recheck their atomic predicate after TRUE. A wake before any wait is not saved.
 * A different value returns TRUE immediately, including for timeout zero.
 * Finite deadlines use CLOCK_MONOTONIC; UINT32_MAX is infinite. Native spurious
 * condition wakes do not consume a different address's or waiter's signal.
 *
 * BOOL success is 1 and preserves Windows LastError. A timed-out equal wait is
 * 0 with documented ERROR_TIMEOUT1460. Defensive invalid size/null/alignment
 * failures use87; native wait/clock failures use31. Those latter values are
 * adapter policies. Wake functions preserve LastError and never dereference
 * Address; an unmatched address (including NULL) is a no-op. CRT errno and
 * native errno are independent. No PE/static TLS, loader or game facts implied.
 */
mb_win_address_wait_status mb_win_address_wait_open(mb_win_address_wait *,mb_win_thread_domain *,int *native_error);
mb_win_address_wait_status mb_win_address_wait_close(mb_win_address_wait *,int *native_error);
mb_win_address_wait_status mb_win_address_wait_acquire(mb_win_address_wait *,mb_win_address_wait_pin *);
mb_win_address_wait_status mb_win_address_wait_release_pin(mb_win_address_wait_pin *);
int32_t mb_win_address_wait_wait(mb_win_address_wait *,volatile void *,const void *,size_t,uint32_t);
void mb_win_address_wait_wake_single(mb_win_address_wait *,void *);
void mb_win_address_wait_wake_all(mb_win_address_wait *,void *);
#ifdef __cplusplus
}
#endif
#endif
