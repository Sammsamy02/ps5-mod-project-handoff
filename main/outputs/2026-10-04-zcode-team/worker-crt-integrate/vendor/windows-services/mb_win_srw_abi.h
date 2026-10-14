#ifndef MB_WIN_SRW_ABI_H
#define MB_WIN_SRW_ABI_H
#include "mb_win_srw.h"
#include "../windows-execution/pe_module.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mb_win_srw_session {
    mb_win_srw_pin pin;
    mb_pe_module *module;
    struct mb_win_srw_session *next;
    uint8_t active;
} mb_win_srw_session;
/* Original stationary zeroed sessions; begin before module EMPTY->LOADING,
 * end after EMPTY and all its calls/objects retire. First/final controls require
 * global call quiescence; other controls/binding are externally serialized.
 * One immutable installed owner while sessions exist; every consumer pins it.
 * Library ASCII-folded KERNEL32.dll, exact eleven names, ordinals unresolved.
 * Invalid session/view -> ERROR; only BOUND writes the output address.
 */
mb_win_srw_status mb_win_srw_begin(mb_win_srw_session *,mb_win_srw *,mb_pe_module *);
mb_win_srw_status mb_win_srw_end(mb_win_srw_session *);
mb_pe_bind_result mb_win_srw_bind(void *,const mb_pe_import *,uint64_t *);
#ifdef __cplusplus
}
#endif
#endif
