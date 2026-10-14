#ifndef NATIVE_STARTUP_OWNED_INPUT_H
#define NATIVE_STARTUP_OWNED_INPUT_H

#include <stddef.h>

struct startup_input {
    unsigned char *bytes;
    size_t length;
};

typedef int (*startup_input_consumer)(const unsigned char *bytes,
                                      size_t length, void *context);

enum startup_input_result {
    STARTUP_INPUT_OK = 0,
    STARTUP_INPUT_INVALID = -1,
    STARTUP_INPUT_TOO_LARGE = -2,
    STARTUP_INPUT_OPEN_ERROR = -3,
    STARTUP_INPUT_MEMORY_ERROR = -4,
    STARTUP_INPUT_READ_ERROR = -5,
    STARTUP_INPUT_TRUNCATED = -6,
    STARTUP_INPUT_CHANGED = -7,
    STARTUP_INPUT_CLOSE_ERROR = -8,
    STARTUP_INPUT_CALLBACK_MISSING = -9
};

/* Loads exactly expected_length bytes, verifies EOF, and transfers ownership
   to input only after the file descriptor has been closed successfully. */
int startup_input_load(struct startup_input *input, const char *path,
                       size_t expected_length, int diagnostic_fd);
int startup_input_deliver(const struct startup_input *input,
                          startup_input_consumer consumer, void *context);
void startup_input_release(struct startup_input *input);

#endif
