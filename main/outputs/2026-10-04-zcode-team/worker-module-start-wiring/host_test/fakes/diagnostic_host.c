/* diagnostic_host.c — host fake of the diagnostic channel with the same
 * observable contract as the target diagnostic.c: ready() proves the
 * bindings, open_channel() retains one fd, event() writes bounded
 * "<tag> 0xVALUE" lines. Lines go through the fake sceKernel* bindings so
 * path redirection applies uniformly. */
#include "diagnostic.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern int sceKernelOpen(const char *, int, uint16_t);
extern int64_t sceKernelWrite(int, const void *, size_t);
extern int sceKernelClose(int);

static int channel_fd = -1;

static const char *diag_path(void)
{
    const char *p = getenv("MRS_DIAG_PATH");
    return p != NULL ? p : "/tmp/modbridge-diag.jsonl";
}

int diagnostic_ready(void)
{
    return 1;
}

int diagnostic_open_retained(void)
{
    return sceKernelOpen(diag_path(), O_WRONLY | O_CREAT | O_APPEND, 0600);
}

int diagnostic_open_channel(void)
{
    if (channel_fd < 0)
        channel_fd = diagnostic_open_retained();
    return channel_fd;
}

int diagnostic_channel_fd(void)
{
    return channel_fd;
}

int diagnostic_close_channel(void)
{
    if (channel_fd < 0)
        return 0;
    int fd = channel_fd;
    channel_fd = -1;
    return sceKernelClose(fd);
}

static void emit(int fd, const char *tag, uint32_t value)
{
    char line[128];
    int n = snprintf(line, sizeof(line), "EVENT %s 0x%08x\n", tag, value);
    if (n > 0)
        (void)sceKernelWrite(fd, line, (size_t)n);
}

void diagnostic_event(const char *tag, uint32_t value)
{
    int fd = channel_fd >= 0 ? channel_fd : diagnostic_open_channel();
    if (fd >= 0)
        emit(fd, tag, value);
}

void diagnostic_missing(unsigned index)
{
    diagnostic_event("missing", index);
}

int diagnostic_fd_event(int fd, const char *tag, int32_t result, int32_t error_number)
{
    (void)error_number;
    if (fd < 0 || tag == NULL)
        return -1;
    emit(fd, tag, (uint32_t)result);
    return 0;
}

int64_t diagnostic_fd_marker(int fd, const char *tag)
{
    if (fd < 0 || tag == NULL)
        return -1;
    emit(fd, tag, 0);
    return 0;
}

int diagnostic_open_data_directory(void)
{
    return -1;
}
