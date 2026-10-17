/*
 * crt_startup.h — host reference prototype of the MSVC CRT DLL startup block.
 *
 * Semantics transcribed from documented UCRT source layout:
 *   - _initterm / _initterm_e           (internal.cpp shape)
 *   - DLL attach/detach choreography    (dll_dllmain.cpp shape)
 *   - _seh_filter_dll decision contract (seh_filter_dll.cpp shape)
 * Host-portable C11, no OS calls, no allocation. NO claim of binary or full
 * MSVC parity — see DESIGN.md section 7 for the divergence list.
 */
#ifndef CRT_STARTUP_H
#define CRT_STARTUP_H

#ifdef __cplusplus
extern "C" {
#endif

/* _PIFV: pointer to an initializer function: int fn(void). Both _initterm and
 * _initterm_e walk tables of this type in documented UCRT. */
typedef int (*crt_init_fn)(void);

/* _initterm: run initializers in [begin, end) in ascending slot order.
 * NULL slots (terminator sentinels, linker padding) are skipped; return
 * values are discarded; the walk cannot be stopped. */
void crt_initterm(crt_init_fn *begin, crt_init_fn *end);

/* _initterm_e: same sweep, but stops at the first non-zero return and returns
 * it. Returns 0 when every non-NULL entry returned 0 (or the table is empty). */
int crt_initterm_e(crt_init_fn *begin, crt_init_fn *end);

/* ---- _seh_filter_dll stub contract ------------------------------------ */

#define CRT_EXCEPTION_CONTINUE_SEARCH   0
#define CRT_EXCEPTION_EXECUTE_HANDLER   1
#define CRT_EXCEPTION_CONTINUE_EXECUTION (-1) /* never returned; listed for clarity */

typedef enum crt_dll_state {
    CRT_DLL_STATE_IDLE = 0,        /* no attach attempted yet */
    CRT_DLL_STATE_INIT_IN_PROGRESS, /* inside the CRT init window */
    CRT_DLL_STATE_INIT_FAILED,      /* filter took the handler at least once */
    CRT_DLL_STATE_INIT_COMPLETE     /* walks finished without failure */
} crt_dll_state;

crt_dll_state crt_dll_state_get(void);
void          crt_dll_state_set(crt_dll_state s); /* test/setup hook */

/* Stub of _seh_filter_dll(exception_code, exception_info).
 * - While state is INIT_IN_PROGRESS: records INIT_FAILED and returns
 *   CRT_EXCEPTION_EXECUTE_HANDLER (once; afterwards the state is no longer
 *   IN_PROGRESS so later faults return CONTINUE_SEARCH).
 * - Every other state: CRT_EXCEPTION_CONTINUE_SEARCH.
 * - Never returns CRT_EXCEPTION_CONTINUE_EXECUTION.
 * Host model only: no exception delivery, no unwinding, arguments unused. */
int crt_seh_filter_dll(unsigned long exception_code, void *exception_info);

/* Detach policy: cleanup (terminators + atexit walk) is legal only after a
 * COMPLETED init. Returns nonzero only for INIT_COMPLETE. */
int crt_dll_detach_should_run_cleanup(void);

/* ---- atexit / _onexit model --------------------------------------------
 * Fixed-capacity LIFO table (real CRT grows dynamically — documented
 * divergence). crt_cexit runs callbacks LIFO and resets the table; reuse
 * after _cexit is a model choice pending source verification. */
#define CRT_ATEXIT_MAX 32
int  crt_atexit(void (*fn)(void)); /* 0 ok; nonzero if fn NULL or table full */
int  crt_atexit_count(void);
void crt_cexit(void);

/* ---- composed attach/detach orchestrators (entry-adapter shape) -------- */

/* DLL_PROCESS_ATTACH body (UCRT dll_dllmain.cpp shape):
 *   enter INIT_IN_PROGRESS (the _seh_filter_dll window)
 *   _initterm(xi_a, xi_z)   — C initializers first
 *   _initterm(xc_a, xc_z)   — then C++ dynamic initializers
 *   leave window: COMPLETE, or stays FAILED if the filter fired
 * Returns 0 on COMPLETE, -1 on failure. Caller invokes the user/retained
 * DllMain body only after a 0 return. */
int crt_dll_process_attach(crt_init_fn *xi_begin, crt_init_fn *xi_end,
                           crt_init_fn *xc_begin, crt_init_fn *xc_end);

/* DLL_PROCESS_DETACH body (UCRT shape): pre-terminators, then terminators,
 * then the atexit table LIFO. No-op unless init completed (1.4/1.6 contract:
 * failed init must not run cleanup for never-fully-constructed objects). */
void crt_dll_process_detach(crt_init_fn *xp_begin, crt_init_fn *xp_end,
                            crt_init_fn *xt_begin, crt_init_fn *xt_end);

#ifdef __cplusplus
}
#endif
#endif /* CRT_STARTUP_H */
