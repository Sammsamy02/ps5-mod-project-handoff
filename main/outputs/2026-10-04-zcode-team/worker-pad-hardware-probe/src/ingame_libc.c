// ingame_libc.c — VERBATIM VENDOR from worker-ingame-probe-v3/src/
// ingame_libc.c (freestanding implementations, no dynamic imports) with the
// ONE added pp_memzero helper declared in ingame_libc.h.
#include "ingame_libc.h"

void *ig_memset(void *dst, int c, size_t n) {
    unsigned char *p = (unsigned char *)dst;
    unsigned char v = (unsigned char)c;
    while (n--) *p++ = v;
    return dst;
}

void *ig_memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

size_t ig_strlen(const char *s) {
    size_t n = 0;
    while (s && s[n]) ++n;
    return n;
}

int ig_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

char *ig_strlcpy_bounded(char *dst, size_t dst_cap, const char *src,
                         size_t max_src) {
    size_t i = 0;
    if (dst_cap == 0) return dst;
    if (src) {
        while (i + 1 < dst_cap && i < max_src && src[i]) { dst[i] = src[i]; ++i; }
    }
    dst[i] = '\0';
    return dst;
}

void pp_memzero(void *dst, size_t n) {
    (void)ig_memset(dst, 0, n);
}
