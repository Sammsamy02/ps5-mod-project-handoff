// qh_libc.c — worker-record-sink-addon (qdrv1, team 2026-10-04). NEW FILE.
//
// PRX-local plain-name memcpy/memset shims over the byte-identical lineage
// helpers (ingame_libc.c). The vendored NativeQueryHarness TU contains a
// 128-byte trivially-copyable struct copy (QueryResultRecord) that clang may
// lower to a `memcpy` call even under -fno-builtin, and its explicit 32-byte
// gate_source copy now calls ig_memcpy (labeled change 4). Defining the two
// plain names here keeps ANY compiler-emitted copy/zeroing call resolved
// INSIDE the module: the import surface stays exactly the 11 audited verbatim
// rows, and no libc++/libc import appears. No other plain libc names are
// defined on purpose — an undefined-symbol failure at packaging is the loud
// signal, never a silent stub.
#include "ingame_libc.h"

void *memcpy(void *dst, const void *src, size_t n) {
    return ig_memcpy(dst, src, n);
}

void *memset(void *dst, int c, size_t n) {
    return ig_memset(dst, c, n);
}
