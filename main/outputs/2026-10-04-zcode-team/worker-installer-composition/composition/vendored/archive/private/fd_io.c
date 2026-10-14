#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "fd_io.h"
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <unistd.h>

static int valid_context(const mb_fd_io_v1 *io) {
    return io && io->struct_size >= sizeof(*io) && io->abi_version == MB_FD_IO_ABI_V1 &&
           io->bytes_used <= io->byte_limit && io->calls_used <= io->call_limit;
}

static int valid_range(uint64_t offset, uint32_t bytes) {
    const unsigned bits = (unsigned)(sizeof(off_t) * CHAR_BIT);
    uint64_t max_offset = INT64_MAX;
    if (bits < 64u) max_offset = (UINT64_C(1) << (bits - 1u)) - 1u;
    if (offset > max_offset || (uint64_t)bytes > max_offset - offset) return 0;
    off_t converted = (off_t)offset;
    return converted >= 0 && (uint64_t)converted == offset;
}

static mb_fd_io_status_v1 validate(void *opaque, int fd, const void *buffer,
                                   uint32_t bytes, uint32_t *actual) {
    mb_fd_io_v1 *io = opaque;
    if (!actual) return MB_FD_IO_BAD_ARGUMENT;
    *actual = 0;
    if (!valid_context(io) || fd < 0 || (bytes && !buffer)) return MB_FD_IO_BAD_ARGUMENT;
    if (!bytes) return MB_FD_IO_OK;
    return MB_FD_IO_OK;
}

int mb_fd_io_read_at(void *opaque, int fd, uint64_t offset,
                     uint8_t *dst, uint32_t bytes, uint32_t *actual) {
    mb_fd_io_v1 *io = opaque;
    mb_fd_io_status_v1 status = validate(opaque, fd, dst, bytes, actual);
    uint32_t done = 0;
    if (status != MB_FD_IO_OK || !bytes) return status;
    if (!valid_range(offset, bytes)) return MB_FD_IO_RANGE;
    if (io->bytes_used > io->byte_limit || (uint64_t)bytes > io->byte_limit - io->bytes_used)
        return MB_FD_IO_LIMIT;
    io->bytes_used += bytes;
    while (done < bytes) {
        size_t remain = (size_t)(bytes - done);
        ssize_t got;
        if (io->calls_used >= io->call_limit) { *actual = done; return MB_FD_IO_LIMIT; }
        ++io->calls_used;
        got = io->pread_fn ? io->pread_fn(io->user, fd, dst + done, remain, (off_t)(offset + done))
                           : pread(fd, dst + done, remain, (off_t)(offset + done));
        if (got < 0) {
            if (errno == EINTR) continue;
            *actual = done;
            return MB_FD_IO_SYSTEM_ERROR;
        }
        if (got == 0) { *actual = done; return MB_FD_IO_SHORT; }
        if ((uint64_t)got > (uint64_t)remain) { *actual = done; return MB_FD_IO_BAD_PROVIDER; }
        done += (uint32_t)got;
    }
    *actual = done;
    return MB_FD_IO_OK;
}

int mb_fd_io_write_at(void *opaque, int fd, uint64_t offset,
                      const uint8_t *src, uint32_t bytes, uint32_t *actual) {
    mb_fd_io_v1 *io = opaque;
    mb_fd_io_status_v1 status = validate(opaque, fd, src, bytes, actual);
    uint32_t done = 0;
    if (status != MB_FD_IO_OK || !bytes) return status;
    if (!valid_range(offset, bytes)) return MB_FD_IO_RANGE;
    if (io->bytes_used > io->byte_limit || (uint64_t)bytes > io->byte_limit - io->bytes_used)
        return MB_FD_IO_LIMIT;
    io->bytes_used += bytes;
    while (done < bytes) {
        size_t remain = (size_t)(bytes - done);
        ssize_t wrote;
        if (io->calls_used >= io->call_limit) { *actual = done; return MB_FD_IO_LIMIT; }
        ++io->calls_used;
        wrote = io->pwrite_fn ? io->pwrite_fn(io->user, fd, src + done, remain, (off_t)(offset + done))
                              : pwrite(fd, src + done, remain, (off_t)(offset + done));
        if (wrote < 0) {
            if (errno == EINTR) continue;
            *actual = done;
            return MB_FD_IO_SYSTEM_ERROR;
        }
        if (wrote == 0) { *actual = done; return MB_FD_IO_SHORT; }
        if ((uint64_t)wrote > (uint64_t)remain) { *actual = done; return MB_FD_IO_BAD_PROVIDER; }
        done += (uint32_t)wrote;
    }
    *actual = done;
    return MB_FD_IO_OK;
}
