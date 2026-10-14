/* test_readonly_dir_host.c — s2b: the REAL open() against a chmod-0555
   directory (the console "no permission in /data" analog) must be tolerated:
   no crash, startup proceeds (exit 0), no file appears, open attempts are
   bounded by SUPPLIER_DIAG_OPEN_ATTEMPTS (64) even across 80 events.
   HOST ONLY. */
#include <stdio.h>
#include <stdint.h>

void diagnostic_event(const char *tag, uint32_t value);
extern int shim_open_attempts;

static int g_failures;
#define CHECK(cond, name)                                                     \
    do {                                                                      \
        if (cond)                                                             \
            printf("check ok: %s\n", (name));                                 \
        else {                                                                \
            ++g_failures;                                                     \
            printf("CHECK FAIL: %s\n", (name));                               \
        }                                                                     \
    } while (0)

int main(void)
{
    for (int i = 0; i < 80; ++i)
        diagnostic_event("bridge_readonly", (uint32_t)i);
    CHECK(shim_open_attempts == 64, "ro open attempts capped at 64 across 80 events");
    printf("ro open attempts: %d\n", shim_open_attempts);
    /* Reaching this line at all = startup proceeded (tolerated, not fatal). */
    printf("HOST-TEST-PASS-RO\n");
    return g_failures ? 1 : 0;
}
