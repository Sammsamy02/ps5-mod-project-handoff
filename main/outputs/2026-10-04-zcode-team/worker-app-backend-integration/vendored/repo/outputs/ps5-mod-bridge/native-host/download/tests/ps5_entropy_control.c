/* Actual adapter/owner controls with only sysctl replaced in their private
 * host objects. These fixture bytes are never a runtime entropy claim. */
#define _POSIX_C_SOURCE 200809L
#include "../ps5_entropy.h"
#include "../owner.h"
#include "entropy_poll.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sysctl.h>

static struct {
    int rc, error;
    size_t returned, written;
} reply = {0, 0, 32, 32};
static unsigned syscalls, boundary_contract = 1, accepted, failures;
static const char *first_failure = "none";
static unsigned fail_on_call;
static const uint8_t fixture[32] = {
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
    0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
    0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
    0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f
};

#define CHECK(expr, name) do { if(!(expr)) { \
    ++failures; if(!strcmp(first_failure, "none")) first_failure = (name); \
} } while(0)

int mb_ps5_entropy_fixture_sysctl(const int *mib, unsigned mib_count,
                                void *output, size_t *length,
                                const void *new_value, size_t new_length)
{
    ++syscalls;
    int good = mib && mib_count == 2 && mib[0] == 1 && mib[1] == 37 &&
               output && length && *length == 32 && !new_value &&
               new_length == 0 && errno == 0;
    boundary_contract &= good;
    if(!good) { errno = EINVAL; return -1; }
    size_t written = fail_on_call == syscalls ? 7u : reply.written;
    if(written > 32) { boundary_contract = 0; errno = EINVAL; return -1; }
    memcpy(output, fixture, written);
    *length = reply.returned;
    if(fail_on_call == syscalls) { errno = EIO; return -1; }
    if(reply.error >= 0) errno = reply.error;
    return reply.rc;
}

struct guarded { uint8_t before[17], output[32], after[17]; };
static int all(const void *data, size_t size, uint8_t expected)
{
    const uint8_t *bytes = data;
    for(size_t i = 0; i < size; ++i) if(bytes[i] != expected) return 0;
    return 1;
}
static void init(struct guarded *storage) { memset(storage, 0xa5, sizeof(*storage)); }
static void canaries(const struct guarded *storage)
{
    CHECK(all(storage->before, 17, 0xa5) && all(storage->after, 17, 0xa5), "output_canaries");
}

static int direct(const char *name)
{
    int success = !strcmp(name, "exact") || !strcmp(name, "stale-errno");
    int invalid_context = !strcmp(name, "invalid-context");
    int invalid_output = !strcmp(name, "invalid-output");
    if(!strcmp(name, "rc-negative")) { reply.rc = -1; reply.error = EIO; }
    else if(!strcmp(name, "rc-negative-no-errno")) { reply.rc = -1; reply.error = 0; }
    else if(!strcmp(name, "rc-positive")) reply.rc = 7;
    else if(!strcmp(name, "errno-nonzero")) reply.error = EIO;
    else if(!strcmp(name, "length-zero")) reply.returned = 0;
    else if(!strcmp(name, "length-one")) { reply.returned = 1; reply.written = 1; }
    else if(!strcmp(name, "length-short")) { reply.returned = 31; reply.written = 31; }
    else if(!strcmp(name, "length-oversized")) reply.returned = 33;
    else if(!strcmp(name, "length-max")) reply.returned = SIZE_MAX;
    else if(!strcmp(name, "stale-errno")) reply.error = -1;
    else if(!success && !invalid_context && !invalid_output) return -1;
    if(reply.rc || reply.error > 0) reply.written = 7;
    struct guarded storage; init(&storage);
    errno = ERANGE;
    int rc = mb_curl_ps5_entropy_read32_v1(invalid_context ? &storage : NULL,
                                         invalid_output ? NULL : storage.output);
    CHECK((rc == 0) == success, "source_acceptance");
    CHECK(syscalls == (unsigned)(!invalid_context && !invalid_output), "one_call_no_retry");
    CHECK(boundary_contract, "sysctl_arguments_and_errno");
    if(!invalid_output) {
        if(success) CHECK(!memcmp(storage.output, fixture, 32), "exact_output");
        else CHECK(all(storage.output, 32, 0), "failure_full_output_wipe");
    }
    else CHECK(all(storage.output, 32, 0xa5), "null_output_leaves_other_objects");
    canaries(&storage);
    return 0;
}

static int poll(int want_success)
{
    struct { uint8_t before[17], output[128], after[17]; } storage;
    memset(&storage, 0xa5, sizeof(storage));
    size_t actual = 99;
    int rc = mbedtls_hardware_poll(NULL, storage.output, 128, &actual);
    CHECK((rc == 0) == want_success, "owner_poll_status");
    CHECK(actual == (want_success ? 32u : 0u), "owner_published_length");
    if(want_success) {
        CHECK(!memcmp(storage.output, fixture, 32), "owner_exact_output");
        CHECK(all(storage.output + 32, 96, 0xa5), "owner_output_tail");
    }
    else CHECK(all(storage.output, 128, 0), "owner_full_capacity_wipe");
    CHECK(all(storage.before, 17, 0xa5) && all(storage.after, 17, 0xa5), "owner_canaries");
    if(rc == 0) ++accepted;
    return rc;
}

static int owned(const char *name, mb_curl_runtime_result_v1 *snapshot)
{
    mb_curl_runtime_params_v1 params = {sizeof(params), 1, NULL, mb_curl_ps5_entropy_read32_v1};
    if(mb_curl_owner_bind_v1(&params)) return -1;
    if(!strcmp(name, "quota")) {
        if(mb_curl_owner_window_begin_v1(5000)) return -1;
        for(unsigned i = 0; i < 256; ++i) if(poll(1)) break;
        CHECK(accepted == 256, "window_allows_256_source_calls");
        CHECK(syscalls == 256, "quota_exact_os_calls");
        (void)poll(0);
        CHECK(syscalls == 256 && !mb_curl_owner_failed_v1(), "quota_refusal_no_source_fault");
        mb_curl_owner_snapshot_v1(snapshot);
        CHECK(snapshot->entropy_callbacks == 256 && snapshot->entropy_bytes == 8192, "quota_lifetime_counters");
        CHECK(mb_curl_owner_window_end_v1() == 0, "end_local_quota");
        int begin = mb_curl_owner_window_begin_v1(5000);
        CHECK(begin == 0, "independent_next_window");
        if(begin == 0) {
            (void)poll(1);
            CHECK(syscalls == 257, "next_window_real_source_call");
            CHECK(mb_curl_owner_window_end_v1() == 0, "next_window_end");
        }
        mb_curl_owner_snapshot_v1(snapshot);
        CHECK(snapshot->entropy_windows == 2 && snapshot->entropy_callbacks == 257 &&
              snapshot->entropy_bytes == 8224 && !snapshot->terminal, "aggregate_counts_retained");
    }
    else if(!strcmp(name, "source-fault")) {
        if(mb_curl_owner_window_begin_v1(5000)) return -1;
        fail_on_call = 2;
        (void)poll(1); (void)poll(0);
        CHECK(syscalls == 2 && mb_curl_owner_failed_v1(), "source_fault_terminal");
        (void)poll(0);
        CHECK(syscalls == 2, "no_os_calls_after_fault");
        CHECK(mb_curl_owner_window_end_v1() != 0, "fault_window_end");
        CHECK(mb_curl_owner_window_begin_v1(5000) != 0, "fault_cannot_open_new_window");
        CHECK(mb_curl_owner_bind_v1(&params) != 0, "fault_cannot_rebind");
        mb_curl_owner_snapshot_v1(snapshot);
        CHECK(snapshot->entropy_callbacks == 2 && snapshot->entropy_bytes == 32,
              "fault_retains_attempt_and_success_counts");
    }
    else if(!strcmp(name, "outside-window")) {
        (void)poll(0);
        CHECK(syscalls == 0 && mb_curl_owner_failed_v1(), "outside_window_terminal_before_source");
        CHECK(mb_curl_owner_window_begin_v1(5000) != 0, "outside_window_cannot_reopen");
    }
    else return -1;
    mb_curl_owner_snapshot_v1(snapshot);
    if(snapshot->window_active) (void)mb_curl_owner_window_end_v1();
    mb_curl_owner_close_v1();
    CHECK(mb_curl_owner_bind_v1(&params) != 0, "closed_cannot_rebind");
    mb_curl_owner_snapshot_v1(snapshot);
    CHECK(!snapshot->window_active && boundary_contract, "closed_and_boundary_contract");
    return 0;
}

int main(int argc, char **argv)
{
    if(argc != 2) return 2;
    mb_curl_runtime_result_v1 snapshot = {0};
    int is_owned = !strcmp(argv[1], "quota") || !strcmp(argv[1], "source-fault") ||
                   !strcmp(argv[1], "outside-window");
    if((is_owned ? owned(argv[1], &snapshot) : direct(argv[1])) != 0) return 2;
    printf("{\"case\":\"%s\",\"passed\":%s,\"failures\":%u,\"first_failure\":\"%s\","
           "\"syscalls\":%u,\"boundary_contract\":%s,\"accepted_polls\":%u,"
           "\"terminal\":%u,\"windows\":%llu,\"entropy_callbacks\":%llu,\"entropy_bytes\":%llu}\n",
           argv[1], failures ? "false" : "true", failures, first_failure, syscalls,
           boundary_contract ? "true" : "false", accepted, snapshot.terminal,
           (unsigned long long)snapshot.entropy_windows,
           (unsigned long long)snapshot.entropy_callbacks, (unsigned long long)snapshot.entropy_bytes);
    return failures ? 1 : 0;
}
