/*
 * main.c - gdbstub launcher.
 *
 * Host:      ./gdbstub                     (mock backend, 127.0.0.1:2345)
 * PS5 payload: ./gdbstub -b ps5 --pid <game-pid>
 *
 * Then on the Mac:
 *      gdb -ex "target remote :2345"        (or)
 *      lldb -o "gdb-remote 127.0.0.1:2345"
 *
 * 'monitor pid <n>' re-targets the process without reconnecting.
 */
#include "gdbstub.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const struct gdb_backend *mock_backend(void);
const struct gdb_backend *ps5_backend(const char *dev);

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [-b mock|ps5] [-p port] [-a addr] [--pid N] "
            "[-v]\n"
            "  -b mock   mock memory backend (default, host testing)\n"
            "  -b ps5    kstuffSharp /dev/kstuff0 ioctl backend\n"
            "  -p port   TCP port (default %d)\n"
            "  -a addr   listen address (default 127.0.0.1)\n"
            "  --pid N   attach to process N at startup\n",
            argv0, GDB_DEFAULT_PORT);
}

int main(int argc, char **argv)
{
    const char *backend = "mock";
    const char *addr = NULL;
    const char *dev = NULL;
    int port = GDB_DEFAULT_PORT;
    uint64_t pid = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-b") && i + 1 < argc)
            backend = argv[++i];
        else if (!strcmp(argv[i], "-p") && i + 1 < argc)
            port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-a") && i + 1 < argc)
            addr = argv[++i];
        else if (!strcmp(argv[i], "--pid") && i + 1 < argc)
            pid = strtoull(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--dev") && i + 1 < argc)
            dev = argv[++i];
        else if (!strcmp(argv[i], "-v"))
            ; /* verbose is the default (stderr log per packet) */
        else {
            usage(argv[0]);
            return 2;
        }
    }

    const struct gdb_backend *be;
    if (!strcmp(backend, "mock"))
        be = mock_backend();
    else if (!strcmp(backend, "ps5"))
        be = ps5_backend(dev);
    else {
        usage(argv[0]);
        return 2;
    }

    if (pid && be->attach && be->attach(be->ctx, pid) != 0) {
        fprintf(stderr, "gdbstub: attach to pid %llu failed\n",
                (unsigned long long)pid);
        return 1;
    }

    return gdbstub_serve(be, addr, port) == 0 ? 0 : 1;
}
