#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char* SystemNative_GetLine(FILE* stream);
static atomic_int liveAllocations;
static atomic_int failMalloc;
static int failGetlineWithAllocation;

void* cap10_malloc(size_t n)
{
    if (atomic_exchange(&failMalloc, 0))
    {
        errno = ENOMEM;
        return NULL;
    }
    void* p = malloc(n);
    if (p != NULL)
        atomic_fetch_add(&liveAllocations, 1);
    return p;
}

void cap10_free(void* p)
{
    if (p != NULL)
        atomic_fetch_sub(&liveAllocations, 1);
    free(p);
    // Deliberately expose accidental errno loss after allocation cleanup.
    errno = ERANGE;
}

ssize_t cap10_getline(char** line, size_t* capacity, FILE* stream)
{
    if (failGetlineWithAllocation)
    {
        *line = malloc(32);
        assert(*line != NULL);
        *capacity = 32;
        atomic_fetch_add(&liveAllocations, 1);
        errno = EIO;
        return -1;
    }
    ssize_t result = getline(line, capacity, stream);
    if (*line != NULL)
        atomic_fetch_add(&liveAllocations, 1);
    return result;
}

static FILE* input(const void* data, size_t length)
{
    FILE* file = tmpfile();
    assert(file != NULL);
    assert(fwrite(data, 1, length, file) == length);
    rewind(file);
    return file;
}

static void expectLine(FILE* file, const void* expected, size_t length)
{
    char* result = SystemNative_GetLine(file);
    assert(result != NULL);
    assert(memcmp(result, expected, length) == 0);
    assert(result[length] == '\0');
    cap10_free(result);
}

static void contentAndOwnership(void)
{
    FILE* file = input("first\nsecond\nlast", 17);
    char* first = SystemNative_GetLine(file);
    assert(first != NULL && strcmp(first, "first\n") == 0);
    expectLine(file, "second\n", 7);
    // A subsequent read must not invalidate the malloc-owned first result.
    assert(strcmp(first, "first\n") == 0);
    expectLine(file, "last", 4);
    assert(SystemNative_GetLine(file) == NULL);
    assert(feof(file));
    cap10_free(first);
    assert(fclose(file) == 0);
    file = input("\n", 1);
    expectLine(file, "\n", 1);
    assert(SystemNative_GetLine(file) == NULL);
    assert(fclose(file) == 0);
    file = input("", 0);
    assert(SystemNative_GetLine(file) == NULL);
    assert(feof(file));
    assert(fclose(file) == 0);
}

static void longAndBinary(void)
{
    size_t length = (size_t)BUFSIZ * 4 + 37;
    char* data = malloc(length);
    assert(data != NULL);
    memset(data, 'x', length);
    data[length - 1] = '\n';
    FILE* file = input(data, length);
    expectLine(file, data, length);
    assert(SystemNative_GetLine(file) == NULL);
    assert(fclose(file) == 0);
    free(data);
    const char binary[] = {'a', '\0', 'b', '\n', 'c'};
    file = input(binary, sizeof(binary));
    expectLine(file, binary, 4);
    expectLine(file, "c", 1);
    assert(fclose(file) == 0);
}

static void actualReadError(void)
{
    FILE* file = tmpfile();
    assert(file != NULL);
    assert(close(fileno(file)) == 0);
    errno = 0;
    assert(SystemNative_GetLine(file) == NULL);
    assert(ferror(file));
    assert(errno == EBADF);
    assert(fclose(file) == EOF);
}

static void failureOwnership(void)
{
    FILE* file = input("one\ntwo\n", 8);
#if TEST_FGETLN_ROUTE
    atomic_store(&failMalloc, 1);
    errno = 0;
    assert(SystemNative_GetLine(file) == NULL);
    assert(errno == ENOMEM);
    assert(atomic_load(&liveAllocations) == 0);
    // Allocation failure must release the stream lock for later reads.
    expectLine(file, "two\n", 4);
#else
    failGetlineWithAllocation = 1;
    errno = 0;
    assert(SystemNative_GetLine(file) == NULL);
    assert(errno == EIO);
    assert(atomic_load(&liveAllocations) == 0);
    failGetlineWithAllocation = 0;
    expectLine(file, "one\n", 4);
#endif
    assert(fclose(file) == 0);
}

static atomic_int concurrentLines;
static void* reader(void* arg)
{
    FILE* file = arg;
    char* line;
    while ((line = SystemNative_GetLine(file)) != NULL)
    {
        assert(strlen(line) == 1025);
        for (size_t i = 1; i < 1024; ++i)
            assert(line[i] == line[0]);
        assert(line[1024] == '\n');
        atomic_fetch_add(&concurrentLines, 1);
        cap10_free(line);
    }
    return NULL;
}

static void concurrentOwnership(void)
{
    FILE* file = tmpfile();
    assert(file != NULL);
    char line[1025];
    for (int i = 0; i < 200; ++i)
    {
        memset(line, i % 2 ? 'a' : 'b', 1024);
        line[1024] = '\n';
        assert(fwrite(line, 1, sizeof(line), file) == sizeof(line));
    }
    rewind(file);
    pthread_t one, two;
    assert(pthread_create(&one, NULL, reader, file) == 0);
    assert(pthread_create(&two, NULL, reader, file) == 0);
    assert(pthread_join(one, NULL) == 0);
    assert(pthread_join(two, NULL) == 0);
    assert(atomic_load(&concurrentLines) == 200);
    assert(fclose(file) == 0);
}

int main(void)
{
    contentAndOwnership();
    longAndBinary();
    actualReadError();
    failureOwnership();
    concurrentOwnership();
    assert(atomic_load(&liveAllocations) == 0);
    puts("PASS: content, EOF, long/binary lines, independent ownership, actual read error, allocation failure, 200 concurrent lines; zero live returned allocations");
    return 0;
}
