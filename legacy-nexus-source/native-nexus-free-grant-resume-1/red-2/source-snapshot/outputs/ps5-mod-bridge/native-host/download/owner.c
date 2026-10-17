#define _POSIX_C_SOURCE 200809L
#include "owner.h"
#include "mbedtls/entropy.h"
#include "entropy_poll.h"
#include "mbedtls/platform_util.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <time.h>

#define ENTROPY_CALL_LIMIT 256u
#define ENTROPY_BYTE_LIMIT 8192u
#define CLOCK_SAMPLE_LIMIT 1048576u
#define NS_PER_SECOND INT64_C(1000000000)
#define MAX_WALL_TIME INT64_C(253402300799)

/* The profile's hardware/time hooks have no owner argument. All library calls
 * and these private windows are serialized by the enclosing runtime. */
static struct {
    unsigned bound, closed, failed, active, expired, limited, in_source;
    unsigned have_mono, have_wall, have_anchor;
    uint32_t window_calls, window_bytes, window_clock_samples;
    uint64_t windows, callbacks, bytes, clock_samples;
    int64_t last_mono, anchor_a, anchor_b, deadline;
    time_t last_wall, anchor_wall;
    void *source_context;
    mb_curl_entropy_read32_fn_v1 read32;
} owner;

static void clear(void *storage, size_t capacity)
{
    volatile unsigned char *bytes = storage;
    for(size_t i = 0; i < capacity; ++i)
        bytes[i] = 0;
}

static int fatal(void)
{
    owner.failed = 1;
    return -1;
}

static int monotonic(int64_t *out)
{
    struct timespec value = {0};
    errno = 0;
    int rc = clock_gettime(CLOCK_MONOTONIC, &value);
    int error = errno;
    const int64_t room = (int64_t)MB_CURL_DOWNLOAD_TIMEOUT_MAX_MS * 1000000;
    if(rc || error || value.tv_sec < 0 ||
       value.tv_sec > (INT64_MAX - 999999999 - room) / NS_PER_SECOND ||
       value.tv_nsec < 0 || value.tv_nsec >= NS_PER_SECOND)
        return fatal();
    int64_t now = (int64_t)value.tv_sec * NS_PER_SECOND + value.tv_nsec;
    if(owner.have_mono && now < owner.last_mono)
        return fatal();
    owner.last_mono = now;
    owner.have_mono = 1;
    *out = now;
    return 0;
}

/* Clock faults survive every window. Only its deadline/work allowance ends
 * locally; neither condition creates another entropy allowance in that job. */
static int sample(time_t *wall, int64_t *now)
{
    if(!owner.bound || owner.closed || !owner.active)
        return fatal();
    if(owner.failed || owner.limited)
        return -1;
    if(owner.window_clock_samples >= CLOCK_SAMPLE_LIMIT) {
        owner.limited = 1;
        return -1;
    }
    if(owner.clock_samples == UINT64_MAX)
        return fatal();
    ++owner.window_clock_samples;
    ++owner.clock_samples;
    int64_t a, b;
    if(monotonic(&a))
        return -1;
    errno = 0;
    time_t value = time(NULL);
    int error = errno;
    if(monotonic(&b))
        return -1;
    if(error || value == (time_t)-1 || value < 0 ||
       value > MAX_WALL_TIME || b - a > INT64_C(100000000) ||
       (owner.have_wall && value < owner.last_wall))
        return fatal();
    if(owner.have_anchor) {
        int64_t low = (a - owner.anchor_b) / NS_PER_SECOND;
        int64_t delta = b - owner.anchor_a;
        int64_t high = delta / NS_PER_SECOND + (delta % NS_PER_SECOND != 0);
        if(value < owner.anchor_wall + low || value > owner.anchor_wall + high)
            return fatal();
    }
    else {
        owner.anchor_wall = value;
        owner.anchor_a = a;
        owner.anchor_b = b;
        owner.have_anchor = 1;
    }
    owner.last_wall = value;
    owner.have_wall = 1;
    if(owner.deadline && b >= owner.deadline)
        owner.expired = 1;
    if(wall)
        *wall = value;
    if(now)
        *now = b;
    return 0;
}

int mb_curl_owner_bind_v1(const mb_curl_runtime_params_v1 *params)
{
    if(owner.bound || owner.closed || owner.failed || !params ||
       params->struct_size != sizeof(*params) ||
       params->abi_version != MB_CURL_DOWNLOAD_ABI_V1 || !params->entropy_read32)
        return -1;
    owner.source_context = params->entropy_context;
    owner.read32 = params->entropy_read32;
    owner.bound = 1;
    return 0;
}

int mb_curl_owner_window_begin_v1(uint32_t timeout_ms)
{
    if(!owner.bound || owner.closed || owner.failed || owner.active ||
       !timeout_ms || timeout_ms > MB_CURL_DOWNLOAD_TIMEOUT_MAX_MS)
        return -1;
    if(owner.windows == UINT64_MAX)
        return fatal();
    ++owner.windows;
    owner.active = 1;
    owner.expired = owner.limited = 0;
    owner.window_calls = owner.window_bytes = owner.window_clock_samples = 0;
    owner.deadline = 0;
    int64_t now;
    if(sample(NULL, &now)) {
        owner.active = 0;
        return -1;
    }
    owner.deadline = now + (int64_t)timeout_ms * 1000000;
    return 0;
}

int mb_curl_owner_window_end_v1(void)
{
    if(!owner.active || owner.in_source)
        return -1;
    if(!owner.failed && !owner.limited)
        (void)sample(NULL, NULL);
    owner.active = 0;
    owner.deadline = 0;
    return owner.failed ? -1 : 0;
}

int mb_curl_owner_guard_v1(void)
{
    if(!owner.bound || owner.closed || !owner.active || owner.failed ||
       owner.expired || owner.limited)
        return 0;
    return !sample(NULL, NULL) && !owner.expired;
}

int mb_curl_owner_failed_v1(void) { return owner.failed != 0; }
int mb_curl_owner_window_expired_v1(void) { return owner.expired != 0; }
void mb_curl_owner_fail_v1(void) { owner.failed = 1; }

void mb_curl_owner_close_v1(void)
{
    if(!owner.bound || owner.closed)
        return;
    if(owner.active || owner.in_source) {
        (void)fatal();
        return;
    }
    owner.closed = 1;
    owner.read32 = NULL;
    owner.source_context = NULL;
}

void mb_curl_owner_snapshot_v1(mb_curl_runtime_result_v1 *result)
{
    if(!result)
        return;
    *result = (mb_curl_runtime_result_v1){
        .struct_size = sizeof(*result), .abi_version = MB_CURL_DOWNLOAD_ABI_V1,
        .terminal = owner.failed, .window_active = owner.active,
        .window_expired = owner.expired, .entropy_windows = owner.windows,
        .entropy_callbacks = owner.callbacks, .entropy_bytes = owner.bytes,
        .clock_samples = owner.clock_samples
    };
}

int mbedtls_hardware_poll(void *data, unsigned char *output,
                         size_t capacity, size_t *olen)
{
    unsigned char staging[32] = {0};
    int bounded = output && capacity >= sizeof(staging) && capacity <= 128u;
    if(olen)
        *olen = 0;
    if(!owner.bound || owner.closed || !owner.active || data || !olen ||
       !bounded || owner.in_source) {
        (void)fatal();
        goto failed;
    }
    if(!mb_curl_owner_guard_v1())
        goto failed;
    if(owner.window_calls >= ENTROPY_CALL_LIMIT ||
       owner.window_bytes > ENTROPY_BYTE_LIMIT - sizeof(staging)) {
        owner.limited = 1;
        goto failed;
    }
    if(owner.callbacks == UINT64_MAX || owner.bytes > UINT64_MAX - sizeof(staging)) {
        (void)fatal();
        goto failed;
    }
    ++owner.window_calls; /* Reserve before the real platform callback. */
    ++owner.callbacks;
    owner.in_source = 1;
    int rc = owner.read32(owner.source_context, staging);
    owner.in_source = 0;
    if(rc) {
        (void)fatal();
        goto failed;
    }
    owner.window_bytes += sizeof(staging);
    owner.bytes += sizeof(staging);
    if(!mb_curl_owner_guard_v1())
        goto failed;
    memcpy(output, staging, sizeof(staging));
    *olen = sizeof(staging);
    clear(staging, sizeof(staging));
    return 0;
failed:
    clear(staging, sizeof(staging));
    if(bounded)
        clear(output, capacity);
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
}

time_t exp_clock_time(time_t *out)
{
    time_t value = (time_t)-1;
    if(sample(&value, NULL) || owner.expired)
        value = (time_t)-1;
    if(out)
        *out = value;
    return value;
}

struct tm *mbedtls_platform_gmtime_r(const mbedtls_time_t *value, struct tm *out)
{
    if(!owner.bound || owner.closed || !owner.active) {
        (void)fatal();
        goto failed;
    }
    if(!mb_curl_owner_guard_v1())
        goto failed;
    if(!value || !out || *value < 0 || *value > MAX_WALL_TIME) {
        (void)fatal();
        goto failed;
    }
    errno = 0;
    struct tm *converted = gmtime(value);
    int error = errno;
    if(!converted || error) {
        (void)fatal();
        goto failed;
    }
    struct tm copy = *converted;
    if(copy.tm_year < 70 || copy.tm_year > 8099 || copy.tm_mon < 0 ||
       copy.tm_mon > 11 || copy.tm_mday < 1 || copy.tm_mday > 31 ||
       copy.tm_hour < 0 || copy.tm_hour > 23 || copy.tm_min < 0 ||
       copy.tm_min > 59 || copy.tm_sec < 0 || copy.tm_sec > 59 ||
       copy.tm_wday < 0 || copy.tm_wday > 6) {
        (void)fatal();
        goto failed;
    }
    static const unsigned days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    unsigned year = (unsigned)(copy.tm_year + 1900);
    unsigned leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    if((unsigned)copy.tm_mday > days[copy.tm_mon] + (copy.tm_mon == 1 && leap) ||
       copy.tm_yday < 0 || (unsigned)copy.tm_yday >= 365u + leap) {
        (void)fatal();
        goto failed;
    }
    if(!mb_curl_owner_guard_v1())
        goto failed;
    *out = copy;
    return out;
failed:
    if(out)
        clear(out, sizeof(*out));
    return NULL;
}
