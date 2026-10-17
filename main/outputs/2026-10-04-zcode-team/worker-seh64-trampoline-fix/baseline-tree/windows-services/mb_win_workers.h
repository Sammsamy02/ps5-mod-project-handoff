#ifndef MB_WIN_WORKERS_H
#define MB_WIN_WORKERS_H
#include "mb_win_threads.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MB_WIN_WAIT_OBJECT_0 UINT32_C(0)
#define MB_WIN_WAIT_TIMEOUT UINT32_C(258)
#define MB_WIN_WAIT_FAILED UINT32_MAX
#define MB_WIN_INFINITE UINT32_MAX
#define MB_WIN_STILL_ACTIVE UINT32_C(259)
typedef struct mb_win_workers { void *state; } mb_win_workers;
typedef struct mb_win_worker_pin {
    mb_win_workers *owner;
    struct mb_win_worker_pin *next;
} mb_win_worker_pin;
typedef struct mb_win_worker_config {
    mb_win_thread_domain *domain;
    void *context;
    int (*acquire_code)(void *context, void *start_address, void **token);
    int (*release_code)(void *context, void *token);
    size_t capacity;
} mb_win_worker_config;
typedef enum mb_win_worker_status {
    MB_WIN_WORKERS_OK=0, MB_WIN_WORKERS_INVALID_ARGUMENT,
    MB_WIN_WORKERS_BAD_STATE, MB_WIN_WORKERS_BUSY,
    MB_WIN_WORKERS_NATIVE_ERROR, MB_WIN_WORKERS_CODE_ERROR
} mb_win_worker_status;
/* Stationary zeroed native owner. Open/close and final bridge retirement require
 * quiescent external calls; workers themselves are counted and prevent close.
 * All ordinary methods synchronize internally. Config/domain/callback code
 * outlive close; callbacks return zero on success. Acquire writes a non-NULL
 * stationary code token only on success, and pins an ACTIVE executable owner.
 * Release failure retains exactly that token for collect retry. Callbacks run
 * without our mutex; they must serialize their own PE owner and not reenter us.
 * collect joins only completed workers, then releases code. Close is BUSY while
 * any handle, worker, failed cleanup token, admission or service pin remains.
 * No native cancellation, pthread_exit, exception or longjmp may escape a start.
 * Runtime code stays loaded until close; collect/close cannot run on a worker
 * being collected. Concurrent close of a handle during wait is unsupported.
 */
mb_win_worker_status mb_win_workers_open(mb_win_workers *,const mb_win_worker_config *,int *);
mb_win_worker_status mb_win_workers_collect(mb_win_workers *,int *);
mb_win_worker_status mb_win_workers_close(mb_win_workers *,int *);
mb_win_worker_status mb_win_workers_acquire(mb_win_workers *,mb_win_worker_pin *);
mb_win_worker_status mb_win_workers_release_pin(mb_win_worker_pin *);
mb_win_thread_domain *mb_win_workers_domain(mb_win_workers *);
/* Current native thread must already be admitted to the domain. The native
 * caller enables creation only around ACTIVE exported work, never DllMain.
 * Every entry callback, including nested LoadLibrary entry, must suspend then
 * restore this depth. Created workers are admitted automatically. Restore is
 * paired on the same thread; all admission ends before owner close.
 */
mb_win_worker_status mb_win_workers_enter_call(mb_win_workers *);
mb_win_worker_status mb_win_workers_leave_call(mb_win_workers *);
uint32_t mb_win_workers_suspend_calls(mb_win_workers *);
void mb_win_workers_restore_calls(mb_win_workers *,uint32_t);
/* Windows-semantics native helpers, requiring an admitted domain thread.
 * Only attrs=NULL, stack_size=0, flags=0 are supported. HANDLE is an opaque
 * monotonic identity, never dereferenced or reused (including owner reopen).
 * IDs are separate nonzero generated DWORD values, never native pthread_t.
 * Success preserves LastError; failure sets explicit adapter policy 6 invalid
 * handle, 8 resource/startup failure, 87 bad argument, 50 unsupported option,
 * 170 creation not admitted, 31 native error. Outputs stay unchanged on failure.
 * Zero/finite/infinite waits are supported only for this owner's thread handles;
 * finite waits use monotonic elapsed time and bounded native sleep polling.
 * Sleep(0) yields; finite Sleep retries EINTR. Infinite Sleep never returns.
 * Unexpected native Sleep failure is fail-stop, since Windows Sleep is void.
 * No static PE TLS, DLL_THREAD_ATTACH/DETACH, Windows cancellation, events,
 * priority, security attributes, or nondefault stack management is provided.
 */
void *mb_win_workers_create(mb_win_workers *,void *attrs,size_t stack_size,
                           void *start,void *arg,uint32_t flags,uint32_t *id);
uint32_t mb_win_workers_wait(mb_win_workers *,void *handle,uint32_t milliseconds);
int32_t mb_win_workers_exit_code(mb_win_workers *,void *handle,uint32_t *code);
int32_t mb_win_workers_close_handle(mb_win_workers *,void *handle);
void mb_win_workers_sleep(mb_win_workers *,uint32_t milliseconds);
/* Native-to-Windows typed call implemented by the separate ABI translation unit. */
uint32_t mb_win_workers_invoke(void *start,void *argument);
#ifdef __cplusplus
}
#endif
#endif
