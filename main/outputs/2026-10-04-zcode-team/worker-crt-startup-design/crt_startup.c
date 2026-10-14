/*
 * crt_startup.c — host reference implementation.
 * See crt_startup.h and DESIGN.md for the documented semantics being modeled
 * and the honest divergence list. No OS calls, no allocation.
 */
#include "crt_startup.h"

static crt_dll_state g_dll_state = CRT_DLL_STATE_IDLE;

crt_dll_state crt_dll_state_get(void)
{
    return g_dll_state;
}

void crt_dll_state_set(crt_dll_state s)
{
    g_dll_state = s;
}

/* _initterm (internal.cpp shape): blind sweep, skip NULLs, discard returns. */
void crt_initterm(crt_init_fn *begin, crt_init_fn *end)
{
    while (begin < end) {
        if (*begin != 0)
            (*begin)();
        ++begin;
    }
}

/* _initterm_e (internal.cpp shape): stop at first non-zero errno_t. */
int crt_initterm_e(crt_init_fn *begin, crt_init_fn *end)
{
    while (begin < end) {
        if (*begin != 0) {
            int r = (*begin)();
            if (r != 0)
                return r;
        }
        ++begin;
    }
    return 0;
}

/* _seh_filter_dll stub (seh_filter_dll.cpp contract). */
int crt_seh_filter_dll(unsigned long exception_code, void *exception_info)
{
    (void)exception_code; /* host stub: no code-specific handling */
    (void)exception_info; /* host stub: no EXCEPTION_POINTERS to record */
    if (g_dll_state == CRT_DLL_STATE_INIT_IN_PROGRESS) {
        g_dll_state = CRT_DLL_STATE_INIT_FAILED;
        return CRT_EXCEPTION_EXECUTE_HANDLER;
    }
    return CRT_EXCEPTION_CONTINUE_SEARCH;
}

int crt_dll_detach_should_run_cleanup(void)
{
    return g_dll_state == CRT_DLL_STATE_INIT_COMPLETE;
}

/* atexit / _onexit model: fixed-capacity LIFO table. */
static void (*g_atexit[CRT_ATEXIT_MAX])(void);
static int   g_atexit_count = 0;

int crt_atexit(void (*fn)(void))
{
    if (fn == 0 || g_atexit_count >= CRT_ATEXIT_MAX)
        return -1;
    g_atexit[g_atexit_count++] = fn;
    return 0;
}

int crt_atexit_count(void)
{
    return g_atexit_count;
}

void crt_cexit(void)
{
    while (g_atexit_count > 0) {
        --g_atexit_count;
        g_atexit[g_atexit_count]();
    }
    /* table implicitly reset (count == 0); reuse allowed — model choice,
     * flagged as a verify-against-sources divergence in DESIGN.md 7.3. */
}

/* Composed DLL_PROCESS_ATTACH (dll_dllmain.cpp shape). */
int crt_dll_process_attach(crt_init_fn *xi_begin, crt_init_fn *xi_end,
                           crt_init_fn *xc_begin, crt_init_fn *xc_end)
{
    g_dll_state = CRT_DLL_STATE_INIT_IN_PROGRESS;
    crt_initterm(xi_begin, xi_end); /* C initializers first */
    crt_initterm(xc_begin, xc_end); /* then C++ dynamic initializers */
    if (g_dll_state == CRT_DLL_STATE_INIT_IN_PROGRESS)
        g_dll_state = CRT_DLL_STATE_INIT_COMPLETE;
    return g_dll_state == CRT_DLL_STATE_INIT_COMPLETE ? 0 : -1;
}

/* Composed DLL_PROCESS_DETACH (dll_dllmain.cpp shape). */
void crt_dll_process_detach(crt_init_fn *xp_begin, crt_init_fn *xp_end,
                            crt_init_fn *xt_begin, crt_init_fn *xt_end)
{
    if (!crt_dll_detach_should_run_cleanup())
        return; /* failed init: no cleanup for never-fully-built objects */
    crt_initterm(xp_begin, xp_end); /* pre-terminators */
    crt_initterm(xt_begin, xt_end); /* terminators */
    crt_cexit();                    /* atexit/_onexit LIFO runs last */
}
