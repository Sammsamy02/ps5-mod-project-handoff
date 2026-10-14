/*
 * ps5_io.c — implementation of ps5_io.h.
 *
 * Origin: worker-installer-ps5io, 2026-10-04.
 *
 * Two OS layers, one shared semantic layer:
 *   PS5IO_HOST defined -> POSIX host build (tests; errno converted to -errno)
 *   otherwise          -> pinned PS5 payload SDK console build, whose only
 *                         external references are the nm-verified sceKernel*
 *                         exports listed in ps5_io.h (signatures mirror the
 *                         pinned SharpProspero declarations).
 *
 * The console TU's own calls are only the pinned kernel set; the compiler
 * additionally emits memcpy/strlen for struct copies (nm-visible, and
 * themselves exported by the pinned libc_stub_weak/libSceLibcInternal — see
 * receipts/console-build.log). No other libc surface is used.
 */
#include "ps5_io.h"

#include <errno.h>

#if defined(PS5IO_HOST)

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#else /* console */

#include <sys/fcntl.h>   /* pinned sysroot O_* values */
#include <sys/stat.h>    /* pinned sysroot mode bits  */
#include <stdio.h>       /* SEEK_SET (macro only)      */

/* Pinned SharpProspero Interop/Kernel signatures (KernelFile.cs,
 * KernelClock.cs); same convention as native-module-probe/probe.c. */
typedef struct {
    uint32_t dev;         /*  0 */
    uint32_t ino;         /*  4 */
    uint16_t mode;        /*  8 kind+permission bits (S_IFMT applies) */
    uint16_t link_count;  /* 10 */
    uint32_t uid;         /* 12 */
    uint32_t gid;         /* 16 */
    uint32_t device_type; /* 20 */
    int64_t  atime_s;     /* 24 */
    int64_t  atime_ns;    /* 32 */
    int64_t  mtime_s;     /* 40 */
    int64_t  mtime_ns;    /* 48 */
    int64_t  ctime_s;     /* 56 */
    int64_t  ctime_ns;    /* 64 */
    int64_t  size;        /* 72 */
    int64_t  blocks;      /* 80 */
    int32_t  block_size;  /* 88 */
    uint32_t flags;       /* 92 */
    uint32_t generation;  /* 96 */
    int64_t  crtime_s;    /* 104 */
    int64_t  crtime_ns;   /* 112 */
} ps5io_sce_stat;         /* sizeof == 120, pinned layout */

typedef struct {
    int64_t tv_sec;
    int64_t tv_nsec;
} ps5io_timespec;

extern int     sceKernelOpen(const char *path, int flags, uint16_t mode);
extern int     sceKernelClose(int descriptor);
extern int64_t sceKernelRead(int descriptor, void *buffer, size_t length);
extern int64_t sceKernelWrite(int descriptor, const void *buffer, size_t length);
extern int64_t sceKernelLseek(int descriptor, int64_t offset, int whence);
extern int     sceKernelMkdir(const char *path, uint16_t mode);
extern int     sceKernelStat(const char *path, ps5io_sce_stat *status);
extern int     sceKernelRename(const char *from, const char *to);
extern int     sceKernelUnlink(const char *path);
extern int     sceKernelFsync(int descriptor);
extern int     sceKernelClockGettime(int clock_id, ps5io_timespec *tp);

#endif

/* ---- static bounded helpers (no libc on console) ------------------------- */

static size_t ps5io_strlen(const char *s)
{
    size_t n = 0;
    if (!s) return 0;
    while (s[n]) ++n;
    return n;
}

static char *ps5io_copy(char *dst, const char *src, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i) dst[i] = src[i];
    return dst + n;
}

/* ---- OS layer ------------------------------------------------------------ */

#if defined(PS5IO_HOST)

static int ps5io_oflags_to_host(unsigned flags);
static int os_open(const char *path, unsigned flags, unsigned mode)
{
    int fd = open(path, ps5io_oflags_to_host(flags), (mode_t)mode);
    return fd >= 0 ? fd : -errno;
}
static int os_close(int fd)
{
    return close(fd) == 0 ? 0 : -errno;
}
static long os_read(int fd, void *buf, size_t n)
{
    ssize_t r = read(fd, buf, n);
    return r >= 0 ? (long)r : (long)-errno;
}
static long os_write(int fd, const void *buf, size_t n)
{
    ssize_t r = write(fd, buf, n);
    return r >= 0 ? (long)r : (long)-errno;
}
static long os_pread(int fd, void *buf, size_t n, uint64_t off)
{
    ssize_t r = pread(fd, buf, n, (off_t)off);
    return r >= 0 ? (long)r : (long)-errno;
}
static int os_mkdir(const char *path, unsigned mode)
{
    return mkdir(path, (mode_t)mode) == 0 ? 0 : -errno;
}
static int os_unlink(const char *path)
{
    return unlink(path) == 0 ? 0 : -errno;
}
static int os_rename(const char *a, const char *b)
{
    return rename(a, b) == 0 ? 0 : -errno;
}
static int os_fsync(int fd)
{
    return fsync(fd) == 0 ? 0 : -errno;
}
static int os_stat(const char *path, ps5io_stat *out)
{
    struct stat st;
    if (stat(path, &st) != 0) return -errno;
    out->exists = 1;
    out->is_dir = S_ISDIR(st.st_mode) ? 1 : (S_ISREG(st.st_mode) ? 0 : -1);
    out->size = (uint64_t)st.st_size;
    out->device = (uint64_t)st.st_dev;
    out->inode = (uint64_t)st.st_ino;
    return 0;
}
static int os_now_ms(uint64_t *out)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return -errno;
    *out = (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
    return 0;
}

static int ps5io_oflags_to_host(unsigned flags)
{
    int f = 0;
    if (flags & PS5IO_RDONLY)     f |= O_RDONLY;
    if (flags & PS5IO_WRONLY)     f |= O_WRONLY;
    if (flags & PS5IO_CREAT)      f |= O_CREAT;
    if (flags & PS5IO_TRUNC)      f |= O_TRUNC;
    if (flags & PS5IO_APPEND)     f |= O_APPEND;
    if (flags & PS5IO_DIRECTORY)  f |= O_DIRECTORY;
    /* Read path hardens against symlink swaps, like the vendored default. */
    if (!(flags & PS5IO_WRONLY))  f |= O_NOFOLLOW | O_CLOEXEC;
    return f;
}

#else /* console: pinned sceKernel exports, negative-errno returns */

static int ps5io_oflags_to_sce(unsigned flags)
{
    int f = 0;
    if (flags & PS5IO_RDONLY)     f |= O_RDONLY;
    if (flags & PS5IO_WRONLY)     f |= O_WRONLY;
    if (flags & PS5IO_CREAT)      f |= O_CREAT;
    if (flags & PS5IO_TRUNC)      f |= O_TRUNC;
    if (flags & PS5IO_APPEND)     f |= O_APPEND;
    if (flags & PS5IO_DIRECTORY)  f |= O_DIRECTORY;
    if (!(flags & PS5IO_WRONLY))  f |= O_NOFOLLOW | O_CLOEXEC;
    return f;
}

static int os_open(const char *path, int flags, unsigned mode)
{
    int fd = sceKernelOpen(path, ps5io_oflags_to_sce(flags), (uint16_t)mode);
    return fd; /* already fd or negative errno */
}
static int os_close(int fd)              { return sceKernelClose(fd); }
static long os_read(int fd, void *b, size_t n)  { return (long)sceKernelRead(fd, b, n); }
static long os_write(int fd, const void *b, size_t n) { return (long)sceKernelWrite(fd, b, n); }
static long os_pread(int fd, void *buf, size_t n, uint64_t off)
{
    /* No pinned pread export: seek + read, offset re-established per call. */
    int64_t rc = sceKernelLseek(fd, (int64_t)off, SEEK_SET);
    if (rc < 0) return (long)rc;
    return (long)sceKernelRead(fd, buf, n);
}
static int os_mkdir(const char *path, unsigned mode) { return sceKernelMkdir(path, (uint16_t)mode); }
static int os_unlink(const char *path)               { return sceKernelUnlink(path); }
static int os_rename(const char *a, const char *b)   { return sceKernelRename(a, b); }
static int os_fsync(int fd)                          { return sceKernelFsync(fd); }
static int os_stat(const char *path, ps5io_stat *out)
{
    ps5io_sce_stat st;
    int rc = sceKernelStat(path, &st);
    if (rc != 0) return rc;
    out->exists = 1;
    out->is_dir = ((st.mode & S_IFMT) == S_IFDIR) ? 1
                : (((st.mode & S_IFMT) == S_IFREG) ? 0 : -1);
    out->size = (uint64_t)st.size;
    out->device = (uint64_t)st.dev;
    out->inode = (uint64_t)st.ino;
    return 0;
}
static int os_now_ms(uint64_t *out)
{
    ps5io_timespec ts;
    int rc = sceKernelClockGettime(4 /* CLOCK_MONOTONIC, pinned sysroot */, &ts);
    if (rc != 0) return rc;
    *out = (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
    return 0;
}

#endif

/* ---- shared semantic layer ------------------------------------------------ */

int ps5io_open(const char *path, unsigned flags, unsigned mode)
{
    if (!path || !path[0]) return -EINVAL;
    return os_open(path, (int)flags, mode);
}

int ps5io_close(int fd)
{
    return os_close(fd);
}

long ps5io_read(int fd, void *buf, size_t n)
{
    size_t take = n;
    if (!buf) return -EINVAL;
    if (take > PS5IO_MAX_RW_CHUNK) take = PS5IO_MAX_RW_CHUNK;
    return os_read(fd, buf, take);
}

long ps5io_write(int fd, const void *buf, size_t n)
{
    size_t take = n;
    if (!buf) return -EINVAL;
    if (take > PS5IO_MAX_RW_CHUNK) take = PS5IO_MAX_RW_CHUNK;
    return os_write(fd, buf, take);
}

int ps5io_write_full(int fd, const void *buf, size_t n)
{
    const char *p = (const char *)buf;
    size_t done = 0;
    if (!buf && n) return -EINVAL;
    while (done < n) {
        size_t want = n - done;
        long r;
        if (want > PS5IO_MAX_RW_CHUNK) want = PS5IO_MAX_RW_CHUNK;
        r = os_write(fd, p + done, want);
        if (r < 0) return (int)r;
        if (r == 0) return -EIO; /* no progress */
        done += (size_t)r;
    }
    return 0;
}

int ps5io_read_full(int fd, void *buf, size_t n, size_t *got)
{
    char *p = (char *)buf;
    size_t done = 0;
    if (!buf && n) return -EINVAL;
    while (done < n) {
        size_t want = n - done;
        long r;
        if (want > PS5IO_MAX_RW_CHUNK) want = PS5IO_MAX_RW_CHUNK;
        r = os_read(fd, p + done, want);
        if (r < 0) return (int)r;
        if (r == 0) break; /* clean EOF */
        done += (size_t)r;
    }
    if (got) *got = done;
    return 0;
}

int ps5io_pread_exact(int fd, uint64_t off, void *buf, size_t n)
{
    char *p = (char *)buf;
    size_t done = 0;
    if (!buf && n) return -EINVAL;
    while (done < n) {
        size_t want = n - done;
        long r;
        if (want > PS5IO_MAX_RW_CHUNK) want = PS5IO_MAX_RW_CHUNK;
        r = os_pread(fd, p + done, want, off + (uint64_t)done);
        if (r < 0) return (int)r;
        if (r == 0) return -EIO; /* short file: wanted n bytes, hit EOF */
        done += (size_t)r;
    }
    return 0;
}

int ps5io_mkdir(const char *path, unsigned mode)
{
    if (!path || !path[0]) return -EINVAL;
    return os_mkdir(path, mode);
}

int ps5io_mkdir_p(const char *path, unsigned mode)
{
    char buf[PS5IO_MAX_PATH];
    size_t n, i;
    ps5io_stat st;
    int rc;
    if (!path || !path[0]) return -EINVAL;
    n = ps5io_strlen(path);
    if (n == 0 || n >= sizeof buf) return n ? -ENAMETOOLONG : -EINVAL;
    for (i = 1; i <= n; ++i) {
        if (i == n || path[i] == '/') {
            size_t len = (i == n) ? n : i;
            if (len == 0) continue;
            ps5io_copy(buf, path, len);
            buf[len] = 0;
            rc = os_mkdir(buf, mode);
            if (rc != 0 && rc != -EEXIST) return rc;
        }
    }
    rc = os_stat(path, &st);
    if (rc != 0) return rc;
    if (st.is_dir != 1) return -ENOTDIR;
    return 0;
}

int ps5io_unlink(const char *path)
{
    if (!path || !path[0]) return -EINVAL;
    return os_unlink(path);
}

int ps5io_rename(const char *from, const char *to)
{
    if (!from || !from[0] || !to || !to[0]) return -EINVAL;
    return os_rename(from, to);
}

int ps5io_fsync(int fd)
{
    return os_fsync(fd);
}

int ps5io_fsync_dir(const char *path)
{
    int fd, rc;
    if (!path || !path[0]) return -EINVAL;
    fd = ps5io_open(path, PS5IO_RDONLY | PS5IO_DIRECTORY, 0);
    if (fd < 0) return fd;
    rc = os_fsync(fd);
    os_close(fd);
    return rc;
}

int ps5io_stat_path(const char *path, ps5io_stat *out)
{
    if (!path || !path[0] || !out) return -EINVAL;
    out->exists = 0;
    out->is_dir = 0;
    out->size = 0;
    out->device = 0;
    out->inode = 0;
    return os_stat(path, out);
}

int ps5io_now_ms(uint64_t *out_ms)
{
    if (!out_ms) return -EINVAL;
    return os_now_ms(out_ms);
}

int ps5io_join(char *out, size_t cap, const char *a, const char *b)
{
    size_t an, bn;
    if (!out || !a || !b || !a[0] || !b[0]) return -EINVAL;
    an = ps5io_strlen(a);
    bn = ps5io_strlen(b);
    if (an >= cap || bn >= cap) return -ENAMETOOLONG;
    if (an + 1 + bn + 1 > cap) return -ENAMETOOLONG;
    ps5io_copy(out, a, an);
    out[an] = '/';
    ps5io_copy(out + an + 1, b, bn);
    out[an + 1 + bn] = 0;
    return 0;
}

const char *ps5io_status_string(int e)
{
    switch (e) {
    case 0:          return "ok";
    case -ENOENT:    return "no such file or directory";
    case -EEXIST:    return "file exists";
    case -EACCES:    return "permission denied";
    case -EINVAL:    return "invalid argument";
    case -ENAMETOOLONG: return "file name too long";
    case -ENOTDIR:   return "not a directory";
    case -EISDIR:    return "is a directory";
    case -EBADF:     return "bad file descriptor";
    case -EIO:       return "io error";
    case -ENOSPC:    return "no space left on device";
    case -EPERM:     return "operation not permitted";
    default:         return "unmapped errno";
    }
}

/* ---- mfb_io binding (single bundle) --------------------------------------- */

static int mfb_cb_read_at(void *vc, uint64_t off, void *buf, size_t n)
{
    ps5io_mfb_ctx *c = (ps5io_mfb_ctx *)vc;
    return ps5io_pread_exact(c->fd, off, buf, n);
}
static int mfb_cb_make_dirs(void *vc, const char *rel)
{
    char path[PS5IO_MAX_PATH];
    ps5io_mfb_ctx *c = (ps5io_mfb_ctx *)vc;
    if (!rel || !rel[0]) return ps5io_mkdir_p(c->root, 0755);
    if (ps5io_join(path, sizeof path, c->root, rel) != 0) return -ENAMETOOLONG;
    return ps5io_mkdir_p(path, 0755);
}
static int mfb_cb_open_write(void *vc, const char *rel)
{
    char path[PS5IO_MAX_PATH];
    ps5io_mfb_ctx *c = (ps5io_mfb_ctx *)vc;
    if (!rel || !rel[0]) return -EINVAL;
    if (ps5io_join(path, sizeof path, c->root, rel) != 0) return -ENAMETOOLONG;
    return ps5io_open(path, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_TRUNC, 0644);
}
static int mfb_cb_write(void *vc, int h, const void *buf, size_t n)
{
    (void)vc;
    return ps5io_write_full(h, buf, n);
}
static int mfb_cb_close(void *vc, int h)
{
    (void)vc;
    return ps5io_close(h);
}
static int mfb_cb_remove(void *vc, const char *rel)
{
    char path[PS5IO_MAX_PATH];
    ps5io_mfb_ctx *c = (ps5io_mfb_ctx *)vc;
    if (!rel || !rel[0]) return -EINVAL;
    if (ps5io_join(path, sizeof path, c->root, rel) != 0) return -ENAMETOOLONG;
    return ps5io_unlink(path);
}

int ps5io_mfb_bind(ps5io_mfb_ctx *ctx, mfb_io *io)
{
    if (!ctx || !io) return -EINVAL;
    io->ctx = ctx;
    io->read_at = mfb_cb_read_at;
    io->make_dirs = mfb_cb_make_dirs;
    io->open_write = mfb_cb_open_write;
    io->write = mfb_cb_write;
    io->close = mfb_cb_close;
    io->remove = mfb_cb_remove;
    return 0;
}

/* ---- mfx_io binding (multi bundle) ---------------------------------------- */

static int mfx_cb_read_at(void *vc, size_t bundle, uint64_t off, void *buf, size_t n)
{
    ps5io_mfx_ctx *c = (ps5io_mfx_ctx *)vc;
    if (bundle >= c->fd_count) return -EINVAL;
    return ps5io_pread_exact(c->fds[bundle], off, buf, n);
}
static int mfx_cb_make_dirs(void *vc, const char *rel)
{
    char path[PS5IO_MAX_PATH];
    ps5io_mfx_ctx *c = (ps5io_mfx_ctx *)vc;
    if (!rel || !rel[0]) return ps5io_mkdir_p(c->root, 0755);
    if (ps5io_join(path, sizeof path, c->root, rel) != 0) return -ENAMETOOLONG;
    return ps5io_mkdir_p(path, 0755);
}
static int mfx_cb_open_write(void *vc, const char *rel)
{
    char path[PS5IO_MAX_PATH];
    ps5io_mfx_ctx *c = (ps5io_mfx_ctx *)vc;
    if (!rel || !rel[0]) return -EINVAL;
    if (ps5io_join(path, sizeof path, c->root, rel) != 0) return -ENAMETOOLONG;
    return ps5io_open(path, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_TRUNC, 0644);
}
static int mfx_cb_write(void *vc, int h, const void *buf, size_t n)
{
    (void)vc;
    return ps5io_write_full(h, buf, n);
}
static int mfx_cb_close(void *vc, int h)
{
    (void)vc;
    return ps5io_close(h);
}
static int mfx_cb_remove(void *vc, const char *rel)
{
    char path[PS5IO_MAX_PATH];
    ps5io_mfx_ctx *c = (ps5io_mfx_ctx *)vc;
    if (!rel || !rel[0]) return -EINVAL;
    if (ps5io_join(path, sizeof path, c->root, rel) != 0) return -ENAMETOOLONG;
    return ps5io_unlink(path);
}

int ps5io_mfx_init(ps5io_mfx_ctx *ctx, const char *root)
{
    size_t i;
    if (!ctx || !root || !root[0]) return -EINVAL;
    if (ps5io_strlen(root) >= sizeof ctx->root) return -ENAMETOOLONG;
    ps5io_copy(ctx->root, root, ps5io_strlen(root) + 1);
    for (i = 0; i < PS5IO_MAX_BUNDLES; ++i) ctx->fds[i] = -1;
    ctx->fd_count = 0;
    return 0;
}

int ps5io_mfx_add(ps5io_mfx_ctx *ctx, int fd)
{
    if (!ctx || fd < 0) return -EINVAL;
    if (ctx->fd_count >= PS5IO_MAX_BUNDLES) return -E2BIG;
    ctx->fds[ctx->fd_count] = fd;
    return (int)ctx->fd_count++;
}

int ps5io_mfx_bind(ps5io_mfx_ctx *ctx, mfx_io *io)
{
    if (!ctx || !io) return -EINVAL;
    io->ctx = ctx;
    io->read_at = mfx_cb_read_at;
    io->make_dirs = mfx_cb_make_dirs;
    io->open_write = mfx_cb_open_write;
    io->write = mfx_cb_write;
    io->close = mfx_cb_close;
    io->remove = mfx_cb_remove;
    return 0;
}

/* ---- mfa_io binding (runtime activation reads) ----------------------------
 * The vendored library joins root+relative itself and hands open_read the
 * full path (join_path in modforge_activation.c), so no joining happens here;
 * reads follow the same contract as the library's POSIX default
 * (O_RDONLY|O_NOFOLLOW|O_CLOEXEC, partial reads fine, 0 = EOF). */

static int mfa_cb_open_read(void *vc, const char *path, int *os_error)
{
    int fd = ps5io_open(path, PS5IO_RDONLY, 0);
    (void)vc;
    if (fd < 0) {
        if (os_error) *os_error = -fd;
        return -1;
    }
    return fd;
}
static long mfa_cb_read(void *vc, int fd, void *buf, size_t n, int *os_error)
{
    long r = ps5io_read(fd, buf, n);
    (void)vc;
    if (r < 0) {
        if (os_error) *os_error = (int)-r;
        return -1;
    }
    return r; /* >= 0 bytes; 0 = clean EOF, per the mfa_io contract */
}
static int mfa_cb_close(void *vc, int fd)
{
    (void)vc;
    return ps5io_close(fd) == 0 ? 0 : -1;
}
static uint64_t mfa_cb_now_ms(void *vc)
{
    uint64_t ms = 0;
    (void)vc;
    if (ps5io_now_ms(&ms) != 0) return 0;
    return ms;
}

int ps5io_mfa_bind(ps5io_mfa_ctx *ctx, const char *root, mfa_io *io)
{
    if (!ctx || !io || !root || !root[0]) return -EINVAL;
    if (ps5io_strlen(root) >= sizeof ctx->root) return -ENAMETOOLONG;
    ps5io_copy(ctx->root, root, ps5io_strlen(root) + 1);
    io->ctx = ctx;
    io->open_read = mfa_cb_open_read;
    io->read = mfa_cb_read;
    io->close = mfa_cb_close;
    io->now_ms = mfa_cb_now_ms;
    return 0;
}
