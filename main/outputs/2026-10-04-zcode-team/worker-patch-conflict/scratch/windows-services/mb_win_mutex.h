#ifndef MB_WIN_MUTEX_H
#define MB_WIN_MUTEX_H
/* VENDORED ADAPTED COPY (origin: pattern of outputs/ps5-mod-bridge/
 * windows-services/mb_win_init_once.h; that original file is unchanged). */
#include "mb_win_threads.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Microsoft x64 contracts modeled here:
 *  - HANDLE is pointer-sized; mutex handles are opaque nonzero tokens.
 *  - BOOL is 32-bit (ReleaseMutex); wait results are 32-bit DWORD codes.
 *  - CreateMutexW(LPSECURITY_ATTRIBUTES, BOOL bInitialOwner, LPCWSTR),
 *    CreateMutexExW(LPSECURITY_ATTRIBUTES, LPCWSTR, DWORD dwFlags, DWORD
 *    dwDesiredAccess), OpenMutexW(DWORD, BOOL, LPCWSTR), ReleaseMutex(HANDLE),
 *    WaitForSingleObject(HANDLE, DWORD), WaitForSingleObjectEx(HANDLE, DWORD,
 *    BOOL bAlertable).
 * Named-mutex object names follow the Windows object namespace: ASCII
 * case-insensitive folding (adapter policy: only A-Z fold, non-ASCII code
 * units compared verbatim), a private per-owner namespace, optional exact
 * "Local\" prefix accepted and stripped, any other backslash refused. */
#define MB_WIN_MUTEX_INITIAL_OWNER UINT32_C(0x1)
#ifndef MB_WIN_WAIT_OBJECT_0
#define MB_WIN_WAIT_OBJECT_0 UINT32_C(0)
#endif
#define MB_WIN_WAIT_ABANDONED UINT32_C(0x80)
#ifndef MB_WIN_WAIT_TIMEOUT
#define MB_WIN_WAIT_TIMEOUT UINT32_C(0x102)
#endif
#ifndef MB_WIN_WAIT_FAILED
#define MB_WIN_WAIT_FAILED UINT32_C(0xffffffff)
#endif
#ifndef MB_WIN_WAIT_INFINITE
#define MB_WIN_WAIT_INFINITE UINT32_C(0xffffffff)
#endif
#define MB_WIN_MUTEX_MAX_NAME UINT32_C(260)
/* Documented Windows error values used by this adapter. */
#define MB_WIN_ERROR_FILE_NOT_FOUND UINT32_C(2)
#define MB_WIN_ERROR_INVALID_HANDLE UINT32_C(6)
#define MB_WIN_ERROR_NOT_ENOUGH_MEMORY UINT32_C(8)
#define MB_WIN_ERROR_NOT_SUPPORTED UINT32_C(50)
#define MB_WIN_ERROR_INVALID_PARAMETER UINT32_C(87)
#define MB_WIN_ERROR_FILENAME_EXCED_RANGE UINT32_C(206)
#ifndef MB_WIN_ERROR_ALREADY_EXISTS
#define MB_WIN_ERROR_ALREADY_EXISTS UINT32_C(183)
#endif
#define MB_WIN_ERROR_NOT_OWNER UINT32_C(288)
#define MB_WIN_ERROR_POSSIBLE_DEADLOCK UINT32_C(771)
typedef struct mb_win_mutex { void *state; } mb_win_mutex;
typedef struct mb_win_mutex_pin { mb_win_mutex *owner; struct mb_win_mutex_pin *next; } mb_win_mutex_pin;
typedef struct mb_win_mutex_config { mb_win_thread_domain *domain; size_t capacity; } mb_win_mutex_config;
typedef enum mb_win_mutex_status {
    MB_WIN_MUTEX_OK=0, MB_WIN_MUTEX_INVALID_ARGUMENT, MB_WIN_MUTEX_BAD_STATE,
    MB_WIN_MUTEX_BUSY, MB_WIN_MUTEX_NATIVE_ERROR
} mb_win_mutex_status;
/* Zeroed stationary owner; capacity bounds simultaneously live mutex objects
 * (named and unnamed). Public mutex handles are nonzero 64-bit tokens only;
 * all native pthread state (mutex, condition variables, waiter queues, the
 * name table, per-thread owned-object registries) stays in the owner's
 * private sidecars exactly like the SRW/CV/InitOnce families; nothing is
 * embedded in caller storage and tokens are never reused. Windows mutexes are
 * NOT recursive: a second acquire by the owner with a finite timeout follows
 * the real timeout path and returns WAIT_TIMEOUT, and with an infinite
 * timeout this adapter refuses the would-be-guaranteed deadlock with
 * WAIT_FAILED + ERROR_POSSIBLE_DEADLOCK instead of hanging (documented
 * adapter deviation; Windows would deadlock). ReleaseMutex by a non-owner
 * fails with ERROR_NOT_OWNER. Abandonment (owning thread exits without
 * release) is detected through this owner's pthread key destructor and hands
 * WAIT_ABANDONED plus ownership to the first queued waiter (FIFO handoff is
 * adapter policy). WaitForSingleObjectEx with a nonzero alertable flag fails
 * explicitly WAIT_FAILED + ERROR_NOT_SUPPORTED: no APC surface is implemented
 * and none is faked. Owner close refuses service pins, blocked waiters or
 * outstanding handles. CloseHandle participation is NOT bound by this family
 * (the workers family owns the KERNEL32 CloseHandle/WaitForSingleObject
 * thread bindings today); mb_win_mutex_close_handle retires tokens for
 * composition, and an object is destroyed only when its last handle retires
 * and no blocked waiter references it; destroying an owned object drops that
 * ownership exactly like the object vanishing on Windows. Success preserves
 * LastError with one Microsoft-documented exception: creating a name that
 * already exists returns a valid handle and sets ERROR_ALREADY_EXISTS.
 * Failed calls write only the documented/policy error, never fake success.
 * Every calling native thread must already be admitted to the thread domain.
 * Not process-shared; no security descriptor model; dwDesiredAccess is
 * validated structurally (DWORD width) but not enforced (adapter policy).
 * Output pointers are optional; failed calls never write them. */
mb_win_mutex_status mb_win_mutex_open(mb_win_mutex *,const mb_win_mutex_config *,int *);
mb_win_mutex_status mb_win_mutex_close(mb_win_mutex *,int *);
mb_win_mutex_status mb_win_mutex_acquire_pin(mb_win_mutex *,mb_win_mutex_pin *);
mb_win_mutex_status mb_win_mutex_release_pin(mb_win_mutex_pin *);
mb_win_thread_domain *mb_win_mutex_domain(mb_win_mutex *);
/* Both create paths collapse here: flags are MB_WIN_MUTEX_INITIAL_OWNER or
 * zero (unknown bits fail 87). name is UTF-16 code units; NULL or an empty
 * name creates an unnamed object. Returns a new handle token or 0. */
uint64_t mb_win_mutex_create(mb_win_mutex *,const uint16_t *name,uint32_t flags);
/* OpenMutexW: named lookup only; NULL name fails 87, unknown name fails
 * ERROR_FILE_NOT_FOUND. Returns a new handle token or 0. */
uint64_t mb_win_mutex_open_named(mb_win_mutex *,const uint16_t *name);
/* Returns 32-bit BOOL: 1 released, 0 failure (6 invalid handle, 288 not
 * owner). */
int32_t mb_win_mutex_release(mb_win_mutex *,uint64_t handle);
/* Returns the 32-bit WAIT_* DWORD. INFINITE waits may block until release or
 * abandonment; the owner-recursion INFINITE case refuses instead of hanging. */
uint32_t mb_win_mutex_wait(mb_win_mutex *,uint64_t handle,uint32_t milliseconds);
/* Alertable flag documented: nonzero fails explicitly WAIT_FAILED +
 * ERROR_NOT_SUPPORTED (no APC surface in this family). */
uint32_t mb_win_mutex_wait_ex(mb_win_mutex *,uint64_t handle,uint32_t milliseconds,int32_t alertable);
/* Retires one handle token (CloseHandle composition point). Objects live
 * until their last handle retires and no blocked waiter references them. */
int32_t mb_win_mutex_close_handle(mb_win_mutex *,uint64_t handle);
/* Dispatch helper for a composed KERNEL32 wait: 1 when token names a live
 * mutex handle of this owner. */
int32_t mb_win_mutex_is_handle(mb_win_mutex *,uint64_t handle);
#ifdef __cplusplus
}
#endif
#endif
