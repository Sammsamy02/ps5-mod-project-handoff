/* host_shims.c — POSIX stand-ins for the kernel imports the in-session poll
   TU binds (sceKernelOpen/sceKernelClose; read/rename are native on the host)
   plus the __error errno word used by diagnostic.c, with deterministic
   failure injection for the at-least-once consume path. Also provides the
   insession_test_log / insession_test_rename hooks the poll TU uses in
   INSESSION_CMD_HOST_TEST builds. HOST TESTS ONLY — never compiled into any
   console PRX. */
#include <fcntl.h>
#include <stdio.h> /* rename declaration */
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

/* ---- failure-injection controls (driven by the test main) ---- */
int shim_fail_open;            /* 1 = every open fails with EACCES (13) */
int shim_open_attempts;        /* total sceKernelOpen calls observed */
int shim_fail_rename;          /* 1 = every rename fails with EPERM (1) */
int shim_rename_attempts;      /* total rename calls observed */

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

void insession_test_log(const char *line)
{
    /* One test log, line-buffered to a FILE* the main installs. */
    extern void test_emit_log_line(const char *line);
    test_emit_log_line(line);
}

int insession_test_rename(const char *oldp, const char *newp)
{
    ++shim_rename_attempts;
    if (shim_fail_rename)
    {
        shim_errno_word = 1;
        return -1;
    }
    return rename(oldp, newp);
}
