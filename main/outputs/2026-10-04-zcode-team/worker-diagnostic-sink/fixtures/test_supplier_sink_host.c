/* test_supplier_sink_host.c — host fixtures for the r-s1 supplier diagnostic
   sink (worker-diagnostic-sink). Tests LOGIC ONLY (the staged diagnostic.c
   compiled against POSIX shims); nothing here claims console behavior.
   Phase order matters: the forced-open-failure phase (A) runs BEFORE any
   successful open, matching process startup where the sink fd is not yet
   open.
     A  forced open failure at startup: 12 events during total open failure
        crash nothing and append nothing; each failed event attempts one
        bounded open (12 of the 64 cap); the first allowed open recovers;
        its line self-reports "open_errno":13; later lines do not repeat it.
     B  order + content + additive entry channel: events append JSONL lines
        in call order, seq ascending, value/value_hex/t_ms correct, legacy
        entry-channel lines unchanged (ADDITIVE proof).
     C  200-event burst: 200 events -> 200 lines, still ordered.
     D  write failure: forced write failure drops exactly that line and
        crashes nothing; a partial short write exercises the bounded retry
        and the line still lands complete; the file stays line-valid.
     E  oversized tag: a 200-char tag yields valid JSON with the tag bounded
        (<=96 chars), never a truncated line.
   The strict per-line JSON validation of every produced file (seq, t_ms,
   tag order, expected omissions) is done by run_host_tests.sh on top.
*/
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void diagnostic_event(const char *tag, uint32_t value);
int diagnostic_open_channel(void);

/* shim controls (host_shims.c) */
extern int shim_fail_open;
extern int shim_open_attempts;
extern int shim_fail_write;
extern int shim_partial_write;

static int g_checks, g_failures;
#define CHECK(cond, name)                                                     \
    do {                                                                      \
        ++g_checks;                                                           \
        if (cond) {                                                           \
            printf("check ok: %s\n", (name));                                 \
        } else {                                                              \
            ++g_failures;                                                     \
            printf("CHECK FAIL: %s\n", (name));                               \
        }                                                                     \
    } while (0)

static char g_supplier_path[512];
static char g_entry_path[512];

static long count_lines(const char *path)
{
    FILE *f = fopen(path, "r");
    long n = 0;
    char buf[2048];
    if (!f)
        return -1; /* absent */
    while (fgets(buf, sizeof buf, f))
        ++n;
    fclose(f);
    return n;
}

static int first_line_contains(const char *path, const char *needle)
{
    FILE *f = fopen(path, "r");
    char buf[2048];
    int hit = 0;
    if (!f)
        return 0;
    if (fgets(buf, sizeof buf, f))
        hit = strstr(buf, needle) != NULL;
    fclose(f);
    return hit;
}

static int file_contains(const char *path, const char *needle)
{
    FILE *f = fopen(path, "r");
    char buf[2048];
    int hit = 0;
    if (!f)
        return 0;
    while (fgets(buf, sizeof buf, f)) {
        if (strstr(buf, needle)) {
            hit = 1;
            break;
        }
    }
    fclose(f);
    return hit;
}

static long count_occurrences(const char *path, const char *needle)
{
    FILE *f = fopen(path, "r");
    char buf[2048];
    long n = 0;
    if (!f)
        return -1;
    while (fgets(buf, sizeof buf, f))
        if (strstr(buf, needle))
            ++n;
    fclose(f);
    return n;
}

int main(int argc, char **argv)
{
    long landed;
    if (argc < 3) {
        fprintf(stderr, "usage: %s SUPPLIER_PATH ENTRY_PATH\n", argv[0]);
        return 2;
    }
    snprintf(g_supplier_path, sizeof g_supplier_path, "%s", argv[1]);
    snprintf(g_entry_path, sizeof g_entry_path, "%s", argv[2]);

    /* ---- phase A: startup open failure (fd never opened yet) ---- */
    remove(g_supplier_path);
    remove(g_entry_path);
    CHECK(count_lines(g_supplier_path) == -1, "A supplier file absent at start");
    shim_fail_open = 1;
    shim_open_attempts = 0;
    for (int i = 0; i < 12; ++i)
        diagnostic_event("bridge_noopen", (uint32_t)i);
    CHECK(count_lines(g_supplier_path) == -1,
          "A no file created during startup open failure");
    CHECK(shim_open_attempts == 12,
          "A every failed event attempts one bounded open (12 of cap 64)");
    shim_fail_open = 0;
    diagnostic_event("bridge_recovered", 1u);
    CHECK(count_lines(g_supplier_path) == 1,
          "A recovery: first allowed open creates file and appends");
    CHECK(first_line_contains(g_supplier_path, "\"seq\":1,") &&
              first_line_contains(g_supplier_path, "\"tag\":\"bridge_recovered\""),
          "A recovered line is seq=1 with the right tag");
    CHECK(file_contains(g_supplier_path, "\"open_errno\":13"),
          "A recovered line self-reports open_errno=13");
    diagnostic_event("bridge_after", 2u);
    CHECK(count_occurrences(g_supplier_path, "\"open_errno\":") == 1,
          "A open_errno self-report does not repeat");
    landed = 2;

    /* ---- phase B: order + content + additive entry channel ---- */
    diagnostic_open_channel(); /* emulate the wired entry opening the shared channel */
    diagnostic_event("bridge_vmentry_entries", 0x592cu);
    diagnostic_event("bridge_lease_reg", 6u);
    diagnostic_event("bridge_exepath_sig", 0x7070612fu);
    CHECK(count_lines(g_supplier_path) == landed + 3,
          "B three events -> three jsonl lines");
    CHECK(first_line_contains(g_supplier_path, "\"tag\":\"bridge_recovered\""),
          "B append-only: phase-A lines still first");
    CHECK(file_contains(g_supplier_path, "\"value\":6,"),
          "B decimal value present");
    CHECK(file_contains(g_supplier_path, "\"value_hex\":\"0x00000006\""),
          "B hex value present");
    CHECK(file_contains(g_supplier_path, "\"t_ms\":"), "B t_ms present");
    CHECK(file_contains(g_entry_path, "bridge_lease_reg value=0x00000006"),
          "B entry channel still receives the legacy line");
    CHECK(count_lines(g_entry_path) == 3, "B entry channel got exactly the 3 lines");
    landed += 3;

    /* ---- phase C: 200-event burst ---- */
    for (unsigned i = 0; i < 200; ++i)
        diagnostic_event("bridge_burst", i);
    CHECK(count_lines(g_supplier_path) == landed + 200,
          "C 200-event burst appends 200 lines");
    CHECK(count_occurrences(g_supplier_path, "\"tag\":\"bridge_burst\"") == 200,
          "C all 200 burst lines present");
    landed += 200;

    /* ---- phase D: write failure + partial write retry ---- */
    shim_fail_write = 1;
    diagnostic_event("bridge_writefail", 7u);
    CHECK(count_lines(g_supplier_path) == landed,
          "D write failure drops exactly that line");
    CHECK(count_lines(g_entry_path) == landed - 2,
          "D shared entry channel equally unaffected by the dropped line");
    shim_fail_write = 0;
    shim_partial_write = 1;
    diagnostic_event("bridge_partial", 9u);
    shim_partial_write = 0;
    CHECK(count_lines(g_supplier_path) == landed + 1,
          "D partial short write recovered by bounded retry");
    CHECK(file_contains(g_supplier_path, "\"tag\":\"bridge_partial\""),
          "D retried line landed complete");
    landed += 1;

    /* ---- phase E: oversized tag stays valid, bounded, never truncates ---- */
    char big[201];
    memset(big, 'x', sizeof big);
    big[0] = 'B';
    big[sizeof big - 1] = '\0';
    diagnostic_event(big, 0xffffffffu);
    CHECK(count_lines(g_supplier_path) == landed + 1, "E long tag adds one line");
    CHECK(file_contains(g_supplier_path, "\"tag\":\"B"), "E long tag line present");
    CHECK(!file_contains(g_supplier_path, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                                   "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                                   "xxxxxxxxxxxxxxxxxxxx\""),
          "E tag bounded (no 120+ x run in file)");

    /* ---- startup-proceeds summary ---- */
    printf("supplier lines final: %ld\n", count_lines(g_supplier_path));
    printf("entry lines final: %ld\n", count_lines(g_entry_path));
    printf("checks=%d failures=%d\n", g_checks, g_failures);
    if (g_failures == 0) {
        printf("HOST-TEST-PASS\n");
        return 0;
    }
    printf("HOST-TEST-FAIL\n");
    return 1;
}
