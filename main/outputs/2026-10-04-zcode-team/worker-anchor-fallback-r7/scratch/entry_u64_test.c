/* Host micro-test: exercise the ACTUAL r7 entry file's new log_append_u64
 * (same-TU include makes the static helper reachable). */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "entry-b/runtime_module_entry_r7.c"
/* link dummies for the entry's extern section markers (unused on this path) */
unsigned char __eh_frame_start[1], __eh_frame_end[1];
unsigned char __eh_frame_hdr_start[1], __eh_frame_hdr_end[1];
unsigned char __dso_handle;
probe_callback __init_array_start[1], __init_array_end[1];
probe_callback __fini_array_start[1], __fini_array_end[1];
static size_t emit(char *b, size_t cap, unsigned long long v)
{
    size_t used = 0;
    log_append_u64(b, &used, cap, (uint64_t)v);
    return used;
}
int main(void)
{
    char b[64];
    memset(b, 0, sizeof b); assert(emit(b, 63, 0) == 1 && b[0] == '0');
    memset(b, 0, sizeof b); assert(emit(b, 63, 12345) == 5 && strcmp(b, "12345") == 0);
    memset(b, 0, sizeof b); assert(emit(b, 63, 4294967296ull) == 10 && strcmp(b, "4294967296") == 0);
    memset(b, 0, sizeof b); assert(emit(b, 63, 18446744073709551615ull) == 20 && strcmp(b, "18446744073709551615") == 0);
    memset(b, 0, sizeof b); assert(emit(b, 3, 12345) == 3 && strcmp(b, "123") == 0); /* capacity clamp: writes up to capacity */
    printf("ENTRY-U64 MICRO-TEST: ALL PASS\n");
    return 0;
}
