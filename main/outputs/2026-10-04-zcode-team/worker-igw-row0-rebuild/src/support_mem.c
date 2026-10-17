// support_mem.c — worker-combined-igf7 (team 2026-10-04).
//
// Freestanding definitions of the ISO C required mem* functions. The probe
// links with -ffreestanding -fno-builtin, so the C sources this probe
// consumes (worker-batch2-live-prep live_gua_reads.c, worker-rung3-chunk-probe
// rung3_chunk_addon.c) emit external references to memcpy/memset. Defining
// them HERE keeps them LOCAL symbols of the module — the import table stays
// EXACTLY the 7 verbatim frozen-contract rows (see import_references.S);
// adding a libc/libkernel import for these would violate the lineage
// import contract and would be a new loader identity (never console-proven).
//
// Simplest correct byte loops; the probe copies at most 96 bytes per read
// and never in a hot path. No alignment assumptions, no OS calls.
#include <stddef.h>

void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d == s || n == 0) return dst;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

void *memset(void *dst, int c, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char v = (unsigned char)c;
    while (n--) *d++ = v;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *x = (const unsigned char *)a;
    const unsigned char *y = (const unsigned char *)b;
    while (n--) {
        if (*x != *y) return (int)*x - (int)*y;
        ++x; ++y;
    }
    return 0;
}
