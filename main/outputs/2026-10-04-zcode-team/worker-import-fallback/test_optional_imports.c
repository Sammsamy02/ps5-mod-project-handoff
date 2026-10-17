/* test_optional_imports.c -- host checks for optional_imports.h.
 *
 * Two modes:
 *   1) strict compile probe of the weak-reference pattern (the SDK flags
 *      the frozen build uses): cc -Wall -Wextra -Werror -O1 -fPIC
 *      -fno-stack-protector -fvisibility=hidden -ffreestanding
 *      -fno-builtin -c  -> use_tu object must compile with the five
 *      imports left UNDEFINED (checked with nm).
 *   2) functional: with all definitions resolved, sorting delegates to the
 *      imported qsort; the internal heapsort (mb__heapsort) is exercised
 *      directly for the unresolved case across edge sizes.
 * Host run: cc -O2 -Wall -Wextra -Werror test_optional_imports.c -o t && ./t
 */
#include "optional_imports.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static int is_sorted(const int *v, size_t n)
{
    size_t i;
    for (i = 1; i < n; ++i)
        if (v[i - 1] > v[i]) return 0;
    return 1;
}

/* Expose the static fallback for direct testing of the unresolved path. */
#define TEST_QUICKSORT_FALLBACK 1
static void sort_fallback(int *v, size_t n)
{
    mb__heapsort(v, n, sizeof(int), cmp_int);
}

int main(void)
{
    optional_import_caps caps;
    int rc = optional_imports_probe(&caps);
    if (rc != 0) { puts("FAIL probe rc"); return 1; }
    /* Host build links real libc, so every weak import resolves. */
    if (!caps.dynamic_module_tools || !caps.qsort_available) {
        puts("FAIL host caps should be fully resolved");
        return 1;
    }
    if (optional_imports_reason(&caps) != OPTIMP_OK) {
        puts("FAIL reason should be OK on host");
        return 1;
    }

    /* delegated path (imported qsort) */
    {
        int v[] = {5, 1, 4, 2, 3, 9, 0, 8, 7, 6, 5, 5};
        optional_qsort(v, sizeof v / sizeof v[0], sizeof(int), cmp_int);
        if (!is_sorted(v, sizeof v / sizeof v[0])) {
            puts("FAIL optional_qsort (delegated)"); return 1;
        }
    }

    /* fallback path (internal heapsort) across sizes incl. duplicates */
    {
        size_t n, k;
        static int v[257];
        unsigned long seed = 0x9E3779B9u;
        for (n = 0; n <= 256; ++n) {
            for (k = 0; k < n; ++k) {
                seed = seed * 6364136223846793005UL + 1442695040888963407UL;
                v[k] = (int)((seed >> 33) % 97); /* heavy duplicates */
            }
            sort_fallback(v, n);
            if (!is_sorted(v, n)) {
                printf("FAIL heapsort n=%zu\n", n); return 1;
            }
        }
    }

    /* degenerate arguments must be no-ops, not crashes */
    optional_qsort(NULL, 0, sizeof(int), cmp_int);
    optional_qsort(NULL, 10, sizeof(int), NULL);
    {
        int one = 42;
        optional_qsort(&one, 1, sizeof(int), cmp_int);
        if (one != 42) { puts("FAIL single element"); return 1; }
    }

    puts("ALL OPTIONAL-IMPORTS TESTS PASSED");
    return 0;
}
