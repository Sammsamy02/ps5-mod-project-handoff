/* host_shims.c — POSIX stand-ins for the three kernel imports the diagnostic
   TU binds (sceKernelOpen/sceKernelWrite/sceKernelClose) plus the __error
   errno word, with deterministic failure injection for the tolerant-IO
   fixtures. HOST TESTS ONLY — never compiled into any console PRX. */
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

/* ---- failure-injection controls (driven by the test main) ---- */
int shim_fail_open;          /* 1 = every open fails with EACCES (13)   */
int shim_fail_open_countdown;/* >0 = fail the next N opens, then pass   */
int shim_open_attempts;      /* total sceKernelOpen calls observed      */
int shim_fail_write;         /* 1 = every write fails with ENOSPC (28)  */
int shim_partial_write;      /* 1 = next write succeeds with half bytes */

static int shim_errno_word;

int *__error(void) { return &shim_errno_word; }

int sceKernelOpen(const char *path, int flags, uint16_t mode)
{
    ++shim_open_attempts;
    if (shim_fail_open || shim_fail_open_countdown > 0) {
        if (shim_fail_open_countdown > 0)
            --shim_fail_open_countdown;
        shim_errno_word = 13; /* EACCES: the read-only-dir console analog */
        return -1;
    }
    return open(path, flags, (mode_t)mode);
}

int64_t sceKernelWrite(int fd, const void *buf, size_t n)
{
    if (shim_fail_write) {
        shim_errno_word = 28;
        return -1;
    }
    if (shim_partial_write && n > 1) {
        /* Emulate a partial short write at the syscall level: emit only the
           first half, forcing the caller's bounded-retry loop to run. */
        shim_partial_write = 0;
        return (int64_t)write(fd, buf, n / 2);
    }
    return (int64_t)write(fd, buf, n);
}

int sceKernelClose(int fd) { return close(fd); }
