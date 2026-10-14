// host_shims.c — worker-combined-igf7 HOST-TEST ONLY (never built into the PRX).
// Maps the 7-import ABI surface onto the host libc so record_sink.c and the
// combined probe compile/run unmodified in host tests. sysctl/getpid/
// clock_gettime are NOT referenced by the host build (the sysctl collection
// block is compiled out under IG_HOST_TEST); __error gets a static errno cell.
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>

int *__error(void) {
    static int host_errno_cell;
    return &host_errno_cell;
}

int sceKernelOpen(const char *path, int flags, uint16_t mode) {
    return open(path, flags, (mode_t)mode);
}

int64_t sceKernelWrite(int fd, const void *buf, size_t n) {
    return (int64_t)write(fd, buf, n);
}

int sceKernelClose(int fd) {
    return close(fd);
}
