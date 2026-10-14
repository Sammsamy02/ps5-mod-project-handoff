/* Startup-owned exact-length input preload, safe to hand to a worker later. */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <probe_config.h>
#include "startup_input.h"
#include "../diagnostic.h"

#define INPUT_READ_EINTR_LIMIT 8u

_Static_assert(PROBE_INPUT_MAX_BYTES > 0 && PROBE_INPUT_MAX_BYTES <= 65536u,
               "Startup input cap must remain bounded at 64 KiB");

static void report(int fd, const char *tag, int32_t result, int error_number)
{
    (void)diagnostic_fd_event(fd, tag, result, error_number);
}

static int close_input(int fd, int diagnostic_fd)
{
    errno = 0;
    int result = close(fd);
    int error_number = result < 0 ? errno : 0;
    report(diagnostic_fd, "startup_input_close_result", result, error_number);
    return result < 0 ? STARTUP_INPUT_CLOSE_ERROR : STARTUP_INPUT_OK;
}

static int read_exact(int fd, unsigned char *bytes, size_t expected,
                      size_t *read_bytes, int diagnostic_fd)
{
    size_t used = 0;
    unsigned interrupts = 0;
    while (used < expected) {
        errno = 0;
        ssize_t result = read(fd, bytes + used, expected - used);
        int error_number = result < 0 ? errno : 0;
        if (result < 0 && error_number == EINTR && interrupts++ < INPUT_READ_EINTR_LIMIT)
            continue;
        if (result < 0) {
            *read_bytes = used;
            report(diagnostic_fd, "startup_input_read_error", (int32_t)result,
                   error_number);
            return STARTUP_INPUT_READ_ERROR;
        }
        if (result == 0) {
            *read_bytes = used;
            report(diagnostic_fd, "startup_input_truncated", (int32_t)used,
                   (int)expected);
            return STARTUP_INPUT_TRUNCATED;
        }
        if ((size_t)result > expected - used) {
            *read_bytes = used;
            report(diagnostic_fd, "startup_input_invalid_read", (int32_t)result, 0);
            return STARTUP_INPUT_READ_ERROR;
        }
        used += (size_t)result;
    }
    *read_bytes = used;
    return STARTUP_INPUT_OK;
}

static int verify_eof(int fd, size_t expected, int diagnostic_fd)
{
    unsigned char extra;
    unsigned interrupts = 0;
    for (;;) {
        errno = 0;
        ssize_t result = read(fd, &extra, 1);
        int error_number = result < 0 ? errno : 0;
        if (result < 0 && error_number == EINTR && interrupts++ < INPUT_READ_EINTR_LIMIT)
            continue;
        if (result < 0) {
            report(diagnostic_fd, "startup_input_eof_check_error",
                   (int32_t)result, error_number);
            return STARTUP_INPUT_READ_ERROR;
        }
        if (result != 0) {
            report(diagnostic_fd, "startup_input_changed", (int32_t)expected,
                   (int32_t)result);
            return STARTUP_INPUT_CHANGED;
        }
        return STARTUP_INPUT_OK;
    }
}

int startup_input_load(struct startup_input *input, const char *path,
                       size_t expected_length, int diagnostic_fd)
{
    if (input == NULL || path == NULL || expected_length == 0 ||
        input->bytes != NULL || input->length != 0) {
        report(diagnostic_fd, "startup_input_invalid_request", (int32_t)expected_length, 0);
        return STARTUP_INPUT_INVALID;
    }
    if (expected_length > PROBE_INPUT_MAX_BYTES) {
        report(diagnostic_fd, "startup_input_too_large", (int32_t)expected_length,
               (int)PROBE_INPUT_MAX_BYTES);
        return STARTUP_INPUT_TOO_LARGE;
    }

    errno = 0;
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    int open_error = fd < 0 ? errno : 0;
    report(diagnostic_fd, "startup_input_open_result", fd, open_error);
    if (fd < 0)
        return STARTUP_INPUT_OPEN_ERROR;

    unsigned char *owned = malloc(expected_length + 1);
    if (owned == NULL) {
        report(diagnostic_fd, "startup_input_allocation_error", -1, 0);
        (void)close_input(fd, diagnostic_fd);
        return STARTUP_INPUT_MEMORY_ERROR;
    }

    size_t read_bytes = 0;
    int result = read_exact(fd, owned, expected_length, &read_bytes, diagnostic_fd);
    if (result == STARTUP_INPUT_OK)
        result = verify_eof(fd, expected_length, diagnostic_fd);
    int close_result = close_input(fd, diagnostic_fd);
    if (result == STARTUP_INPUT_OK && close_result != STARTUP_INPUT_OK)
        result = close_result;
    if (result != STARTUP_INPUT_OK) {
        free(owned);
        return result;
    }

    owned[expected_length] = '\0';
    input->bytes = owned;
    input->length = expected_length;
    report(diagnostic_fd, "startup_input_loaded", (int32_t)expected_length, 0);
    return STARTUP_INPUT_OK;
}

int startup_input_deliver(const struct startup_input *input,
                          startup_input_consumer consumer, void *context)
{
    if (input == NULL || input->bytes == NULL || input->length == 0)
        return STARTUP_INPUT_INVALID;
    if (consumer == NULL)
        return STARTUP_INPUT_CALLBACK_MISSING;
    return consumer(input->bytes, input->length, context);
}

void startup_input_release(struct startup_input *input)
{
    if (input == NULL)
        return;
    free(input->bytes);
    input->bytes = NULL;
    input->length = 0;
}
