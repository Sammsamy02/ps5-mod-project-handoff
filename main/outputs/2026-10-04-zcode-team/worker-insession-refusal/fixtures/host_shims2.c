/* host_shims2.c — POSIX stand-ins for the kernel imports the in-session
   service TU binds (sceKernelOpen/sceKernelWrite/sceKernelClose; read/rename
   are native on the host) plus the __error errno word used by diagnostic.c,
   with deterministic failure injection for the at-least-once consume path —
   and FAKE same-PRX bridges: a fake bridge_kernel_write_copyin_probe
   (records every call, returns a preset class — proves FRESH per-command
   probes) and fake batched IO controls a..d (records every call, returns a
   preset rc + a result-tag string). Also provides the insession2_test_rename
   hook the service TU uses in INSESSION_CMD2_HOST_TEST builds.
   HOST TESTS ONLY — never compiled into any console PRX. */
#include <fcntl.h>
#include <stdio.h> /* rename declaration */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

/* ---- failure-injection controls (driven by the test main) ---- */
int shim_fail_open;       /* 1 = every open fails with EACCES (13) */
int shim_open_attempts;   /* total sceKernelOpen calls observed */
int shim_fail_rename;     /* 1 = every rename fails with EPERM (1) */
int shim_rename_attempts; /* total rename calls observed */

/* ---- fake copyin-bridge probe (the service must call it FRESH each
   pool_probe command; the real bridge is stateless per call too — the
   once-per-boot cache lives in the initializer TU, which is absent here) */
int fake_probe_calls;
uint64_t fake_probe_last_addr;
uint64_t fake_probe_last_len;
int fake_probe_class = 1; /* 1 readable / 0 refused / -1 unavailable */

/* ---- fake batched IO controls (worker-io-controls stand-ins) ---- */
int fake_io_calls[4];
int fake_io_rc[4] = {0, 0, 0, 0};
static const char *fake_io_tags[4] = {
    "ioa-ok", "iob-ok", "ioc-ok", "iod-ok"};

static int shim_errno_word;

int *__error(void) { return &shim_errno_word; }

int sceKernelOpen(const char *path, int flags, uint16_t mode)
{
    ++shim_open_attempts;
    if (shim_fail_open)
    {
        shim_errno_word = 13;
        return -1;
    }
    return open(path, flags, (mode_t)mode);
}

int64_t sceKernelWrite(int fd, const void *buf, size_t n)
{
    return (int64_t)write(fd, buf, n);
}

int sceKernelClose(int fd) { return close(fd); }

int insession2_test_rename(const char *oldp, const char *newp)
{
    ++shim_rename_attempts;
    if (shim_fail_rename)
    {
        shim_errno_word = 1;
        return -1;
    }
    return rename(oldp, newp);
}

int bridge_kernel_write_copyin_probe(uint64_t addr, uint64_t len)
{
    ++fake_probe_calls;
    fake_probe_last_addr = addr;
    fake_probe_last_len = len;
    return fake_probe_class;
}

static int fake_io_control(unsigned slot, char *tags_out, unsigned cap)
{
    ++fake_io_calls[slot];
    if (tags_out != 0 && cap > 0)
    {
        size_t i = 0;
        const char *t = fake_io_tags[slot];
        for (; t[i] != '\0' && i + 1 < cap; ++i)
            tags_out[i] = t[i];
        tags_out[i] = '\0';
    }
    return fake_io_rc[slot];
}

int bridge_io_control_a(char *tags_out, unsigned cap)
{
    return fake_io_control(0, tags_out, cap);
}
int bridge_io_control_b(char *tags_out, unsigned cap)
{
    return fake_io_control(1, tags_out, cap);
}
int bridge_io_control_c(char *tags_out, unsigned cap)
{
    return fake_io_control(2, tags_out, cap);
}
int bridge_io_control_d(char *tags_out, unsigned cap)
{
    return fake_io_control(3, tags_out, cap);
}
