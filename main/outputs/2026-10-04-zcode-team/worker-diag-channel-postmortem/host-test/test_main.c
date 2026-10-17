/* test_main.c — scenario runner for the composition-5 fail-open host test.
 *
 * Each scenario runs in its own process (fresh statics: entry_guard,
 * lifecycle, channel fds). The runner (run_host_test.sh) passes two temp
 * paths: the translated /data/plugins/UE4SS.log and the shim diagnostic
 * channel. Exit 0 iff every assertion of the scenario passes.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lifecycle.h"
#include "diagnostic_shim.h"

int module_start(size_t argc, const void *args);

static int failures;
static int checks;

static void expect(int ok, const char *scenario, const char *what)
{
    ++checks;
    if (ok) {
        printf("PASS %s: %s\n", scenario, what);
    } else {
        ++failures;
        printf("FAIL %s: %s\n", scenario, what);
    }
}

static int count_lines(const char *path, const char *needle, char first_line_out[256])
{
    FILE *f = fopen(path, "r");
    if (!f)
        return -1; /* absent */
    char buf[512];
    int count = 0;
    first_line_out[0] = '\0';
    while (fgets(buf, sizeof(buf), f)) {
        if (strstr(buf, needle)) {
            if (count == 0) {
                size_t n = strlen(buf);
                if (n >= 256)
                    n = 255;
                memcpy(first_line_out, buf, n);
                first_line_out[n] = '\0';
            }
            ++count;
        }
    }
    fclose(f);
    return count;
}

static void set_paths(int argc, char **argv)
{
    /* argv[1] is the scenario name; the runner passes the UE4SS failover log
     * as argv[2] and the diagnostic channel path as argv[3]. (The original
     * draft read argv[1]/argv[2], which redirected the failover writes into
     * scenario-named relative files that accumulated across runs — the cause
     * of the 2026-10-04 HOST-TEST-FAIL receipt.) */
    if (argc >= 4) {
        snprintf(g_failover_log_path, sizeof(g_failover_log_path), "%s", argv[2]);
        snprintf(g_diag_channel_path, sizeof(g_diag_channel_path), "%s", argv[3]);
    }
}

int main(int argc, char **argv)
{
    const char *scenario = argc >= 2 ? argv[1] : "";
    set_paths(argc, argv);
    g_diag_ready = 1;

    if (strcmp(scenario, "t1_channel_open_failed") == 0) {
        /* Diagnostic channel open fails (e.g. EACCES on /data probe log). */
        g_diag_open_result = -2;
        int rc = module_start(0, NULL);
        char line[256];
        int n = count_lines(g_failover_log_path, "modbridge-entry-failover/v1 ", line);
        expect(rc == -22, scenario, "module_start returned -22");
        expect(n == 1, scenario, "exactly one failover line appended to UE4SS.log");
        expect(n == 1 && strstr(line, "channel_open_failed") != NULL, scenario,
               "line carries distinct token channel_open_failed");
        expect(n == 1 && strstr(line, "rc=0xfffffffe") != NULL, scenario,
               "line carries the failed open return in hex (rc=0xfffffffe for -2)");
        /* The diag channel itself must have received nothing. */
        char scratch1[256];
        expect(count_lines(g_diag_channel_path, "module_start_entered", scratch1) <= 0, scenario,
               "no module_start_entered event reached the diagnostic channel");
    } else if (strcmp(scenario, "t2_bindings_null") == 0) {
        /* diagnostic_ready() false. NOTE: on this host the kernel symbols are
         * real functions, so the failover write itself succeeds; on console
         * with truly unresolved imports nothing can be written (documented
         * structural limit, see FAILOPEN-PATCH.md). */
        g_diag_ready = 0;
        int rc = module_start(0, NULL);
        char line[256];
        int n = count_lines(g_failover_log_path, "modbridge-entry-failover/v1 ", line);
        expect(rc == -22, scenario, "module_start returned -22");
        expect(n == 1, scenario, "exactly one failover line appended");
        expect(n == 1 && strstr(line, "diag_bindings_null") != NULL, scenario,
               "line carries distinct token diag_bindings_null");
        expect(n == 1 && strstr(line, "rc=0xffffffea") != NULL, scenario,
               "line carries rc=-22 in hex");
    } else if (strcmp(scenario, "t3_wrong_state") == 0) {
        /* Drive state to FINISHED with a failed start, then re-enter. */
        g_stub_start_rc = -1;
        g_stub_final_state = PROBE_FINISHED;
        int rc1 = module_start(0, NULL);
        char line[256];
        int after_first = count_lines(g_failover_log_path, "modbridge-entry-failover/v1 ", line);
        expect(rc1 == -1, scenario, "first module_start returns the failed start rc (-1)");
        expect(after_first <= 0, scenario,
               "no failover line for the in-scope-only -1 path (no -20/-21/-22 hit)");
        g_stub_start_rc = 0;
        int rc2 = module_start(0, NULL);
        int after_second = count_lines(g_failover_log_path, "modbridge-entry-failover/v1 ", line);
        expect(rc2 == -21, scenario, "re-entry with FINISHED state returns -21");
        expect(after_second == 1, scenario, "exactly one failover line after -21");
        expect(after_second == 1 && strstr(line, "wrong_state") != NULL, scenario,
               "line carries distinct token wrong_state");
        expect(after_second == 1 && strstr(line, "rc=0xffffffeb") != NULL, scenario,
               "line carries rc=-21 in hex");
    } else if (strcmp(scenario, "t4_guard_busy") == 0) {
        /* Recursive re-entry while the first module_start holds the guard. */
        g_stub_recursive_reentry = 1;
        int rc = module_start(0, NULL);
        char line[256];
        int n = count_lines(g_failover_log_path, "modbridge-entry-failover/v1 ", line);
        expect(rc == 0, scenario, "outer module_start still completed (returned 0)");
        expect(n == 1, scenario, "exactly one failover line for the inner -20");
        expect(n == 1 && strstr(line, "entry_guard_busy") != NULL, scenario,
               "line carries distinct token entry_guard_busy");
        expect(n == 1 && strstr(line, "rc=0xffffffec") != NULL, scenario,
               "line carries rc=-20 in hex");
    } else if (strcmp(scenario, "t5_success_silent") == 0) {
        /* Healthy open + successful start: NO failover output, and the
         * idempotent PROBE_ACTIVE re-entry stays silent too. */
        g_stub_start_rc = 0;
        g_stub_final_state = PROBE_ACTIVE;
        int rc1 = module_start(0, NULL);
        int rc2 = module_start(0, NULL);
        char line[256];
        int n = count_lines(g_failover_log_path, "modbridge-entry-failover/v1 ", line);
        expect(rc1 == 0, scenario, "first module_start returned 0");
        expect(rc2 == 0, scenario, "idempotent ACTIVE re-entry returned 0");
        expect(n <= 0, scenario, "zero failover lines on the success path");
        char scratch2[256];
        expect(count_lines(g_diag_channel_path, "module_start_entered", scratch2) == 1, scenario,
               "positive control: success path emitted module_start_entered on the diag channel");
    } else if (strcmp(scenario, "t7_failover_refused") == 0) {
        /* Worst case: channel fails AND the UE4SS.log append is refused.
         * Must still return -22 without crashing; no line can exist. */
        g_diag_open_result = -2;
        g_kernel_failover_open_result = -13;
        int rc = module_start(0, NULL);
        char line[256];
        int n = count_lines(g_failover_log_path, "modbridge-entry-failover/v1 ", line);
        expect(rc == -22, scenario, "module_start still returned -22");
        expect(n <= 0, scenario, "no failover line could be written (graceful)");
        expect(count_lines(g_diag_channel_path, "module_start_entered", line) <= 0, scenario,
               "no diagnostic events either");
    } else {
        printf("FAIL unknown scenario '%s'\n", scenario);
        return 2;
    }

    printf("%s %s checks=%d failures=%d\n", failures ? "SCENARIO-FAIL" : "SCENARIO-PASS",
           scenario, checks, failures);
    return failures ? 1 : 0;
}
