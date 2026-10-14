#ifndef MB_WIN_WORKERS_ABI_H
#define MB_WIN_WORKERS_ABI_H
#include "mb_win_workers.h"
#include "../windows-execution/pe_module.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mb_win_worker_session {
    mb_win_worker_pin pin;
    mb_pe_module *module;
    struct mb_win_worker_session *next;
    uint8_t active;
} mb_win_worker_session;
/* One installed worker owner. Stationary zeroed sessions begin before EMPTY
 * module load; end only after that module is EMPTY with its calls retired.
 * Controls/binding are externally serialized. First install/final retirement
 * require calls quiescent; owner pins keep domain/backend alive. Every module
 * that may call these imports needs a session; callback code leases prevent
 * module unload while workers run. No global transient callback context.
 * Library ASCII-folded KERNEL32.dll, exact names only, no ordinal binding.
 */
mb_win_worker_status mb_win_workers_begin(mb_win_worker_session *,mb_win_workers *,mb_pe_module *);
mb_win_worker_status mb_win_workers_end(mb_win_worker_session *);
mb_pe_bind_result mb_win_workers_bind(void *,const mb_pe_import *,uint64_t *);
#ifdef __cplusplus
}
#endif
#endif
