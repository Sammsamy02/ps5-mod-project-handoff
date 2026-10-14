#ifndef MB_OWNED_WORKER_CRTSTARTUP_H
#define MB_OWNED_WORKER_CRTSTARTUP_H
/* Combined-payload fixture hook (worker-combined-dll-allfam): the census CRT
 * DLL startup block through actual PE imports. All seven exercised names are
 * the census-observed api-ms-win-crt-runtime-l1-1-0.dll startup imports
 * (worker-dll-mod-retention/baseline-dll-census.json): _initterm, _initterm_e,
 * _crt_atexit, _cexit, _initialize_onexit_table, _register_onexit_function,
 * _execute_onexit_table. _seh_filter_dll (vcruntime140.dll home, zero observed
 * census rows) stays unexercised this pass. Semantics per mb_win_crt_startup.h:
 * the onexit table arrives initialized-but-unbacked (registration returns NULL
 * until storage is attached by the entry adapter), the global atexit table
 * runs LIFO through _cexit, and _initterm_e stops at the first nonzero
 * errno_t. Error returns -90..-97 are unique across this tree's fixtures
 * (fileio -80..-89). Runs in the consumer exercise window with the module
 * ACTIVE, which the binder accepts (LOADING or ACTIVE). */
typedef __INT32_TYPE__ mb_crt_i32;
typedef int (*mb_crt_init_fn)(void);
typedef void (*mb_crt_exit_fn)(void);
struct mb_crt_onexit_table { mb_crt_exit_fn **first, **last, **end; };
__declspec(dllimport) void _initterm(mb_crt_init_fn *begin, mb_crt_init_fn *end);
__declspec(dllimport) mb_crt_i32 _initterm_e(mb_crt_init_fn *begin, mb_crt_init_fn *end);
__declspec(dllimport) mb_crt_i32 _crt_atexit(mb_crt_exit_fn fn);
__declspec(dllimport) void _cexit(void);
__declspec(dllimport) mb_crt_i32 _initialize_onexit_table(struct mb_crt_onexit_table *table);
__declspec(dllimport) mb_crt_exit_fn *_register_onexit_function(struct mb_crt_onexit_table *table, mb_crt_exit_fn fn);
__declspec(dllimport) mb_crt_i32 _execute_onexit_table(struct mb_crt_onexit_table *table);
static struct {
    unsigned walker, walker_e, atexit_ran;
    mb_crt_i32 initterm_e_stop;
} mb_crt_state;
static int mb_crt_w1(void) { ++mb_crt_state.walker; return 0; }
static int mb_crt_w2(void) { ++mb_crt_state.walker; return 0; }
static int mb_crt_we_ok(void) { ++mb_crt_state.walker_e; return 0; }
static int mb_crt_we_fail(void) { mb_crt_state.initterm_e_stop = 99; return 99; }
static void mb_crt_atexit_fn(void) { ++mb_crt_state.atexit_ran; }
/* Every exercised name returns its documented value; any deviation aborts the
 * exercise with a unique negative code before the 86/93 base results. */
static mb_crt_i32 mb_worker_crtstartup_exercise(void) {
    mb_crt_state.walker = 0; mb_crt_state.walker_e = 0;
    mb_crt_state.atexit_ran = 0; mb_crt_state.initterm_e_stop = 0;
    mb_crt_init_fn blind[3] = {mb_crt_w1, 0, mb_crt_w2}; /* interior sentinel NULL skipped */
    _initterm(blind, blind + 3);
    if(mb_crt_state.walker != 2)return -90;
    mb_crt_init_fn ok[2] = {mb_crt_we_ok, 0};
    if(_initterm_e(ok, ok + 2) != 0 || mb_crt_state.walker_e != 1)return -91;
    mb_crt_init_fn stop[1] = {mb_crt_we_fail};
    if(_initterm_e(stop, stop + 1) != 99)return -92;
    if(_crt_atexit(mb_crt_atexit_fn) != 0)return -93;
    _cexit();
    if(mb_crt_state.atexit_ran != 1)return -94;
    struct mb_crt_onexit_table table;
    if(_initialize_onexit_table(&table) != 0)return -95;
    /* Unbacked table: registration fails (NULL), execution is a 0 no-op. */
    if(_register_onexit_function(&table, mb_crt_atexit_fn) != 0)return -96;
    if(_execute_onexit_table(&table) != 0)return -97;
    return 1;
}
#endif
