/*
 * ps5_io_dir.c — see ps5_io_dir.h for the binding rationale and receipts.
 *
 * Origin: worker-installer-composition, 2026-10-04 (owned).
 * One implementation for both flavors: host (PS5IO_HOST test builds) and
 * console (pinned payload SDK). Imports are only opendir/readdir/closedir
 * (libc, nm-verified), rmdir (libkernel, nm-verified), strlen/strcmp/memcpy
 * (libc) and ps5io_unlink/ps5io_join (the vendored ps5_io sceKernel set on
 * console / POSIX on host). No other symbol is referenced.
 */
#include "ps5_io_dir.h"

#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>             /* rmdir */

#include "ps5_io.h"          /* vendored, unmodified (worker-installer-ps5io) */

int ps5io_dir_rm_rf(const char *path) {
    DIR *d = opendir(path);
    if (!d) {
        /* not a directory (or absent): unlink through the pinned binding.
         * Absent counts as gone — worker-7's host rm_rf semantics. */
        int rc = ps5io_unlink(path);
        return (rc == 0 || rc == -ENOENT) ? 0 : -1;
    } else {
        struct dirent *e;
        int rc = 0;
        while ((e = readdir(d))) {
            char child[PS5IO_MAX_PATH];
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            if (ps5io_join(child, sizeof child, path, e->d_name)) { rc = -1; continue; }
            if (ps5io_dir_rm_rf(child)) rc = -1;
        }
        closedir(d);
        if (rmdir(path)) rc = -1;
        return rc;
    }
}

int ps5io_dir_each_json(const char *dir, void (*fn)(const char *title)) {
    DIR *d = opendir(dir);
    struct dirent *e;
    int called = 0;
    if (!d) return -1;
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        char title[64];
        if (n <= 5 || strcmp(e->d_name + n - 5, ".json")) continue;
        n -= 5;
        if (n >= sizeof title) continue;
        memcpy(title, e->d_name, n);
        title[n] = 0;
        fn(title);
        called++;
    }
    closedir(d);
    return called;
}
