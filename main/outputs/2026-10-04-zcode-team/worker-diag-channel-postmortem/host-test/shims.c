/* shims.c — host stand-ins for the composition-5 fail-open host test.
 *
 * Provides: sceKernelOpen/Write/Close (with /data path translation and
 * failure injection), the diagnostic.h sink (controllable failure), the
 * probe_api/lifecycle externs, and the ELF boundary symbols module_entry.c
 * references. Compiled for x86_64 and run under Rosetta; the file under test
 * (module_entry_patched.c) is byte-exact, unmodified.
 */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "lifecycle.h"
#include "diagnostic_shim.h"

/* ---------------- failure-injection state (set by test_main) ------------- */
int g_kernel_failover_open_result;   /* 0 = succeed; else forced open rc for UE4SS.log */
int g_diag_ready;                    /* mirrors diagnostic_ready() */
int g_diag_open_result;              /* 0 = open real temp channel; <0 = forced fail */

char g_failover_log_path[512];
char g_diag_channel_path[512];
static int diag_fd = -1;

/* ---------------- kernel import shims ------------------------------------ */
static const char *map_path(const char *path)
{
    if (strcmp(path, "/data/plugins/UE4SS.log") == 0)
        return g_failover_log_path;
    if (strcmp(path, "/data/townfall_worker_input.lua") == 0)
        return g_diag_channel_path; /* any existing file; unused by scenarios */
    return path;
}

int sceKernelOpen(const char *path, int flags, uint16_t mode)
{
    if (strcmp(path, "/data/plugins/UE4SS.log") == 0 && g_kernel_failover_open_result != 0)
        return g_kernel_failover_open_result;
    fprintf(stderr, "SHIM-OPEN path=%s flags=0x%x mode=%u -> ", path, flags, mode); int r = open(map_path(path), flags, (mode_t)mode); fprintf(stderr, "r=%d\n", r); return r;
}

int64_t sceKernelWrite(int fd, const void *buffer, size_t length)
{
    fprintf(stderr, "SHIM-WRITE fd=%d len=%zu buf0=%.20s\n", fd, length, (const char*)buffer); return (int64_t)write(fd, buffer, length);
}

int sceKernelClose(int fd)
{
    return close(fd);
}

/* ---------------- diagnostic.h sink shim (controllable) ------------------ */
int diagnostic_ready(void)
{
    return g_diag_ready;
}

int diagnostic_open_retained(void)
{
    if (!g_diag_ready)
        return -1;
    return open(g_diag_channel_path, O_WRONLY | O_CREAT | O_APPEND, 0600);
}

int diagnostic_open_channel(void)
{
    if (g_diag_open_result < 0)
        return g_diag_open_result;
    if (diag_fd < 0)
        diag_fd = diagnostic_open_retained();
    return diag_fd;
}

int diagnostic_channel_fd(void)
{
    return diag_fd;
}

int diagnostic_close_channel(void)
{
    if (diag_fd < 0)
        return 0;
    int fd = diag_fd;
    diag_fd = -1;
    return close(fd);
}

int diagnostic_open_data_directory(void)
{
    return -1;
}

int diagnostic_close_retained(int fd)
{
    return fd < 0 ? 0 : close(fd);
}

static void diag_line(int fd, const char *tag, const char *mid, uint32_t value)
{
    if (fd < 0)
        return;
    char line[256];
    int n = snprintf(line, sizeof(line), "[diag-shim] %s%s value=0x%x\n", tag, mid, value);
    if (n > 0)
        (void)!write(fd, line, (size_t)n);
}

void diagnostic_event(const char *tag, uint32_t value)
{
    diag_line(diag_fd, tag, "", value);
}

void diagnostic_missing(unsigned index)
{
    diag_line(diag_fd, "missing_import", "", index);
}

int diagnostic_fd_event(int fd, const char *tag, int32_t result, int32_t error_number)
{
    if (fd < 0)
        return -1;
    char line[256];
    int n = snprintf(line, sizeof(line), "[diag-shim] %s result=0x%x errno=0x%x\n",
                     tag, (unsigned)result, (unsigned)error_number);
    if (n > 0)
        (void)!write(fd, line, (size_t)n);
    return (int)n;
}

int64_t diagnostic_fd_marker(int fd, const char *tag)
{
    if (fd < 0)
        return -1;
    char line[256];
    int n = snprintf(line, sizeof(line), "[diag-shim] %s\n", tag);
    if (n > 0)
        return write(fd, line, (size_t)n);
    return -1;
}

/* ---------------- probe lifecycle stubs (controllable) ------------------- */
int g_stub_start_rc;                 /* probe_lifecycle_start return */
enum probe_lifecycle_state g_stub_final_state = PROBE_ACTIVE;
int g_stub_recursive_reentry;        /* if nonzero, stub re-calls module_start */

int module_start(size_t argc, const void *args);

static struct probe_lifecycle *g_last_lifecycle;

int probe_lifecycle_start(struct probe_lifecycle *lc, const struct probe_lifecycle_ops *ops,
                          probe_callback *init_begin, probe_callback *init_end,
                          probe_callback *fini_begin, probe_callback *fini_end,
                          const void *eh_start, const void *eh_end,
                          const void *eh_hdr_start, const void *eh_hdr_end)
{
    (void)ops; (void)init_begin; (void)init_end; (void)fini_begin; (void)fini_end;
    (void)eh_start; (void)eh_end; (void)eh_hdr_start; (void)eh_hdr_end;
    g_last_lifecycle = lc;
    if (g_stub_recursive_reentry) {
        int inner = module_start(0, NULL);
        printf("shim: recursive re-entry module_start returned %d\n", inner);
    }
    if (lc)
        lc->state = g_stub_final_state;
    return g_stub_start_rc;
}

int probe_lifecycle_stop(struct probe_lifecycle *lc, const struct probe_lifecycle_ops *ops)
{
    (void)ops;
    if (lc)
        lc->state = PROBE_FINISHED;
    return 0;
}

int probe_runtime_validate_and_preload(void) { return 0; }
void probe_runtime_log_status(void) {}
int probe_worker_start(void) { return 0; }
int probe_worker_quiesce_join(void) { return 0; }
int probe_worker_is_current(void) { return 0; }
void probe_runtime_release_input(void) {}
int probe_call_constructor_catching(probe_callback cb) { (void)cb; return 0; }

/* ---------------- ELF boundary symbols ----------------------------------- */
probe_callback __init_array_start[1];
probe_callback __init_array_end[1];
probe_callback __fini_array_start[1];
probe_callback __fini_array_end[1];
unsigned char __eh_frame_start[1];
unsigned char __eh_frame_end[1];
unsigned char __eh_frame_hdr_start[1];
unsigned char __eh_frame_hdr_end[1];
unsigned char __dso_handle;
void __cxa_finalize(void *handle) { (void)handle; }

struct probe_lifecycle *shim_last_lifecycle(void) { return g_last_lifecycle; }
