/* kernel_host.c — host fakes of the three proven kernel import bindings.
 * Redirects every "/data/" path onto $MRS_DATA_ROOT so the entry's hardcoded
 * target paths (/data/plugins/UE4SS.log) land inside the test directory with
 * zero entry-source changes. Flags are passed through (host Linux/macOS flag
 * values differ from the SDK's, so the caller's fake-flags are translated by
 * the individual fake consumers, while kernel_host translates raw
 * O_WRONLY|O_CREAT|O_APPEND|O_EXCL|O_TRUNC by host value). */
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *data_root(void)
{
    const char *r = getenv("MRS_DATA_ROOT");
    return r != NULL ? r : "/tmp/mrs-data";
}

/* Returns a malloc'd translated path; caller frees. */
static char *translate(const char *path)
{
    size_t root_len;
    char *out;
    if (strncmp(path, "/data/", 6) != 0)
        return strdup(path);
    root_len = strlen(data_root());
    out = malloc(root_len + strlen(path) + 1);
    if (out == NULL)
        return NULL;
    memcpy(out, data_root(), root_len);
    strcpy(out + root_len, path + strlen("/data"));
    return out;
}

int sceKernelOpen(const char *path, int flags, uint16_t mode)
{
    char *t = translate(path);
    if (t == NULL)
        return -1;
    int fd = open(t, flags, (mode_t)mode);
    free(t);
    return fd;
}

int64_t sceKernelWrite(int fd, const void *buffer, size_t length)
{
    return (int64_t)write(fd, buffer, length);
}

int sceKernelClose(int fd)
{
    return close(fd);
}
