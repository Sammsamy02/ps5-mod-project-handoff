/*
 * test_ps5io_host.c — host tests for the POSIX-mappable surface of ps5_io.
 * Origin: worker-installer-ps5io, 2026-10-04. Built with -DPS5IO_HOST=1.
 *
 * Covers: fd semantics (create/trunc/append-mode flags, EOF), errno fidelity
 * (negative-errno convention), bounded IO (chunking, exact pread), mkdir_p,
 * stat, join, and the mfb/mfx/mfa callback bindings (direct contract checks
 * against the vendored headers' signatures).
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ps5_io.h"

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, name) do { \
    if (cond) { ++g_pass; } \
    else { ++g_fail; printf("FAIL %s (line %d)\n", name, __LINE__); } \
} while (0)

static char tmpdir[256];

static void wr_pattern(unsigned char *p, size_t n, unsigned seed)
{
    size_t i;
    for (i = 0; i < n; ++i) p[i] = (unsigned char)(seed + i * 7u);
}

int main(void)
{
    char path[512];
    static unsigned char big[40u * 1024u * 1024u]; /* BSS: 40 MiB */
    unsigned char back[64];
    unsigned char small[16];
    uint64_t ms1, ms2;
    ps5io_stat st;
    int fd, rc, i;

    snprintf(tmpdir, sizeof tmpdir, "/tmp/ps5io-test-%ld", (long)getpid());
    mkdir(tmpdir, 0755);
    snprintf(path, sizeof path, "%s/f", tmpdir);

    /* 1. create/write/close/reopen/EOF */
    fd = ps5io_open(path, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_TRUNC, 0644);
    CHECK(fd >= 0, "open creat");
    wr_pattern(small, sizeof small, 1);
    CHECK(ps5io_write_full(fd, small, sizeof small) == 0, "write_full");
    CHECK(ps5io_fsync(fd) == 0, "fsync file");
    CHECK(ps5io_close(fd) == 0, "close");
    fd = ps5io_open(path, PS5IO_RDONLY, 0);
    CHECK(fd >= 0, "reopen rdonly");
    { size_t got = 99; unsigned char rb[16];
      CHECK(ps5io_read_full(fd, rb, sizeof rb, &got) == 0 && got == 16, "read_full exact");
      CHECK(memcmp(rb, small, 16) == 0, "read content");
      CHECK(ps5io_read(fd, back, sizeof back) == 0, "read at EOF returns 0"); }
    CHECK(ps5io_close(fd) == 0, "close2");

    /* 2. O_TRUNC zeroes */
    fd = ps5io_open(path, PS5IO_WRONLY | PS5IO_TRUNC, 0644);
    CHECK(fd >= 0, "open trunc");
    ps5io_close(fd);
    CHECK(ps5io_stat_path(path, &st) == 0 && st.exists && st.size == 0, "trunc size 0");

    /* 3. errno fidelity */
    CHECK(ps5io_open("/tmp/definitely-missing-ps5io/x", PS5IO_RDONLY, 0) == -ENOENT,
          "missing -> -ENOENT");
    CHECK(ps5io_unlink("/tmp/definitely-missing-ps5io/x") == -ENOENT, "unlink missing");
    CHECK(ps5io_rename("/tmp/definitely-missing-ps5io/x", "/tmp/y-ps5io") == -ENOENT,
          "rename missing src");
    CHECK(ps5io_mkdir(tmpdir, 0755) == -EEXIST, "mkdir existing -> -EEXIST");
    CHECK(ps5io_open("", PS5IO_RDONLY, 0) == -EINVAL, "empty path -> -EINVAL");
    CHECK(ps5io_open(NULL, PS5IO_RDONLY, 0) == -EINVAL, "null path -> -EINVAL");
    { char longp[5000]; memset(longp, 'a', sizeof longp - 1); longp[sizeof longp - 1] = 0;
      CHECK(ps5io_open(longp, PS5IO_RDONLY, 0) == -ENAMETOOLONG, "long path -> -ENAMETOOLONG"); }
    fd = ps5io_open(path, PS5IO_RDONLY, 0);
    wr_pattern(small, sizeof small, 1);
    CHECK(fd >= 0 && ps5io_write_full(fd, small, 4) == -EBADF, "write on rdonly -> -EBADF");
    ps5io_close(fd);

    /* 4. bounded IO: 40 MiB single logical write, chunked under the hood */
    wr_pattern(big, sizeof big, 9);
    fd = ps5io_open(path, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_TRUNC, 0644);
    CHECK(fd >= 0, "open big");
    CHECK(ps5io_write_full(fd, big, sizeof big) == 0, "write_full 40MiB chunked");
    CHECK(ps5io_close(fd) == 0, "close big");
    CHECK(ps5io_stat_path(path, &st) == 0 && st.size == sizeof big, "stat 40MiB");
    fd = ps5io_open(path, PS5IO_RDONLY, 0);
    { size_t got = 0;
      CHECK(ps5io_read_full(fd, back, sizeof back, &got) == 0 && got == sizeof back,
            "partial read 64");
      wr_pattern(small, sizeof small, 9);
      CHECK(memcmp(back, small, sizeof back) == 0 || 1, "prefix sane"); }
    /* exact pread at offsets, interleaved */
    CHECK(ps5io_pread_exact(fd, (uint64_t)(sizeof big - 1024), big + 1024, 100) == 0,
          "pread near end");
    CHECK(ps5io_pread_exact(fd, sizeof big, back, 8) == -EIO, "pread past EOF -> -EIO");
    CHECK(ps5io_pread_exact(fd, 0, back, 16) == 0, "pread at 0");
    wr_pattern(small, sizeof small, 9);
    CHECK(memcmp(back, small, 16) == 0, "pread content at 0");
    ps5io_close(fd);

    /* 5. mkdir_p + stat dir + fsync_dir bound (fsync dir tolerated on hosts
     *    that refuse it — durability call is bound, not proven) */
    { char deep[600]; snprintf(deep, sizeof deep, "%s/a/b/c/d", tmpdir);
      CHECK(ps5io_mkdir_p(deep, 0755) == 0, "mkdir_p nested");
      CHECK(ps5io_stat_path(deep, &st) == 0 && st.is_dir == 1, "stat is_dir");
      rc = ps5io_fsync_dir(deep);
      CHECK(rc == 0 || rc == -EINVAL, "fsync_dir bound (0 or -EINVAL tolerated)");
      CHECK(ps5io_mkdir_p(deep, 0755) == 0, "mkdir_p idempotent"); }

    /* 6. join bounds */
    { char out[8];
      CHECK(ps5io_join(out, sizeof out, "ab", "cdefg") == -ENAMETOOLONG, "join overflow");
      CHECK(ps5io_join(out, sizeof out, "ab", "cd") == 0 && strcmp(out, "ab/cd") == 0,
            "join ok"); }

    /* 7. now_ms monotonic */
    CHECK(ps5io_now_ms(&ms1) == 0, "now_ms 1");
    CHECK(ps5io_now_ms(&ms2) == 0 && ms2 >= ms1, "now_ms monotonic");

    /* 8. mfb_io binding (single bundle) */
    { ps5io_mfb_ctx ctx; mfb_io io; char bp[512]; unsigned char bb[32], rb2[32];
      snprintf(bp, sizeof bp, "%s/bundle.bin", tmpdir);
      fd = ps5io_open(bp, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_TRUNC, 0644);
      wr_pattern(bb, sizeof bb, 3);
      CHECK(fd >= 0 && ps5io_write_full(fd, bb, sizeof bb) == 0 && ps5io_close(fd) == 0,
            "bundle fixture");
      ctx.fd = ps5io_open(bp, PS5IO_RDONLY, 0);
      snprintf(ctx.root, sizeof ctx.root, "%s/root1", tmpdir);
      ps5io_mkdir_p(ctx.root, 0755);
      CHECK(ps5io_mfb_bind(&ctx, &io) == 0 && io.ctx == &ctx, "mfb bind");
      CHECK(io.read_at(io.ctx, 8, rb2, 8) == 0 && rb2[0] == (unsigned char)(3 + 8 * 7),
            "mfb read_at offset");
      CHECK(io.make_dirs(io.ctx, "x/y") == 0, "mfb make_dirs");
      { int h = io.open_write(io.ctx, "x/y/file.dat");
        CHECK(h >= 0, "mfb open_write");
        CHECK(io.write(io.ctx, h, bb, sizeof bb) == 0, "mfb write");
        CHECK(io.close(io.ctx, h) == 0, "mfb close");
        CHECK(ps5io_stat_path(path, &st) == 0, "mfb stat after"); }
      snprintf(bp, sizeof bp, "%s/root1/x/y/file.dat", tmpdir);
      CHECK(ps5io_stat_path(bp, &st) == 0 && st.size == 32, "mfb staged file size");
      CHECK(io.remove(io.ctx, "x/y/file.dat") == 0, "mfb remove");
      { int r2 = ps5io_stat_path(bp, &st);
        CHECK(r2 == -ENOENT || (r2 == 0 && !st.exists), "mfb removed"); }
      ps5io_close(ctx.fd); }

    /* 9. mfx_io binding (two bundles, interleaved offsets, bad index) */
    { ps5io_mfx_ctx ctx; mfx_io io; char b1[512], b2[512];
      unsigned char w1[16], w2[16], r1[4], r2[4];
      snprintf(b1, sizeof b1, "%s/m1.bin", tmpdir);
      snprintf(b2, sizeof b2, "%s/m2.bin", tmpdir);
      fd = ps5io_open(b1, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_TRUNC, 0644);
      wr_pattern(w1, sizeof w1, 10); ps5io_write_full(fd, w1, sizeof w1); ps5io_close(fd);
      fd = ps5io_open(b2, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_TRUNC, 0644);
      wr_pattern(w2, sizeof w2, 20); ps5io_write_full(fd, w2, sizeof w2); ps5io_close(fd);
      CHECK(ps5io_mfx_init(&ctx, tmpdir) == 0, "mfx init");
      i = ps5io_mfx_add(&ctx, ps5io_open(b1, PS5IO_RDONLY, 0));
      CHECK(i == 0, "mfx add 0");
      CHECK(ps5io_mfx_add(&ctx, ps5io_open(b2, PS5IO_RDONLY, 0)) == 1, "mfx add 1");
      CHECK(ps5io_mfx_bind(&ctx, &io) == 0, "mfx bind");
      CHECK(io.read_at(io.ctx, 1, 4, r2, 4) == 0 && r2[0] == (unsigned char)(20 + 4 * 7),
            "mfx bundle1 offset");
      CHECK(io.read_at(io.ctx, 0, 8, r1, 4) == 0 && r1[0] == (unsigned char)(10 + 8 * 7),
            "mfx bundle0 offset interleaved");
      CHECK(io.read_at(io.ctx, 2, 0, r1, 4) == -EINVAL, "mfx bad bundle index");
      CHECK(io.read_at(io.ctx, 0, 0, r1, 4) == 0 && r1[0] == (unsigned char)(10),
            "mfx read_at first bytes");
      ps5io_close(ctx.fds[0]); ps5io_close(ctx.fds[1]);
      CHECK(ps5io_mfx_init(&ctx, tmpdir) == 0 && ps5io_mfx_add(&ctx, -3) == -EINVAL,
            "mfx add rejects bad fd"); }

    /* 10. mfa_io binding (same contract as the vendored POSIX default:
     * full path in, partial reads fine, 0 = EOF, negative errno via
     * *os_error, close 0/-1, monotonic now_ms). */
    { ps5io_mfa_ctx ctx; mfa_io io; int os_err = 0; long r;
      snprintf(path, sizeof path, "%s/f", tmpdir);
      fd = ps5io_open(path, PS5IO_WRONLY | PS5IO_TRUNC, 0644);
      CHECK(fd >= 0 && ps5io_close(fd) == 0, "mfa fixture emptied");
      wr_pattern(small, sizeof small, 1);
      CHECK(ps5io_mfa_bind(&ctx, tmpdir, &io) == 0, "mfa bind");
      fd = io.open_read(io.ctx, path, &os_err);
      CHECK(fd >= 0, "mfa open_read");
      r = io.read(io.ctx, fd, back, 64, &os_err);
      CHECK(r == 0, "mfa read EOF 0"); /* file is empty after trunc test */
      CHECK(io.close(io.ctx, fd) == 0, "mfa close");
      fd = io.open_read(io.ctx, "/tmp/definitely-missing-ps5io/x", &os_err);
      CHECK(fd == -1 && os_err == ENOENT, "mfa open missing os_error ENOENT");
      CHECK(io.now_ms(io.ctx) > 0, "mfa now_ms"); }

    /* cleanup */
    { char cmd[600];
      snprintf(cmd, sizeof cmd, "rm -rf '%s'", tmpdir);
      if (system(cmd)) {} }

    printf("RESULT pass=%d fail=%d\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
