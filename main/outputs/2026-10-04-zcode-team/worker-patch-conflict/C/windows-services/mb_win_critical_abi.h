#ifndef MB_WIN_CRITICAL_ABI_H
#define MB_WIN_CRITICAL_ABI_H
#include "mb_win_critical.h"
#include "../windows-execution/pe_module.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mb_win_critical_session {
    mb_win_critical_pin pin;
    mb_pe_module *module;
    struct mb_win_critical_session *next;
    uint8_t active;
} mb_win_critical_session;
/* Exact seven KERNEL32.dll critical-section imports, ASCII-folded DLL and exact
 * function names, ordinals unsupported. One installed owner; zeroed stationary
 * session begins at EMPTY before load and ends only after EMPTY/calls retired.
 * External controls/binding serialized; first install/final end quiescent.
 * Every calling thread must already enter the domain. Successful calls and
 * contended TryEnter preserve LastError (adapter policy). Bounded capacity
 * exhaustion makes InitAndSpin return0/error8; native error31, invalid use87.
 * This explicit limit differs from modern Windows' guaranteed init success.
 * Other failure-return-free APIs fail-stop rather than inventing success.
 */
mb_win_critical_status mb_win_critical_begin(mb_win_critical_session *,mb_win_critical *,mb_pe_module *);
mb_win_critical_status mb_win_critical_end(mb_win_critical_session *);
mb_pe_bind_result mb_win_critical_bind(void *,const mb_pe_import *,uint64_t *);
#ifdef __cplusplus
}
#endif
#endif
