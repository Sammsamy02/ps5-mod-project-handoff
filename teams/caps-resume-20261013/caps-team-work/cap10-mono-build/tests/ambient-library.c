/* Actual Mach-O test fixture. It records whether the OS loader ran its constructor. */
#include <stdio.h>
#include <stdlib.h>
__attribute__((constructor)) static void record_load(void)
{
    const char *path = getenv("CAP10_AMBIENT_MARKER");
    if (path) { FILE *file = fopen(path, "a"); if (file) { fputs("loaded\n", file); fclose(file); } }
}
__attribute__((visibility("default"))) int cap10_loader_sentinel(void) { return 99; }
