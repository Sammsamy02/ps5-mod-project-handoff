#include "../mb_win_entry.h"
#include <stddef.h>
#include <stdio.h>

static unsigned failures;
#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #c); ++failures; \
} } while (0)

static void *seen_base, *seen_reserved;
static uint32_t seen_reason;
static int32_t requested_result;
static unsigned entry_calls;

/* This is real executable host code with the Microsoft x64 ABI. The bridge is
 * a separate translation unit, preventing call substitution by the optimizer. */
static int32_t __attribute__((ms_abi)) entry(void *base, uint32_t reason,
    void *reserved) {
    seen_base = base;
    seen_reason = reason;
    seen_reserved = reserved;
    ++entry_calls;
    return requested_result;
}

/* Catches swapped/truncated arguments, skipped/repeated calls, and boolean
 * normalization or sign extension that changes the caller-visible BOOL32. */
int main(void) {
    unsigned char base[8] = {0}, reserved[8] = {0};
    uint32_t unused = UINT32_C(0xa55a55aa);
    const struct {
        uint32_t reason;
        int32_t result;
        int null_reserved;
    } cases[] = {
        {1, 0, 1}, {0, 1, 1}, {1, -17, 0},
        {0, INT32_MIN, 0}, {UINT32_MAX, INT32_MAX, 0},
        {UINT32_C(0x80000000), INT32_C(0x12345678), 1}
    };
    void *address = (void *)(uintptr_t)&entry;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        void *base_argument = base + i;
        void *reserved_argument = cases[i].null_reserved ? NULL : reserved + i;
        requested_result = cases[i].result;
        CHECK(mb_win_call_entry(&unused, address, base_argument,
            cases[i].reason, reserved_argument) == cases[i].result);
        CHECK(entry_calls == i + 1);
        CHECK(seen_base == base_argument);
        CHECK(seen_reason == cases[i].reason);
        CHECK(seen_reserved == reserved_argument);
        CHECK(unused == UINT32_C(0xa55a55aa));
    }
    if (failures) { fprintf(stderr, "%u checks failed\n", failures); return 1; }
    puts("6 entry calls passed: native-to-Win64 arguments, single invocation and unchanged BOOL32");
    return 0;
}
