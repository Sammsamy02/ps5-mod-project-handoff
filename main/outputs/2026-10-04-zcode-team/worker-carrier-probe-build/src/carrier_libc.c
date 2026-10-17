// carrier_libc.c — worker-carrier-probe-build (cc1, team 2026-10-04).
// NEW FILE. Same role as worker-fc1-validator-probe/src/fc1pool_libc.c:
// PRX-local plain-name memcpy/memset shims over the byte-identical lineage
// helpers (ingame_libc.c) so any compiler-emitted copy/zeroing call resolves
// INSIDE the module and the import surface stays exactly the audited
// verbatim rows (here: the igf 7 + nanosleep + pthread_create +
// pthread_detach). No other plain libc names are defined on purpose — an
// undefined-symbol failure at packaging is the loud signal, never a silent
// stub.
#include "ingame_libc.h"

void *memcpy(void *dst, const void *src, size_t n) {
    return ig_memcpy(dst, src, n);
}

void *memset(void *dst, int c, size_t n) {
    return ig_memset(dst, c, n);
}
