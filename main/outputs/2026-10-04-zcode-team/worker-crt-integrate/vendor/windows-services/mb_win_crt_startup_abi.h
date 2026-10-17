#ifndef MB_WIN_CRT_STARTUP_ABI_H
#define MB_WIN_CRT_STARTUP_ABI_H
/* VENDORED ADAPTED COPY (origin: pattern of worker-4-win-services/sources/
 * mb_win_init_once_abi.h; that file is unchanged). */
#include "mb_win_crt_startup.h"
#include "../windows-execution/pe_module.h" /* promotion adaptation: tree include convention (as mb_win_fileio_abi.h) */
#ifdef __cplusplus
extern "C" {
#endif
/* One session per retained module being bound. Zeroed stationary session;
 * begin before the module leaves MB_PE_MODULE_EMPTY, end after it returns to
 * EMPTY. Begin/end require global call quiescence; binding is externally
 * serialized. All sessions share the single freestanding owner inside
 * mb_win_crt_startup.c (no domain pin: this family has no native pthread
 * state). Bound names and libraries, ASCII-folded, ordinals unresolved:
 *   api-ms-win-crt-runtime-l1-1-0.dll (the exact census-observed library for
 *   every startup import of both retained DLLs, dwmapi.dll and UE4SS.dll):
 *     _initterm, _initterm_e, _crt_atexit, _initialize_onexit_table,
 *     _register_onexit_function, _execute_onexit_table, _cexit
 *   ucrtbase.dll: the same seven exports (documented canonical home).
 *   vcruntime140.dll: _initterm, _initterm_e, _seh_filter_dll (documented
 *     vcruntime home; NO census row itemizes it — bound for family
 *     completeness only, zero observed rows).
 * Everything else (including the census-observed out-of-scope
 * _initialize_narrow_environment, _errno, and the api-ms-win-crt-stdio*
 * __stdio_common_* imports) returns UNRESOLVED so the import pass carries it
 * to its owning family — never a fake success. The shims use the documented
 * Microsoft x64 signatures (errno_t/int 32-bit, unsigned long 32-bit,
 * _onexit_t pointer-sized); invalid session/view -> ERROR; only BOUND writes
 * the output address.
 */
typedef struct mb_win_crt_startup_session {
    mb_pe_module *module;
    struct mb_win_crt_startup_session *next;
    uint8_t active;
} mb_win_crt_startup_session;
typedef enum mb_win_crt_startup_status {
    MB_WIN_CRT_STARTUP_OK = 0, MB_WIN_CRT_STARTUP_INVALID_ARGUMENT,
    MB_WIN_CRT_STARTUP_BAD_STATE, MB_WIN_CRT_STARTUP_BUSY
} mb_win_crt_startup_status;
mb_win_crt_startup_status mb_win_crt_startup_begin_session(mb_win_crt_startup_session *,
                                                           mb_pe_module *);
mb_win_crt_startup_status mb_win_crt_startup_end_session(mb_win_crt_startup_session *);
mb_pe_bind_result mb_win_crt_startup_bind(void *, const mb_pe_import *, uint64_t *);
#ifdef __cplusplus
}
#endif
#endif
