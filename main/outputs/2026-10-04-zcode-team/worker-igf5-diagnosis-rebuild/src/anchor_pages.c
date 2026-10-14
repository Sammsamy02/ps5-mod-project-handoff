// anchor_pages.c — worker-code-anchor-reads (team 2026-10-04).
// PURE helpers; see anchor_pages.h. Freestanding-safe (uses ingame_libc.h
// only where a host build is not harmed; kept dependency-free here).
#include "anchor_pages.h"

int igcar_page_range(uint64_t addr, uint64_t len, uint64_t page_size,
                     uint64_t *first, uint32_t *npages) {
    if (page_size == 0 || (page_size & (page_size - 1)) != 0) return -2;
    if (len == 0) return -1;
    uint64_t start = addr & ~(page_size - 1);
    /* last byte the access touches (inclusive) */
    uint64_t last = addr + (len - 1);
    if (last < addr) return -1;              /* overflow */
    uint64_t end = last & ~(page_size - 1);  /* first page of last byte */
    if (end < start) return -1;              /* cannot happen w/o overflow */
    uint64_t n = ((end - start) / page_size) + 1;
    if (n > 4) return -1;                    /* bounded: our reads are 16 B */
    if (first) *first = start;
    if (npages) *npages = (uint32_t)n;
    return 0;
}

int igcar_bytes_match(const uint8_t *a, const uint8_t *b, int n,
                      unsigned *diff_mask) {
    unsigned mask = 0;
    int equal = 1;
    for (int i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            equal = 0;
            if (i < 32) mask |= 1u << i;
        }
    }
    if (diff_mask) *diff_mask = mask;
    return equal ? 1 : 0;
}
