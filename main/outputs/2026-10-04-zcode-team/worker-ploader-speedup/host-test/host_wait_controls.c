// Host-only wait controls: the REAL extracted wait_stable readiness logic from a
// ploader main.c; all privileged/external operations are substituted at their
// external call boundary (kill/usleep/kernel_dynlib_*/pt_resolve/clock_gettime).
// No native loader, no SDK, no ptrace, no console.
//
// Proves (worker-ploader-speedup, patch 05r):
//   1. the wall-clock deadline fires before tick exhaustion when per-tick cost
//      exceeds the sleep (the measured ~129ms/tick launch-10 burn class), and
//   2. a later plugin for the same pid skips the 500-tick burn via the cached
//      PROVEN-stable re-check, while genuine regression fails closed into the
//      full wait.
// Built against the unamended source too (red run): controls 02 and 03 then
// fail by construction - they ARE the two behavioral deltas of this patch.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>

// ---- simulated clock and environment knobs ---------------------------------
static uint64_t g_fake_us;        // simulated monotonic clock, microseconds
static unsigned g_usleep_calls;   // number of recorded sleeps
static unsigned g_compute_us;     // simulated kernel-call compute charged per tick
static int      g_alive = 1;      // kill(pid,0) verdict
static int      g_dep_ok = 1;     // are the three core deps resolvable
static int      g_count = 20;     // module count when fixed
static int      g_flap;           // if set, module count alternates every read
static unsigned g_count_reads;

static void fake_clock_gettime(struct timespec* ts) {
    ts->tv_sec = (time_t)(g_fake_us / 1000000ull);
    ts->tv_nsec = (long)((g_fake_us % 1000000ull) * 1000ull);
}
static int mock_kill(pid_t pid, int sig) { (void)pid; (void)sig; return g_alive ? 0 : -1; }
static int mock_usleep(unsigned us) {
    g_fake_us += (uint64_t)us + g_compute_us; // sleep + simulated per-tick compute
    g_usleep_calls++;
    return 0;
}
static int mock_dynlib_handle(pid_t pid, const char* name, uint32_t* h) {
    (void)pid; (void)name;
    if (!g_dep_ok) return -1;
    static uint32_t next = 1;
    if (next > 3) next = 1;
    *h = next++;
    return 0;
}
static intptr_t mock_mapbase(pid_t pid, uint32_t h) {
    (void)pid;
    // model count_modules: handles 0..g_count-1 map, the rest do not; with the
    // flap knob the module space MOVES (count alternates) on every read, so a
    // stable count never holds for even two consecutive ticks
    g_count_reads++;
    int present = g_count - (int)((g_flap && (g_count_reads & 1)) ? 1 : 0);
    uint32_t idx = (h < 0x40) ? h : (h >= 0x2000 && h < 0x2040 ? h - 0x2000 + 0x40 : 0xFFFF);
    return (g_dep_ok && idx < (uint32_t)present) ? 0x2000 : 0;
}
static int mock_pt_resolve(pid_t pid) { (void)pid; return g_dep_ok ? 1 : 0; }

// route the extracted code to the mocks at its external call boundary
#define clock_gettime(clk, tp) fake_clock_gettime(tp)
#define kill                   mock_kill
#define usleep                 mock_usleep
#define kernel_dynlib_handle   mock_dynlib_handle
#define kernel_dynlib_mapbase_addr mock_mapbase
#define pt_resolve(p, nid)     mock_pt_resolve(p)
#include "extracted.inc"

// keep now_ms referenced on UNAMENDED sources (no wall-clock deadline there
// calls it) so the red run compiles and fails its controls at RUNTIME, which
// is the honest red signal, rather than at compile time
static long __attribute__((unused)) host_now_ms_keepalive(void) { return now_ms(); }

// ---- control helpers --------------------------------------------------------
static char g_needs[3][MAX_DEP_NAME_LEN];
static void reset_env(void) {
    g_fake_us = 0; g_usleep_calls = 0; g_compute_us = 0;
    g_alive = 1; g_dep_ok = 1; g_count = 20; g_flap = 0; g_count_reads = 0;
    memset(&g_t0, 0, sizeof g_t0);
#ifdef HAS_CACHE
    proven_stable_pid = 0;
#endif
    snprintf(g_needs[0], MAX_DEP_NAME_LEN, "libkernel.sprx");
    snprintf(g_needs[1], MAX_DEP_NAME_LEN, "libc.prx");
    snprintf(g_needs[2], MAX_DEP_NAME_LEN, "libSceFios2.prx");
}
static uint64_t phase_started_us;
static unsigned phase_started_sleeps;
static void mark(void) { phase_started_us = g_fake_us; phase_started_sleeps = g_usleep_calls; }
static uint64_t elapsed_us(void) { return g_fake_us - phase_started_us; }
static unsigned sleeps_in_phase(void) { return g_usleep_calls - phase_started_sleeps; }

static int run_wait(pid_t pid, int mode, int max_ms) {
    mode_params_t mp = get_mode_params(mode);
    return wait_stable(pid, g_needs, 3, &mp, max_ms);
}

// ---- controls ----------------------------------------------------------------
static int c01_fast_path_proves(void) {
    reset_env(); mark();
    int rc = run_wait(7, 2, 25000); // mode 2: stable_ms 1000 -> 20 ticks, settle 600
    if (rc != 1) return 0;
    // 19 tick sleeps + 1 settle sleep; 19*50ms + 600ms = 1550ms (fast path unchanged)
    if (sleeps_in_phase() != 20) return 0;
    if (elapsed_us() != 1550000ull) return 0;
#ifdef HAS_CACHE
    if (proven_stable_pid != 7) return 0; // fast path PROVED it: memo populated
#endif
    return 1;
}
static int c02_second_plugin_skips_burn(void) {
    reset_env();
    if (run_wait(7, 2, 25000) != 1) return 0; // plugin 1 pays the fast path
    mark();
    int rc = run_wait(7, 2, 25000);           // plugin 2, same boot
    if (rc != 1) return 0;
    // cached path: exactly 4 re-check ticks (200ms); the re-paid burn would be
    // 20 ticks here (fast) or 500 ticks mid-boot - never 4
    if (sleeps_in_phase() != 4) return 0;
    if (elapsed_us() != 200000ull) return 0;
    return 1;
}
static int c03_wallclock_deadline_fires(void) {
    reset_env();
#ifdef HAS_CACHE
    proven_stable_pid = 0;
#endif
    g_flap = 1;          // module count moves every read: never 20 stable ticks
    g_compute_us = 79000; // measured launch-10 class: 50ms sleep + ~264 kernel
                          // calls ~ 129ms/tick -> unbounded burn would be
                          // 500 * 129ms = 64.5s wall (the measured 64.6s)
    mark();
    int rc = run_wait(7, 2, 25000);
    if (rc != 1) return 0;               // "try anyway if still alive" preserved
    if (g_alive && rc != 1) return 0;
    uint64_t el = elapsed_us();
    unsigned n = sleeps_in_phase();
    if (n >= 500) return 0;              // tick-count bound would hit 500
    if (el < 25000000ull) return 0;      // never before the wall budget
    if (el > 26400000ull) return 0;      // deadline fired: ~25026ms, not 64.5s
    return 1;
}
static int c04_regression_dep_dropped_fails_closed(void) {
    reset_env();
    if (run_wait(7, 2, 25000) != 1) return 0; // proven
    g_dep_ok = 0;                             // stability genuinely regressed
    mark();
    int rc = run_wait(7, 2, 25000);
    if (rc != 1) return 0;                    // alive: try-anyway semantics unchanged
    if (sleeps_in_phase() <= 4) return 0;     // cache did NOT skip: full revalidation ran
#ifdef HAS_CACHE
    if (proven_stable_pid != 0) return 0;     // exhaust path never re-caches
#endif
    return 1;
}
static int c05_regression_count_flap_fails_closed(void) {
    reset_env();
    if (run_wait(7, 2, 25000) != 1) return 0; // proven
    g_flap = 1;                               // module space moving again
    mark();
    int rc = run_wait(7, 2, 25000);
    if (rc != 1) return 0;
    if (sleeps_in_phase() <= 4) return 0;     // flapping count detected, fell through
    return 1;
}
static int c06_death_in_cache_recheck_returns_zero(void) {
    reset_env();
    if (run_wait(7, 2, 25000) != 1) return 0; // proven
    g_alive = 0;                              // process died between plugins
    mark();
    int rc = run_wait(7, 2, 25000);
    if (rc != 0) return 0;                    // death semantics unchanged
    if (sleeps_in_phase() != 0) return 0;     // fails immediately, no burn either way
    return 1;
}
static int c07_exhaust_path_never_caches(void) {
    reset_env();
    g_flap = 1; g_compute_us = 0;             // never stable; sleep-only 50ms ticks
    if (run_wait(7, 2, 25000) != 1) return 0; // exhaust: try anyway
    // tick exhaustion with NO proof: the memo must still be empty, so a second
    // caller re-pays the full wait (fail-closed; only PROVEN stability caches)
    if (sleeps_in_phase() < 500) {
        // patched code bounds this at the deadline; either way not cached
    }
    mark();
#ifdef HAS_CACHE
    if (proven_stable_pid != 0) return 0;
#endif
    unsigned before = g_usleep_calls;
    if (run_wait(7, 2, 25000) != 1) return 0;
    if (g_usleep_calls - before < 500) return 0; // re-paid in full: nothing was cached
    return 1;
}
static int c08_different_pid_never_reuses_memo(void) {
#ifdef HAS_CACHE
    reset_env();
    if (run_wait(7, 2, 25000) != 1) return 0; // pid 7 proven
    mark();
    if (run_wait(8, 2, 25000) != 1) return 0; // different pid: full wait required
    if (sleeps_in_phase() != 20) return 0;    // fast path, NOT the 4-tick cached path
    if (proven_stable_pid != 8) return 0;
    return 1;
#else
    reset_env(); mark();
    return run_wait(8, 2, 25000) == 1 && sleeps_in_phase() == 20;
#endif
}
static int c09_died_while_waiting_first_call(void) {
    reset_env();
    g_alive = 0;                              // dies before any proof exists
    mark();
    if (run_wait(7, 2, 25000) != 0) return 0;
    if (sleeps_in_phase() != 0) return 0;
    return 1;
}

int main(void) {
    const struct { const char* name; int (*fn)(void); } controls[] = {
        {"01-fast-path-proves-and-settles", c01_fast_path_proves},
        {"02-second-plugin-skips-burn", c02_second_plugin_skips_burn},
        {"03-wallclock-deadline-fires", c03_wallclock_deadline_fires},
        {"04-regression-dep-dropped-fails-closed", c04_regression_dep_dropped_fails_closed},
        {"05-regression-count-flap-fails-closed", c05_regression_count_flap_fails_closed},
        {"06-death-in-cache-recheck-returns-zero", c06_death_in_cache_recheck_returns_zero},
        {"07-exhaust-path-never-caches", c07_exhaust_path_never_caches},
        {"08-different-pid-never-reuses-memo", c08_different_pid_never_reuses_memo},
        {"09-died-while-waiting-first-call", c09_died_while_waiting_first_call},
    };
    int passed = 0, failed = 0;
    for (size_t i = 0; i < sizeof controls / sizeof controls[0]; i++) {
        int ok = controls[i].fn();
        printf("%s %s\n", ok ? "PASS" : "FAIL", controls[i].name);
        ok ? passed++ : failed++;
    }
    printf("wait-controls: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
