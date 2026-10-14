#ifndef MB_WIN_CRT_STARTUP_H
#define MB_WIN_CRT_STARTUP_H
/* VENDORED ADAPTED COPY (origin: semantics of outputs/2026-10-04-zcode-team/
 * worker-crt-startup-design/crt_startup.h host prototype, unchanged there;
 * structure follows the windows-services family owner pattern of
 * worker-4-win-services/sources/mb_win_init_once.h). */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* CRT DLL startup block required by the retained MSVC-linked PE images. The
 * census (worker-dll-mod-retention/baseline-dll-census.json, via the
 * controller addendum) is the authority for what the retained bodies import,
 * all of it from api-ms-win-crt-runtime-l1-1-0.dll: _initterm, _initterm_e,
 * _crt_atexit, _initialize_onexit_table, _register_onexit_function,
 * _execute_onexit_table, _cexit (both DLLs); _initialize_narrow_environment,
 * _errno, __stdio_common_vsprintf/vfprintf are observed too but belong to the
 * env/errno/stdio families and are NOT implemented here. _seh_filter_dll is
 * implemented for family completeness (documented VCRUNTIME140.dll home; no
 * observed census row itemizes it). All semantics transcribed from the
 * documented UCRT/MSVC CRT source layout (dll_dllmain.cpp, internal.cpp,
 * seh_filter_dll.cpp, onexit.c); NO full MSVC parity claim — divergences:
 * no real SEH delivery (this unit owns the filter decision only; the entry
 * adapter owns delivery), atexit and onexit tables are fixed-capacity with
 * caller-provided slot storage (the UCRT grows them on the heap), and table
 * reuse after execution is a documented model choice pending vendored-source
 * verification. Freestanding C11: no OS calls, no allocation, no libc.
 */
typedef int (*mb_win_crt_init_fn)(void);  /* _PIFV: int fn(void) initializer */
typedef void (*mb_win_crt_exit_fn)(void); /* _PVFV/_onexit_t: void fn(void)  */
/* Documented __onexit_table_t shape {first,last,end} over caller-owned slot
 * storage; 24 bytes, 8-aligned on Win64. first==last==end==NULL is the
 * initialized-but-unbacked state (registration fails until storage is
 * attached by the entry adapter). */
typedef struct mb_win_crt_onexit_table {
    uintptr_t *first, *last, *end;
} mb_win_crt_onexit_table;
/* Documented _seh_filter_dll result codes. */
#define MB_WIN_CRT_EXCEPTION_CONTINUE_SEARCH 0
#define MB_WIN_CRT_EXCEPTION_EXECUTE_HANDLER 1
#define MB_WIN_CRT_EXCEPTION_CONTINUE_EXECUTION (-1) /* never returned */
/* Process-wide DLL attach-window state. One instance is correct because the
 * Windows loader serializes DLL_PROCESS_ATTACH windows across the process. */
typedef enum mb_win_crt_dll_state {
    MB_WIN_CRT_DLL_STATE_IDLE = 0,
    MB_WIN_CRT_DLL_STATE_INIT_IN_PROGRESS,
    MB_WIN_CRT_DLL_STATE_INIT_FAILED,
    MB_WIN_CRT_DLL_STATE_INIT_COMPLETE
} mb_win_crt_dll_state;
mb_win_crt_dll_state mb_win_crt_dll_state_get(void);
void mb_win_crt_dll_state_set(mb_win_crt_dll_state); /* test/setup hook */
/* _initterm: blind ascending sweep of [begin,end); NULL slots (terminator
 * sentinel .z slot, linker padding) are skipped; return values discarded;
 * cannot be stopped. */
void mb_win_crt_initterm(mb_win_crt_init_fn *begin, mb_win_crt_init_fn *end);
/* _initterm_e: same sweep; stops at the first nonzero errno_t return and
 * returns it; 0 when all non-NULL entries returned 0 or the table is empty. */
int mb_win_crt_initterm_e(mb_win_crt_init_fn *begin, mb_win_crt_init_fn *end);
/* _seh_filter_dll decision stub: while the state is INIT_IN_PROGRESS it
 * records INIT_FAILED and returns EXECUTE_HANDLER exactly once (the state
 * leaves the window so a second fault propagates); every other state returns
 * CONTINUE_SEARCH; never CONTINUE_EXECUTION. code is uint32_t because the
 * Windows unsigned long is 32-bit; info (EXCEPTION_POINTERS*) is recorded by
 * the real CRT, unused by this decision model. */
int mb_win_crt_seh_filter_dll(uint32_t code, void *info);
/* Cleanup is legal only after a completed init: nonzero exactly when the
 * state is INIT_COMPLETE (a failed attach must not run terminators/atexit for
 * never-fully-constructed objects). */
int mb_win_crt_dll_detach_should_run_cleanup(void);
/* Global atexit table (the one _crt_atexit feeds; what _cexit runs). Fixed
 * capacity 32, LIFO, resets after execution (reuse is the documented model
 * choice). Registration returns 0, or nonzero for a NULL fn or a full table. */
#define MB_WIN_CRT_ATEXIT_MAX 32
int mb_win_crt_atexit(mb_win_crt_exit_fn fn);
int mb_win_crt_atexit_count(void);
void mb_win_crt_cexit(void); /* _cexit: run the global table LIFO, then reset */
/* Documented UCRT onexit-table signatures, fixed-capacity model.
 * _initialize_onexit_table: 0 and a NULL-backed empty table, or nonzero
 * (EINVAL) for a NULL table. _register_onexit_function: the function on
 * success, NULL for a NULL table/function, an unbacked or full table.
 * _execute_onexit_table: 0 after a LIFO walk that empties the table
 * (double execution is a no-op), nonzero for a NULL table. Registration
 * during execution is a contract violation (unspecified here, like calling
 * the real UCRT reentrantly from its own callbacks). */
int mb_win_crt_initialize_onexit_table(mb_win_crt_onexit_table *table);
mb_win_crt_exit_fn mb_win_crt_register_onexit_function(mb_win_crt_onexit_table *table,
                                                       mb_win_crt_exit_fn fn);
int mb_win_crt_execute_onexit_table(mb_win_crt_onexit_table *table);
/* Owner-only (not a Windows export) wiring helper: back a table with
 * caller-owned slot storage of exactly capacity entries (end = first +
 * capacity). 0 on success; nonzero for NULL arguments, zero capacity, or
 * overflow. The entry adapter calls it at attach time with the retained
 * image's own storage; nothing here ever allocates. */
int mb_win_crt_onexit_table_attach_storage(mb_win_crt_onexit_table *table,
                                           void **slots, size_t capacity);
/* Composed DLL_PROCESS_ATTACH (dll_dllmain.cpp shape): enter the init window,
 * _initterm(xi) then _initterm(xc), leave window COMPLETE (0) or FAILED (-1).
 * The caller invokes the retained user DllMain body only after 0. */
int mb_win_crt_dll_process_attach(mb_win_crt_init_fn *xi_begin, mb_win_crt_init_fn *xi_end,
                                  mb_win_crt_init_fn *xc_begin, mb_win_crt_init_fn *xc_end);
/* Composed DLL_PROCESS_DETACH: pre-terminators, terminators, then the global
 * atexit table LIFO last; a no-op unless init completed. */
void mb_win_crt_dll_process_detach(mb_win_crt_init_fn *xp_begin, mb_win_crt_init_fn *xp_end,
                                   mb_win_crt_init_fn *xt_begin, mb_win_crt_init_fn *xt_end);
#ifdef __cplusplus
}
#endif
#endif
