#define _DARWIN_C_SOURCE 1
#define _GNU_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "../private/posix_compat.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static struct mb_archive_posix_raw_result response;
static enum mb_archive_posix_call expected_call;
static unsigned calls;
static int checks;
static int failures;

#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

struct mb_archive_posix_raw_result
mb_archive_posix_test_call(enum mb_archive_posix_call call, long argument1,
                           long argument2, long argument3, long argument4,
                           long argument5) {
    (void)argument1;
    (void)argument2;
    (void)argument3;
    (void)argument4;
    (void)argument5;
    ++calls;
    if (call != expected_call) {
        fprintf(stderr, "FAIL unexpected syscall adapter kind %d\n", (int)call);
        ++failures;
    }
    return response;
}

static void set_response(enum mb_archive_posix_call call, long value,
                         unsigned char carry) {
    expected_call = call;
    response.value = value;
    response.carry = carry;
    calls = 0;
    errno = 0;
}

static void test_clear_carry_fd17(void) {
    set_response(MB_ARCHIVE_POSIX_OPENAT, 17, 0);
    CHECK(mb_archive_posix_openat(42, "member", O_RDONLY, 0) == 17);
    CHECK(calls == 1);
}

static void test_carry_openat_eexist17(void) {
    /* EEXIST is 17 on the host too: the carry bit distinguishes errno from
     * a successful open returning descriptor 17. */
    set_response(MB_ARCHIVE_POSIX_OPENAT, EEXIST, 1);
    CHECK(mb_archive_posix_openat(42, "member", O_RDONLY, 0) == -1);
    CHECK(errno == EEXIST);
    CHECK(calls == 1);
}

static void test_carry_fstatat_enoent(void) {
    struct stat st;
    set_response(MB_ARCHIVE_POSIX_FSTATAT, ENOENT, 1);
    CHECK(mb_archive_posix_fstatat(42, "member", &st,
                                   AT_SYMLINK_NOFOLLOW) == -1);
    CHECK(errno == ENOENT);
    CHECK(calls == 1);
}

static void test_carry_fstatat_eacces(void) {
    struct stat st;
    set_response(MB_ARCHIVE_POSIX_FSTATAT, EACCES, 1);
    CHECK(mb_archive_posix_fstatat(42, "member", &st,
                                   AT_SYMLINK_NOFOLLOW) == -1);
    CHECK(errno == EACCES);
    CHECK(calls == 1);
}

int main(void) {
    test_clear_carry_fd17();
    test_carry_openat_eexist17();
    test_carry_fstatat_enoent();
    test_carry_fstatat_eacces();
    printf("checks=%d failures=%d cases=4\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
