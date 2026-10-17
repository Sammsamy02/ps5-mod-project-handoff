/*
 * ps5_io.h — PS5 kernel-file I/O adapter for the modforge installer core.
 *
 * Origin: worker-installer-ps5io, 2026-10-04.
 * Consumers: worker-7's vendored installer core (modforge_bundle.h mfb_io,
 * modforge_compose.h mfx_io, modforge_activation.h mfa_io).
 *
 * Every filesystem operation the installer core needs on the console goes
 * through this adapter. On the console (PS5IO_PS5, default when building with
 * the pinned PS5 payload SDK) it binds ONLY these pinned, nm-verified exports
 * of the pinned target libs:
 *
 *   sceKernelOpen        sceKernelRead       sceKernelWrite     sceKernelClose
 *   sceKernelLseek       sceKernelMkdir      sceKernelStat      sceKernelRename
 *   sceKernelUnlink      sceKernelFsync      sceKernelClockGettime
 *
 * Signatures match the pinned SharpProspero Interop/Kernel/KernelFile.cs /
 * KernelClock.cs declarations (same convention as native-module-probe/probe.c).
 * No other external symbol is referenced by the console build (verified by nm
 * over the object; see receipts/). The host build (PS5IO_HOST=1) maps the same
 * logic onto POSIX and exists solely so the semantics below can be tested.
 *
 * Result convention (uniform across both builds): functions return 0 on
 * success and a NEGATIVE errno value (-ENOENT, -EEXIST, ...) on failure.
 * The console sceKernel* API already returns negative errno; the host build
 * converts. ps5io_status_string() names the common codes.
 *
 * Path rules: paths are NUL-terminated byte strings; every produced path is
 * bounded by PS5IO_MAX_PATH (4096, equal to MB_ARCHIVE_MAX_PATH_BYTES);
 * overflow returns -ENAMETOOLONG. Empty or NULL path components return
 * -EINVAL. No path component is ever modified beyond the caller's buffers.
 *
 * O-flag semantics (ps5io_oflags): explicit, small, deliberately not the raw
 * O_* bitmask, so the mapping is inspectable:
 *   PS5IO_RDONLY          read-only (implies O_NOFOLLOW on the read path)
 *   PS5IO_WRONLY          write-only
 *   PS5IO_RDWR            read-write (one fd both directions; sink ABI demand)
 *   PS5IO_CREAT           create if missing (mode argument applies)
 *   PS5IO_TRUNC           truncate to zero on open
 *   PS5IO_APPEND          append before each write
 *   PS5IO_DIRECTORY       path must be a directory (open for fsync-of-dir)
 * Modes are POSIX symbolic (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH == 0644 etc.);
 * the console build uses the pinned sysroot's <sys/stat.h> values, the host
 * build the host's. The installer writes files 0644, directories 0755.
 *
 * RDWR binding (worker-ps5io-rdwr, closing worker-e2e-final REPORT F1): the
 * real download file sink admits only an fd whose O_ACCMODE is exactly O_RDWR
 * (fcntl F_GETFL check, archive/private/file_sink.c:31), so this adapter
 * exports PS5IO_RDWR and maps it to the pinned SDK's own O_RDWR bit —
 * target/include/sys/fcntl.h of the pinned ps5-payload-sdk (sdk-v0.43):
 *     O_RDONLY 0x0000   O_WRONLY 0x0001   O_RDWR 0x0002   O_ACCMODE 0x0003
 * (BSD layout, verified by receipts/sdk-oflags.txt; the host build maps to the
 * host <fcntl.h> O_RDWR, also 0x2 on macOS). PS5IO_RDWR itself keeps the
 * adapter convention of NOT being the raw O_* value: it is the next free
 * symbolic bit (64), so the mapping stays inspectable and double-binding is
 * impossible. Read/write access selection is exclusive: ps5io_open validates
 * the flag word and refuses (-EINVAL) anything but exactly one of RDONLY /
 * WRONLY / RDWR, TRUNC or APPEND on RDONLY, DIRECTORY on a write-capable
 * mode, and any unknown bit. A write-capable fd that is not write-ONLY
 * (RDWR included) still gets the O_NOFOLLOW|O_CLOEXEC hardening, so a fresh
 * sink tmp cannot be redirected through a planted symlink.
 *
 * Bounded I/O: single read/write calls are split into chunks of at most
 * PS5IO_MAX_RW_CHUNK bytes; *_full primitives loop until completion, EOF or
 * error, never spinning on a short transfer that makes no progress.
 *
 * HONESTY: nothing here was executed on a console. Durability calls
 * (fsync/fsync_dir) are bound but their effect on the console filesystem is
 * unproven (target_resolution_proven=false); directory opens for fsync and
 * sceKernelStat struct offsets follow the pinned SharpProspero layout.
 */
#ifndef PS5_IO_H
#define PS5_IO_H

#include <stddef.h>
#include <stdint.h>

/* Vendored, unmodified installer-core interfaces (origin: worker-7). */
#include "modforge_bundle.h"    /* mfb_io */
#include "modforge_compose.h"   /* mfx_io */
#include "modforge_activation.h"/* mfa_io */

#ifdef __cplusplus
extern "C" {
#endif

#define PS5IO_MAX_PATH   4096u
#define PS5IO_MAX_RW_CHUNK (16u * 1024u * 1024u) /* per-syscall bound */
#define PS5IO_MAX_BUNDLES 64u                    /* mfx bundle slots */

enum {
    PS5IO_RDONLY     = 1,
    PS5IO_WRONLY     = 2,
    PS5IO_CREAT      = 4,
    PS5IO_TRUNC      = 8,
    PS5IO_APPEND     = 16,
    PS5IO_DIRECTORY  = 32,
    PS5IO_RDWR       = 64  /* maps to pinned-SDK O_RDWR 0x0002 (see above) */
};

#define PS5IO_ACCMASK (PS5IO_RDONLY | PS5IO_WRONLY | PS5IO_RDWR)
#define PS5IO_KNOWN_FLAGS \
    (PS5IO_ACCMASK | PS5IO_CREAT | PS5IO_TRUNC | PS5IO_APPEND | PS5IO_DIRECTORY)

/* ---- primitives (identical semantics on console and host) ---------------- */

int ps5io_open(const char *path, unsigned flags, unsigned mode); /* fd or -errno */
int ps5io_close(int fd);                                         /* 0 or -errno */
long ps5io_read(int fd, void *buf, size_t n);      /* >0 bytes, 0 EOF, or -errno */
long ps5io_write(int fd, const void *buf, size_t n); /* >0 bytes or -errno */
/* Write all n bytes. 0 or -errno. */
int ps5io_write_full(int fd, const void *buf, size_t n);
/* Read up to n bytes; *got is 0 at clean EOF. 0 or -errno. */
int ps5io_read_full(int fd, void *buf, size_t n, size_t *got);
/* Exact pread: read exactly n bytes at absolute offset off. 0 or -errno.
 * Console implementation is lseek(SEEK_SET)+read loop per call (the pinned
 * export set has no pread sceKernel), so offsets are re-established on every
 * call and interleaved bundle reads stay exact. */
int ps5io_pread_exact(int fd, uint64_t off, void *buf, size_t n);

int ps5io_mkdir(const char *path, unsigned mode);            /* 0 or -errno */
/* Create path and every missing parent. 0 if the final directory already
 * exists, like the host slice's mkdirs. -errno otherwise. */
int ps5io_mkdir_p(const char *path, unsigned mode);
int ps5io_unlink(const char *path);                          /* 0 or -errno */
int ps5io_rename(const char *from, const char *to);          /* 0 or -errno */
int ps5io_fsync(int fd);                                     /* 0 or -errno */
/* Open a directory and fsync it (durability of renames/creates inside it). */
int ps5io_fsync_dir(const char *path);

typedef struct {
    uint64_t size;      /* bytes */
    uint64_t device;    /* containing device (quarantine identity) */
    uint64_t inode;     /* file serial number (quarantine identity) */
    int is_dir;         /* 1 directory, 0 regular, -1 other/unknown */
    int exists;         /* 1 if the path resolved */
} ps5io_stat;
int ps5io_stat_path(const char *path, ps5io_stat *out);      /* 0 or -errno */

/* Monotonic milliseconds (CLOCK_MONOTONIC). 0 or -errno. */
int ps5io_now_ms(uint64_t *out_ms);

/* Bounded join: out = a '/' b. 0 or -errno (-ENAMETOOLONG, -EINVAL). */
int ps5io_join(char *out, size_t cap, const char *a, const char *b);
const char *ps5io_status_string(int neg_errno);

/* ---- mfb_io: single-bundle install (modforge_bundle.h) ------------------- */

typedef struct {
    int  fd;              /* read-only bundle descriptor, caller-owned */
    char root[PS5IO_MAX_PATH]; /* mod root all writes are relative to */
} ps5io_mfb_ctx;

/* Fills *io with callbacks bound to *ctx. ctx->fd must already be open and
 * ctx->root set. Returns 0. */
int ps5io_mfb_bind(ps5io_mfb_ctx *ctx, mfb_io *io);

/* ---- mfx_io: multi-bundle compose (modforge_compose.h) ------------------- */

typedef struct {
    int  fds[PS5IO_MAX_BUNDLES];  /* read-only descriptors, caller-owned */
    size_t fd_count;
    char root[PS5IO_MAX_PATH];    /* staging root for writes */
} ps5io_mfx_ctx;

int ps5io_mfx_init(ps5io_mfx_ctx *ctx, const char *root);
int ps5io_mfx_add(ps5io_mfx_ctx *ctx, int fd);  /* slot index or -errno */
int ps5io_mfx_bind(ps5io_mfx_ctx *ctx, mfx_io *io);

/* ---- mfa_io: runtime activation reads (modforge_activation.h) ------------ */

typedef struct {
    char root[PS5IO_MAX_PATH];    /* activation root; paths arrive joined */
} ps5io_mfa_ctx;

int ps5io_mfa_bind(ps5io_mfa_ctx *ctx, const char *root, mfa_io *io);

#ifdef __cplusplus
}
#endif

#endif /* PS5_IO_H */
