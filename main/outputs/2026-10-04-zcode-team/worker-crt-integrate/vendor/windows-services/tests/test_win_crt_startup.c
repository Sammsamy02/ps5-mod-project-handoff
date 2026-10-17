/* VENDORED ADAPTED COPY (origin: pattern of worker-4-win-services/sources/
 * tests/test_win_init_once.c, unchanged there; test semantics ported from
 * worker-crt-startup-design/test_crt_startup.c, unchanged there, and extended
 * with the census-observed onexit-table family and binder suites). */
#ifdef MB_WIN_CRT_STARTUP_LAYOUT
#include "../mb_win_crt_startup_abi.h"
const unsigned long long mb_crt_startup_layout[] = {sizeof(void *), sizeof(size_t),
    sizeof(uint32_t), sizeof(int32_t), sizeof(mb_win_crt_onexit_table),
    _Alignof(mb_win_crt_onexit_table), offsetof(mb_win_crt_onexit_table, first),
    offsetof(mb_win_crt_onexit_table, last), offsetof(mb_win_crt_onexit_table, end),
    sizeof(mb_win_crt_startup_session), offsetof(mb_win_crt_startup_session, module),
    offsetof(mb_win_crt_startup_session, active)};
#else
#include "../mb_win_crt_startup_abi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#undef assert
#define assert(x) do { if(!(x)){fprintf(stderr,"CHECK failed at %s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);} } while(0)
#define WIN __attribute__((ms_abi))
static mb_pe_module module;
static mb_win_crt_startup_session session;
/* Documented Microsoft x64 signatures of the bound names. */
typedef void (WIN *initterm_fn)(mb_win_crt_init_fn *, mb_win_crt_init_fn *);
typedef int32_t (WIN *initterm_e_fn)(mb_win_crt_init_fn *, mb_win_crt_init_fn *);
typedef int32_t (WIN *seh_fn)(uint32_t, void *);
typedef int32_t (WIN *atexit_fn)(mb_win_crt_exit_fn);
typedef void (WIN *cexit_fn)(void);
typedef int32_t (WIN *init_table_fn)(mb_win_crt_onexit_table *);
typedef void *(WIN *register_fn)(mb_win_crt_onexit_table *, mb_win_crt_exit_fn);
typedef int32_t (WIN *execute_fn)(mb_win_crt_onexit_table *);
static initterm_fn io_initterm;
static initterm_e_fn io_initterm_e;
static seh_fn io_seh_filter_dll;
static atexit_fn io_crt_atexit;
static cexit_fn io_cexit;
static init_table_fn io_initialize_onexit_table;
static register_fn io_register_onexit_function;
static execute_fn io_execute_onexit_table;
static void *bind_in(const char *library, size_t library_size, const char *name) {
    mb_pe_import i; memset(&i, 0, sizeof i);
    i.library = (mb_pe_string){library, library_size};
    i.name = (mb_pe_string){name, strlen(name)};
    uint64_t address = 0xdeadbeefdeadbeefull;
    assert(mb_win_crt_startup_bind(&session, &i, &address) == MB_PE_BIND_BOUND);
    return (void *)(uintptr_t)address;
}
/* Census-observed runtime library (every startup_imports row of both DLLs). */
static void *bind(const char *name) {
    return bind_in("API-MS-WIN-CRT-RUNTIME-L1-1-0.DLL", 33, name);
}
static mb_pe_bind_result try_bind(const char *library, size_t library_size, const char *name,
                                  uint8_t by_ordinal, uint64_t *out) {
    mb_pe_import i; memset(&i, 0, sizeof i);
    i.library = (mb_pe_string){library, library_size};
    i.name = (mb_pe_string){name, name ? strlen(name) : 0};
    i.by_ordinal = by_ordinal;
    return mb_win_crt_startup_bind(&session, &i, out);
}
/* ---- ordered-call log and synthetic initializers ------------------------ */
#define LOG_CAP 128
static char g_log[LOG_CAP];
static int g_log_n;
static void log_reset(void) { memset(g_log, 0, sizeof g_log); g_log_n = 0; }
static void logc(char c) { assert(g_log_n + 1 < LOG_CAP); g_log[g_log_n++] = c; }
static int i1(void) { logc('1'); return 0; }
static int i2(void) { logc('2'); return 0; }
static int i3(void) { logc('3'); return 0; }
static int i4(void) { logc('4'); return 0; }
static int e_ok(void) { logc('e'); return 0; }
static int e_bad(void) { logc('B'); return 7; }   /* errno-style failure */
static int e_after(void) { logc('Z'); return 0; } /* must never run after e_bad */
static void a_A(void) { logc('A'); }
static void a_B(void) { logc('B'); }
static void a_C(void) { logc('C'); }
/* Tables. The trailing NULL is the documented __*_z terminator slot. */
static mb_win_crt_init_fn g_xi[] = {i1, NULL, i3, i2, NULL}; /* interior NULL + sentinel */
static mb_win_crt_init_fn g_xc[] = {i3, NULL, i1, NULL};
static mb_win_crt_init_fn g_xe[] = {e_ok, e_bad, e_after, NULL};
static mb_win_crt_init_fn g_xp[] = {NULL, i4, NULL}; /* empty-leading pre-terminators */
static mb_win_crt_init_fn g_xt[] = {i2, NULL};
static int t_state_is(mb_win_crt_dll_state s) { return mb_win_crt_dll_state_get() == s; }
static int ctor_registers_atexit(void) { logc('+'); assert(io_crt_atexit(a_B) == 0); return 0; }
static int ctor_faults(void) { /* host model: __except filter evaluated mid-walk */
    logc('!');
    assert(io_seh_filter_dll(0xC0000005u, 0) == MB_WIN_CRT_EXCEPTION_EXECUTE_HANDLER);
    return 0;
}
int main(void) {
    assert(sizeof(mb_win_crt_onexit_table) == 24 && _Alignof(mb_win_crt_onexit_table) == 8);
    memset(&module, 0, sizeof module);
    assert(mb_win_crt_startup_begin_session(&session, &module) == MB_WIN_CRT_STARTUP_OK);
    module.state = MB_PE_MODULE_LOADING;
    io_initterm = bind("_initterm");
    io_initterm_e = bind("_initterm_e");
    io_seh_filter_dll = (seh_fn)bind_in("VCRUNTIME140.dll", 16, "_seh_filter_dll"); /* documented vcruntime home */
    io_crt_atexit = (atexit_fn)bind("_crt_atexit");
    io_cexit = (cexit_fn)bind("_cexit");
    io_initialize_onexit_table = (init_table_fn)bind("_initialize_onexit_table");
    io_register_onexit_function = (register_fn)bind("_register_onexit_function");
    io_execute_onexit_table = (execute_fn)bind("_execute_onexit_table");
    /* ---- 1. _initterm: order, NULL skipping, sentinel safety ------------- */
    log_reset();
    io_initterm(g_xi, g_xi + 4); /* end = terminator slot (exclusive) */
    assert(strcmp(g_log, "132") == 0);
    log_reset();
    io_initterm(g_xi, g_xi + 5); /* terminator slot itself is NULL: skipped */
    assert(strcmp(g_log, "132") == 0); /* identical: sentinel guarantee */
    log_reset();
    io_initterm(g_xc, g_xc + 3);
    assert(strcmp(g_log, "31") == 0);
    puts("PASS _initterm order/null-skip/sentinel");
    /* ---- 2. _initterm_e: error channel + stop ---------------------------- */
    mb_win_crt_init_fn ok_tbl[] = {e_ok, NULL, NULL};
    log_reset();
    assert(io_initterm_e(ok_tbl, ok_tbl + 2) == 0 && strcmp(g_log, "e") == 0);
    log_reset();
    assert(io_initterm_e(g_xe, g_xe + 4) == 7); /* first error wins, later not run */
    assert(strcmp(g_log, "eB") == 0);           /* 'Z' absent: walk stopped */
    puts("PASS _initterm_e error channel + stop");
    /* ---- 3. _seh_filter_dll decision matrix ------------------------------- */
    mb_win_crt_dll_state_set(MB_WIN_CRT_DLL_STATE_IDLE);
    assert(io_seh_filter_dll(0xC0000005u, 0) == MB_WIN_CRT_EXCEPTION_CONTINUE_SEARCH);
    assert(t_state_is(MB_WIN_CRT_DLL_STATE_IDLE));
    assert(io_seh_filter_dll(0xC0000005u, 0) == MB_WIN_CRT_EXCEPTION_CONTINUE_SEARCH);
    mb_win_crt_dll_state_set(MB_WIN_CRT_DLL_STATE_INIT_COMPLETE);
    assert(io_seh_filter_dll(0xE06D7363u, 0) == MB_WIN_CRT_EXCEPTION_CONTINUE_SEARCH);
    assert(t_state_is(MB_WIN_CRT_DLL_STATE_INIT_COMPLETE));
    mb_win_crt_dll_state_set(MB_WIN_CRT_DLL_STATE_INIT_IN_PROGRESS);
    assert(io_seh_filter_dll(0xC0000005u, 0) == MB_WIN_CRT_EXCEPTION_EXECUTE_HANDLER);
    assert(t_state_is(MB_WIN_CRT_DLL_STATE_INIT_FAILED)); /* leaves the window */
    assert(io_seh_filter_dll(0xC0000005u, 0) == MB_WIN_CRT_EXCEPTION_CONTINUE_SEARCH);
    assert(t_state_is(MB_WIN_CRT_DLL_STATE_INIT_FAILED));
    puts("PASS _seh_filter_dll decision matrix");
    /* ---- 4. atexit/_cexit: LIFO, cap, reset/reuse -------------------------- */
    log_reset();
    assert(io_crt_atexit(a_A) == 0 && io_crt_atexit(a_B) == 0 && io_crt_atexit(a_C) == 0);
    assert(mb_win_crt_atexit_count() == 3);
    io_cexit();
    assert(strcmp(g_log, "CBA") == 0);   /* LIFO */
    assert(mb_win_crt_atexit_count() == 0);
    log_reset();
    io_cexit();
    assert(g_log_n == 0);                /* double _cexit runs nothing */
    int filled = 0;
    for (int n = 0; n < MB_WIN_CRT_ATEXIT_MAX; n++) filled += mb_win_crt_atexit(a_A) == 0;
    assert(filled == MB_WIN_CRT_ATEXIT_MAX);
    assert(mb_win_crt_atexit(a_A) != 0); /* capacity: documented divergence */
    assert(io_crt_atexit(NULL) != 0);    /* NULL registration rejected */
    io_cexit();
    assert(mb_win_crt_atexit_count() == 0);
    puts("PASS atexit/_cexit LIFO/cap/reset");
    /* ---- 5. detach cleanup policy polarities ------------------------------- */
    log_reset();
    mb_win_crt_dll_state_set(MB_WIN_CRT_DLL_STATE_INIT_FAILED);
    assert(!mb_win_crt_dll_detach_should_run_cleanup());
    mb_win_crt_dll_process_detach(g_xp, g_xp + 3, g_xt, g_xt + 2);
    assert(g_log_n == 0); /* failed init: zero cleanup */
    mb_win_crt_dll_state_set(MB_WIN_CRT_DLL_STATE_IDLE);
    assert(!mb_win_crt_dll_detach_should_run_cleanup());
    mb_win_crt_dll_state_set(MB_WIN_CRT_DLL_STATE_INIT_COMPLETE);
    assert(mb_win_crt_dll_detach_should_run_cleanup());
    puts("PASS detach cleanup policy");
    /* ---- 6. full scenario: successful attach/detach ------------------------- */
    mb_win_crt_init_fn g_xc_atexit[] = {ctor_registers_atexit, NULL};
    log_reset();
    mb_win_crt_dll_state_set(MB_WIN_CRT_DLL_STATE_IDLE);
    assert(mb_win_crt_atexit_count() == 0);
    assert(mb_win_crt_dll_process_attach(g_xi, g_xi + 5, g_xc_atexit, g_xc_atexit + 2) == 0);
    assert(t_state_is(MB_WIN_CRT_DLL_STATE_INIT_COMPLETE));
    assert(strcmp(g_log, "132+") == 0); /* xi walk then xc walk, then user-visible init */
    log_reset();
    mb_win_crt_dll_process_detach(g_xp, g_xp + 3, g_xt, g_xt + 2);
    assert(strcmp(g_log, "42B") == 0); /* pre-term '4', term '2', atexit 'B' LIFO last */
    puts("PASS attach/detach success scenario");
    /* ---- 7. full scenario: mid-init failure --------------------------------- */
    mb_win_crt_init_fn g_xc_fault[] = {ctor_faults, NULL};
    log_reset();
    mb_win_crt_dll_state_set(MB_WIN_CRT_DLL_STATE_IDLE);
    assert(io_crt_atexit(a_A) == 0); /* registered before the fault: must NOT run */
    assert(mb_win_crt_dll_process_attach(g_xi, g_xi + 5, g_xc_fault, g_xc_fault + 2) == -1);
    assert(t_state_is(MB_WIN_CRT_DLL_STATE_INIT_FAILED));
    log_reset();
    mb_win_crt_dll_process_detach(g_xp, g_xp + 3, g_xt, g_xt + 2);
    assert(g_log_n == 0); /* no terminators, no atexit */
    puts("PASS attach failure scenario (cleanup skipped)");
    /* ---- 8. onexit table family (census-observed UCRT signatures) ----------- */
    mb_win_crt_onexit_table table;
    assert(io_initialize_onexit_table(NULL) != 0); /* documented EINVAL path */
    assert(io_initialize_onexit_table(&table) == 0);
    assert(table.first == NULL && table.last == NULL && table.end == NULL);
    assert(io_register_onexit_function(&table, a_A) == NULL); /* unbacked: no fake success */
    static void *slots[4];
    assert(mb_win_crt_onexit_table_attach_storage(&table, slots, 4) == 0);
    assert(io_register_onexit_function(NULL, a_A) == NULL);
    assert(io_register_onexit_function(&table, NULL) == NULL);
    assert(io_register_onexit_function(&table, a_A) == (void *)(uintptr_t)a_A);
    assert(io_register_onexit_function(&table, a_B) == (void *)(uintptr_t)a_B);
    assert(io_register_onexit_function(&table, a_C) == (void *)(uintptr_t)a_C);
    assert(io_execute_onexit_table(NULL) != 0);
    log_reset();
    assert(io_execute_onexit_table(&table) == 0);
    assert(strcmp(g_log, "CBA") == 0); /* LIFO */
    assert(table.last == table.first && table.first != NULL);
    for (int n = 0; n < 4; n++) assert(slots[n] == NULL); /* executed slots cleared */
    log_reset();
    assert(io_execute_onexit_table(&table) == 0 && g_log_n == 0); /* double execute: no-op */
    assert(io_register_onexit_function(&table, a_A) == (void *)(uintptr_t)a_A); /* reuse after execute */
    log_reset();
    assert(io_execute_onexit_table(&table) == 0 && strcmp(g_log, "A") == 0);
    /* capacity: 4 slots, the fifth registration fails explicitly */
    for (int n = 0; n < 4; n++) assert(io_register_onexit_function(&table, a_B) == (void *)(uintptr_t)a_B);
    assert(io_register_onexit_function(&table, a_C) == NULL); /* full: no allocation */
    log_reset();
    assert(io_execute_onexit_table(&table) == 0 && g_log_n == 4);
    /* re-attach only on an empty table; pending registrations refuse */
    assert(io_register_onexit_function(&table, a_A) == (void *)(uintptr_t)a_A);
    assert(mb_win_crt_onexit_table_attach_storage(&table, slots, 4) != 0);
    assert(io_execute_onexit_table(&table) == 0);
    assert(mb_win_crt_onexit_table_attach_storage(&table, slots, 4) == 0); /* re-init OK */
    assert(table.first == (uintptr_t *)slots && table.last == table.first && table.end == table.first + 4);
    puts("PASS onexit table initialize/register/execute LIFO/cap/reuse");
    /* ---- 9. binder and session refusals ------------------------------------- */
    uint64_t out = 0;
    module.state = MB_PE_MODULE_LOADING;
    /* ASCII-folded library already exercised by bind(); exact-fold name: */
    assert(try_bind("api-ms-win-crt-runtime-l1-1-0.dll", 33, "_INITTERM", 0, &out) == MB_PE_BIND_UNRESOLVED);
    assert(try_bind("api-ms-win-crt-runtime-l1-1-0.dll", 33, "_initterm", 1, &out) == MB_PE_BIND_UNRESOLVED); /* ordinal */
    assert(try_bind("api-ms-win-crt-runtime-l1-1-0.dll", 33, "_not_a_crt_name", 0, &out) == MB_PE_BIND_UNRESOLVED);
    assert(try_bind("kernel32.dll", 12, "_initterm", 0, &out) == MB_PE_BIND_UNRESOLVED); /* wrong library */
    /* census-observed out-of-scope names fall through to their owning families */
    assert(try_bind("api-ms-win-crt-runtime-l1-1-0.dll", 33, "_initialize_narrow_environment", 0, &out) == MB_PE_BIND_UNRESOLVED);
    assert(try_bind("api-ms-win-crt-runtime-l1-1-0.dll", 33, "_errno", 0, &out) == MB_PE_BIND_UNRESOLVED);
    assert(try_bind("api-ms-win-crt-stdio-l1-1-0.dll", 31, "__stdio_common_vsprintf", 0, &out) == MB_PE_BIND_UNRESOLVED);
    /* documented vcruntime home for the filter, NOT observed in either census DLL */
    assert(try_bind("VCRUNTIME140.dll", 16, "_seh_filter_dll", 0, &out) == MB_PE_BIND_BOUND);
    assert(try_bind("vcruntime140.dll", 16, "_crt_atexit", 0, &out) == MB_PE_BIND_UNRESOLVED);
    /* canonical ucrtbase home of the same runtime exports */
    assert(try_bind("UCRTBASE.dll", 12, "_cexit", 0, &out) == MB_PE_BIND_BOUND);
    /* ERROR polarity: invalid args; BOUND-only writes */
    out = 0xfeedfaceull;
    assert(mb_win_crt_startup_bind(&session, NULL, &out) == MB_PE_BIND_ERROR && out == 0xfeedfaceull);
    assert(mb_win_crt_startup_bind(NULL, NULL, &out) == MB_PE_BIND_ERROR);
    assert(try_bind("", 0, "_initterm", 0, &out) == MB_PE_BIND_UNRESOLVED); /* empty library: family contract, not ERROR */
    assert(try_bind(NULL, 5, "_initterm", 0, &out) == MB_PE_BIND_ERROR); /* borrowed data/size mismatch */
    /* session lifecycle */
    assert(mb_win_crt_startup_begin_session(&session, &module) == MB_WIN_CRT_STARTUP_BAD_STATE); /* already active */
    mb_pe_module other; memset(&other, 0, sizeof other);
    assert(mb_win_crt_startup_begin_session(&session, &other) == MB_WIN_CRT_STARTUP_BAD_STATE);
    mb_win_crt_startup_session second; memset(&second, 0, sizeof second);
    assert(mb_win_crt_startup_begin_session(&second, &module) == MB_WIN_CRT_STARTUP_BAD_STATE); /* module taken */
    assert(mb_win_crt_startup_begin_session(NULL, &module) == MB_WIN_CRT_STARTUP_INVALID_ARGUMENT);
    module.state = MB_PE_MODULE_ACTIVE;
    assert(mb_win_crt_startup_end_session(&session) == MB_WIN_CRT_STARTUP_BUSY); /* ACTIVE module */
    module.state = MB_PE_MODULE_EMPTY;
    assert(mb_win_crt_startup_end_session(&session) == MB_WIN_CRT_STARTUP_OK);
    memset(&session, 0, sizeof session);
    assert(mb_win_crt_startup_end_session(&session) == MB_WIN_CRT_STARTUP_BAD_STATE); /* stale */
    assert(mb_win_crt_startup_begin_session(&session, &module) == MB_WIN_CRT_STARTUP_OK); /* reuse */
    module.state = MB_PE_MODULE_ACTIVE;
    puts("PASS binder fold/ordinal/polarity + session lifecycle");
    mb_win_crt_dll_state_set(MB_WIN_CRT_DLL_STATE_IDLE);
    puts("ALL TESTS PASSED (9/9 suites)");
    return 0;
}
#endif
