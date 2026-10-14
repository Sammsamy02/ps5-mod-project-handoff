/* fc1_pages.c — worker-fname-runtime-probe (team 2026-10-04). Pure. */
#include "fc1_pages.h"

int igfc1_page_range(uint64_t addr, uint64_t len, uint64_t page_size,
                     uint64_t *first, uint32_t *npages) {
    if (len == 0) return -3;
    if (page_size == 0 || (page_size & (page_size - 1)) != 0) return -2;
    uint64_t begin = addr & ~(page_size - 1);
    /* ceil((addr % ps + len) / ps), overflow-checked */
    uint64_t in_page = addr - begin;
    if (in_page > UINT64_MAX - len) return -1;
    uint64_t span = in_page + len;
    uint64_t n = (span + page_size - 1) / page_size;
    if (n > 8) return -1;                     /* bounded: FC1 reads <= 64 B */
    if (begin > UINT64_MAX - (n * page_size)) return -1;
    *first = begin;
    *npages = (uint32_t)n;
    return 0;
}

int igfc1_bytes_match(const uint8_t *a, const uint8_t *b, int n,
                      unsigned *diff_mask) {
    unsigned mask = 0;
    if (n > 64) n = 64;
    for (int i = 0; i < n; ++i)
        if (a[i] != b[i]) mask |= (1u << i);
    *diff_mask = mask;
    return mask == 0 ? -1 : 0;
}

void igfc1_hex_encode(const uint8_t *b, size_t n, char *out) {
    static const char hexd[] = "0123456789abcdef";
    for (size_t i = 0; i < n; ++i) {
        out[2 * i] = hexd[(b[i] >> 4) & 15];
        out[2 * i + 1] = hexd[b[i] & 15];
    }
    out[2 * n] = '\0';
}
