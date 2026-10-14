/* test_wiring.c — scenario driver: each invocation runs ONE scenario in a
 * fresh process with its own /data root, diag path, status path and bridge
 * log, then module_start (+ module_stop when asked). The run_host_tests.sh
 * script asserts the observable outcomes with grep. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int module_start(size_t argc, const void *args);
int module_stop(size_t argc, const void *args);

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: test_wiring <scenario>\n");
        return 64;
    }
    const char *scenario = argv[1];
    int rc = module_start(0, NULL);
    printf("module_start=%d\n", rc);

    if (strcmp(scenario, "stop") == 0 || strcmp(scenario, "stopfail") == 0 ||
        strcmp(scenario, "double_start") == 0) {
        int stop_rc = module_stop(0, NULL);
        printf("module_stop=%d\n", stop_rc);
    }
    if (strcmp(scenario, "double_start") == 0) {
        int again = module_start(0, NULL);
        printf("module_start_again=%d\n", again);
    }
    return 0;
}
