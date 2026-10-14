#include "entropy.h"
#include "http.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <time.h>

struct clock_reply {
    int return_code;
    struct timespec time;
};

static struct {
    int return_code;
    int error_number;
    size_t returned_length;
    size_t written_length;
    struct clock_reply before;
    struct clock_reply after;
} reply;

static unsigned char *caller_buffer;
static size_t *active_length;
static unsigned syscall_calls;
static unsigned clock_calls;
static int mib_ok;
static int buffer_ok;
static int request_length_ok;
static int new_value_ok;
static int call_order_ok;
static int clock_ids_ok = 1;
static int incoming_errno = -1;

int sysctl(const int *name, unsigned int name_length, void *old_value,
           size_t *old_length, const void *new_value, size_t new_length) {
    size_t index;
    ++syscall_calls;
    incoming_errno = errno;
    mib_ok = name != NULL && name_length == 2 &&
             name[0] == CTL_KERN && name[1] == KERN_ARND;
    buffer_ok = old_value == caller_buffer;
    request_length_ok = old_length != NULL && *old_length == 32;
    new_value_ok = new_value == NULL && new_length == 0;
    call_order_ok = clock_calls == 1;
    if (buffer_ok) {
        for (index = 0; index < reply.written_length && index < 32; ++index) {
            caller_buffer[index] = (unsigned char)(0x80 + index);
        }
    }
    if (old_length != NULL) {
        *old_length = reply.returned_length;
        active_length = old_length;
    }
    if (reply.error_number >= 0) {
        errno = reply.error_number;
    }
    return reply.return_code;
}

int clock_gettime(clockid_t clock_id, struct timespec *value) {
    struct clock_reply selected;
    clock_ids_ok = clock_ids_ok && clock_id == CLOCK_MONOTONIC;
    ++clock_calls;
    if (clock_calls == 1) {
        selected = reply.before;
        errno = 79;
    } else {
        selected = reply.after;
        errno = 97;
        if (active_length != NULL) {
            /* This pointer is used only during the observer's live call. */
            *active_length = SIZE_MAX;
            active_length = NULL;
        }
    }
    *value = selected.time;
    return selected.return_code;
}

static int has_value(const unsigned char *buffer, size_t length,
                     unsigned char wanted) {
    size_t index;
    for (index = 0; index < length; ++index) {
        if (buffer[index] != wanted) {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv) {
    struct {
        unsigned char before[17];
        unsigned char scratch[MB_ENTROPY_BYTES];
        unsigned char after[17];
    } storage;
    struct mb_entropy_result result;
    struct host_info info = {
        .version = "entropy-fixture", .build_id = "fixture", .profile = "fixture",
        .origin = "test-only", .pid = 1, .uid = 2, .euid = 2,
        .page_size = 4096, .lifetime = 60,
        .drbg = {.status="unavailable", .reason="desktop_fixture", .elapsed_ms=-1,
                 .api_call_limit=MB_ENTROPY_API_LIMIT}
    };
    struct http_response first;
    struct http_response second;
    const unsigned char request[] =
        "GET /report HTTP/1.1\r\nHost: 127.0.0.1:18080\r\n\r\n";
    int report_cached;
    if (argc != 11) {
        return 2;
    }
    reply.return_code = (int)strtol(argv[1], NULL, 10);
    reply.error_number = (int)strtol(argv[2], NULL, 10);
    reply.returned_length = strcmp(argv[3], "max") == 0
                                ? SIZE_MAX
                                : (size_t)strtoull(argv[3], NULL, 10);
    reply.written_length = (size_t)strtoull(argv[4], NULL, 10);
    reply.before.return_code = (int)strtol(argv[5], NULL, 10);
    reply.before.time.tv_sec = (time_t)strtoll(argv[6], NULL, 10);
    reply.before.time.tv_nsec = strtol(argv[7], NULL, 10);
    reply.after.return_code = (int)strtol(argv[8], NULL, 10);
    reply.after.time.tv_sec = (time_t)strtoll(argv[9], NULL, 10);
    reply.after.time.tv_nsec = strtol(argv[10], NULL, 10);

    memset(&storage, 0xa5, sizeof(storage));
    memset(&result, 0, sizeof(result));
    result.elapsed_ms = 12345;
    caller_buffer = storage.scratch;
    errno = 73;
    mb_entropy_observe(&result, storage.scratch);
    /* Never dereference observer-owned locals after its call has returned. */
    active_length = NULL;
    strcpy(info.authority, "127.0.0.1:18080");
    strcpy(info.url_origin, "http://127.0.0.1:18080");
    strcpy(info.core_report, "{}");
    info.entropy = result;
    http_handle(request, sizeof(request) - 1, &info, &first);
    http_handle(request, sizeof(request) - 1, &info, &second);
    report_cached = first.status == 200 && second.status == 200 &&
                    first.size == second.size &&
                    memcmp(first.body, second.body, first.size) == 0;

    printf("{\"status\":\"%s\",\"reason\":\"%s\","
           "\"call_count\":%u,\"return_code\":%d,\"error_number\":%d,"
           "\"returned_length\":%zu,\"elapsed_ms\":%" PRId64 ","
           "\"syscall_calls\":%u,\"clock_calls\":%u,"
           "\"mib_ok\":%d,\"buffer_ok\":%d,\"request_length_ok\":%d,"
           "\"new_value_ok\":%d,\"call_order_ok\":%d,\"clock_ids_ok\":%d,"
           "\"incoming_errno\":%d,\"scratch_cleared\":%d,"
           "\"before_canary_ok\":%d,\"after_canary_ok\":%d,"
           "\"http_status\":%d,\"report_cached\":%d,\"report\":%s}\n",
           result.status, result.reason, result.call_count, result.return_code,
           result.error_number, result.returned_length, result.elapsed_ms,
           syscall_calls, clock_calls, mib_ok, buffer_ok, request_length_ok,
           new_value_ok, call_order_ok, clock_ids_ok, incoming_errno,
           has_value(storage.scratch, sizeof(storage.scratch), 0),
           has_value(storage.before, sizeof(storage.before), 0xa5),
           has_value(storage.after, sizeof(storage.after), 0xa5),
           first.status, report_cached, first.body);
    return 0;
}
