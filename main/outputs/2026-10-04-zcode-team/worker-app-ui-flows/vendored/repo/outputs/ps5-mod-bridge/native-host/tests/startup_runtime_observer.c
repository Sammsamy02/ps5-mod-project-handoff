/* Private link-time observer for the startup verifier. It delegates to the
 * real runtime functions and can only inject a failing entropy callback. */
#include "download/curl_download.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int event_fd(void)
{
    const char *value = getenv("MB_TEST_EVENT_FD");
    char *end = NULL;
    errno = 0;
    long fd = value ? strtol(value, &end, 10) : -1;
    return errno || !value || !*value || !end || *end || fd < 0 || fd > 1048576 ? -1 : (int)fd;
}

static void record(const char *name, int status)
{
    int fd = event_fd();
    if(fd < 0)
        return;
    char line[64];
    int length = snprintf(line, sizeof(line), "%s:%d\n", name, status);
    if(length <= 0 || (size_t)length >= sizeof(line))
        return;
    size_t sent = 0;
    while(sent < (size_t)length) {
        ssize_t written = write(fd, line + sent, (size_t)length - sent);
        if(written < 0 && errno == EINTR)
            continue;
        if(written <= 0)
            return;
        sent += (size_t)written;
    }
}

static int refuse_entropy(void *context, uint8_t output[32])
{
    (void)context;
    if(output)
        memset(output, 0x5a, 32);
    return -1;
}

int mb_test_runtime_start_v1(const mb_curl_runtime_params_v1 *params,
                             mb_curl_runtime_result_v1 *result)
{
    record("start.enter", 0);
    mb_curl_runtime_params_v1 controlled = *params;
    if(getenv("MB_TEST_REFUSE_ENTROPY"))
        controlled.entropy_read32 = refuse_entropy;
    int status = mb_curl_runtime_start_v1(&controlled, result);
    record("start.exit", status);
    return status;
}

int mb_test_runtime_close_v1(mb_curl_runtime_result_v1 *result)
{
    record("close.enter", 0);
    int status = mb_curl_runtime_close_v1(result);
    record("close.exit", status);
    return status;
}
