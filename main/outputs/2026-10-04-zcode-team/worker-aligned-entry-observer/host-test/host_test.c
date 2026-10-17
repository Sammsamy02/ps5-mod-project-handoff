/* Host test for the observer revision logic. Runs on the build host (macOS)
   against the SAME source/probe/observer.c that ships in the PRX. No console
   code, no target execution. Shim seams:
     - test_failing_pread forces the read()-drain fallback path.
     - real temp files exercise stat/pread/read/open/close bounds. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../source/probe/observer.h"

/* Drain-fallback variant of observer_read_tail: shim_drain.c is the same
   observer.c compiled with pread shimmed to fail, all symbols renamed. */
int obs_drain_read_tail(const char *path, int64_t file_size, unsigned char *buf,
                        size_t cap, size_t *len_out, size_t drain_limit);

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("FAIL %s:%d %s\n", __func__, __LINE__, #cond); \
        ++failures; \
    } \
} while (0)

static void test_classify(void)
{
    CHECK(observer_classify(-1, 0) == OBSERVER_SAME);   /* no baseline */
    CHECK(observer_classify(5, 5) == OBSERVER_SAME);
    CHECK(observer_classify(5, 6) == OBSERVER_GREW);
    CHECK(observer_classify(6, 5) == OBSERVER_SHRANK);
    CHECK(observer_classify(0, 1) == OBSERVER_GREW);    /* 0-byte -> 1 byte */
}

static void try_line(const char *input, size_t len, int window_at_start,
                     const char *expected, int expect_truncated)
{
    char out[64];
    int truncated = -1;
    size_t n = observer_last_line((const unsigned char *)input, len,
                                  window_at_start, out, sizeof(out), &truncated);
    if (strcmp(out, expected) != 0 || truncated != expect_truncated) {
        printf("FAIL line: in=\"%.32s\" got=\"%s\" (len %zu, trunc %d), want=\"%s\" (trunc %d)\n",
               input ? input : "", out, n, truncated, expected, expect_truncated);
        ++failures;
    }
}

static void test_last_line(void)
{
    try_line("hello\nworld", 11, 0, "world", 0);          /* no trailing NL */
    try_line("hello\nworld\n", 12, 0, "world", 0);        /* complete line */
    try_line("hello\nworld\r\n", 13, 0, "world", 0);      /* CRLF terminator */
    try_line("alpha\n\n", 7, 0, "", 0);                   /* last line empty */
    try_line("\n", 1, 1, "", 0);                          /* bare newline */
    try_line("a\r\nb", 4, 0, "b", 0);                     /* CR dropped */
    try_line("abc", 3, 1, "abc", 0);                      /* whole file, no NL */
    try_line("abc", 3, 0, "abc", 1);                      /* window fragment */
    try_line("a\01b\xc3\xa9" "c", 6, 1, "a?b??c", 0);     /* non-printables */
    try_line("", 0, 0, "", 0);                            /* empty window */
    /* cap truncation: out capacity 4 -> at most 3 chars + NUL */
    char out[4];
    int truncated = 0;
    size_t n = observer_last_line((const unsigned char *)"abcdef", 6, 1,
                                  out, sizeof(out), &truncated);
    CHECK(n == 3 && truncated == 1 && strcmp(out, "abc") == 0);
}

static char dir_template[] = "/tmp/observer-host-test-a-XXXXXX";
static char dir_template2[] = "/tmp/observer-host-test-b-XXXXXX";

static void test_stat_and_exists(void)
{
    char *dir = mkdtemp(dir_template);
    CHECK(dir != NULL);
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/UE4SS.log", dir);
    FILE *f = fopen(file_path, "wb");
    CHECK(f != NULL);
    fputs("first\nsecond\n", f);
    fclose(f);

    int64_t size = -1;
    int is_dir = -1;
    CHECK(observer_stat_path(file_path, &size, &is_dir) == 0);
    CHECK(size == 13 && is_dir == 0);
    CHECK(observer_stat_path(dir, &size, &is_dir) == 0);
    CHECK(is_dir == 1);

    int exists_dir = -1;
    CHECK(observer_check_exists(file_path, &exists_dir) == 1 && exists_dir == 0);
    CHECK(observer_check_exists(dir, &exists_dir) == 1 && exists_dir == 1);
    CHECK(observer_check_exists("/tmp/observer-host-test-missing-path", NULL) == 0);
    CHECK(observer_stat_path("/tmp/observer-host-test-missing-path", &size, NULL) == -1);
}

static void write_file(const char *path, size_t bytes, unsigned seed)
{
    FILE *f = fopen(path, "wb");
    assert(f != NULL);
    for (size_t i = 0; i < bytes; ++i)
        fputc((int)(('a' + (seed + i) % 26)), f);
    fclose(f);
}

static void test_read_tail(const char *dir)
{
    char path[256];
    unsigned char buf[512];
    size_t len = (size_t)-1;

    /* Smaller than cap. */
    snprintf(path, sizeof(path), "%s/small.log", dir);
    write_file(path, 10, 0);
    CHECK(observer_read_tail(path, 10, buf, sizeof(buf), &len, 1u << 20)
          == OBSERVER_TAIL_OK);
    CHECK(len == 10 && memcmp(buf, "abcdefghij", 10) == 0);

    /* Larger than cap: exactly the last cap bytes. */
    snprintf(path, sizeof(path), "%s/big.log", dir);
    write_file(path, 600, 1);
    CHECK(observer_read_tail(path, 600, buf, sizeof(buf), &len, 1u << 20)
          == OBSERVER_TAIL_OK);
    CHECK(len == 512);
    CHECK(buf[0] == (unsigned char)('a' + (1 + 600 - 512) % 26));
    CHECK(buf[511] == (unsigned char)('a' + (1 + 599) % 26));

    /* Exactly cap bytes. */
    snprintf(path, sizeof(path), "%s/exact.log", dir);
    write_file(path, 512, 2);
    CHECK(observer_read_tail(path, 512, buf, sizeof(buf), &len, 1u << 20)
          == OBSERVER_TAIL_OK);
    CHECK(len == 512);

    /* Missing file. */
    CHECK(observer_read_tail("/tmp/observer-host-test-missing", 5, buf,
                             sizeof(buf), &len, 1u << 20) == OBSERVER_TAIL_ERROR);

    /* Drain fallback (pread shimmed to fail), file within drain cap. */
    snprintf(path, sizeof(path), "%s/drain.log", dir);
    write_file(path, 600, 3);
    CHECK(obs_drain_read_tail(path, 600, buf, sizeof(buf), &len, 1u << 20)
          == OBSERVER_TAIL_OK);
    CHECK(len == 512);
    CHECK(buf[511] == (unsigned char)('a' + (3 + 599) % 26));

    /* Drain fallback hitting the cap. */
    snprintf(path, sizeof(path), "%s/huge.log", dir);
    write_file(path, (2u << 20), 4);
    CHECK(obs_drain_read_tail(path, (2u << 20), buf, sizeof(buf), &len, 1024)
          == OBSERVER_TAIL_DRAIN_CAPPED);
}

static void test_growth_sequence(const char *dir)
{
    /* End-to-end logic sequence: a fake UE4SS.log grows between polls. */
    char path[256];
    snprintf(path, sizeof(path), "%s/seq.log", dir);
    FILE *f = fopen(path, "wb");
    assert(f != NULL);
    fclose(f);
    int64_t baseline = -1;
    int64_t sizes[5];
    struct stat info;
    enum observer_growth seen[5];
    for (int i = 0; i < 5; ++i) {
        if (i == 2) { /* one poll with no growth */
        } else {
            f = fopen(path, "ab");
            assert(f != NULL);
            fputs(i == 4 ? "[UE4SS] ready\n" : "[UE4SS] boot step\n", f);
            fclose(f);
        }
        assert(stat(path, &info) == 0);
        sizes[i] = (int64_t)info.st_size;
        seen[i] = observer_classify(baseline, sizes[i]);
        if (seen[i] == OBSERVER_GREW || baseline < 0)
            baseline = sizes[i];
    }
    CHECK(seen[0] == OBSERVER_SAME);                           /* baseline */
    CHECK(seen[1] == OBSERVER_GREW);
    CHECK(seen[2] == OBSERVER_SAME);                           /* quiet poll */
    CHECK(seen[3] == OBSERVER_GREW);
    CHECK(seen[4] == OBSERVER_GREW);
    /* Last-line read of the final state reports the ready line. */
    unsigned char buf[512];
    size_t len = 0;
    CHECK(observer_read_tail(path, sizes[4], buf, sizeof(buf), &len, 1u << 20)
          == OBSERVER_TAIL_OK);
    char out[64];
    int truncated = -1;
    observer_last_line(buf, len, sizes[4] <= 512, out, sizeof(out), &truncated);
    CHECK(strcmp(out, "[UE4SS] ready") == 0 && truncated == 0);
}

int main(void)
{
    test_classify();
    test_last_line();
    test_stat_and_exists();
    char *dir = mkdtemp(dir_template2);
    if (dir == NULL) {
        perror("mkdtemp");
        return 2;
    }
    test_read_tail(dir);
    test_growth_sequence(dir);
    if (failures == 0) {
        printf("HOST_TEST_PASSED all observer logic cases\n");
        return 0;
    }
    printf("HOST_TEST_FAILED cases=%d\n", failures);
    return 1;
}
