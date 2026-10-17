// anchor_pages.h — worker-code-anchor-reads (team 2026-10-04).
// PURE helpers for the mprotect-first code-anchor reads (host-testable:
// no syscalls, no imports, no faulting dereferences). Compiled BOTH into
// the PRX and into tests/host_main.c.
#ifndef IGCAR_ANCHOR_PAGES_H
#define IGCAR_ANCHOR_PAGES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Page range covering [addr, addr+len).
   0 ok -> *first = page-aligned start, *npages in [1, 4];
   -1 on overflow, -2 if page_size is not a power of two or zero. */
int igcar_page_range(uint64_t addr, uint64_t len, uint64_t page_size,
                     uint64_t *first, uint32_t *npages);

/* 1 iff ALL n bytes are equal; sets *diff_mask (bit i set when a[i]!=b[i],
   n <= 32). Returns 0 when any byte differs, -1 when equal (mirrors the
   igf4 anchor record semantics: match 1 == equal, 0 == differ). */
int igcar_bytes_match(const uint8_t *a, const uint8_t *b, int n,
                      unsigned *diff_mask);

#ifdef __cplusplus
}
#endif

#endif  // IGCAR_ANCHOR_PAGES_H
