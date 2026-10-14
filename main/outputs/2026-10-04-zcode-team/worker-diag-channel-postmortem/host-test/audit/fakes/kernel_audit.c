/* kernel_audit.c — host fake of the three proven kernel import bindings for
 * the DELIVERED-entry audit (worker-module-start-wiring/runtime_module_entry.c,
 * sha256 1c041f8b...). Same /data translation as the wiring worker's
 * kernel_host.c, plus MRS_UE4SS_REFUSE to force the UE4SS.log append to fail
 * (audit scenario a7: does the second sink still fire?). */
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

static char *translate(const char *path)
{
    if (strncmp(path, "/data/", 6) != 0)
        return strdup(path);
    const char *root = data_root();
    size_t root_len = strlen(root);
    char *out = malloc(root_len + strlen(path) + 1);
    if (out == NULL)
        return NULL;
    memcpy(out, root, root_len);
    strcpy(out + root_len, path + strlen("/data"));
    return out;
}

int sceKernelOpen(const char *path, int flags, uint16_t mode)
{
    const char *refuse = getenv("MRS_UE4SS_REFUSE");
    if (refuse != NULL && atoi(refuse) != 0 &&
        strcmp(path, "/data/plugins/UE4SS.log") == 0)
        return -13; /* simulate EACCES on the failover log */
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
