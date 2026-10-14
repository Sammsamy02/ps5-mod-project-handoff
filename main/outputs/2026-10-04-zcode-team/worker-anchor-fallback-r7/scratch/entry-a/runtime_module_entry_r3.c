/*
 * runtime_module_entry.c — THE previously missing module_start caller.
 *
 * Replaces lifecycle/module_entry.c in the game-bound composition. The probe
 * lifecycle (constructor run, state machine, entry_guard, diagnostics) is
 * preserved byte-for-byte in behavior; on top of a clean constructor run this
 * entry now ACTUALLY STARTS THE COMPOSED RUNTIME:
 *
 *   1. diagnostic channel (unchanged)
 *   2. probe_lifecycle_start: ALL init_array constructors (the UE4SS units,
 *      each exception-caught) + probe worker  (unchanged)
 *   3. mfa_activate(<mod root>) — reads/validates the activation ONCE, the
 *      only moment /data is reachable from the game process
 *   4. modbridge_runtime_start_with_activation():
 *        lmf capture (VMMAP-first) -> TryMakeNativeLoaderFactsFromSupplier
 *        -> MakeNativeProgramContext
 *        -> StartActivatedRuntimeWithAck
 *             = InstallDefaultEngineBindings()
 *             + MakeRuntimeAckConfig(status_root = the EXACT mod root,
 *               pid = getpid(), now_ms = CLOCK_MONOTONIC ms)
 *             + PrepareActivatedUE4SSRuntime(context, activation, title,
 *               runtime_ack)   [opens the single RuntimeAckSink, builds the
 *               NativeModSource from the activation]
 *             + owner->start() [serialized init thread; returns immediately]
 *   5. module_stop: owner stop (joins the init thread, ack 'stopped') then
 *      probe_lifecycle_stop (probe worker join + fini array + DSO finalize).
 *
 * Observable failure discipline: EVERY failure writes (a) a diagnostic event
 * through the proven channel, (b) one append-only line to the UE4SS.log file
 * the runtime itself uses (<module dir>/UE4SS.log), and (c) best-effort a
 * `failed` line into <mod root>/runtime-status.txt through the vendored
 * mfa_status API (RUNTIME-ACK point 4; only on terminal failure paths where
 * no second sink can ever open — WIRING-SPEC S4 one-sink rule holds).
 *
 * Return semantics: wiring failures return 0 — the game runs UNMODDED rather
 * than half-modded, and the loader must never kill the game for a mod
 * failure; every reason is on disk. The preserved probe-lifecycle error
 * returns (-20/-21/-22, constructor-exception result) are unchanged.
 *
 * All C99; only imports already in the frozen contract (sceKernelOpen/
 * sceKernelWrite/sceKernelClose bindings proven by diagnostic.c; getpid/
 * clock_gettime measured in the import contract; mfa_* are DEFINED inside
 * the composed runtime object).
 */

#include "lifecycle.h"
#include "../probe/probe_api.h"
#include "../diagnostic.h"
#include "runtime_start_bridge.h"
#include <modforge_activation.h>
#include <stdint.h>
#include <sys/fcntl.h>
#include <time.h>
#include <unistd.h>

/* Same three proven kernel import bindings diagnostic.c uses. */
extern int sceKernelOpen(const char *, int, uint16_t);
extern int64_t sceKernelWrite(int, const void *, size_t);
extern int sceKernelClose(int);

/* Composition facts — must equal package-profile.json (composition-4-fix)
 * and the staged layout. The UE4SS log path is the module directory join the
 * runtime itself derives (UE4SSProgram::setup_paths: root = module parent;
 * native-context branch: log directory = working directory). */
#define MODBRIDGE_TITLE_ID        "PPSA33286"
#define MODBRIDGE_MOD_ROOT        "/data/modbridge/roots/PPSA33286"
#define MODBRIDGE_MODULE_PATH     "/data/plugins/modbridge_supplier_composition.prx"
#define MODBRIDGE_UE4SS_LOG_PATH  "/data/plugins/UE4SS.log"

#define RUNTIME_LOG_LINE_CAPACITY 256u

/* Target entry attributes are the original frozen pair. The x86-only pieces
 * (force_align_arg_pointer, the rsp read) degrade on host-test builds where
 * they do not exist; the emitted target code is identical to module_entry.c. */
#if defined(__x86_64__) || defined(_M_X64)
#define MODBRIDGE_ENTRY_ATTRS __attribute__((visibility("default"), force_align_arg_pointer))
#define MODBRIDGE_READ_ENTRY_SP(current_sp) \
    __asm__ volatile ("mov %%rsp, %0" : "=r"(current_sp))
#else
#define MODBRIDGE_ENTRY_ATTRS __attribute__((visibility("default")))
#define MODBRIDGE_READ_ENTRY_SP(current_sp) ((current_sp) = incoming_sp)
#endif

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
static unsigned char runtime_started; /* wiring attempted at most once */
static unsigned char runtime_active;  /* owner exists; module_stop must stop it */

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

/* ---- failure observability helpers (bounded, no formatting imports) ---- */

static void log_append(char *buffer, size_t *used, size_t capacity, const char *text)
{
    while (*text && *used < capacity)
        buffer[(*used)++] = *text++;
}

/* Printable-ASCII sanitize: control/non-ASCII bytes become '?'. */
static void log_append_sanitized(char *buffer, size_t *used, size_t capacity, const char *text)
{
    if (text == NULL)
        return;
    while (*text && *used < capacity) {
        unsigned char c = (unsigned char)*text++;
        buffer[(*used)++] = (char)((c >= 0x20 && c <= 0x7e) ? c : '?');
    }
}

static void log_append_i32(char *buffer, size_t *used, size_t capacity, int32_t value)
{
    uint32_t magnitude = (value < 0) ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
    if (value < 0 && *used < capacity)
        buffer[(*used)++] = '-';
    char decimal[10];
    size_t n = 0;
    do {
        decimal[n++] = (char)('0' + (magnitude % 10u));
        magnitude /= 10u;
    } while (magnitude != 0 && n < sizeof(decimal));
    while (n != 0 && *used < capacity)
        buffer[(*used)++] = decimal[--n];
}

/* Append one line to the UE4SS.log file the runtime itself uses. /data is
 * reachable here (module_start); later fresh opens fail by contract, so this
 * happens strictly before any runtime thread starts. Best effort: any open or
 * write failure is invisible here but the diagnostic channel still carries
 * the event. */
static void write_ue4ss_log_line(const char *stage, int code, const char *detail)
{
    if (!diagnostic_ready())
        return; /* import bindings unproven: never call them */
    char line[RUNTIME_LOG_LINE_CAPACITY];
    size_t used = 0;
    log_append(line, &used, sizeof(line) - 1, "modbridge module_start failed: ");
    log_append_sanitized(line, &used, sizeof(line) - 1, stage);
    log_append(line, &used, sizeof(line) - 1, " code=");
    log_append_i32(line, &used, sizeof(line) - 1, code);
    log_append(line, &used, sizeof(line) - 1, " detail=");
    log_append_sanitized(line, &used, sizeof(line) - 1, detail);
    if (used < sizeof(line) - 1)
        line[used++] = '\n';
    int fd = sceKernelOpen(MODBRIDGE_UE4SS_LOG_PATH,
                           O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW, 0600);
    if (fd < 0)
        return;
    size_t offset = 0;
    while (offset < used) {
        int64_t written = sceKernelWrite(fd, line + offset, used - offset);
        if (written <= 0 || (uint64_t)written > used - offset)
            break;
        offset += (size_t)written;
    }
    (void)sceKernelClose(fd);
}

/* Best-effort RUNTIME-ACK point 4 (`failed`) line through the vendored C
 * status API. Only reached on terminal failure paths, where the bridge never
 * ran (or already stopped) and no RuntimeAckSink exists: opening here cannot
 * violate the one-sink-per-process rule (WIRING-SPEC S4). */
static void write_status_failed(const char *stage, int code, const char *detail)
{
    mfa_status status = {-1, 0};
    uint64_t now = 0;
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        now = (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
    if (mfa_status_open(&status, MODBRIDGE_MOD_ROOT, (long)getpid(), now) != 0)
        return; /* mod root not present/writable: the diagnostic + log still hold */
    char note[128];
    size_t used = 0;
    log_append_sanitized(note, &used, sizeof(note) - 1, stage);
    if (used < sizeof(note) - 1)
        note[used++] = ' ';
    log_append_sanitized(note, &used, sizeof(note) - 1, detail);
    if (used < sizeof(note))
        note[used] = '\0';
    else
        note[sizeof(note) - 1] = '\0';
    (void)mfa_status_write(&status, "failed", code, note);
    mfa_status_close(&status);
}

static void runtime_failure(const char *stage, int code, const char *detail)
{
    diagnostic_event(stage, (uint32_t)code);
    write_ue4ss_log_line(stage, code, detail);
    write_status_failed(stage, code, detail);
}

/* The wiring itself. Runs exactly once per process, only after a clean
 * constructor run, while /data is still reachable. */
static void start_composed_runtime(void)
{
    if (__atomic_test_and_set(&runtime_started, __ATOMIC_ACQUIRE))
        return;
    diagnostic_event("runtime_wiring_begin", 0);

    /* Step 1: read the activation from the mod root (all-or-nothing; any
     * failure releases everything and reports through mfa_error). */
    mfa_activation *activation = NULL;
    mfa_config config;
    mfa_error error;
    size_t i;
    for (i = 0; i < sizeof(config); ++i)
        ((unsigned char *)&config)[i] = 0;
    for (i = 0; i < sizeof(error); ++i)
        ((unsigned char *)&error)[i] = 0;
    config.root = MODBRIDGE_MOD_ROOT;
    config.title_id = MODBRIDGE_TITLE_ID;
    /* preload_roles NULL = defaults (lua_mod, config, script); limits zero =
     * defaults; deadline 0 = none; cancel NULL; io NULL = POSIX default. */
    int mfa_result = mfa_activate(&config, &activation, &error);
    if (mfa_result != MFA_OK) {
        char detail[160];
        size_t used = 0;
        const char *name = mfa_result_name(mfa_result);
        log_append_sanitized(detail, &used, sizeof(detail) - 1,
                             name != NULL ? name : "MFA_UNKNOWN");
        if (error.message[0] != '\0') {
            log_append(detail, &used, sizeof(detail) - 1, " ");
            log_append_sanitized(detail, &used, sizeof(detail) - 1, error.message);
        }
        if (error.path[0] != '\0') {
            log_append(detail, &used, sizeof(detail) - 1, " ");
            log_append_sanitized(detail, &used, sizeof(detail) - 1, error.path);
        }
        detail[used < sizeof(detail) ? used : sizeof(detail) - 1] = '\0';
        runtime_failure("mfa_activate_failed", mfa_result, detail);
        diagnostic_event("mfa_activate_os_error", (uint32_t)error.os_error);
        return; /* game continues UNMODDED; module_start returns 0 */
    }
    diagnostic_event("mfa_activated_low",
                     (uint32_t)((uint64_t)mfa_preloaded_bytes(activation) & 0xffffffffu));
    diagnostic_event("mfa_activated_high",
                     (uint32_t)((uint64_t)mfa_preloaded_bytes(activation) >> 32));

    /* Steps 2-6: loader facts -> NativeProgramContext ->
     * InstallDefaultEngineBindings + RuntimeAckConfig(status_root = mod root)
     * + PrepareActivatedUE4SSRuntime(NativeModSource from the activation) +
     * owner->start(). Activation ownership ALWAYS transfers with the call. */
    char detail[192];
    detail[0] = '\0';
    int start_result = modbridge_runtime_start_with_activation(
        MODBRIDGE_MODULE_PATH, MODBRIDGE_MOD_ROOT, MODBRIDGE_TITLE_ID,
        activation, detail, (unsigned)sizeof(detail));
    if (start_result == MRS_OK) {
        runtime_active = 1;
        diagnostic_event("runtime_start_ok", 0);
        return; /* the runtime continues on its own init thread */
    }
    /* r3 (entry-review P2-1): at MRS_ERR_START the runtime ack sink may
     * already hold the failed session; a terminal O_TRUNC status write here
     * would erase its detail. Log loudly to UE4SS.log + diagnostics only. */
    if (start_result == MRS_ERR_START) {
        diagnostic_event("runtime_start_failed", (uint32_t)start_result);
        write_ue4ss_log_line("runtime_start_failed", start_result,
                             detail[0] != '\0' ? detail : "no detail");
    } else {
        runtime_failure("runtime_start_failed", start_result,
                        detail[0] != '\0' ? detail : "no detail");
    }
}

MODBRIDGE_ENTRY_ATTRS
int module_start(size_t argc, const void *args)
{
    (void)argc; (void)args;
    if (__atomic_test_and_set(&entry_guard, __ATOMIC_ACQUIRE))
        return -20;
    if (lifecycle.state == PROBE_ACTIVE)
        return leave_entry(0);
    if (lifecycle.state != PROBE_NEW)
        return leave_entry(-21);
    int channel = diagnostic_ready() ? diagnostic_open_channel() : -1;
    if (channel < 0)
        { write_ue4ss_log_line("diag_channel_open_failed", -22, "diagnostic channel unavailable before wiring"); return leave_entry(-22); }
    diagnostic_event("module_start_entered", 0);
    /* Record only low alignment bits of our own frame. No caller stack data
       is dereferenced; the compiler restores the original stack on return. */
    uintptr_t incoming_sp = (uintptr_t)__builtin_frame_address(0) + sizeof(void *);
    uintptr_t current_sp;
    MODBRIDGE_READ_ENTRY_SP(current_sp);
    diagnostic_event("entry_incoming_sp_mod16", (uint32_t)(incoming_sp & 15u));
    diagnostic_event("entry_call_sp_mod16", (uint32_t)(current_sp & 15u));
    diagnostic_event("constructor_count", (uint32_t)(
        ((uintptr_t)__init_array_end - (uintptr_t)__init_array_start) / sizeof(probe_callback)));
    int result = probe_lifecycle_start(&lifecycle, &ops,
        __init_array_start, __init_array_end, __fini_array_start, __fini_array_end,
        __eh_frame_start, __eh_frame_end, __eh_frame_hdr_start, __eh_frame_hdr_end);
    diagnostic_event("probe_lifecycle_result", (uint32_t)result);
    if (result != 0) {
        /* Preserved terminal handling: constructors failed or the probe
         * worker refused. No runtime wiring on a broken unit set. */
        diagnostic_event("runtime_start_skipped", (uint32_t)result);
        if (lifecycle.state == PROBE_FINISHED) {
            probe_runtime_release_input();
            (void)diagnostic_close_channel();
        }
        return leave_entry(result);
    }
    start_composed_runtime();
    diagnostic_event("module_start_return", 0);
    return leave_entry(0);
}

MODBRIDGE_ENTRY_ATTRS
int module_stop(size_t argc, const void *args)
{
    (void)argc; (void)args;
    if (__atomic_test_and_set(&entry_guard, __ATOMIC_ACQUIRE))
        return -20;
    if (runtime_active) {
        diagnostic_event("runtime_stop_begin", 0);
        int stop_result = modbridge_runtime_stop();
        diagnostic_event("runtime_stop_result", (uint32_t)stop_result);
        runtime_active = 0; /* the owner is gone (or was never stoppable) */
        if (stop_result != 0)
            runtime_failure("runtime_stop_failed", stop_result,
                            "owner stop reported failure");
    }
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
