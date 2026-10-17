#include <stddef.h>
#include <stdint.h>
#include <sys/fcntl.h>

#include "probe_config.h"

/* Signatures match SharpProspero's pinned Interop/Kernel/KernelFile.cs. */
extern int sceKernelOpen(const char *path, int flags, uint16_t mode);
extern int64_t sceKernelWrite(int descriptor, const void *buffer, size_t length);
extern int sceKernelClose(int descriptor);

/* Own status only: 1 = not run, 0 = appended, 2/3/4 = open/write/close failed. */
static int probe_result = 1;

__attribute__((visibility("default")))
int modbridge_probe_result(void)
{
    return probe_result;
}

static void probe_append(const char *line, size_t length)
{
    size_t offset = 0;
    int result = 0;
    int descriptor = sceKernelOpen(MODBRIDGE_PROBE_LOG,
                                  O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW,
                                  0600);
    if (descriptor < 0) {
        probe_result = 2;
        return;
    }

    while (offset < length) {
        int64_t written = sceKernelWrite(descriptor, line + offset,
                                        length - offset);
        if (written <= 0 || (uint64_t)written > length - offset) {
            result = 3;
            break;
        }
        offset += (size_t)written;
    }
    if (sceKernelClose(descriptor) < 0 && result == 0)
        result = 4;
    probe_result = result;
}

static void modbridge_probe_init_array(void)
{
    static const char line[] = MODBRIDGE_PROBE_INIT_LINE;
    probe_append(line, sizeof(line) - 1);
}

__attribute__((visibility("default")))
int module_start(size_t argument_size, const void *arguments)
{
    static const char line[] = MODBRIDGE_PROBE_START_LINE;
    (void)argument_size;
    (void)arguments;
    probe_append(line, sizeof(line) - 1);
    return 0;
}

/* The PS5 compiler's constructor attribute emits .ctors. This PRX writer
 * supports DT_INIT_ARRAY instead, so retain exactly one explicit entry. */
__attribute__((used, section(".init_array")))
static void (*const probe_init_entry)(void) = modbridge_probe_init_array;
