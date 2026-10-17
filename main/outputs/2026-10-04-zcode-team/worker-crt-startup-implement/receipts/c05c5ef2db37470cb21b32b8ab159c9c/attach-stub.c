/* VENDORED ADAPTED COPY (origin: semantics of outputs/2026-10-04-zcode-team/
 * worker-crt-startup-design/crt_startup.c host prototype, unchanged there;
 * structure follows the windows-services family owner pattern of
 * worker-4-win-services/sources/mb_win_init_once.c). Freestanding: no OS
 * calls, no allocation, no libc. */
#include "mb_win_crt_startup.h"

static mb_win_crt_dll_state g_dll_state = MB_WIN_CRT_DLL_STATE_IDLE;

mb_win_crt_dll_state mb_win_crt_dll_state_get(void) { return g_dll_state; }

void mb_win_crt_dll_state_set(mb_win_crt_dll_state s) { g_dll_state = s; }

/* _initterm (documented internal.cpp shape). */
void mb_win_crt_initterm(mb_win_crt_init_fn *begin, mb_win_crt_init_fn *end)
{
    while (begin < end) {
        if (*begin) (*begin)();
        ++begin;
    }
}

/* _initterm_e (documented internal.cpp shape): stop at first nonzero errno_t. */
int mb_win_crt_initterm_e(mb_win_crt_init_fn *begin, mb_win_crt_init_fn *end)
{
    while (begin < end) {
        if (*begin) {
            int r = (*begin)();
            if (r) return r;
        }
        ++begin;
    }
    return 0;
}

/* _seh_filter_dll decision stub (documented seh_filter_dll.cpp contract):
 * swallow exactly the first fault of the init window, propagate everything
 * else. */
int mb_win_crt_seh_filter_dll(uint32_t code, void *info)
{
    (void)code; /* host model: no code-specific handling */
    (void)info; /* host model: no EXCEPTION_POINTERS recorded */
    if (g_dll_state == MB_WIN_CRT_DLL_STATE_INIT_IN_PROGRESS) {
        g_dll_state = MB_WIN_CRT_DLL_STATE_INIT_FAILED;
        return MB_WIN_CRT_EXCEPTION_EXECUTE_HANDLER;
    }
    return MB_WIN_CRT_EXCEPTION_CONTINUE_SEARCH;
}

int mb_win_crt_dll_detach_should_run_cleanup(void)
{
    return g_dll_state == MB_WIN_CRT_DLL_STATE_INIT_COMPLETE;
}

/* Global atexit table: fixed capacity, LIFO, reset on execution. */
static mb_win_crt_exit_fn g_atexit[MB_WIN_CRT_ATEXIT_MAX];
static int g_atexit_count;

int mb_win_crt_atexit(mb_win_crt_exit_fn fn)
{
    if (!fn || g_atexit_count >= MB_WIN_CRT_ATEXIT_MAX) return -1;
    g_atexit[g_atexit_count++] = fn;
    return 0;
}

int mb_win_crt_atexit_count(void) { return g_atexit_count; }

void mb_win_crt_cexit(void)
{
    while (g_atexit_count > 0) {
        mb_win_crt_exit_fn fn = g_atexit[--g_atexit_count];
        g_atexit[g_atexit_count] = 0;
        fn();
    }
    /* Table reset (count 0, slots NULL): reuse allowed — documented model
     * choice pending vendored-source verification. */
}

/* ---- documented UCRT onexit-table family, fixed-capacity model ---------- */

int mb_win_crt_initialize_onexit_table(mb_win_crt_onexit_table *table)
{
    if (!table) return -1; /* documented EINVAL path */
    table->first = table->last = table->end = 0;
    return 0;
}

mb_win_crt_exit_fn mb_win_crt_register_onexit_function(mb_win_crt_onexit_table *table,
                                                       mb_win_crt_exit_fn fn)
{
    /* NULL-backed (initialized but no storage attached) and full tables fail
     * explicitly; an executed-and-emptied but still-backed table accepts
     * registration again (documented reuse model choice). */
    if (!table || !fn || !table->first || table->last == table->end)
        return 0;
    *table->last++ = (uintptr_t)fn;
    return fn;
}

int mb_win_crt_execute_onexit_table(mb_win_crt_onexit_table *table)
{
    if (!table) return -1;
    while (table->last > table->first) {
        mb_win_crt_exit_fn fn = (mb_win_crt_exit_fn)*--table->last;
        *table->last = 0; /* clear the executed slot before invoking */
        fn();
    }
    return 0; /* double execution walks an empty table: documented model choice */
}

int mb_win_crt_onexit_table_attach_storage(mb_win_crt_onexit_table *table,
                                           void **slots, size_t capacity)
{
    /* (Re-)attachment is legal only on an empty table (unbacked or already
     * executed/emptied): a table with pending registrations keeps its LIFO
     * promises and refuses. */
    if (!table || !slots || !capacity || table->first != table->last ||
        capacity > SIZE_MAX / sizeof(void *)) return -1;
    table->first = (uintptr_t *)slots;
    table->last = table->first;
    table->end = table->first + capacity;
    return 0;
}

/* ---- composed attach/detach orchestrators (dll_dllmain.cpp shape) ------- */

int mb_win_crt_dll_process_attach(mb_win_crt_init_fn *xi_begin, mb_win_crt_init_fn *xi_end,
                                  mb_win_crt_init_fn *xc_begin, mb_win_crt_init_fn *xc_end)
{(void)xi_begin;(void)xi_end;(void)xc_begin;(void)xc_end;return -1;}

void mb_win_crt_dll_process_detach(mb_win_crt_init_fn *xp_begin, mb_win_crt_init_fn *xp_end,
                                   mb_win_crt_init_fn *xt_begin, mb_win_crt_init_fn *xt_end)
{
    if (!mb_win_crt_dll_detach_should_run_cleanup())
        return; /* failed init: no cleanup for never-fully-built objects */
    mb_win_crt_initterm(xp_begin, xp_end); /* pre-terminators */
    mb_win_crt_initterm(xt_begin, xt_end); /* terminators */
    mb_win_crt_cexit();                    /* global atexit/_cexit LIFO last */
}
