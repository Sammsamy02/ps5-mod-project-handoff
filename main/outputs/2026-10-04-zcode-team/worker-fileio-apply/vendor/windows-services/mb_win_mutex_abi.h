#ifndef MB_WIN_MUTEX_ABI_H
#define MB_WIN_MUTEX_ABI_H
/* VENDORED ADAPTED COPY (origin: pattern of outputs/ps5-mod-bridge/
 * windows-services/mb_win_init_once_abi.h; that original file is unchanged). */
#include "mb_win_mutex.h"
#include "../windows-execution/pe_module.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mb_win_mutex_session {
    mb_win_mutex_pin pin;
    mb_pe_module *module;
    struct mb_win_mutex_session *next;
    uint8_t active;
} mb_win_mutex_session;
/* Original stationary zeroed sessions; begin before module EMPTY->LOADING,
 * end after EMPTY and all its calls/objects retire. First/final controls
 * require global call quiescence; other controls/binding are externally
 * serialized. One immutable installed owner while sessions exist; every
 * consumer pins it. Library ASCII-folded KERNEL32.dll, exact six names,
 * ordinals unresolved. Signatures follow the Microsoft x64 documentation:
 * HANDLE CreateMutexW(LPSECURITY_ATTRIBUTES,BOOL,LPCWSTR),
 * HANDLE CreateMutexExW(LPSECURITY_ATTRIBUTES,LPCWSTR,DWORD,DWORD),
 * HANDLE OpenMutexW(DWORD,BOOL,LPCWSTR), BOOL ReleaseMutex(HANDLE),
 * DWORD WaitForSingleObject(HANDLE,DWORD) and
 * DWORD WaitForSingleObjectEx(HANDLE,DWORD,BOOL): HANDLE pointer-sized,
 * BOOL/DWORD 32-bit, LPCWSTR UTF-16 code units. Invalid session/view ->
 * ERROR; only BOUND writes the output address. NOTE: mb_win_workers_abi.c
 * already binds WaitForSingleObject/CloseHandle for thread handles; the
 * WaitForSingleObject bridge here is the mutex route and the controller
 * composes the two through the shared handle namespaces
 * (mb_win_mutex_is_handle first, thread wait otherwise). */
mb_win_mutex_status mb_win_mutex_begin_session(mb_win_mutex_session *,mb_win_mutex *,mb_pe_module *);
mb_win_mutex_status mb_win_mutex_end_session(mb_win_mutex_session *);
mb_pe_bind_result mb_win_mutex_bind(void *,const mb_pe_import *,uint64_t *);
#ifdef __cplusplus
}
#endif
#endif
