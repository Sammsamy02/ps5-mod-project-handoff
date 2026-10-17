/* Diagnostic sink uses only the three kernel imports proven by the v2 probe. */
#include <stddef.h>
#include <stdint.h>
#include <sys/fcntl.h>
#include <probe_config.h>
#include "diagnostic.h"
#include "generated/import_names.h"

extern int sceKernelOpen(const char *, int, uint16_t);
extern int64_t sceKernelWrite(int, const void *, size_t);
extern int sceKernelClose(int);
#define DIAGNOSTIC_LINE_CAPACITY 224u
#define DIAGNOSTIC_WRITE_ATTEMPTS 64u
static int channel_fd = -1;
int diagnostic_ready(void)
{
    /* Check the actual call bindings. The carried R_X86_64_64 table slots are
       themselves under test, so they must not gate this diagnostic channel. */
    int (*volatile open_binding)(const char *, int, uint16_t) = sceKernelOpen;
    int64_t (*volatile write_binding)(int, const void *, size_t) = sceKernelWrite;
    int (*volatile close_binding)(int) = sceKernelClose;
    return open_binding != NULL && write_binding != NULL && close_binding != NULL;
}

static void append(char *buffer, size_t *used, size_t capacity, const char *text)
{
    while (*text && *used < capacity)
        buffer[(*used)++] = *text++;
}

static void hex(char *buffer, size_t *used, uint32_t value)
{
    static const char digits[] = "0123456789abcdef";
    append(buffer, used, 510, "0x");
    for (int shift = 28; shift >= 0 && *used < 510; shift -= 4)
        buffer[(*used)++] = digits[(value >> shift) & 15];
}

static void write_line(char *buffer, size_t used)
{
    if (!diagnostic_ready())
        return;
    buffer[used++] = '\n';
    int descriptor = channel_fd;
    if (descriptor < 0)
        return;
    size_t offset = 0;
    while (offset < used) {
        int64_t written = sceKernelWrite(descriptor, buffer + offset, used - offset);
        if (written <= 0 || (uint64_t)written > used - offset)
            break;
        offset += (size_t)written;
    }
}

int diagnostic_open_retained(void)
{
    if (!diagnostic_ready())
        return -1;
    return sceKernelOpen(PROBE_LOG_PATH,
        O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW, 0600);
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

static void append_u32_hex(char *buffer, size_t *used, uint32_t value)
{
    static const char digits[] = "0123456789abcdef";
    for (int shift = 28; shift >= 0 && *used < DIAGNOSTIC_LINE_CAPACITY - 1; shift -= 4)
        buffer[(*used)++] = digits[(value >> shift) & 15];
}

static int64_t write_bounded(int fd, const char *buffer, size_t length)
{
    size_t offset = 0;
    unsigned attempts = 0;
    while (offset < length && attempts < DIAGNOSTIC_WRITE_ATTEMPTS) {
        ++attempts;
        int64_t written = sceKernelWrite(fd, buffer + offset, length - offset);
        if (written <= 0 || (uint64_t)written > length - offset)
            return written < 0 ? written : -5;
        offset += (size_t)written;
    }
    return offset == length ? (int64_t)offset : -110;
}

int diagnostic_fd_event(int fd, const char *tag, int32_t result, int32_t error_number)
{
    if (fd < 0 || tag == NULL)
        return -1;
    char line[DIAGNOSTIC_LINE_CAPACITY];
    size_t used = 0, tag_length = 0;
    append(line, &used, sizeof(line) - 1, "[" PROBE_TITLE "] " PROBE_MARKER " ");
    while (tag[tag_length] && tag_length < 48)
        ++tag_length;
    for (size_t i = 0; i < tag_length && used < DIAGNOSTIC_LINE_CAPACITY - 1; ++i)
        line[used++] = tag[i];
    append(line, &used, sizeof(line) - 1, " result=0x");
    append_u32_hex(line, &used, (uint32_t)result);
    append(line, &used, sizeof(line) - 1, " errno=0x");
    append_u32_hex(line, &used, (uint32_t)error_number);
    line[used++] = '\n';
    return (int)write_bounded(fd, line, used);
}

int64_t diagnostic_fd_marker(int fd, const char *tag)
{
    if (fd < 0 || tag == NULL)
        return -1;
    char line[DIAGNOSTIC_LINE_CAPACITY];
    size_t used = 0, tag_length = 0;
    append(line, &used, sizeof(line) - 1, "[" PROBE_TITLE "] " PROBE_MARKER " ");
    while (tag[tag_length] && tag_length < 48)
        ++tag_length;
    for (size_t i = 0; i < tag_length && used < DIAGNOSTIC_LINE_CAPACITY - 1; ++i)
        line[used++] = tag[i];
    line[used++] = '\n';
    return write_bounded(fd, line, used);
}

int diagnostic_close_retained(int fd)
{
    return fd < 0 ? 0 : sceKernelClose(fd);
}

void diagnostic_event(const char *tag, uint32_t value)
{
    char buffer[512];
    size_t used = 0;
    append(buffer, &used, sizeof(buffer) - 2, "[" PROBE_TITLE "] " PROBE_MARKER " ");
    append(buffer, &used, sizeof(buffer) - 2, tag);
    append(buffer, &used, sizeof(buffer) - 2, " value=");
    hex(buffer, &used, value);
    write_line(buffer, used);
}

void diagnostic_missing(unsigned index)
{
    if (index >= PROBE_IMPORT_NAME_COUNT)
        return;
    char buffer[512];
    size_t used = 0;
    append(buffer, &used, sizeof(buffer) - 2, "[" PROBE_TITLE "] " PROBE_MARKER " missing_import slot=");
    hex(buffer, &used, index);
    append(buffer, &used, sizeof(buffer) - 2, " ");
    append(buffer, &used, sizeof(buffer) - 2, probe_import_names[index]);
    write_line(buffer, used);
}
