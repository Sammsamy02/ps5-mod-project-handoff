#define _POSIX_C_SOURCE 200809L
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif
#include "prepare_union.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Exercise the borrowed-descriptor contract with authored fixtures. */
static unsigned checks;
static void require(int ok, const char *message) {
    ++checks;
    if (!ok) { fprintf(stderr, "%s\n", message); exit(1); }
}
static uint64_t bytes(int fd) {
    struct stat st;
    require(fstat(fd, &st) == 0 && st.st_size >= 0, "stat failed");
    return (uint64_t)st.st_size;
}
static int fresh(int dir, const char *name, int flags) {
    int fd = openat(dir, name, flags | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    require(fd >= 0, "exclusive output creation failed");
    return fd;
}
static void positions(const mb_pak_union_params_v1 *p) {
    require(fcntl(p->base_fd, F_GETFD) >= 0 && fcntl(p->mod_fd, F_GETFD) >= 0 &&
            fcntl(p->output_fd, F_GETFD) >= 0, "borrowed FD was closed");
    require(lseek(p->base_fd, 0, SEEK_CUR) == 7 &&
            lseek(p->mod_fd, 0, SEEK_CUR) == 8 &&
            lseek(p->output_fd, 0, SEEK_CUR) == 9, "borrowed FD cursor changed");
}
static void position(const mb_pak_union_params_v1 *p) {
    require(lseek(p->base_fd, 7, SEEK_SET) == 7 &&
            lseek(p->mod_fd, 8, SEEK_SET) == 8 &&
            lseek(p->output_fd, 9, SEEK_SET) == 9, "seek failed");
}
static void refusal(mb_pak_union_params_v1 *p, const char *label) {
    mb_pak_union_result_v1 r;
    uint64_t output_before = bytes(p->output_fd);
    position(p);
    memset(&r, 0, sizeof(r));
    require(mb_pak_union_prepare_v1(p, &r) != MB_PAK_UNION_OK, label);
    require(!r.archive_complete, "refusal claimed archive completion");
    require(bytes(p->output_fd) == output_before, "refusal modified output length");
    positions(p);
}
int main(int argc, char **argv) {
    mb_pak_union_params_v1 p;
    mb_pak_union_result_v1 r;
    int dir, base_rw, original_output, original_base;
    uint64_t base_size, mod_size;
    if (argc != 4) return 2;
    memset(&p, 0, sizeof(p));
    p.struct_size = sizeof(p); p.abi_version = MB_PAK_UNION_ABI_V1;
    p.immutable_inputs = 1; p.unpublished_output = 1;
    p.base_fd = open(argv[1], O_RDONLY | O_NOFOLLOW);
    p.mod_fd = open(argv[2], O_RDONLY | O_NOFOLLOW);
    require(p.base_fd >= 0 && p.mod_fd >= 0, "input open failed");
    require(mkdir(argv[3], 0700) == 0, "fresh directory required");
    dir = open(argv[3], O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    require(dir >= 0, "directory open failed");
    p.base_bytes = base_size = bytes(p.base_fd);
    p.mod_bytes = mod_size = bytes(p.mod_fd);
    p.output_fd = fresh(dir, "success.pak", O_RDWR);
    position(&p);
    require(mb_pak_union_prepare_v1(&p, &r) == MB_PAK_UNION_OK && r.archive_complete,
            "valid borrowed-FD preparation failed");
    positions(&p);
    require(bytes(p.output_fd) == r.archive_bytes && r.allocation_live_after_cleanup == 0,
            "invalid completed size or leaked allocation");
    require(close(p.output_fd) == 0, "output close failed");
    p.output_fd = fresh(dir, "refused.pak", O_RDWR);
    ++p.base_bytes;
    refusal(&p, "declared-length mismatch admitted");
    --p.base_bytes;
    p.immutable_inputs = 0;
    refusal(&p, "missing immutable assertion admitted");
    p.immutable_inputs = 1;
    base_rw = open(argv[1], O_RDWR | O_NOFOLLOW);
    require(base_rw >= 0, "writable source control open failed");
    original_base = p.base_fd; p.base_fd = base_rw;
    refusal(&p, "writable source admitted");
    p.base_fd = original_base;
    require(close(base_rw) == 0, "writable source close failed");
    original_output = p.output_fd;
    p.output_fd = fresh(dir, "append.pak", O_RDWR | O_APPEND);
    refusal(&p, "append output admitted");
    require(close(p.output_fd) == 0, "append close failed");
    p.output_fd = original_output;
    require(pwrite(p.output_fd, "keep", 4, 0) == 4, "dirty output setup failed");
    refusal(&p, "nonempty output admitted");
    {
        char sentinel[4];
        require(pread(p.output_fd, sentinel, sizeof(sentinel), 0) == 4 &&
                memcmp(sentinel, "keep", 4) == 0, "refusal changed existing bytes");
    }
    require(bytes(p.base_fd) == base_size && bytes(p.mod_fd) == mod_size,
            "input length changed");
    require(close(p.output_fd) == 0 && close(p.mod_fd) == 0 && close(p.base_fd) == 0 &&
            close(dir) == 0, "caller cleanup failed");
    printf("{\"status\":\"pass\",\"checks\":%u,\"borrowed_fds_preserved\":true,"
           "\"cursors_preserved\":true,\"console_actions\":false}\n", checks);
    return 0;
}
