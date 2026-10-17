#ifndef MB_WIN_THREADS_H
#define MB_WIN_THREADS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mb_win_thread_domain { void *state; } mb_win_thread_domain;
typedef struct mb_win_thread_pin {
    mb_win_thread_domain *owner;
    struct mb_win_thread_pin *next;
} mb_win_thread_pin;
#define MB_WIN_TLS_CAPACITY UINT32_C(1088)
#define MB_WIN_TLS_MINIMUM_AVAILABLE UINT32_C(64)
#define MB_WIN_TLS_OUT_OF_INDEXES UINT32_MAX
#define MB_WIN_ERROR_SUCCESS UINT32_C(0)
#define MB_WIN_ERROR_NOT_ENOUGH_MEMORY UINT32_C(8)
#define MB_WIN_ERROR_INVALID_PARAMETER UINT32_C(87)
typedef enum mb_win_thread_status {
    MB_WIN_THREADS_OK = 0, MB_WIN_THREADS_INVALID_ARGUMENT,
    MB_WIN_THREADS_BAD_STATE, MB_WIN_THREADS_BUSY, MB_WIN_THREADS_NATIVE_ERROR
} mb_win_thread_status;

/* Native-C pthread-backed errno, last-error and dynamic Windows TLS storage.
 * Zero-initialize and keep the domain stationary. Its state is private.
 * Open/close are externally serialized with all entry/exit/control operations;
 * workers must be joined and calls quiescent before final close. Enter/leave
 * may run concurrently on distinct native threads. Enter is idempotent, not a
 * nesting counter: one leave invalidates the slot even after repeated enters.
 * Enter must succeed before
 * that thread invokes Windows code; leave only after its Windows calls and
 * borrowed errno pointer use end. Native thread exit also releases registration.
 * Close refuses while threads, service pins or allocated TLS indices remain,
 * and retains ownership on
 * native cleanup failure. The domain and this code outlive heap owners, bridge
 * sessions, callbacks and worker/destructor execution. Outputs are independent.
 * errno() never allocates; an unregistered thread gets NULL, so admission must
 * happen outside the Windows call. Native host errno is a separate object.
 * Lock/unlock callbacks require a live domain and valid native mutex operations;
 * violation terminates instead of silently proceeding without serialization.
 */
mb_win_thread_status mb_win_threads_open(mb_win_thread_domain *, int *native_error);
mb_win_thread_status mb_win_threads_enter(mb_win_thread_domain *, int *native_error);
mb_win_thread_status mb_win_threads_leave(mb_win_thread_domain *, int *native_error);
mb_win_thread_status mb_win_threads_close(mb_win_thread_domain *, int *native_error);
int32_t *mb_win_threads_errno(void *domain);

/* Service pins are caller-owned zeroed stationary tokens. Acquire/release take
 * the domain mutex internally; serialize them with domain open/close. A bridge
 * pins the domain before publishing imports and retains the pin until consumers
 * and all calls retire. A pin keeps the domain alive even with no threads.
 *
 * New last-error/TLS APIs require an open domain and an admitted current thread;
 * violating that contract aborts. errno() retains its older NULL-on-unregistered
 * contract. errno/last_error pointer lookup does not lock (heap callbacks may
 * already hold the mutex); each points to separate per-thread 32-bit state.
 * Borrowed pointers remain valid only until the thread leaves/exits.
 *
 * Dynamic TLS has a bounded capacity of 1088 indices in this domain. Allocation
 * selects the lowest available index and resets that slot to NULL on all live
 * threads; new admitted threads start with NULL slots. Get/Set accept indices
 * 0..63 even when not allocated, following the Windows minimum-index guarantee;
 * higher indices must be allocated. Reuse never exposes a previous value.
 * Free releases only an allocated index; it never destroys caller pointees.
 * Get clears the current thread's last-error to zero on every successful access,
 * including a NULL result. Successful Alloc/Free/Set preserve last-error.
 * Failure policy: exhaustion -> OUT_OF_INDEXES and last-error 8; invalid index
 * -> NULL/zero and last-error 87. These numeric failure codes are this adapter's
 * policy, not claims that every Windows version uses those codes.
 * TLS operations lock internally: do not hold mb_win_threads_lock around them.
 * No lazy allocation during Get/Set, PE static TLS, FLS callbacks, Windows thread
 * creation, loader notifications or exception handling is supplied.
 */
mb_win_thread_status mb_win_threads_acquire(mb_win_thread_domain *, mb_win_thread_pin *);
mb_win_thread_status mb_win_threads_release_pin(mb_win_thread_pin *);
uint32_t *mb_win_threads_last_error(void *domain);
uint32_t mb_win_threads_tls_alloc(void *domain);
int32_t mb_win_threads_tls_free(void *domain, uint32_t index);
void *mb_win_threads_tls_get(void *domain, uint32_t index);
int32_t mb_win_threads_tls_set(void *domain, uint32_t index, void *value);
void mb_win_threads_lock(void *domain);
void mb_win_threads_unlock(void *domain);
void *mb_win_threads_allocate(void *domain, size_t size);
void mb_win_threads_release(void *domain, void *allocation);
#ifdef __cplusplus
}
#endif
#endif
