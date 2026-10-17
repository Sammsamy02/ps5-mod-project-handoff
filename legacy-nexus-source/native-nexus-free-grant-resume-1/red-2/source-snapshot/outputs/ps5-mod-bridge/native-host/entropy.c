#define _POSIX_C_SOURCE 200809L
#include "entropy.h"
#ifndef HOST_NATIVE
#define HOST_NATIVE 0
#endif
#if HOST_NATIVE
#include <errno.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <time.h>

static int64_t entropy_nanoseconds(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) || value.tv_sec < 0 ||
        value.tv_sec > (INT64_MAX - 999999999) / 1000000000 ||
        value.tv_nsec < 0 || value.tv_nsec >= 1000000000L) return -1;
    return (int64_t)value.tv_sec * 1000000000 + value.tv_nsec;
}

static void classify(struct mb_entropy_result *result) {
    result->status = "failed";
    if (result->return_code != 0) result->reason = "api_return";
    else if (result->error_number != 0) result->reason = "errno_nonzero";
    else if (result->returned_length == 0) result->reason = "no_progress";
    else if (result->returned_length < MB_ENTROPY_BYTES) result->reason = "short_result";
    else if (result->returned_length > MB_ENTROPY_BYTES) result->reason = "oversized_result";
    else { result->status = "passed"; result->reason = "exact_result"; }
}
#endif

static void clear32(unsigned char buffer[MB_ENTROPY_BYTES]) {
    volatile unsigned char *owned = buffer;
    for (size_t i = 0; i < MB_ENTROPY_BYTES; ++i) owned[i] = 0;
}

int mb_entropy_read32(struct mb_entropy_result *result,
                      unsigned char buffer[MB_ENTROPY_BYTES],
                      struct mb_entropy_budget *budget) {
    if (!result || !buffer || !budget) {
        if (buffer) clear32(buffer);
        return -1;
    }
    *result = (struct mb_entropy_result){
        .status = "unavailable", .reason = "desktop_fixture", .elapsed_ms = -1
    };
#if HOST_NATIVE
    if (budget->api_count >= MB_ENTROPY_API_LIMIT) {
        result->status = "failed";
        result->reason = "budget_exhausted";
        clear32(buffer);
        return -1;
    }
    ++budget->api_count;
    int selector[2] = {CTL_KERN, KERN_ARND};
    size_t length = MB_ENTROPY_BYTES;
    int64_t before = entropy_nanoseconds();
    errno = 0;
    int rc = sysctl(selector, 2, buffer, &length, NULL, 0);
    int api_errno = errno;
    result->return_code = rc;
    result->error_number = api_errno;
    result->returned_length = length;
    result->call_count = 1;
    int64_t after = entropy_nanoseconds();
    if (before >= 0 && after >= before) result->elapsed_ms = (after - before) / 1000000;
    classify(result);
    if (result->return_code == 0 && result->error_number == 0 &&
        result->returned_length == MB_ENTROPY_BYTES) return 0;
#endif
    clear32(buffer);
    return -1;
}

void mb_entropy_observe(struct mb_entropy_result *result,
                        unsigned char buffer[MB_ENTROPY_BYTES]) {
    struct mb_entropy_budget budget = {0};
    (void)mb_entropy_read32(result, buffer, &budget);
    if (buffer) clear32(buffer);
}
