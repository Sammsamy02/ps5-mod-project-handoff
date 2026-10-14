/* test_audit.c — scenario driver for the DELIVERED-entry audit.
 * Each invocation runs ONE scenario in a fresh process (fresh statics:
 * entry_guard, lifecycle, runtime_started, channel fds):
 *   plain    module_start once
 *   reentry  module_start twice in-process (drives the -21 wrong-state branch)
 *   stop     module_start + module_stop
 * rc values are printed as "module_start=<rc>" lines for grep assertions. */
#include <stdio.h>
#include <string.h>

int module_start(size_t argc, const void *args);
int module_stop(size_t argc, const void *args);

int main(int argc, char **argv)
{
    const char *scenario = argc >= 2 ? argv[1] : "plain";
    int rc = module_start(0, NULL);
    printf("module_start=%d\n", rc);
    if (strcmp(scenario, "reentry") == 0) {
        int again = module_start(0, NULL);
        printf("module_start_again=%d\n", again);
    }
    if (strcmp(scenario, "stop") == 0) {
        int stop_rc = module_stop(0, NULL);
        printf("module_stop=%d\n", stop_rc);
    }
    return 0;
}
