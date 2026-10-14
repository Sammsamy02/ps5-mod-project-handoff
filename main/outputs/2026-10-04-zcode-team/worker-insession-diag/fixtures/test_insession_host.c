/* test_insession_host.c — host fixture for the in-session command consumer.
   worker-insession-diag, 2026-10-05. HOST TESTS ONLY — never compiled into
   any console PRX. Pattern: worker-diagnostic-sink fixtures (staged source +
   POSIX shims + strict python post-validation in run_host_tests.sh).

   Scenarios:
     s0  grammar-v1 table (36 cases, direct parser calls)
     s1  idle polls: no intake file -> opens happen, nothing else does
     s2  diag end-to-end through the REAL staged diagnostic.c: sink row
         tag=insession_probe1 value=7, entry log line, consumed byte-identical
     s3  noop: ack log line only, sink untouched, consumed
     s4  malformed: loud REJECT, consumed, never re-read (3 polls, 1 line)
     s5  unknown cmd "reload": REJECTED, NEVER executed (v1 safety core)
     s6  oversize >=512B: rejected without parsing, consumed
     s7  rename-failure injection: execute-then-rename => at-least-once
         (re-execution after failed consume), then successful consume
     s8  (run script) strict python validation of every sink/log file

   INTAKE_PATH / CONSUMED_PATH / INTAKE_TESTLOG / SUPPLIER_DIAG_PATH are
   passed on the compiler command line by run_host_tests.sh.
*/
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "insession_cmd_core.h"
#include "insession_cmd_poll.h"
#include "diagnostic.h" /* diagnostic_open_channel: module_start's job on console */

static int g_fails = 0;
static int g_checks = 0;
static int g_scenario = 0;

#define CHECK(cond, ...)                                                    \
    do                                                                      \
    {                                                                       \
        ++g_checks;                                                         \
        if (!(cond))                                                        \
        {                                                                   \
            ++g_fails;                                                      \
            printf("[host-test] FAIL s%d: ", g_scenario);                   \
            printf(__VA_ARGS__);                                            \
            printf("\n");                                                   \
        }                                                                   \
    } while (0)

/* ---- test log capture (called by host_shims.c) ---- */
void test_emit_log_line(const char *line)
{
    FILE *f = fopen(INTAKE_TESTLOG, "a");
    if (f)
    {
        fputs(line, f);
        fputc('\n', f);
        fclose(f);
    }
}

static int testlog_count(const char *needle)
{
    char buf[512];
    int count = 0;
    FILE *f = fopen(INTAKE_TESTLOG, "r");
    if (!f)
        return 0;
    while (fgets(buf, sizeof(buf), f))
        if (strstr(buf, needle))
            ++count;
    fclose(f);
    return count;
}

/* Cumulative: test_emit_log_line appends; scenarios never truncate, so both
   the in-scenario counts and the end-of-run python totals see every line. */
static void testlog_reset(void) {}

/* ---- file helpers ---- */
static void write_intake(const char *bytes, size_t n)
{
    FILE *f = fopen(INTAKE_PATH, "wb");
    if (!f)
    {
        fprintf(stderr, "cannot stage intake path=%s errno=%s\n", INTAKE_PATH,
                strerror(errno));
        exit(2);
    }
    if (n)
        fwrite(bytes, 1, n, f);
    fclose(f);
}

static void write_intake_str(const char *s) { write_intake(s, strlen(s)); }

static int intake_exists(void)
{
    FILE *f = fopen(INTAKE_PATH, "rb");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

static int consumed_bytes_match(const char *expect, size_t n)
{
    char buf[1024];
    size_t got;
    FILE *f = fopen(CONSUMED_PATH, "rb");
    if (!f)
        return 0;
    got = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    return got == n && memcmp(buf, expect, n) == 0;
}

static long sink_line_count(void)
{
    long lines = 0;
    int c;
    FILE *f = fopen(SUPPLIER_DIAG_PATH, "r");
    if (!f)
        return 0;
    while ((c = fgetc(f)) != EOF)
        if (c == '\n')
            ++lines;
    fclose(f);
    return lines;
}

static void consume_files_clear(void)
{
    remove(INTAKE_PATH);
    remove(CONSUMED_PATH);
    testlog_reset();
}

extern int shim_open_attempts;
extern int shim_fail_rename;
extern int shim_rename_attempts;

static void poll_n(int n)
{
    int i;
    for (i = 0; i < n; ++i)
        insession_cmd_poll();
}

/* ---- s0: grammar table ---- */
struct grammar_case
{
    const char *label;
    const char *bytes;
    size_t n;           /* 0 = use strlen */
    int verdict;        /* expected enum insession_verdict */
    const char *tag;    /* expected tag (DIAG only) */
    uint32_t repeat;    /* expected repeat (DIAG only) */
    const char *reason; /* expected reason literal (NULL if accepted) */
};

static void run_grammar_case(const struct grammar_case *gc)
{
    struct insession_cmd cmd;
    size_t n = gc->n ? gc->n : strlen(gc->bytes);
    insession_parse_v1(gc->bytes, n, &cmd);
    CHECK(cmd.verdict == gc->verdict, "%s: verdict %d != %d", gc->label,
          cmd.verdict, gc->verdict);
    if (gc->verdict == INSESSION_DIAG)
    {
        CHECK(strcmp(cmd.tag, gc->tag) == 0, "%s: tag '%s' != '%s'",
              gc->label, cmd.tag, gc->tag);
        CHECK(cmd.repeat == gc->repeat, "%s: repeat %u != %u", gc->label,
              cmd.repeat, gc->repeat);
    }
    if (gc->reason)
        CHECK(cmd.reason != NULL && strcmp(cmd.reason, gc->reason) == 0,
              "%s: reason '%s' != '%s'", gc->label,
              cmd.reason ? cmd.reason : "(null)", gc->reason);
    else
        CHECK(cmd.reason == NULL, "%s: reason not NULL", gc->label);
}

/* exactly 48 chars of [A-Za-z0-9_] — a MACRO, so it concatenates into the
   diag48 grammar-case string literal */
#define TAG48 \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijkl"

static void scenario_grammar(void)
{
    /* 0xc3 0xa9 as separate literals: a maximal-munch hex escape would eat
       the following '9' into \xc3a9. */
    static const char tag_nonascii[] =
        "{\"cmd\":\"diag\",\"tag\":\"a" "\xc3" "\xa9" "\",\"repeat\":1}";
    static const struct grammar_case cases[] = {
        {"diag-min", "{\"cmd\":\"diag\",\"tag\":\"probe1\",\"repeat\":1}", 0,
         INSESSION_DIAG, "probe1", 1, NULL},
        {"diag-order-free",
         "{\"repeat\":7,\"tag\":\"probe1\",\"cmd\":\"diag\"}", 0,
         INSESSION_DIAG, "probe1", 7, NULL},
        {"diag-max-repeat",
         "{\"cmd\":\"diag\",\"tag\":\"a.b-c_d9\",\"repeat\":1024}", 0,
         INSESSION_DIAG, "a.b-c_d9", 1024, NULL},
        {"diag-ws-around",
         "\n\t {\"cmd\":\"diag\",\"tag\":\"t1\",\"repeat\":2}\n ", 0,
         INSESSION_DIAG, "t1", 2, NULL},
        {"noop", "{\"cmd\":\"noop\"}", 0, INSESSION_NOOP, "", 0, NULL},
        {"noop-ws", "  { \"cmd\" : \"noop\" }  ", 0, INSESSION_NOOP, "", 0,
         NULL},
        {"nul-independent",
         "{\"cmd\":\"noop\"}EXTRA", strlen("{\"cmd\":\"noop\"}"),
         INSESSION_NOOP, "", 0, NULL},
        {"empty-object", "{}", 0, INSESSION_REJECT, "", 0, "missing cmd"},
        {"diag-no-repeat", "{\"cmd\":\"diag\",\"tag\":\"x\"}", 0,
         INSESSION_REJECT, "", 0, "diag needs tag+repeat"},
        {"diag-no-tag", "{\"cmd\":\"diag\",\"repeat\":5}", 0,
         INSESSION_REJECT, "", 0, "diag needs tag+repeat"},
        {"repeat-zero", "{\"cmd\":\"diag\",\"tag\":\"x\",\"repeat\":0}", 0,
         INSESSION_REJECT, "", 0, "repeat out of range"},
        {"repeat-over", "{\"cmd\":\"diag\",\"tag\":\"x\",\"repeat\":1025}", 0,
         INSESSION_REJECT, "", 0, "repeat out of range"},
        {"repeat-way-over",
         "{\"cmd\":\"diag\",\"tag\":\"x\",\"repeat\":10000}", 0,
         INSESSION_REJECT, "", 0, "malformed"}, /* 5 digits: parser cap */
        {"repeat-huge", "{\"cmd\":\"diag\",\"tag\":\"x\",\"repeat\":100000}",
         0, INSESSION_REJECT, "", 0, "malformed"},
        {"repeat-negative",
         "{\"cmd\":\"diag\",\"tag\":\"x\",\"repeat\":-1}", 0,
         INSESSION_REJECT, "", 0, "malformed"},
        {"repeat-string",
         "{\"cmd\":\"diag\",\"tag\":\"x\",\"repeat\":\"5\"}", 0,
         INSESSION_REJECT, "", 0, "malformed"},
        {"repeat-leading-zero",
         "{\"cmd\":\"diag\",\"tag\":\"x\",\"repeat\":01}", 0,
         INSESSION_REJECT, "", 0, "malformed"},
        {"tag-empty", "{\"cmd\":\"diag\",\"tag\":\"\",\"repeat\":1}", 0,
         INSESSION_REJECT, "", 0, "empty tag"},
        {"tag-space",
         "{\"cmd\":\"diag\",\"tag\":\"has space\",\"repeat\":1}", 0,
         INSESSION_REJECT, "", 0, "bad tag charset"},
        {"tag-slash", "{\"cmd\":\"diag\",\"tag\":\"sl/ash\",\"repeat\":1}", 0,
         INSESSION_REJECT, "", 0, "bad tag charset"},
        {"tag-nonascii", tag_nonascii, 0, INSESSION_REJECT, "", 0,
         "malformed"},
        {"tag-escape",
         "{\"cmd\":\"diag\",\"tag\":\"a\\\\b\",\"repeat\":1}", 0,
         INSESSION_REJECT, "", 0, "malformed"},
        {"noop-extra-key", "{\"cmd\":\"noop\",\"tag\":\"x\"}", 0,
         INSESSION_REJECT, "", 0, "noop with extra keys"},
        {"noop-extra-repeat", "{\"cmd\":\"noop\",\"repeat\":1}", 0,
         INSESSION_REJECT, "", 0, "noop with extra keys"},
        {"unknown-cmd", "{\"cmd\":\"reload\",\"tag\":\"x\",\"repeat\":1}", 0,
         INSESSION_REJECT, "", 0, "unknown cmd"},
        {"unknown-cmd-case",
         "{\"cmd\":\"DIAG\",\"tag\":\"x\",\"repeat\":1}", 0,
         INSESSION_REJECT, "", 0, "unknown cmd"},
        {"unknown-key",
         "{\"cmd\":\"diag\",\"tag\":\"x\",\"repeat\":1,\"extra\":1}", 0,
         INSESSION_REJECT, "", 0, "unknown key"},
        {"dup-key",
         "{\"cmd\":\"diag\",\"cmd\":\"noop\",\"tag\":\"x\",\"repeat\":1}", 0,
         INSESSION_REJECT, "", 0, "duplicate key"},
        {"empty-input", "", 0, INSESSION_REJECT, "", 0, "malformed"},
        {"open-brace", "{", 0, INSESSION_REJECT, "", 0, "malformed"},
        {"trailing-bytes",
         "{\"cmd\":\"diag\",\"tag\":\"x\",\"repeat\":1} garbage", 0,
         INSESSION_REJECT, "", 0, "trailing bytes"},
        {"missing-colon", "{\"cmd\" \"diag\"}", 0, INSESSION_REJECT, "", 0,
         "malformed"},
        {"trailing-comma",
         "{\"cmd\":\"diag\",\"tag\":\"x\",\"repeat\":1,}", 0,
         INSESSION_REJECT, "", 0, "malformed"},
        {"array-input", "[1,2]", 0, INSESSION_REJECT, "", 0, "malformed"},
        {"null-input", "null", 0, INSESSION_REJECT, "", 0, "malformed"},
    };
    struct grammar_case diag48 = {
        "tag-48-boundary",
        "{\"cmd\":\"diag\",\"tag\":\"" TAG48 "\",\"repeat\":1}", 0,
        INSESSION_DIAG, TAG48, 1, NULL};
    char longtag[160];
    struct grammar_case tag49;
    size_t off = 0;
    const char *pre = "{\"cmd\":\"diag\",\"tag\":\"";
    const char *post = "\",\"repeat\":1}";

    g_scenario = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        run_grammar_case(&cases[i]);
    run_grammar_case(&diag48);

    strcpy(longtag, pre);
    off = strlen(pre);
    memset(longtag + off, 'a', 49);
    off += 49;
    strcpy(longtag + off, post);
    tag49.label = "tag-49-over";
    tag49.bytes = longtag;
    tag49.n = 0;
    tag49.verdict = INSESSION_REJECT;
    tag49.tag = "";
    tag49.repeat = 0;
    tag49.reason = "malformed";
    run_grammar_case(&tag49);

    printf("[host-test] s0 grammar table: %d cases\n",
           (int)(sizeof(cases) / sizeof(cases[0])) + 2);
}

/* ---- s1: idle polls ---- */
static void scenario_idle(void)
{
    int opens_before = shim_open_attempts;
    g_scenario = 1;
    consume_files_clear();
    poll_n(5);
    CHECK(shim_open_attempts - opens_before == 5,
          "expected 5 idle opens, got %d",
          shim_open_attempts - opens_before);
    CHECK(testlog_count("insession-cmd") == 0, "idle produced log lines");
    CHECK(!intake_exists() && access(CONSUMED_PATH, F_OK) != 0,
          "idle produced files");
    printf("[host-test] s1 idle: 5 polls, 5 open-tries, zero side effects\n");
}

/* ---- s2: diag end-to-end ---- */
static void scenario_diag(void)
{
    const char *cmd = "{\"cmd\":\"diag\",\"tag\":\"probe1\",\"repeat\":7}";
    long sink_before = sink_line_count();
    int renames;
    g_scenario = 2;
    consume_files_clear();
    write_intake_str(cmd);
    shim_rename_attempts = 0;
    poll_n(3); /* executed exactly once; polls 2-3 find nothing */
    renames = shim_rename_attempts;
    CHECK(sink_line_count() - sink_before == 1, "sink grew by %ld, want 1",
          sink_line_count() - sink_before);
    CHECK(testlog_count(
              "insession-cmd: diag tag=probe1 repeat=7 emitted (consumed)") ==
              1,
          "expected exactly one diag log line");
    CHECK(testlog_count("REJECTED") == 0, "unexpected REJECT line");
    CHECK(!intake_exists(), "intake still present (not consumed)");
    CHECK(consumed_bytes_match(cmd, strlen(cmd)), "consumed bytes differ");
    CHECK(renames == 1, "rename attempts %d != 1", renames);
    printf("[host-test] s2 diag: 1 sink row, 1 log line, consumed "
           "byte-identical, 1 rename, 3 polls\n");
}

/* ---- s3: noop ---- */
static void scenario_noop(void)
{
    long sink_before = sink_line_count();
    g_scenario = 3;
    consume_files_clear();
    write_intake_str("{\"cmd\":\"noop\"}");
    poll_n(2);
    CHECK(sink_line_count() == sink_before, "noop wrote a sink row");
    CHECK(testlog_count("insession-cmd: noop ack (consumed)") == 1,
          "expected exactly one ack line");
    CHECK(!intake_exists(), "intake not consumed");
    printf("[host-test] s3 noop: ack line only, sink untouched, consumed\n");
}

/* ---- s4: malformed, never re-read ---- */
static void scenario_malformed(void)
{
    long sink_before = sink_line_count();
    g_scenario = 4;
    consume_files_clear();
    write_intake_str("{not json");
    poll_n(3);
    CHECK(sink_line_count() == sink_before, "malformed touched the sink");
    CHECK(testlog_count(
              "INSESSION-CMD REJECTED (malformed); consumed, not executed") ==
              1,
          "expected exactly one REJECT line across 3 polls (never re-read)");
    CHECK(!intake_exists(), "malformed intake not consumed");
    printf("[host-test] s4 malformed: loud reject once, consumed, never "
           "re-read\n");
}

/* ---- s5: unknown command never executes ---- */
static void scenario_unknown_cmd(void)
{
    const char *cmd = "{\"cmd\":\"reload\",\"tag\":\"x\",\"repeat\":1}";
    long sink_before = sink_line_count();
    g_scenario = 5;
    consume_files_clear();
    write_intake_str(cmd);
    poll_n(2);
    CHECK(sink_line_count() == sink_before, "unknown cmd touched the sink");
    CHECK(testlog_count(
              "INSESSION-CMD REJECTED (unknown cmd); consumed, not executed") ==
              1,
          "expected the unknown-cmd REJECT line");
    CHECK(testlog_count("diag tag=x") == 0,
          "diag dispatch ran for reload"); /* needle unique to a dispatched
                                              diag of tag x; s2's probe1 line
                                              must not match (cumulative log) */
    CHECK(consumed_bytes_match(cmd, strlen(cmd)), "consumed bytes differ");
    printf("[host-test] s5 unknown cmd: REJECTED and consumed, never "
           "executed\n");
}

/* ---- s6: oversize ---- */
static void scenario_oversize(void)
{
    long sink_before = sink_line_count();
    char big[600];
    g_scenario = 6;
    consume_files_clear();
    memset(big, 'a', sizeof(big));
    write_intake(big, sizeof(big));
    poll_n(2);
    CHECK(sink_line_count() == sink_before, "oversize touched the sink");
    CHECK(testlog_count("INSESSION-CMD REJECTED oversize") == 1,
          "expected the oversize REJECT line");
    CHECK(!intake_exists(), "oversize intake not consumed");
    printf("[host-test] s6 oversize: rejected without parsing, consumed\n");
}

/* ---- s7: rename failure => at-least-once ---- */
static void scenario_rename_failure(void)
{
    const char *cmd = "{\"cmd\":\"diag\",\"tag\":\"flaky\",\"repeat\":3}";
    long sink_before = sink_line_count();
    g_scenario = 7;
    consume_files_clear();
    write_intake_str(cmd);
    shim_fail_rename = 1;
    poll_n(1); /* executes, consume fails LOUDLY, file stays */
    CHECK(sink_line_count() - sink_before == 1,
          "first poll did not emit (sink delta %ld)",
          sink_line_count() - sink_before);
    CHECK(testlog_count("INSESSION-CMD CONSUME-RENAME FAILED") == 1,
          "expected exactly one loud consume-failure line");
    CHECK(intake_exists(), "intake vanished despite rename failure");
    shim_fail_rename = 0;
    poll_n(1); /* re-executes (at-least-once) then consumes */
    CHECK(sink_line_count() - sink_before == 2,
          "sink delta %ld != 2 executions", sink_line_count() - sink_before);
    CHECK(testlog_count(
              "insession-cmd: diag tag=flaky repeat=3 emitted (consumed)") == 2,
          "expected exactly two executions of the same command");
    CHECK(!intake_exists(), "intake still present after recovery");
    printf("[host-test] s7 rename-failure: execute-then-rename, loud "
           "failure, re-executed once, then consumed (at-least-once)\n");
}

int main(void)
{
    shim_fail_rename = 0;
    shim_rename_attempts = 0;
    shim_open_attempts = 0;
    /* On console module_start opens the retained entry channel before any
       event; the fixture does the equivalent once, up front. */
    if (diagnostic_open_channel() < 0)
    {
        fprintf(stderr, "entry channel did not open\n");
        return 2;
    }

    scenario_grammar();
    scenario_idle();
    scenario_diag();
    scenario_noop();
    scenario_malformed();
    scenario_unknown_cmd();
    scenario_oversize();
    scenario_rename_failure();

    printf("[host-test] checks=%d fails=%d\n", g_checks, g_fails);
    if (g_fails == 0)
        printf("HOST-TEST-PASS\n");
    return g_fails == 0 ? 0 : 1;
}
