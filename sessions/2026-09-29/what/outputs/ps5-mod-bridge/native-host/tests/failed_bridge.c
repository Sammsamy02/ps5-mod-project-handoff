/* Isolated host fixture: no production failure-injection API. */
#include "core_bridge.h"
int mb_core_selftest(char *output, size_t capacity, size_t *length) {
    if (output && capacity) output[0]=0;
    if (length) *length=0;
    return MB_CORE_INTERNAL_ERROR;
}
