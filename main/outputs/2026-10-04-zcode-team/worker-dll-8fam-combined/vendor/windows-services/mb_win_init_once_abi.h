#ifndef MB_WIN_INIT_ONCE_ABI_H
#define MB_WIN_INIT_ONCE_ABI_H
/* VENDORED ADAPTED COPY (origin: pattern of outputs/ps5-mod-bridge/
 * windows-services/mb_win_srw_abi.h; that original file is unchanged). */
#include "mb_win_init_once.h"
#include "../windows-execution/pe_module.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mb_win_init_once_session {
    mb_win_init_once_pin pin;
    mb_pe_module *module;
    struct mb_win_init_once_session *next;
    uint8_t active;
} mb_win_init_once_session;
/* Original stationary zeroed sessions; begin before module EMPTY->LOADING,
 * end after EMPTY and all its calls/objects retire. First/final controls
 * require global call quiescence; other controls/binding are externally
 * serialized. One immutable installed owner while sessions exist; every
 * consumer pins it. Library ASCII-folded KERNEL32.dll, exact two names,
 * ordinals unresolved. Both imports use the documented Microsoft x64
 * signatures: BOOL(PINIT_ONCE, DWORD, PBOOL, LPVOID*) and
 * BOOL(PINIT_ONCE, DWORD, LPVOID) with the 32-bit BOOL return, 8-byte
 * INIT_ONCE storage. Invalid session/view -> ERROR; only BOUND writes the
 * output address.
 */
mb_win_init_once_status mb_win_init_once_begin_session(mb_win_init_once_session *,mb_win_init_once *,mb_pe_module *);
mb_win_init_once_status mb_win_init_once_end_session(mb_win_init_once_session *);
mb_pe_bind_result mb_win_init_once_bind(void *,const mb_pe_import *,uint64_t *);
#ifdef __cplusplus
}
#endif
#endif
