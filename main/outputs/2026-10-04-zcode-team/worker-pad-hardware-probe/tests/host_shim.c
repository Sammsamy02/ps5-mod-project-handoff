// host_shim.c — HOST-ONLY shims for the pad probe host test
// (worker-pad-hardware-probe, team 2026-10-04). Provides the console
// imports record_sink.c consumes (sceKernelOpen/Write/Close) on macOS so
// the UNMODIFIED vendored sink compiles and runs on the host. __error is
// NOT redefined: macOS errno.h already declares `extern int *__error(void)`
// and libSystem exports it (defining it here would recurse — `errno` is
// `(*__error())` on Darwin). Never compiled into the PRX
// (build_pad_probe.py compiles only src/).
#include "pad_abi.h"

#include <fcntl.h>
#include <unistd.h>

int sceKernelOpen(const char *path, int flags, uint16_t mode)
{
    return open(path, flags | O_CLOEXEC, (mode_t)mode);
}

int64_t sceKernelWrite(int fd, const void *buf, size_t n)
{
    return (int64_t)write(fd, buf, n);
}

int sceKernelClose(int fd)
{
    return close(fd);
}
