/*
 * mb_stream_bridge.c — see mb_stream_bridge.h.
 * Origin: worker-streaming-bridge, 2026-10-04 (owned).
 *
 * Host flavor: plain POSIX (open/write/fsync/rename). The console flavor
 * must replace the five mb_posix_* shims with ps5_io equivalents before any
 * console link — the same explicit split modinst_download.c documents.
 */
#define _DARWIN_C_SOURCE 1
#define _GNU_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "mb_stream_bridge.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MB_BRIDGE_REHASH_CHUNK 8192u

/* ------------------------- console/host I/O shims ------------------------- */
static int mb_posix_open_sink(const char *path) {
    return open(path, O_RDWR | O_CREAT | O_TRUNC, 0600);
}
static int mb_posix_open_readback(const char *path) {
    return open(path, O_RDONLY);
}
static int mb_posix_fsync(int fd) { return fsync(fd); }
static int mb_posix_rename(const char *from, const char *to) {
    return rename(from, to);
}
static int mb_posix_fsync_dir_of(const char *path) {
    /* fsync the directory containing path (publish durability). */
    char buf[4096];
    size_t n = strlen(path);
    int fd, e;
    if (n == 0 || n >= sizeof buf) return -1;
    memcpy(buf, path, n + 1);
    while (n > 1 && buf[n - 1] != '/') --n;
    buf[n == 1 ? 1 : n - 1] = 0; /* keep "/" or strip the final component */
    fd = open(n == 1 ? "/" : buf, O_RDONLY | O_DIRECTORY);
    if (fd < 0) return -1;
    e = fsync(fd);
    if (close(fd)) e = -1;
    return e;
}
static int mb_posix_unlink(const char *path) { return unlink(path); }

/* ------------------------------ internal ops ------------------------------ */
static int bridge_args_ok(const mb_stream_bridge_v1 *b) {
    return b && b->struct_size == sizeof(*b) && b->abi_version == MB_STREAM_BRIDGE_ABI_V1 &&
           !b->reserved && b->tmp_path && b->tmp_path[0];
}

static void retire_fds(mb_stream_bridge_v1 *b) {
    if (b->fd_w >= 0) { close(b->fd_w); b->fd_w = -1; }
    if (b->fd_r >= 0) { close(b->fd_r); b->fd_r = -1; }
}

int mb_stream_bridge_init_v1(mb_stream_bridge_v1 *b, const char *tmp_path,
                             const mb_stream_bridge_params_v1 *params,
                             const mb_stream_bridge_hash_v1 *hash) {
    if (!b || !tmp_path || !tmp_path[0] || !params ||
        params->struct_size != sizeof(*params) ||
        params->abi_version != MB_STREAM_BRIDGE_ABI_V1 ||
        params->threshold_bytes > MB_STREAM_BRIDGE_THRESHOLD_MAX_BYTES ||
        !params->max_bytes || params->max_bytes > MB_STREAM_BRIDGE_MAX_BYTES ||
        params->threshold_bytes > params->max_bytes || !hash ||
        !hash->init_v1 || !hash->update_v1 || !hash->finish_v1)
        return MB_STREAM_BRIDGE_BAD_ARGUMENT;
    memset(b, 0, sizeof(*b));
    b->struct_size = (uint32_t)sizeof(*b);
    b->abi_version = MB_STREAM_BRIDGE_ABI_V1;
    b->fd_w = b->fd_r = -1;
    b->threshold_bytes = params->threshold_bytes;
    b->max_bytes = params->max_bytes;
    b->hash = *hash;
    b->tmp_path = (char *)malloc(strlen(tmp_path) + 1);
    if (!b->tmp_path) return MB_STREAM_BRIDGE_IO_ERROR;
    strcpy(b->tmp_path, tmp_path);
    b->fd_w = mb_posix_open_sink(tmp_path);
    if (b->fd_w < 0) { free(b->tmp_path); b->tmp_path = 0; return MB_STREAM_BRIDGE_IO_ERROR; }
    b->fd_r = mb_posix_open_readback(tmp_path);
    if (b->fd_r < 0) { retire_fds(b); free(b->tmp_path); b->tmp_path = 0;
        mb_posix_unlink(tmp_path); return MB_STREAM_BRIDGE_IO_ERROR; }
    if (b->hash.init_v1(b->hash.context)) {
        retire_fds(b); free(b->tmp_path); b->tmp_path = 0;
        mb_posix_unlink(tmp_path); return MB_STREAM_BRIDGE_IO_ERROR;
    }
    b->state = MB_STREAM_BRIDGE_CREATED;
    return MB_STREAM_BRIDGE_OK;
}

int mb_stream_bridge_sink_write_v1(mb_stream_bridge_v1 *b, const uint8_t *buf,
                                   uint32_t len, uint32_t *written) {
    if (!bridge_args_ok(b) || (!buf && len) || !written)
        return MB_STREAM_BRIDGE_BAD_ARGUMENT;
    *written = 0;
    if (b->state != MB_STREAM_BRIDGE_CREATED && b->state != MB_STREAM_BRIDGE_STREAMING)
        return MB_STREAM_BRIDGE_BAD_STATE;
    if ((uint64_t)len > b->max_bytes - b->received_bytes)
        return MB_STREAM_BRIDGE_LIMIT;
    if (len) {
        ssize_t got = write(b->fd_w, buf, len);
        if (got < 0 || (uint32_t)got != len) return MB_STREAM_BRIDGE_IO_ERROR;
        if (b->hash.update_v1(b->hash.context, buf, len)) return MB_STREAM_BRIDGE_IO_ERROR;
    }
    b->received_bytes += len;
    b->state = MB_STREAM_BRIDGE_STREAMING;
    *written = len;
    return MB_STREAM_BRIDGE_OK;
}

int mb_stream_bridge_sink_read_v1(mb_stream_bridge_v1 *b, uint64_t off,
                                  uint8_t *buf, uint32_t len, uint32_t *read) {
    size_t got = 0;
    if (!bridge_args_ok(b) || (!buf && len) || !read) return MB_STREAM_BRIDGE_BAD_ARGUMENT;
    *read = 0;
    if (off > b->received_bytes || len > b->received_bytes - off)
        return MB_STREAM_BRIDGE_BAD_ARGUMENT;
    while (got < len) {
        ssize_t r = pread(b->fd_r, buf + got, len - got, (off_t)(off + got));
        if (r <= 0) return MB_STREAM_BRIDGE_IO_ERROR;
        got += (size_t)r;
    }
    *read = len;
    return MB_STREAM_BRIDGE_OK;
}

int mb_stream_bridge_sink_finish_v1(mb_stream_bridge_v1 *b, uint64_t total_bytes,
                                    const uint8_t sha256[32]) {
    if (!bridge_args_ok(b) || !sha256) return MB_STREAM_BRIDGE_BAD_ARGUMENT;
    if (b->state != MB_STREAM_BRIDGE_STREAMING && b->state != MB_STREAM_BRIDGE_CREATED)
        return MB_STREAM_BRIDGE_BAD_STATE;
    if (total_bytes != b->received_bytes) return MB_STREAM_BRIDGE_LIMIT;
    memcpy(b->sink_sha256, sha256, 32);
    b->sink_total = total_bytes;
    b->state = MB_STREAM_BRIDGE_STREAM_COMPLETE;
    return MB_STREAM_BRIDGE_OK;
}

/* Full-file re-read re-hash; intake.c's source_end performs the identical
 * verification against the declared digest, so the bridge front-loads it to
 * avoid handing the scanner a file that can only fail late. */
static int rehash_matches(mb_stream_bridge_v1 *b, int fd, uint64_t size) {
    uint8_t chunk[MB_BRIDGE_REHASH_CHUNK], digest[32];
    uint64_t off = 0;
    if (b->hash.init_v1(b->hash.context)) return 0;
    while (off < size) {
        size_t want = (size - off > MB_BRIDGE_REHASH_CHUNK) ? MB_BRIDGE_REHASH_CHUNK : (size_t)(size - off);
        ssize_t got = pread(fd, chunk, want, (off_t)off);
        if (got < 0 || (size_t)got != want) return 0;
        if (b->hash.update_v1(b->hash.context, chunk, want)) return 0;
        off += want;
    }
    /* EOF probe: exactly zero bytes readable at size (no concurrent writer). */
    if (pread(fd, chunk, 1, (off_t)size) != 0) return 0;
    if (b->hash.finish_v1(b->hash.context, digest)) return 0;
    return memcmp(digest, b->sink_sha256, 32) == 0;
}

int mb_stream_bridge_handoff_v1(mb_stream_bridge_v1 *b, mb_stream_bridge_source_v1 *out) {
    struct stat st;
    int flags, fd = -1, e = MB_STREAM_BRIDGE_OK;
    if (!bridge_args_ok(b) || !out) return MB_STREAM_BRIDGE_BAD_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (b->state == MB_STREAM_BRIDGE_STREAMING || b->state == MB_STREAM_BRIDGE_CREATED) {
        if (b->received_bytes < b->threshold_bytes) return MB_STREAM_BRIDGE_NOT_READY;
        return MB_STREAM_BRIDGE_BAD_STATE; /* gate open would need completion */
    }
    if (b->state != MB_STREAM_BRIDGE_STREAM_COMPLETE) return MB_STREAM_BRIDGE_BAD_STATE;
    if (b->received_bytes < b->threshold_bytes) return MB_STREAM_BRIDGE_NOT_READY;
    if (mb_posix_fsync(b->fd_w)) { e = MB_STREAM_BRIDGE_IO_ERROR; goto fail; }
    retire_fds(b);
    fd = mb_posix_open_readback(b->tmp_path); /* fresh consumer descriptor */
    if (fd < 0) { e = MB_STREAM_BRIDGE_IO_ERROR; goto fail; }
    flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fstat(fd, &st)) { e = MB_STREAM_BRIDGE_IO_ERROR; goto fail; }
    if (!S_ISREG(st.st_mode) || (flags & O_ACCMODE) != O_RDONLY || st.st_size < 0 ||
        (uint64_t)st.st_size != b->sink_total) { e = MB_STREAM_BRIDGE_IDENTITY_MISMATCH; goto fail; }
    if (!rehash_matches(b, fd, b->sink_total)) { e = MB_STREAM_BRIDGE_IDENTITY_MISMATCH; goto fail; }
    out->fd = fd;
    out->size = b->sink_total;
    memcpy(out->sha256, b->sink_sha256, 32);
    b->state = MB_STREAM_BRIDGE_HANDOFF_COMPLETE;
    return MB_STREAM_BRIDGE_OK;
fail:
    if (fd >= 0) close(fd);
    b->state = MB_STREAM_BRIDGE_FAILED;
    return e;
}

int mb_stream_bridge_publish_v1(mb_stream_bridge_v1 *b, const char *final_path) {
    if (!bridge_args_ok(b) || !final_path || !final_path[0]) return MB_STREAM_BRIDGE_BAD_ARGUMENT;
    if (b->state != MB_STREAM_BRIDGE_HANDOFF_COMPLETE) return MB_STREAM_BRIDGE_BAD_STATE;
    if (mb_posix_rename(b->tmp_path, final_path)) return MB_STREAM_BRIDGE_IO_ERROR;
    if (mb_posix_fsync_dir_of(final_path)) return MB_STREAM_BRIDGE_IO_ERROR;
    b->tmp_path[0] = 0; /* consumed by the rename; abort must not unlink */
    return MB_STREAM_BRIDGE_OK;
}

int mb_stream_bridge_source_close_v1(mb_stream_bridge_source_v1 *s) {
    if (!s) return MB_STREAM_BRIDGE_BAD_ARGUMENT;
    if (s->fd >= 0 && close(s->fd)) { s->fd = -1; return MB_STREAM_BRIDGE_IO_ERROR; }
    memset(s, 0, sizeof(*s));
    s->fd = -1;
    return MB_STREAM_BRIDGE_OK;
}

int mb_stream_bridge_abort_v1(mb_stream_bridge_v1 *b) {
    if (!b || b->struct_size != sizeof(*b)) return MB_STREAM_BRIDGE_BAD_ARGUMENT;
    retire_fds(b);
    if (b->tmp_path && b->tmp_path[0]) mb_posix_unlink(b->tmp_path);
    free(b->tmp_path);
    b->tmp_path = 0;
    b->state = MB_STREAM_BRIDGE_ABORTED;
    return MB_STREAM_BRIDGE_OK;
}
