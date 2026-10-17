// ingame_libc.h — VERBATIM VENDOR from worker-ingame-probe-v3/src/
// ingame_libc.h (lineage provenance in that file) with ONE ADDED helper,
// pp_memzero (bulk zero for the ported PS5_PadData stack sample; same
// freestanding family as ig_memset). No libc imports: the PRX carries its
// own implementations so the import table stays exactly the audited strong
// set (see pad_abi.h provenance).
#ifndef INGAME_LIBC_H
#define INGAME_LIBC_H

#include <stddef.h>

void *ig_memset(void *dst, int c, size_t n);
void *ig_memcpy(void *dst, const void *src, size_t n);
size_t ig_strlen(const char *s);
int ig_strcmp(const char *a, const char *b);
// Bounded copy; always NUL-terminates within dst_cap. Returns dst.
char *ig_strlcpy_bounded(char *dst, size_t dst_cap, const char *src,
                         size_t max_src);
// ADDED for the pad probe: zero n bytes at dst (ig_memset(dst,0,n) alias
// with a clearer name at call sites; same freestanding implementation).
void pp_memzero(void *dst, size_t n);

#endif  // INGAME_LIBC_H
