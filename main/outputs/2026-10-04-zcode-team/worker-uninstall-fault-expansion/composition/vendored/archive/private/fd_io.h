#ifndef MB_FD_IO_H
#define MB_FD_IO_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define MB_FD_IO_ABI_V1 1u

typedef enum mb_fd_io_status_v1 {
    MB_FD_IO_OK = 0,
    MB_FD_IO_BAD_ARGUMENT = 1,
    MB_FD_IO_RANGE = 2,
    MB_FD_IO_LIMIT = 3,
    MB_FD_IO_SHORT = 4,
    MB_FD_IO_SYSTEM_ERROR = 5,
    MB_FD_IO_BAD_PROVIDER = 6
} mb_fd_io_status_v1;

typedef ssize_t (*mb_fd_io_pread_fn)(void *user, int fd, void *dst, size_t bytes, off_t offset);
typedef ssize_t (*mb_fd_io_pwrite_fn)(void *user, int fd, const void *src, size_t bytes, off_t offset);

/* One serialized caller-owned adapter context. byte_limit charges requested
 * bytes once per callback; call_limit charges every pread/pwrite attempt,
 * including EINTR and short-progress retries. The caller validates FD type,
 * access mode, identity and disjointness before invoking these callbacks. */
typedef struct mb_fd_io_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint64_t byte_limit;
    uint64_t bytes_used;
    uint64_t call_limit;
    uint64_t calls_used;
    void *user;
    mb_fd_io_pread_fn pread_fn;
    mb_fd_io_pwrite_fn pwrite_fn;
} mb_fd_io_v1;

/* Exact positional callbacks for a regular, caller-owned descriptor. The
 * write FD must be writable and not O_APPEND. On failure, *actual reports
 * bytes already transferred; success guarantees *actual == bytes. These
 * functions never open, close, truncate or publish. */
int mb_fd_io_read_at(void *ctx, int fd, uint64_t offset,
                     uint8_t *dst, uint32_t bytes, uint32_t *actual);
int mb_fd_io_write_at(void *ctx, int fd, uint64_t offset,
                      const uint8_t *src, uint32_t bytes, uint32_t *actual);

#endif
