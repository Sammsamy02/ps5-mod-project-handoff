// test_exec_write_probe_host.c — record/DECISION logic host suite
// (worker-writability-probe-build, team 2026-10-04).
//
// Drives ig_run_exec_write_probe twice with a MOCKED mprotect
// (tests/xwp_host_shims.c): an ARMED binary and an UNARMED binary share
// this main; the suite's python runner compiles both and validates the
// emitted JSONL records (run_host_checks.py).
//
// Test table (13 rows; see tests/generated/): the armed run must produce
// EXACTLY 8 attempts (the budget) with restore pairing after each, two
// refusals with zero mprotect calls, one design-unconfirmed skip, and
// budget exhaustion for the tail rows. The unarmed run must produce ZERO
// mock-mprotect calls while still emitting the full record sequence with
// outcome mprotect_import_absent (11).
#include "anchor_store.h"
#include "exec_write_probe.h"
#include "record_sink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* From xwp_host_shims.c: */
typedef struct MockCall { uint64_t addr, len, prot; int rc, eno; } MockCall;
extern MockCall g_mock_log[];
extern unsigned g_mock_calls;
extern uint64_t g_mock_fail_addr;
void mock_reset(int fail_addr_set, uint64_t fail_addr);

extern unsigned g_xwp_attempts_used;
extern unsigned g_xwp_rows_seen;
extern unsigned g_xwp_rows_attempted;
extern unsigned g_xwp_rows_refused;
extern unsigned g_xwp_rows_skipped;

static int g_checks;
static void check(int cond, const char *what) {
    ++g_checks;
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", what);
        exit(1);
    }
}

/* Test-table addresses (mirrored in tests/generated + the mock map). */
#define A_RX_OK      0x180000000ull
#define A_UNCONF     0x185000000ull
#define A_RWX_FAIL   0x181000000ull
#define A_MISMATCH   0x183000000ull
#define A_UNCOVERED  0x184000000ull
#define A_RWX_OK     0x182000000ull
#define A_BUDGET(k)  (0x190000000ull + 0x100000ull * (k))
#define PAGE 0x1000ull

int main(int argc, char **argv) {
    const char *out_path = argc > 1 ? argv[1] : "out-host.jsonl";
    const char *mirror_path = argc > 2 ? argv[2] : "out-host-mirror.log";
    int armed = argc > 3 ? atoi(argv[3]) : -1;
    if (armed != 0 && armed != 1) {
        fprintf(stderr, "usage: %s <out.jsonl> <mirror.log> <0|1>\n", argv[0]);
        return 2;
    }

    IgSink js, ms;
    if (ig_sink_open(&js, out_path) != 0) return 2;
    if (ig_sink_open(&ms, mirror_path) != 0) return 2;

    /* ---- armed scenario: scripted failure on the RWX-fail row. ---- */
    mock_reset(1, A_RWX_FAIL);
    int rc = ig_run_exec_write_probe(&js, &ms);
    check(rc == 0, "run returns 0 (fail-open)");

    if (armed) {
        /* EXACTLY the budget in mock calls: 8 attempts + 8 restores. */
        check(g_mock_calls == 16, "armed run makes exactly 16 mock calls "
                                  "(8 attempts + 8 restores)");
        check(g_xwp_attempts_used == 8, "attempt budget reached exactly 8");
        check(g_xwp_rows_seen == xwp_transitions_count, "all rows seen");
        check(g_xwp_rows_attempted == 8, "8 rows attempted");
        check(g_xwp_rows_refused == 2, "2 rows refused (mismatch, uncovered)");
        check(g_xwp_rows_skipped == 3, "3 rows skipped (unconf + 2 budget)");

        /* Pairing/order: every attempt is immediately followed by a
           restore to from_prot on the same address. */
        for (unsigned k = 0; k < 8; ++k) {
            const MockCall *att = &g_mock_log[2 * k];
            const MockCall *res = &g_mock_log[2 * k + 1];
            check(att->addr == res->addr, "restore targets the attempted addr");
            check(res->prot == XWP_PROT_X, "restore requests from_prot=X");
            check(res->rc == 0, "restore succeeds for every scripted row");
            check(att->len == PAGE, "attempt length == page_count*0x1000");
        }
        /* X->RX first (attempt 0), then X->RWX (attempt 1 = the failing
           far-page row; attempt 2 = the RWX-ok row). */
        check(g_mock_log[0].addr == A_RX_OK && g_mock_log[0].prot == (XWP_PROT_R | XWP_PROT_X),
              "FIRST attempt is X->RX on the confirmed row");
        check(g_mock_log[2].addr == A_RWX_FAIL && g_mock_log[2].prot == (XWP_PROT_R | XWP_PROT_W | XWP_PROT_X),
              "second attempt is X->RWX (scripted failure)");
        check(g_mock_log[2].rc == -1 && g_mock_log[2].eno == 12 /*ENOMEM*/,
              "scripted RWX failure carries rc=-1 errno=ENOMEM");
        check(g_mock_log[4].addr == A_RWX_OK && g_mock_log[4].prot == (XWP_PROT_R | XWP_PROT_W | XWP_PROT_X),
              "third attempt is the X->RWX far-page success");
        /* Rows that must NEVER reach mprotect: */
        for (unsigned k = 0; k < 16; ++k) {
            check(g_mock_log[k].addr != A_UNCONF, "unconfirmed row never calls mprotect");
            check(g_mock_log[k].addr != A_MISMATCH, "prot-mismatch row never calls mprotect");
            check(g_mock_log[k].addr != A_UNCOVERED, "uncovered row never calls mprotect");
        }
        /* Budget rows 6..10 attempted (k=0..4), 11..12 skipped: */
        check(g_mock_log[6].addr == A_BUDGET(0), "attempt 4 is the first budget row");
        check(g_mock_log[14].addr == A_BUDGET(4), "attempt 8 is budget row k=4");
    } else {
        check(g_mock_calls == 0, "UNARMED build makes ZERO mprotect mock calls");
        check(g_xwp_rows_attempted == 8, "unarmed run stub-attempts 8 rows");
        check(g_xwp_rows_refused == 2, "unarmed gates still refuse 2 rows");
        check(g_xwp_rows_skipped == 3, "unarmed run still skips 3 rows");
        check(g_xwp_rows_seen == xwp_transitions_count, "unarmed run sees every row");
    }

    (void)ig_sink_close(&js);
    (void)ig_sink_close(&ms);
    printf("PASS %s: %d checks\n", armed ? "armed" : "unarmed", g_checks);
    return 0;
}
