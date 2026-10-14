/* throttle_host.c — idle-cost fixture: the poll's throttle divisor.
   worker-insession-diag, 2026-10-05. Compiled with
   -DINSESSION_CMD_POLL_EVERY=100: 250 loop ticks must produce EXACTLY 2
   intake opens (ticks 100 and 200) and nothing else — the idle event-loop
   cost is one integer increment per tick and one failed open per poll. */
#include <stdio.h>

#include "insession_cmd_poll.h"

extern int shim_open_attempts;
extern int shim_rename_attempts;

/* The throttle run never stages an intake file, so nothing should ever call
   this; the definition exists only to satisfy the link with the real
   diagnostic.c + host_shims.c objects (no stub kernel symbols anywhere). */
void test_emit_log_line(const char *line)
{
    (void)line;
    fprintf(stderr, "UNEXPECTED LOG LINE IN THROTTLE RUN: %s\n", line);
}

int main(void)
{
    int i;
    for (i = 0; i < 250; ++i)
        insession_cmd_poll();
    printf("[host-test] throttle: 250 ticks, opens=%d renames=%d\n",
           shim_open_attempts, shim_rename_attempts);
    if (shim_open_attempts != 2 || shim_rename_attempts != 0)
    {
        printf("[host-test] FAIL: expected opens=2 renames=0\n");
        return 1;
    }
    printf("THROTTLE-TEST-PASS\n");
    return 0;
}
