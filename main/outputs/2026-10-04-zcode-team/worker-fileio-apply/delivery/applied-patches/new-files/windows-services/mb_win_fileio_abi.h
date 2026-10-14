#ifndef MB_WIN_FILEIO_ABI_H
#define MB_WIN_FILEIO_ABI_H
/* VENDORED ADAPTED COPY (origin: pattern of outputs/ps5-mod-bridge/
 * windows-services/mb_win_init_once_abi.h; that original file is unchanged). */
#include "mb_win_fileio.h"
#include "../windows-execution/pe_module.h" /* promotion adaptation: tree include convention (as mb_win_mutex_abi.h) */
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mb_win_fileio_session {
    mb_win_fileio_pin pin;
    mb_pe_module *module;
    struct mb_win_fileio_session *next;
    uint8_t active;
} mb_win_fileio_session;
/* Original stationary zeroed sessions; begin before module EMPTY->LOADING,
 * end after EMPTY and all its calls/objects retire. First/final controls
 * require global call quiescence; other controls/binding are externally
 * serialized. One immutable installed owner while sessions exist; every
 * consumer pins it. Library ASCII-folded KERNEL32.dll, exact twenty names,
 * ordinals unresolved. All imports use the documented Microsoft x64
 * signatures (32-bit BOOL/DWORD returns and parameters, pointer-sized
 * HANDLE/LPCWSTR, 8-byte HANDLE tokens); no host file descriptor ever
 * crosses this boundary. Invalid session/view -> ERROR; only BOUND writes
 * the output address. */
mb_win_fileio_status mb_win_fileio_begin_session(mb_win_fileio_session *,mb_win_fileio *,mb_pe_module *);
mb_win_fileio_status mb_win_fileio_end_session(mb_win_fileio_session *);
mb_pe_bind_result mb_win_fileio_bind(void *,const mb_pe_import *,uint64_t *);
#ifdef __cplusplus
}
#endif
#endif
