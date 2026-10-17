/* use_optional_imports.c -- strict freestanding compile probe (no execution).
 * Compiles the exact weak-reference pattern with the frozen build's flags;
 * the five optional imports stay UNDEFINED in the object (verified by nm in
 * verify_optional_imports.sh), proving the header introduces no hidden
 * strong dependency of its own. */
#include "optional_imports.h"

int use_probe(optional_import_caps *caps) { return optional_imports_probe(caps); }
int use_reason(const optional_import_caps *caps) { return optional_imports_reason(caps); }
void use_sort(void *b, size_t n, size_t s, int (*c)(const void *, const void *)) { optional_qsort(b, n, s, c); }
