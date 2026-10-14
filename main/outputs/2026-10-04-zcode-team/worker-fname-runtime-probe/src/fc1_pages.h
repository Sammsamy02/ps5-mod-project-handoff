/* fc1_pages.h — worker-fname-runtime-probe (team 2026-10-04).
 *
 * PURE helpers for the FC-1 mprotect-first runtime probe. Host-testable:
 * no syscalls, no imports, no foreign dereferences. Semantics mirror the
 * igf5 lineage (worker-code-anchor-reads/src/anchor_pages.[ch]) so the
 * controller can vendor both side by side in igf6 without surprises.
 */
#ifndef IGFC1_PAGES_H
#define IGFC1_PAGES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Page range covering [addr, addr+len).
   0 ok -> *first = page-aligned start, *npages in [1, 8];
   -1 on overflow, -2 if page_size is zero or not a power of two,
   -3 if len == 0. */
int igfc1_page_range(uint64_t addr, uint64_t len, uint64_t page_size,
                     uint64_t *first, uint32_t *npages);

/* 1 iff ALL n bytes are equal (n <= 64); sets *diff_mask (bit i set when
   a[i] != b[i]). Returns 0 when any byte differs, -1 when equal.
   Mirrors the igf4/igf5 record semantics: match field 1 == equal,
   0 == differ. */
int igfc1_bytes_match(const uint8_t *a, const uint8_t *b, int n,
                      unsigned *diff_mask);

/* Hex encode n bytes into out (2n chars + NUL). n <= 64 -> out needs 129. */
void igfc1_hex_encode(const uint8_t *b, size_t n, char *out);

#ifdef __cplusplus
}
#endif

#endif /* IGFC1_PAGES_H */
