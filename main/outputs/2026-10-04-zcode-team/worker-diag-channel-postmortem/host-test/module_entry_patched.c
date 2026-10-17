#include "lifecycle.h"
#include "../probe/probe_api.h"
#include "../diagnostic.h"
#include <stdint.h>
#include <sys/fcntl.h>

/* Fail-open entry diagnostics (composition-5): the three frozen bootstrap
   imports, declared exactly as in the pinned diagnostic.c implementation. */
extern int sceKernelOpen(const char *, int, uint16_t);
extern int64_t sceKernelWrite(int, const void *, size_t);
extern int sceKernelClose(int);

extern probe_callback __init_array_start[], __init_array_end[];
extern probe_callback __fini_array_start[], __fini_array_end[];
extern unsigned char __eh_frame_start[], __eh_frame_end[];
extern unsigned char __eh_frame_hdr_start[], __eh_frame_hdr_end[];
/* The producer defines this symbol at the image base. Use its address, as the
   compiler does in __cxa_atexit calls; do not read ELF-header bytes as a value. */
extern unsigned char __dso_handle;
extern void __cxa_finalize(void *);

static struct probe_lifecycle lifecycle = PROBE_LIFECYCLE_INIT;
static unsigned char entry_guard;
static unsigned constructor_index;

/* ---- fail-open entry diagnostics (composition-5) ----------------------
   module_start must never return -20/-21/-22 without leaving exactly one
   line in /data/plugins/UE4SS.log (O_WRONLY|O_CREAT|O_APPEND, 0644 — the
   exact flag set proven writable in-game by the modbridge_path_test
   receipt "PATH_TEST: /data/plugins/UE4SS.log writable ...").
   C only, fixed stack buffer, no allocation, no variadic formatting. */
#define ENTRY_FAILOVER_PATH "/data/plugins/UE4SS.log"
#define ENTRY_FAILOVER_LINE_CAPACITY 192u

static void failover_append(char *buffer, unsigned *used, unsigned capacity,
                            const char *text)
{
    while (*text && *used < capacity)
        buffer[(*used)++] = *text++;
}

static void failover_append_hex(char *buffer, unsigned *used, unsigned capacity,
                                int value)
{
    static const char digits[] = "0123456789abcdef";
    unsigned v = (unsigned)value;
    failover_append(buffer, used, capacity, "0x");
    for (int shift = 28; shift >= 0; shift -= 4)
        if (*used < capacity)
            buffer[(*used)++] = digits[(v >> shift) & 15u];
}

static void entry_failover_write(const char *reason, int code)
{
    /* Volatile copies, exactly like diagnostic.c:19-22: guards against
       elision and models unresolved-import NULL bindings. */
    int (*volatile open_binding)(const char *, int, uint16_t) = sceKernelOpen;
    int64_t (*volatile write_binding)(int, const void *, size_t) = sceKernelWrite;
    int (*volatile close_binding)(int) = sceKernelClose;
    if (open_binding == NULL || write_binding == NULL || close_binding == NULL)
        return; /* no kernel imports: nothing can be written at all */
    char line[ENTRY_FAILOVER_LINE_CAPACITY];
    unsigned used = 0;
    unsigned limit = (unsigned)sizeof(line) - 1; /* last byte reserved for '\n' */
    /* Reason stage reserves 15 bytes for " rc=0x????????\n". */
    failover_append(line, &used, limit - 15, "modbridge-entry-failover/v1 ");
    failover_append(line, &used, limit - 15, reason);
    failover_append(line, &used, limit, " rc=");
    failover_append_hex(line, &used, limit, code);
    line[used++] = '\n';
    int fd = open_binding(ENTRY_FAILOVER_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
        return; /* last-resort channel refused: no further fallback exists */
    size_t offset = 0;
    while (offset < used) {
        int64_t written = write_binding(fd, line + offset, used - offset);
        if (written <= 0 || (size_t)written > used - offset)
            break;
        offset += (size_t)written;
    }
    (void)close_binding(fd);
}

static int invoke_constructor(probe_callback callback)
{
    unsigned index = constructor_index++;
    diagnostic_event("constructor_begin", index);
    int result = probe_call_constructor_catching(callback);
    diagnostic_event("constructor_return", (uint32_t)result);
    return result;
}

static void finalize_dso(void *handle)
{
    diagnostic_event("dso_finalize_begin", 0);
    __cxa_finalize(handle);
    diagnostic_event("dso_finalize_complete", 0);
}

static const struct probe_lifecycle_ops ops = {
    probe_runtime_validate_and_preload, probe_runtime_log_status,
    probe_worker_start, probe_worker_quiesce_join, probe_worker_is_current,
    invoke_constructor, finalize_dso, &__dso_handle
};

static int leave_entry(int result)
{
    __atomic_clear(&entry_guard, __ATOMIC_RELEASE);
    return result;
}

__attribute__((visibility("default"), force_align_arg_pointer))
int module_start(size_t argc, const void *args)
{
    (void)argc; (void)args;
    if (__atomic_test_and_set(&entry_guard, __ATOMIC_ACQUIRE)) {
        entry_failover_write("entry_guard_busy", -20);
        return -20;
    }
    if (lifecycle.state == PROBE_ACTIVE)
        return leave_entry(0);
    if (lifecycle.state != PROBE_NEW) {
        entry_failover_write("wrong_state", -21);
        return leave_entry(-21);
    }
    if (!diagnostic_ready()) {
        entry_failover_write("diag_bindings_null", -22);
        return leave_entry(-22);
    }
    int channel = diagnostic_open_channel();
    if (channel < 0) {
        entry_failover_write("channel_open_failed", channel);
        return leave_entry(-22);
    }
    diagnostic_event("module_start_entered", 0);
    /* Record only low alignment bits of our own frame. No caller stack data
       is dereferenced; the compiler restores the original stack on return. */
    uintptr_t incoming_sp = (uintptr_t)__builtin_frame_address(0) + sizeof(void *);
    uintptr_t current_sp;
    __asm__ volatile ("mov %%rsp, %0" : "=r"(current_sp));
    diagnostic_event("entry_incoming_sp_mod16", (uint32_t)(incoming_sp & 15u));
    diagnostic_event("entry_call_sp_mod16", (uint32_t)(current_sp & 15u));
    diagnostic_event("constructor_count", (uint32_t)(
        ((uintptr_t)__init_array_end - (uintptr_t)__init_array_start) / sizeof(probe_callback)));
    int result = probe_lifecycle_start(&lifecycle, &ops,
        __init_array_start, __init_array_end, __fini_array_start, __fini_array_end,
        __eh_frame_start, __eh_frame_end, __eh_frame_hdr_start, __eh_frame_hdr_end);
    diagnostic_event("module_start_return", (uint32_t)result);
    if (result != 0 && lifecycle.state == PROBE_FINISHED) {
        probe_runtime_release_input();
        (void)diagnostic_close_channel();
    }
    return leave_entry(result);
}

__attribute__((visibility("default"), force_align_arg_pointer))
int module_stop(size_t argc, const void *args)
{
    (void)argc; (void)args;
    if (__atomic_test_and_set(&entry_guard, __ATOMIC_ACQUIRE))
        return -20;
    int result = probe_lifecycle_stop(&lifecycle, &ops);
    diagnostic_event("module_stop_return", (uint32_t)result);
    if (result == 0 && lifecycle.state == PROBE_FINISHED) {
        probe_runtime_release_input();
        (void)diagnostic_close_channel();
    }
    return leave_entry(result);
}

/* Source-level markers in the same FProperty initializer; no initialization skipped. */
__attribute__((visibility("hidden")))
void modbridge_ctor_phase(unsigned phase)
{
    diagnostic_event("fproperty_phase", phase);
}

__attribute__((visibility("hidden")))
void modbridge_ctor_value(unsigned key, unsigned long long value)
{
    diagnostic_event("ffield_trace_key", key);
    diagnostic_event("ffield_value_low", (uint32_t)value);
    diagnostic_event("ffield_value_high", (uint32_t)(value >> 32));
}
