/*
 * Owned implementation using ISO C fread byte semantics (C11 7.21.8.1).
 * No FILE internals are accessed. Hidden visibility is essential: the system
 * fread implementation must continue to bind its own internal stdio helpers.
 */
#include <stdio.h>

__attribute__((visibility("hidden")))
int fgetc(FILE *stream) {
    unsigned char c;
    return fread(&c, 1, 1, stream) == 1 ? (int)c : EOF;
}
