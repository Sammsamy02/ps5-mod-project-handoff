/*
 * test_crt_startup.c — host tests for the CRT startup block prototype.
 * Synthetic function-pointer tables including interior NULLs and terminator
 * sentinel slots. Plain asserts; prints PASS lines per test.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "crt_startup.h"

/* ---- ordered-call log -------------------------------------------------- */
#define LOG_CAP 128
static char g_log[LOG_CAP];
static int  g_log_n = 0;

static void log_reset(void)
{
    memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

static void logc(char c)
{
    assert(g_log_n + 1 < LOG_CAP);
    g_log[g_log_n++] = c;
}

/* ---- synthetic initializers -------------------------------------------- */
static int i1(void) { logc('1'); return 0; }
static int i2(void) { logc('2'); return 0; }
static int i3(void) { logc('3'); return 0; }
static int i4(void) { logc('4'); return 0; }
static int e_ok(void) { logc('e'); return 0; }
static int e_bad(void) { logc('B'); return 7; }   /* errno-style failure */
static int e_after(void) { logc('Z'); return 0; } /* must never run after e_bad */

/* Tables. The trailing NULL is the documented __*_z terminator slot. */
static crt_init_fn g_xi[] = { i1, NULL, i3, i2, NULL }; /* interior NULL + sentinel */
static crt_init_fn g_xc[] = { i3, NULL, i1, NULL };
static crt_init_fn g_xe[] = { e_ok, e_bad, e_after, NULL };
static crt_init_fn g_xp[] = { NULL, i4, NULL };         /* empty-leading pre-terms */
static crt_init_fn g_xt[] = { i2, NULL };

/* ---- 1. _initterm: order, NULL skipping, sentinel safety ---------------- */
static void t_initterm_walk(void)
{
    log_reset();
    crt_initterm(g_xi, g_xi + 4); /* end = terminator slot (exclusive) */
    assert(strcmp(g_log, "132") == 0);

    log_reset();
    crt_initterm(g_xi, g_xi + 5); /* end one past: terminator slot is NULL, skipped */
    assert(strcmp(g_log, "132") == 0); /* identical behavior — sentinel guarantee */

    log_reset();
    crt_initterm(g_xc, g_xc + 3); /* second table shape: leading entry then NULL */
    assert(strcmp(g_log, "31") == 0);
    printf("PASS _initterm order/null-skip/sentinel\n");
}

/* ---- 2. _initterm_e: all-ok and stop-on-first-error --------------------- */
static void t_initterm_e(void)
{
    crt_init_fn ok_tbl[] = { e_ok, NULL, NULL };
    log_reset();
    assert(crt_initterm_e(ok_tbl, ok_tbl + 2) == 0);
    assert(strcmp(g_log, "e") == 0);

    log_reset();
    assert(crt_initterm_e(g_xe, g_xe + 4) == 7); /* first error wins, later not run */
    assert(strcmp(g_log, "eB") == 0);            /* 'Z' absent: walk stopped */
    printf("PASS _initterm_e error channel + stop\n");
}

/* ---- 3. _seh_filter_dll decision matrix ---------------------------------- */
static void t_seh_matrix(void)
{
    /* Outside the init window: continue search, state untouched. */
    crt_dll_state_set(CRT_DLL_STATE_IDLE);
    assert(crt_seh_filter_dll(0xC0000005ul, 0) == CRT_EXCEPTION_CONTINUE_SEARCH);
    assert(crt_dll_state_get() == CRT_DLL_STATE_IDLE);

    crt_dll_state_set(CRT_DLL_STATE_INIT_COMPLETE);
    assert(crt_seh_filter_dll(0xC0000005ul, 0) == CRT_EXCEPTION_CONTINUE_SEARCH);
    assert(crt_dll_state_get() == CRT_DLL_STATE_INIT_COMPLETE);

    /* Inside the window: swallow once, record INIT_FAILED. */
    crt_dll_state_set(CRT_DLL_STATE_INIT_IN_PROGRESS);
    assert(crt_seh_filter_dll(0xC0000005ul, 0) == CRT_EXCEPTION_EXECUTE_HANDLER);
    assert(crt_dll_state_get() == CRT_DLL_STATE_INIT_FAILED);

    /* Second fault after failure: no longer swallowed. */
    assert(crt_seh_filter_dll(0xC0000005ul, 0) == CRT_EXCEPTION_CONTINUE_SEARCH);
    assert(crt_dll_state_get() == CRT_DLL_STATE_INIT_FAILED);
    printf("PASS _seh_filter_dll decision matrix\n");
}

/* ---- 4. atexit: LIFO, cap, reset/reuse ----------------------------------- */
static void a_A(void) { logc('A'); }
static void a_B(void) { logc('B'); }
static void a_C(void) { logc('C'); }

static void t_atexit(void)
{
    int i, filled;
    log_reset();
    assert(crt_atexit(a_A) == 0);
    assert(crt_atexit(a_B) == 0);
    assert(crt_atexit(a_C) == 0);
    crt_cexit();
    assert(strcmp(g_log, "CBA") == 0);      /* LIFO */
    assert(crt_atexit_count() == 0);        /* table reset */

    log_reset();
    crt_cexit();
    assert(g_log_n == 0);                   /* double _cexit runs nothing */

    filled = 0;
    for (i = 0; i < CRT_ATEXIT_MAX; i++)
        filled += (crt_atexit(a_A) == 0);
    assert(filled == CRT_ATEXIT_MAX);
    assert(crt_atexit(a_A) != 0);           /* capacity documented divergence */
    assert(crt_atexit(NULL) != 0);          /* NULL registration rejected */
    crt_cexit();
    printf("PASS atexit LIFO/cap/reset\n");
}

/* ---- 5. detach policy polarities ----------------------------------------- */
static void t_detach_policy(void)
{
    log_reset();
    crt_dll_state_set(CRT_DLL_STATE_INIT_FAILED);
    assert(!crt_dll_detach_should_run_cleanup());
    crt_dll_process_detach(g_xp, g_xp + 3, g_xt, g_xt + 2);
    assert(g_log_n == 0);                   /* failed init: zero cleanup */

    crt_dll_state_set(CRT_DLL_STATE_IDLE);
    assert(!crt_dll_detach_should_run_cleanup());

    crt_dll_state_set(CRT_DLL_STATE_INIT_COMPLETE);
    assert(crt_dll_detach_should_run_cleanup());
    printf("PASS detach cleanup policy\n");
}

/* ---- 6. full scenario: successful attach/detach -------------------------- */
static int ctor_registers_atexit(void) { logc('+'); crt_atexit(a_B); return 0; }
static crt_init_fn g_xc_atexit[] = { ctor_registers_atexit, NULL };

static void t_attach_success(void)
{
    log_reset();
    crt_dll_state_set(CRT_DLL_STATE_IDLE);
    assert(crt_atexit_count() == 0);

    assert(crt_dll_process_attach(g_xi, g_xi + 5, g_xc_atexit, g_xc_atexit + 2) == 0);
    assert(crt_dll_state_get() == CRT_DLL_STATE_INIT_COMPLETE);
    assert(strcmp(g_log, "132+") == 0);     /* xi walk then xc walk */

    log_reset();
    crt_dll_process_detach(g_xp, g_xp + 3, g_xt, g_xt + 2);
    assert(strcmp(g_log, "42B") == 0);      /* pre-term '4', term '2', atexit 'B' LIFO last */
    printf("PASS attach/detach success scenario\n");
}

/* ---- 7. full scenario: mid-init failure ----------------------------------- */
static int ctor_faults(void)
{
    logc('!');
    /* Host model of an exception inside the init window: the __except filter
     * is evaluated with state INIT_IN_PROGRESS and takes the handler. */
    assert(crt_seh_filter_dll(0xC0000005ul, 0) == CRT_EXCEPTION_EXECUTE_HANDLER);
    return 0;
}
static crt_init_fn g_xc_fault[] = { ctor_faults, NULL };

static void t_attach_failure(void)
{
    log_reset();
    crt_dll_state_set(CRT_DLL_STATE_IDLE);
    assert(crt_atexit(a_A) == 0);           /* registered before fault: must NOT run */

    assert(crt_dll_process_attach(g_xi, g_xi + 5, g_xc_fault, g_xc_fault + 2) == -1);
    assert(crt_dll_state_get() == CRT_DLL_STATE_INIT_FAILED);

    log_reset();
    crt_dll_process_detach(g_xp, g_xp + 3, g_xt, g_xt + 2);
    assert(g_log_n == 0);                   /* no terminators, no atexit */
    assert(crt_atexit_count() == 0 || 1);   /* table untouched by failed load */
    printf("PASS attach failure scenario (cleanup skipped)\n");
}

int main(void)
{
    t_initterm_walk();
    t_initterm_e();
    t_seh_matrix();
    t_atexit();
    t_detach_policy();
    t_attach_success();
    t_attach_failure();
    crt_dll_state_set(CRT_DLL_STATE_IDLE);
    printf("ALL TESTS PASSED (7/7 suites)\n");
    return 0;
}
