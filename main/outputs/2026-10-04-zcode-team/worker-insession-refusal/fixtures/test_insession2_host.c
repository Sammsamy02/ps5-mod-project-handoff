/* test_insession2_host.c — scenario driver for the refusal-surviving
   in-session command service. worker-insession-refusal, 2026-10-05.
   HOST FIXTURE ONLY.

   Scenario map:
     s0  grammar-v2 table (pure parser, ~55 rows; v1 rows keep v1 verdicts)
     s6  idle poll: no intake file -> zero effects
     s2  unwired io_probe + unregistered pool_probe -> loud bounded replies
     s1  wired dispatch core: io_probe b, pool_probe (fresh call #1), log, noop
     s7  freshness: a second pool_probe command calls the bridge AGAIN (#2)
     s4  rejects: malformed / unknown cmd / oversize / duplicate key /
         trailing bytes / bad op / escape — loud, consumed, never re-read
     s5  rename-failure injection: execute + loud failure + at-least-once
         repeat, then recovery
     s3  THE DIRECTIVE PROOF (real thread): service_start with NO event loop,
         NO pump, NO engine — note_initializer_refused() set — dispatch HELD
         before settle, then diag executes + consumes after settle; the
         refused triage line appears exactly once
     s8  real-thread io_probe a + d through the intake file
     s10 service_stop joins within the bound; after stop a staged command is
         left untouched
   Every scenario prints PASS/FAIL; the binary ends with HOST-TEST-PASS only
   if zero checks failed. The run_host_tests2.sh wrapper adds strict python
   validation of the supplier sink, the entry channel and UE4SS.log. */
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "insession_cmd2_core.h"
#include "insession_cmd2_service.h"
#include "diagnostic.h" /* diagnostic_open_channel: module_start's job on console */

#ifndef INTAKE_PATH
#define INTAKE_PATH "/tmp/in-session-cmd.json"
#endif
#ifndef CONSUMED_PATH
#define CONSUMED_PATH INTAKE_PATH ".consumed"
#endif
#ifndef UE4SSLOG_PATH
#define UE4SSLOG_PATH "/tmp/UE4SS-test.log"
#endif

/* shims */
extern int shim_fail_open;
extern int shim_open_attempts;
extern int shim_fail_rename;
extern int shim_rename_attempts;
extern int fake_probe_calls;
extern uint64_t fake_probe_last_addr;
extern uint64_t fake_probe_last_len;
extern int fake_probe_class;
extern int fake_io_calls[4];
extern int fake_io_rc[4];
extern int bridge_io_control_a(char *tags_out, unsigned cap);
extern int bridge_io_control_b(char *tags_out, unsigned cap);
extern int bridge_io_control_c(char *tags_out, unsigned cap);
extern int bridge_io_control_d(char *tags_out, unsigned cap);

static int g_fails;
static int g_checks;

static void check(int cond, const char *what)
{
    ++g_checks;
    if (cond)
    {
        printf("  ok: %s\n", what);
    }
    else
    {
        ++g_fails;
        printf("  FAIL: %s\n", what);
    }
}

static void stage_intake(const char *body)
{
    FILE *f = fopen(INTAKE_PATH, "wb");
    if (!f)
    {
        printf("  FATAL: cannot stage intake\n");
        exit(2);
    }
    if (body)
        fputs(body, f);
    fclose(f);
}

static char *slurp(const char *path)
{
    static char buf[1 << 16];
    FILE *f = fopen(path, "rb");
    size_t got;
    if (!f)
        return NULL;
    got = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

static unsigned count_substr(const char *hay, const char *needle)
{
    unsigned n = 0;
    size_t len = strlen(needle);
    if (!hay)
        return 0;
    while ((hay = strstr(hay, needle)) != NULL)
    {
        ++n;
        hay += len;
    }
    return n;
}

static int file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static long long now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void msleep(int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* wait until a staged intake is consumed (bounded); returns 1 on success */
static int wait_consumed(int timeout_ms)
{
    long long deadline = now_ms() + timeout_ms;
    while (now_ms() < deadline)
    {
        if (!file_exists(INTAKE_PATH) && file_exists(CONSUMED_PATH))
            return 1;
        msleep(20);
    }
    return 0;
}

static void remove_both(void)
{
    (void)unlink(INTAKE_PATH);
    (void)unlink(CONSUMED_PATH);
}

/* ---- s0: the grammar table ------------------------------------------------ */
static void grammar_case(const char *input, int expect_verdict,
                         const char *expect_reason, const char *label)
{
    struct insession2_cmd cmd;
    char buf[600];
    size_t n = strlen(input);
    if (n >= sizeof(buf))
        n = sizeof(buf) - 1;
    memcpy(buf, input, n);
    insession2_parse(buf, n, &cmd);
    ++g_checks;
    if (cmd.verdict != expect_verdict ||
        (expect_reason && (!cmd.reason || strcmp(cmd.reason, expect_reason) != 0)) ||
        (!expect_reason && cmd.reason != NULL))
    {
        ++g_fails;
        printf("  FAIL s0: %s -> verdict=%d reason=%s (want %d/%s)\n", label,
               cmd.verdict, cmd.reason ? cmd.reason : "(null)", expect_verdict,
               expect_reason ? expect_reason : "(null)");
    }
}

#define DIAG_V INSESSION2_DIAG
#define NOOP_V INSESSION2_NOOP
#define IO_V INSESSION2_IO_PROBE
#define POOL_V INSESSION2_POOL_PROBE
#define LOG_V INSESSION2_LOG
#define REJ_V INSESSION2_REJECT

static void run_s0_grammar(void)
{
    printf("s0 grammar v2 table\n");
    /* v1 carriers (identical verdicts) */
    grammar_case("{\"cmd\":\"noop\"}", NOOP_V, NULL, "noop");
    grammar_case("  {\"cmd\":\"noop\"}  ", NOOP_V, NULL, "noop ws");
    grammar_case("{\"repeat\":7,\"tag\":\"a.b-c_d\",\"cmd\":\"diag\"}", DIAG_V,
                 NULL, "diag order-free");
    grammar_case("{\"cmd\":\"diag\",\"tag\":\"abc\",\"repeat\":1024}", DIAG_V,
                 NULL, "diag repeat max");
    grammar_case("{\"cmd\":\"diag\",\"tag\":\"abc\",\"repeat\":1}", DIAG_V,
                 NULL, "diag repeat min");
    grammar_case("{\"cmd\":\"diag\",\"tag\":\"abc\",\"repeat\":0}", REJ_V,
                 "repeat out of range", "repeat 0");
    grammar_case("{\"cmd\":\"diag\",\"tag\":\"abc\",\"repeat\":1025}", REJ_V,
                 "repeat out of range", "repeat 1025");
    grammar_case("{\"tag\":\"a\",\"tag\":\"b\",\"cmd\":\"diag\",\"repeat\":1}",
                 REJ_V, "duplicate key", "diag duplicate key");
    grammar_case("{\"cmd\":\"diag\",\"tag\":\"\",\"repeat\":3}", REJ_V,
                 "empty tag", "diag empty tag");
    grammar_case("{\"cmd\":\"diag\",\"tag\":\"a b\",\"repeat\":3}", REJ_V,
                 "bad tag charset", "diag tag space");
    grammar_case("{\"cmd\":\"diag\",\"tag\":\"a\\tb\",\"repeat\":3}", REJ_V,
                 "malformed", "diag tag escape");
    grammar_case("{\"cmd\":\"noop\",\"x\":1}", REJ_V, "unknown key",
                 "noop extra key");
    grammar_case("{\"cmd\":\"reload\"}", REJ_V, "unknown cmd",
                 "unknown cmd reload");
    grammar_case("{\"cmd\":\"DIAG\",\"tag\":\"a\",\"repeat\":1}", REJ_V,
                 "unknown cmd", "case sensitive");
    grammar_case("{\"tag\":\"a\"}", REJ_V, "missing cmd", "missing cmd");
    grammar_case("{\"cmd\":\"diag\",\"tag\":\"a\"}", REJ_V,
                 "diag needs exactly tag+repeat", "diag missing repeat");
    grammar_case("[{\"cmd\":\"noop\"}]", REJ_V, "malformed", "array");
    grammar_case("{\"cmd\":\"noop\"", REJ_V, "malformed", "unterminated");
    grammar_case("{\"cmd\":\"noop\"} x", REJ_V, "trailing bytes",
                 "trailing bytes");
    grammar_case("{\"cmd\":\"diag\",\"tag\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                 "aaaaaaaaaaaaaaaa\",\"repeat\":5}",
                 DIAG_V, NULL, "tag 48 ok");
    grammar_case("{\"cmd\":\"diag\",\"tag\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                 "aaaaaaaaaaaaaaaaa\",\"repeat\":5}",
                 REJ_V, "malformed", "tag 49");
    /* v2: io_probe */
    grammar_case("{\"cmd\":\"io_probe\",\"op\":\"a\"}", IO_V, NULL, "io a");
    grammar_case("{\"cmd\":\"io_probe\",\"op\":\"b\"}", IO_V, NULL, "io b");
    grammar_case("{\"cmd\":\"io_probe\",\"op\":\"c\"}", IO_V, NULL, "io c");
    grammar_case("{\"cmd\":\"io_probe\",\"op\":\"d\"}", IO_V, NULL, "io d");
    grammar_case("{\"op\":\"d\",\"cmd\":\"io_probe\"}", IO_V, NULL, "io order");
    grammar_case("{\"cmd\":\"io_probe\",\"op\":\"e\"}", REJ_V, "bad op",
                 "io op e");
    grammar_case("{\"cmd\":\"io_probe\",\"op\":\"A\"}", REJ_V, "bad op",
                 "io op uppercase");
    grammar_case("{\"cmd\":\"io_probe\",\"op\":\"ab\"}", REJ_V, "bad op",
                 "io op two chars");
    grammar_case("{\"cmd\":\"io_probe\"}", REJ_V, "io_probe needs exactly op",
                 "io missing op");
    grammar_case("{\"cmd\":\"io_probe\",\"op\":\"a\",\"tag\":\"x\"}", REJ_V,
                 "io_probe needs exactly op", "io extra tag");
    grammar_case("{\"cmd\":\"io_probe\",\"op\":\"a\",\"op\":\"b\"}", REJ_V,
                 "duplicate key", "io dup op");
    /* v2: pool_probe */
    grammar_case("{\"cmd\":\"pool_probe\"}", POOL_V, NULL, "pool");
    grammar_case(" {\"cmd\":\"pool_probe\"}\n", POOL_V, NULL, "pool ws");
    grammar_case("{\"cmd\":\"pool_probe\",\"repeat\":1}", REJ_V,
                 "pool_probe takes no extra keys", "pool extra repeat");
    grammar_case("{\"cmd\":\"pool_probe\",\"tag\":\"x\"}", REJ_V,
                 "pool_probe takes no extra keys", "pool extra tag");
    /* v2: log */
    grammar_case("{\"cmd\":\"log\",\"text\":\"hello townfall\"}", LOG_V, NULL,
                 "log");
    grammar_case("{\"text\":\"x\",\"cmd\":\"log\"}", LOG_V, NULL, "log order");
    grammar_case("{\"cmd\":\"log\",\"text\":\"\"}", REJ_V, "empty text",
                 "log empty");
    /* boundary lengths built programmatically (exact 96 = ok, 97 = reject) */
    {
        static char gen[160];
        int len;
        for (len = 96; len <= 97; ++len)
        {
            int i;
            memset(gen, 0, sizeof(gen));
            strcat(gen, "{\"cmd\":\"log\",\"text\":\"");
            for (i = 0; i < len; ++i)
                strcat(gen, "a");
            strcat(gen, "\"}");
            grammar_case(gen, len == 96 ? LOG_V : REJ_V,
                         len == 96 ? NULL : "malformed",
                         len == 96 ? "log 96 ok" : "log 97");
        }
    }
    grammar_case("{\"cmd\":\"log\",\"text\":\"a\\tb\"}", REJ_V, "malformed",
                 "log tab escape");
    grammar_case("{\"cmd\":\"log\"}", REJ_V, "log needs exactly text",
                 "log missing text");
    grammar_case("{\"cmd\":\"log\",\"text\":\"hi\",\"tag\":\"x\"}", REJ_V,
                 "log needs exactly text", "log extra tag");
    printf("s0 done\n");
}

int main(void)
{
    char *logtxt;
    long long t0;
    int awaited;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("worker-insession-refusal host test (grammar v2 + refusal-"
           "surviving service)\n");
    if (diagnostic_open_channel() < 0)
    {
        printf("FATAL: diagnostic channel unavailable (module_start opens it "
               "on console)\n");
        return 2;
    }
    (void)unlink(UE4SSLOG_PATH);
    remove_both();

    run_s0_grammar();
    check(g_fails == 0, "s0 grammar table clean");

    /* s6: idle poll — no intake file, zero effects */
    printf("s6 idle poll\n");
    {
        int opens_before = shim_open_attempts;
        insession_cmd2_poll_once();
        check(shim_open_attempts == opens_before + 1, "s6 exactly one open-try");
        check(!file_exists(CONSUMED_PATH), "s6 nothing consumed");
        check((logtxt = slurp(UE4SSLOG_PATH)) == NULL || strlen(logtxt) == 0,
              "s6 zero log lines");
    }

    /* s2: unwired io_probe + unregistered pool_probe */
    printf("s2 unwired replies\n");
    stage_intake("{\"cmd\":\"io_probe\",\"op\":\"a\"}");
    insession_cmd2_poll_once();
    check(!file_exists(INTAKE_PATH) && file_exists(CONSUMED_PATH),
          "s2 unwired io consumed");
    check(fake_io_calls[0] == 0, "s2 no control called when unwired");
    logtxt = slurp(UE4SSLOG_PATH);
    check(count_substr(logtxt,
                       "insession-cmd: io_probe op=a: controls not wired "
                       "(consumed)") == 1,
          "s2 not-wired line");
    remove_both();
    stage_intake("{\"cmd\":\"pool_probe\"}");
    insession_cmd2_poll_once();
    logtxt = slurp(UE4SSLOG_PATH);
    check(count_substr(logtxt,
                       "insession-cmd: pool_probe: target not registered "
                       "(consumed)") == 1,
          "s2 not-registered line");
    check(fake_probe_calls == 0, "s2 bridge never called unregistered");
    remove_both();

    /* s1: wired dispatch core (still no thread — deterministic) */
    printf("s1 wired dispatch core\n");
    insession_cmd2_register_io_control(1, &bridge_io_control_b);
    insession_cmd2_register_io_control(0, &bridge_io_control_a);
    insession_cmd2_register_io_control(2, &bridge_io_control_c);
    insession_cmd2_register_io_control(3, &bridge_io_control_d);
    insession_cmd2_register_pool_probe_target(0x123456000ull, 0x20000u);
    fake_io_rc[1] = 42;
    stage_intake("{\"cmd\":\"io_probe\",\"op\":\"b\"}");
    insession_cmd2_poll_once();
    check(fake_io_calls[1] == 1, "s1 control b called once");
    logtxt = slurp(UE4SSLOG_PATH);
    check(count_substr(logtxt,
                       "insession-cmd: io_probe op=b rc=42 tags=iob-ok "
                       "(consumed)") == 1,
          "s1 wired io line with rc + tags");

    stage_intake("{\"cmd\":\"pool_probe\"}");
    insession_cmd2_poll_once();
    check(fake_probe_calls == 1, "s1 bridge called once");
    check(fake_probe_last_addr == 0x123456000ull, "s1 registered addr used");
    check(fake_probe_last_len == 0x20000u, "s1 registered len used");
    logtxt = slurp(UE4SSLOG_PATH);
    check(count_substr(logtxt,
                       "insession-cmd: pool_probe addr=0x0000000123456000 "
                       "len=0x00020000 class=1 fresh (consumed)") == 1,
          "s1 pool_probe line");

    stage_intake("{\"cmd\":\"log\",\"text\":\"hello townfall\"}");
    insession_cmd2_poll_once();
    logtxt = slurp(UE4SSLOG_PATH);
    check(count_substr(logtxt, "insession-cmd log: hello townfall") == 1,
          "s1 log text line");

    stage_intake("{\"cmd\":\"noop\"}");
    insession_cmd2_poll_once();
    logtxt = slurp(UE4SSLOG_PATH);
    check(count_substr(logtxt, "insession-cmd: noop ack (consumed)") == 1,
          "s1 noop ack line");
    remove_both();

    /* s7: freshness — pool_probe again = a SECOND bridge call */
    printf("s7 fresh pool probe\n");
    stage_intake("{\"cmd\":\"pool_probe\"}");
    insession_cmd2_poll_once();
    check(fake_probe_calls == 2, "s7 bridge called AGAIN (fresh, not cached)");
    remove_both();

    /* s4: rejects — loud, consumed, never re-read */
    printf("s4 rejects\n");
    {
        const char *cases[] = {
            "garbage",
            "{\"cmd\":\"reload\"}",
            "{\"cmd\":\"noop\"} trailing",
            "{\"cmd\":\"noop\",\"noop\":1}",
            "{\"cmd\":\"io_probe\",\"op\":\"z\"}",
            "{\"cmd\":\"log\",\"text\":\"bad\\tescape\"}",
        };
        unsigned i;
        int renames_before;
        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        {
            remove_both();
            stage_intake(cases[i]);
            renames_before = shim_rename_attempts;
            insession_cmd2_poll_once();
            check(!file_exists(INTAKE_PATH) && file_exists(CONSUMED_PATH),
                  "s4 case consumed");
            check(shim_rename_attempts == renames_before + 1,
                  "s4 exactly one rename");
        }
        /* oversize: staged beyond the 512 B cap */
        {
            char big[600];
            memset(big, 'x', sizeof(big));
            big[sizeof(big) - 1] = '\0';
            remove_both();
            stage_intake(big);
            insession_cmd2_poll_once();
            check(!file_exists(INTAKE_PATH), "s4 oversize consumed");
            logtxt = slurp(UE4SSLOG_PATH);
            check(count_substr(logtxt, "REJECTED oversize (>=512B)") == 1,
                  "s4 oversize loud line");
        }
        logtxt = slurp(UE4SSLOG_PATH);
        check(count_substr(logtxt, "INSESSION-CMD REJECTED (malformed)") >= 1,
              "s4 malformed loud");
        check(count_substr(logtxt, "INSESSION-CMD REJECTED (unknown cmd)") >= 1,
              "s4 unknown cmd loud");
        /* never re-read: one more poll with no file changes nothing */
        {
            unsigned rejects_before =
                count_substr(slurp(UE4SSLOG_PATH), "INSESSION-CMD REJECTED");
            remove_both();
            insession_cmd2_poll_once();
            logtxt = slurp(UE4SSLOG_PATH);
            check(count_substr(logtxt, "INSESSION-CMD REJECTED") ==
                      rejects_before,
                  "s4 no re-read");
        }
    }

    /* s5: rename failure -> at-least-once, then recovery */
    printf("s5 rename-failure at-least-once\n");
    shim_fail_rename = 1;
    stage_intake("{\"cmd\":\"diag\",\"tag\":\"flaky\",\"repeat\":3}");
    insession_cmd2_poll_once();
    check(file_exists(INTAKE_PATH), "s5 file kept on rename failure");
    logtxt = slurp(UE4SSLOG_PATH);
    check(count_substr(logtxt, "INSESSION-CMD CONSUME-RENAME FAILED") == 1,
          "s5 loud failure line");
    shim_fail_rename = 0;
    insession_cmd2_poll_once();
    check(!file_exists(INTAKE_PATH) && file_exists(CONSUMED_PATH),
          "s5 recovered on next poll");
    remove_both();

    /* s3: THE DIRECTIVE PROOF — real thread, refused flag set, no event loop,
       no pump, no engine. Dispatch held before settle, executes after. */
    printf("s3 refused-boot end-to-end (real service thread)\n");
    insession_cmd2_service_start();
    insession_cmd2_note_initializer_refused();
    msleep(150); /* several poll cycles */
    logtxt = slurp(UE4SSLOG_PATH);
    check(count_substr(logtxt,
                       "insession-cmd: initializer refused (recoverable); "
                       "in-session service remains callable") == 1,
          "s3 refused triage line exactly once");
    check(fake_probe_calls == 2, "s3 thread made no bridge calls pre-settle");
    stage_intake("{\"cmd\":\"diag\",\"tag\":\"probe1\",\"repeat\":7}");
    msleep(300); /* held: several full poll cycles with a staged file */
    check(file_exists(INTAKE_PATH),
          "s3 dispatch HELD before settle (file untouched)");
    logtxt = slurp(UE4SSLOG_PATH);
    check(count_substr(logtxt, "diag tag=probe1") == 0,
          "s3 nothing executed pre-settle");
    insession_cmd2_settle_startup();
    awaited = wait_consumed(3000);
    check(awaited, "s3 diag consumed after settle");
    logtxt = slurp(UE4SSLOG_PATH);
    check(count_substr(logtxt,
                       "insession-cmd: diag tag=probe1 repeat=7 emitted "
                       "(consumed)") == 1,
          "s3 diag executed with refused flag set");

    /* s8: real-thread io_probe a + d through the intake */
    printf("s8 real-thread io probes\n");
    stage_intake("{\"cmd\":\"io_probe\",\"op\":\"d\"}");
    check(wait_consumed(3000), "s8 io d consumed via thread");
    stage_intake("{\"cmd\":\"io_probe\",\"op\":\"a\"}");
    check(wait_consumed(3000), "s8 io a consumed via thread");
    msleep(120); /* allow the effect lines to land */
    check(fake_io_calls[3] >= 1, "s8 control d ran on the poll thread");
    check(fake_io_calls[0] >= 1, "s8 control a ran (post-registration)");

    /* s10: stop kills within the bound; nothing dispatches afterwards */
    printf("s10 stop bound\n");
    t0 = now_ms();
    insession_cmd2_service_stop();
    {
        long long joined_ms = now_ms() - t0;
        char note[128];
        snprintf(note, sizeof(note), "s10 joined in %lld ms (< 500)",
                 joined_ms);
        check(joined_ms < 500, note);
    }
    insession_cmd2_service_stop(); /* idempotent second stop */
    stage_intake("{\"cmd\":\"noop\"}");
    msleep(300);
    check(file_exists(INTAKE_PATH), "s10 no consumption after stop");
    remove_both();

    printf("checks=%d fails=%d\n", g_checks, g_fails);
    if (g_fails == 0)
    {
        printf("HOST-TEST-PASS\n");
        return 0;
    }
    printf("HOST-TEST-FAIL\n");
    return 1;
}
