/* Observer primitives: read-only stat/pread/read against observed paths.
   Import surface used (all rows already present in the frozen 191-strong
   console-proven contract): stat, pread, read, open, close. lseek is NOT in
   the contract and is never called. No observed path is ever opened for
   writing. Compiled -ffreestanding -fno-builtin like the frozen recipe. */
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "observer.h"

int observer_stat_path(const char *path, int64_t *size_out, int *is_dir_out)
{
    struct stat info;
    if (path == NULL || size_out == NULL)
        return -1;
    if (stat(path, &info) != 0)
        return -1;
    *size_out = (int64_t)info.st_size;
    if (is_dir_out != NULL)
        *is_dir_out = S_ISDIR(info.st_mode) ? 1 : 0;
    return 0;
}

int observer_check_exists(const char *path, int *is_dir_out)
{
    struct stat info;
    if (path == NULL)
        return -1;
    if (stat(path, &info) != 0)
        return errno == ENOENT ? 0 : -1;
    if (is_dir_out != NULL)
        *is_dir_out = S_ISDIR(info.st_mode) ? 1 : 0;
    return 1;
}

enum observer_growth observer_classify(int64_t previous, int64_t current)
{
    if (previous < 0)
        return OBSERVER_SAME; /* no baseline yet */
    if (current > previous)
        return OBSERVER_GREW;
    if (current < previous)
        return OBSERVER_SHRANK;
    return OBSERVER_SAME;
}

/* One bounded positional read ending at file_size. Retry EINTR a bounded
   number of times. Returns bytes read (>=0), or -1 on failure. */
static ssize_t tail_pread(int fd, unsigned char *buf, size_t cap,
                          int64_t file_size)
{
    off_t start = file_size > (int64_t)cap ? (off_t)(file_size - (int64_t)cap) : 0;
    unsigned interrupts = 0;
    for (;;) {
        errno = 0;
        ssize_t result = pread(fd, buf, cap, start);
        if (result < 0 && errno == EINTR && interrupts++ < 8u)
            continue;
        return result;
    }
}

/* Fallback: bounded forward drain with the console-proven read() row.
   Discards bytes until at most cap remain, never exceeding drain_limit total
   bytes read. Returns bytes captured (>=0), -1 on read error, and sets
   *capped when the limit was hit before reaching the tail window. */
static ssize_t tail_drain_read(int fd, unsigned char *buf, size_t cap,
                               int64_t file_size, size_t drain_limit, int *capped)
{
    unsigned char sink[512];
    int64_t remaining = file_size;
    size_t drained = 0;
    *capped = 0;
    while (remaining > (int64_t)cap) {
        size_t want = remaining - (int64_t)cap > (int64_t)sizeof(sink)
                          ? sizeof(sink) : (size_t)(remaining - (int64_t)cap);
        unsigned interrupts = 0;
        ssize_t result;
        for (;;) {
            errno = 0;
            result = read(fd, sink, want);
            if (result < 0 && errno == EINTR && interrupts++ < 8u)
                continue;
            break;
        }
        if (result < 0)
            return -1;
        if (result == 0)
            return 0; /* file shrank under us; nothing to capture */
        remaining -= result;
        drained += (size_t)result;
        if (drained > drain_limit) {
            *capped = 1;
            return -1;
        }
    }
    /* Capture the final window. */
    size_t used = 0;
    while (used < cap) {
        unsigned interrupts = 0;
        ssize_t result;
        for (;;) {
            errno = 0;
            result = read(fd, buf + used, cap - used);
            if (result < 0 && errno == EINTR && interrupts++ < 8u)
                continue;
            break;
        }
        if (result < 0)
            return -1;
        if (result == 0)
            break;
        used += (size_t)result;
    }
    return (ssize_t)used;
}

int observer_read_tail(const char *path, int64_t file_size, unsigned char *buf,
                       size_t cap, size_t *len_out, size_t drain_limit)
{
    if (path == NULL || buf == NULL || len_out == NULL || cap == 0 ||
        file_size < 0)
        return OBSERVER_TAIL_ERROR;
    /* Fresh descriptor per growth event: no long-lived fd into a file the
       UE4SS runtime keeps appending (also robust across rotate/rename). */
    int fd = open(path, O_RDONLY | O_NOFOLLOW);
    if (fd < 0)
        return OBSERVER_TAIL_ERROR;
    ssize_t got = tail_pread(fd, buf, cap, file_size);
    if (got >= 0) {
        *len_out = (size_t)got;
        int closed = close(fd);
        return closed == 0 ? OBSERVER_TAIL_OK : OBSERVER_TAIL_ERROR;
    }
    /* pread failed: bounded fallback through the console-proven read row. */
    int capped = 0;
    got = tail_drain_read(fd, buf, cap, file_size, drain_limit, &capped);
    int close_result = close(fd);
    if (got < 0)
        return capped ? OBSERVER_TAIL_DRAIN_CAPPED : OBSERVER_TAIL_ERROR;
    if (close_result != 0)
        return OBSERVER_TAIL_ERROR;
    *len_out = (size_t)got;
    return OBSERVER_TAIL_OK;
}

size_t observer_last_line(const unsigned char *buf, size_t len,
                          int window_at_start, char *out, size_t cap,
                          int *truncated_out)
{
    size_t used = 0;
    int truncated = 0;
    if (cap == 0 || out == NULL)
        return 0;
    out[0] = '\0';
    if (buf == NULL || len == 0) {
        if (truncated_out != NULL)
            *truncated_out = 0;
        return 0;
    }
    /* Log writers terminate lines with LF: report the last COMPLETE line,
       i.e. strip one trailing LF (and an immediately preceding CR), then
       take the text after the previous LF. A file with no trailing newline
       contributes its final partial line instead. */
    size_t end = len;
    int stripped_terminator = 0;
    if (buf[end - 1] == '\n') {
        --end;
        stripped_terminator = 1;
        if (end > 0 && buf[end - 1] == '\r')
            --end;
    }
    size_t start = 0;
    int found_newline = 0;
    for (size_t i = end; i > 0; --i) {
        if (buf[i - 1] == '\n') {
            start = i;
            found_newline = 1;
            break;
        }
    }
    if (!found_newline && !stripped_terminator && !window_at_start) {
        /* No line boundary anywhere in the window and the window began
           mid-file: this text is a fragment of a longer line. */
        start = 0;
        truncated = 1;
    }
    for (size_t i = start; i < end; ++i) {
        unsigned char c = buf[i];
        if (c == '\r')
            continue; /* drop CR entirely */
        char emitted = (c >= 0x20u && c <= 0x7Eu) ? (char)c : '?';
        if (used + 1 >= cap) {
            truncated = 1;
            break;
        }
        out[used++] = emitted;
    }
    out[used] = '\0';
    if (truncated_out != NULL)
        *truncated_out = truncated;
    return used;
}
