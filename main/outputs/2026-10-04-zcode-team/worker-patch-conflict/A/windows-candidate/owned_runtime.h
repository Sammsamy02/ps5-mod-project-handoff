#ifndef MB_OWNED_RUNTIME_H
#define MB_OWNED_RUNTIME_H
#include "../windows-execution/pe_module.h"
#include <stdint.h>
/* One owned diagnostic instance. Calls are serialized; every native entry
 * realigns its own stack. Fixture bytes are immutable embedded owned inputs.
 * open/run execute once. cleanup alone may be retried; original failure stays
 * in the report and no DLL is executed a second time. The caller must keep this
 * code/storage and memory adapter alive until cleanup_pending becomes zero.
 * If native thread leave fails, cleanup must retry on that same native thread;
 * another thread is refused while that registration obligation remains.
 * The opt-in entry profile retains its registered thread across run return
 * until loader detach/cleanup is complete. Its cleanup must use that same
 * thread even after a successful run. The default no-entry profile still
 * attempts thread leave at the end of run. */
typedef struct mb_owned_dll_report {
    uint32_t started, run_attempted, completed, cleanup_pending;
    uint32_t failure_stage, native_status, windows_error;
    uint32_t first_module_cause;
    uint32_t consumer_imports, helper_imports;
    uint32_t source_acquires[2], source_releases[2];
    uint64_t results[2];
    uint32_t stage, dll_calls_started, dll_calls_returned;
    uint32_t entry_enabled, entry_attach_calls[2], entry_detach_calls[2], entry_contract_errors;
    int32_t entry_attach_result[2], entry_detach_result[2];
    uint32_t clock_enabled, clock_session_begins[2], clock_session_ends[2];
    uint32_t worker_enabled, worker_calls_started, worker_calls_returned;
    uint64_t worker_begin[4], worker_result[4];
    uint32_t sync_enabled;
    uint32_t init_once_enabled;
    uint32_t mutex_enabled;
} mb_owned_dll_report;
enum mb_owned_dll_stage {
    MB_OWNED_STAGE_NONE, MB_OWNED_STAGE_INITIALIZING, MB_OWNED_STAGE_READY,
    MB_OWNED_STAGE_THREAD_ENTER, MB_OWNED_STAGE_LOAD_CONSUMER,
    MB_OWNED_STAGE_RESOLVE_EXPORTS, MB_OWNED_STAGE_EXERCISE,
    MB_OWNED_STAGE_UNLOAD, MB_OWNED_STAGE_LOOKUP, MB_OWNED_STAGE_RUN_RETURNED,
    MB_OWNED_STAGE_CLEANUP, MB_OWNED_STAGE_RETIRED
};
int mb_owned_dll_open(const mb_pe_memory_ops *);
int mb_owned_dll_run(void);
int mb_owned_dll_cleanup(void);
void mb_owned_dll_result(mb_owned_dll_report *);
#endif
