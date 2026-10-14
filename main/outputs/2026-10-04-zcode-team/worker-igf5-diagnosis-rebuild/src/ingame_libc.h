// ingame_libc.h / minimal freestanding string helpers.
// No libc imports: the PRX carries its own implementations so the import
// table stays exactly the audited strong set (see ingame_abi.h provenance).
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

#endif  // INGAME_LIBC_H
