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
 *   3. derive_running_title() — r8 (generic mod root): the running title id
 *      comes from TWO independent sources that must agree (ploader.ini
 *      sections enabling this module, and the live game sandbox's own
 *      sce_sys/param.json); <mod root> = /data/modbridge/roots/<title>.
 *      Missing, malformed or disagreeing sources fail closed (game runs
 *      UNMODDED). No game is named anywhere in this file.
 *      mfa_activate(<mod root>) — reads/validates the activation ONCE, the
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
 * the runtime itself uses (<module dir>/UE4SS.log) — r7: prefixed with
 * `[t=<monotonic ms>ms pid=<pid>]` (playbook freshness recommendation, so
 * triage can order interleaved failure lines across launches; both values
 * come from imports the entry already links, no new imports), and (c)
 * best-effort a `failed` line into <mod root>/runtime-status.txt through the
 * vendored mfa_status API (RUNTIME-ACK point 4; only on terminal failure
 * paths where no second sink can ever open — WIRING-SPEC S4 one-sink rule
 * holds).
 *
 * Return semantics: wiring failures return 0 — the game runs UNMODDED rather
 * than half-modded, and the loader must never kill the game for a mod
 * failure; every reason is on disk. The preserved probe-lifecycle error
 * returns (-20/-21/-22, constructor-exception result) are unchanged.
 *
 * All C99; only imports already in the frozen contract (sceKernelOpen/
 * sceKernelWrite/sceKernelClose bindings proven by diagnostic.c; getpid/
 * clock_gettime measured in the import contract; mfa_* are DEFINED inside
 * the composed runtime object). r8: the title derivation reads files with
 * sceKernelOpen + sceKernelPread + sceKernelClose — sceKernelPread is
 * already one of the PRX's frozen import identities (the bridge's
 * code-image cache reads /data with the same trio during module_start), so
 * the PRX import set does not change.
 */

#include "lifecycle.h"
#include "../probe/probe_api.h"
#include "../diagnostic.h"
#include "runtime_start_bridge.h"
/* Defined by the same composition; not a system import. */
extern int bridge_native_code_residence_required(void);
extern int bridge_native_pad_code_residence_required(void);
extern int bridge_native_thread_authority_retire(void);
#include <modforge_activation.h>
#include <stdint.h>
#include <sys/fcntl.h>
#include <time.h>
#include <unistd.h>

/* Same three proven kernel import bindings diagnostic.c uses. */
extern int sceKernelOpen(const char *, int, uint16_t);
extern int64_t sceKernelWrite(int, const void *, size_t);
extern int sceKernelClose(int);
/* r8: already a frozen PRX import identity (libkernel); same declaration as
 * runtime_start_bridge.cpp's code-image cache (fd, buffer, length, offset). */
extern int64_t sceKernelPread(int, void *, size_t, int64_t);

/* Composition facts — identical for every game (r8: nothing here names a
 * game; the title id and the per-title mod root are DERIVED at module_start,
 * see derive_running_title). The UE4SS log path is the module directory join
 * the runtime itself derives (UE4SSProgram::setup_paths: root = module
 * parent; native-context branch: log directory = working directory). */
#define MODBRIDGE_MOD_ROOTS_PARENT "/data/modbridge/roots/"
#define MODBRIDGE_MODULE_PATH     "/data/plugins/modbridge_supplier_composition.prx"
#define MODBRIDGE_UE4SS_LOG_PATH  "/data/plugins/UE4SS.log"
/* r8 title sources. ploader.ini path + grammar: ps5-plugin-loader main.c
 * (DEFAULT_INI_PATH, read_ini/parse_section/parse_mode/normalize_path) and
 * the deployed file's own layout (evidence/ploader-ini-backup-20261008.ini).
 * Sandbox layout: /mnt/sandbox/<title>_<NNN>/app0 (FTP listings of a running
 * game, 2026-10-01 and 2026-10-06; console re-proof required). */
#define MODBRIDGE_PLOADER_INI_PATH  "/data/plugins/ploader.ini"
#define MODBRIDGE_PLOADER_PLUGIN_DIR "/data/plugins/"
#define MODBRIDGE_SANDBOX_PARENT    "/mnt/sandbox/"
#define MODBRIDGE_PARAM_JSON_TAIL   "/app0/sce_sys/param.json"

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
/* r8: the derived running title and its mod root. Written once, inside
 * module_start, before any runtime thread exists; empty strings until a
 * derivation succeeded (then status writes target this root). */
static char resolved_title[16];
static char resolved_mod_root[64];

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

/* r7: unsigned 64-bit decimal (monotonic ms / pid) for the log-line prefix.
 * Same bounded, no-formatting-imports discipline as log_append_i32. */
static void log_append_u64(char *buffer, size_t *used, size_t capacity, uint64_t value)
{
    char decimal[20];
    size_t n = 0;
    do {
        decimal[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0 && n < sizeof(decimal));
    while (n != 0 && *used < capacity)
        buffer[(*used)++] = decimal[--n];
}

/* Append one line to the UE4SS.log file the runtime itself uses. /data is
 * reachable here (module_start); later fresh opens fail by contract, so this
 * happens strictly before any runtime thread starts. Best effort: any open or
 * write failure is invisible here but the diagnostic channel still carries
 * the event. */
static void write_ue4ss_log_text(const char *head, const char *stage, int code,
                                 const char *detail);

static void write_ue4ss_log_line(const char *stage, int code, const char *detail)
{
    write_ue4ss_log_text("modbridge module_start failed: ", stage, code, detail);
}

/* r8: same bounded writer with a caller-chosen head, so the derived title is
 * logged on success too ("modbridge title: ..."). */
static void write_ue4ss_log_text(const char *head, const char *stage, int code,
                                 const char *detail)
{
    if (!diagnostic_ready())
        return; /* import bindings unproven: never call them */
    char line[RUNTIME_LOG_LINE_CAPACITY];
    size_t used = 0;
    /* r7 freshness prefix — the SAME monotonic-ms + pid pair the status
     * line already carries (write_status_failed): enables ordering and
     * attribution of interleaved UE4SS.log failure lines during playbook
     * triage. clock_gettime/getpid are proven imports; no new imports. */
    uint64_t now_ms = 0;
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        now_ms = (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
    log_append(line, &used, sizeof(line) - 1, "[t=");
    log_append_u64(line, &used, sizeof(line) - 1, now_ms);
    log_append(line, &used, sizeof(line) - 1, "ms pid=");
    log_append_u64(line, &used, sizeof(line) - 1, (uint64_t)getpid());
    log_append(line, &used, sizeof(line) - 1, "] ");
    log_append(line, &used, sizeof(line) - 1, head);
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
    /* r8: no derived title = no known mod root: never guess one. The
     * diagnostic event + UE4SS.log line still carry the failure. */
    if (resolved_mod_root[0] == '\0')
        return;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        now = (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
    if (mfa_status_open(&status, resolved_mod_root, (long)getpid(), now) != 0)
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

/* ==== r8: generic running-title derivation ================================
 *
 * Source 1 (loader configuration): /data/plugins/ploader.ini read with the
 *   loader's OWN grammar (ps5-plugin-loader main.c read_ini: fgets(1024)
 *   lines, trim, ';'/'#' comments, "[section]" = title, "path = mode",
 *   atoi mode, 1 early / 2 late, <=0 or >2 skipped, path without '/' gets
 *   the /data/plugins/ prefix, ".." or >255 chars skipped). Result: the set
 *   E of section titles that enable THIS module path. The loader injects a
 *   module only into a process whose app-info title equals such a section,
 *   so the running title is in E.
 * Source 2 (the running game's own package, through its live sandbox):
 *   /mnt/sandbox/<T>_<NNN>/app0/sce_sys/param.json for every T in E and
 *   NNN 000..099. T is CONFIRMED when one of those files is strict JSON whose
 *   root "titleId" equals T. A readable file there that is malformed,
 *   oversized or names another title fails closed.
 * Decision: exactly one confirmed title -> use it. None or several -> fail
 *   closed. (mfa_activate then checks that the derived root's load list
 *   names the same title: a consistency check of that folder, NOT a third
 *   source for which game is running.)
 * Both sources describe the console as a whole (configured titles, mounted
 *   sandboxes); neither observes THIS process. The link "this process is that
 *   sandbox" rests on one game running at a time (DESIGN.md section 3).
 * Fail closed = no activation, the game runs UNMODDED, the reason goes to the
 * diagnostic channel and UE4SS.log. Title ids accepted: 4 x 'A'-'Z' then
 * 5 x '0'-'9' (shape only, never a prefix list), so a built path can never
 * leave /data/modbridge/roots/ or /mnt/sandbox/.
 * No libc calls, no allocation: two static buffers, bounded loops.
 */
#define MBT_TITLE_LEN          9u
#define MBT_MAX_TITLES         16u
#define MBT_FILE_CAP           (64u * 1024u)   /* ini and param.json (policy) */
#define MBT_JSON_MAX_DEPTH     16u             /* policy */
#define MBT_SANDBOX_INSTANCES  100u            /* <T>_000..<T>_099 (policy) */
#define MBT_LOADER_LINE_CAP    1024u           /* ploader: char line[1024] */
#define MBT_LOADER_TID_CAP     17u             /* ploader: MAX_TITLE_ID_LEN + 1 */
#define MBT_LOADER_PATH_CAP    256u            /* ploader: MAX_PATH_LEN + 1 */

enum {
    MBT_OK = 0,
    MBT_E_INI_MISSING = -101,    /* ploader.ini absent / not openable        */
    MBT_E_INI_READ = -102,       /* read error, oversized or NUL byte        */
    MBT_E_INI_NO_TITLE = -103,   /* no section enables this module           */
    MBT_E_INI_TOO_MANY = -104,   /* more than MBT_MAX_TITLES titles          */
    MBT_E_PARAM_READ = -105,     /* a sandbox param.json unreadable/oversized */
    MBT_E_PARAM_JSON = -106,     /* a sandbox param.json is not strict JSON   */
    MBT_E_PARAM_DISAGREE = -107, /* a sandbox param.json names another title */
    MBT_E_NOT_CONFIRMED = -108,  /* no live sandbox confirms any title       */
    MBT_E_AMBIGUOUS = -109       /* several configured titles confirmed      */
};

static unsigned char mbt_buffer[MBT_FILE_CAP];

typedef struct {
    char titles[MBT_MAX_TITLES][MBT_TITLE_LEN + 1];
    unsigned count;
    unsigned invalid_sections; /* enable this module, name is not a title id */
    unsigned overflow;         /* distinct titles beyond MBT_MAX_TITLES      */
} mbt_title_set;

static size_t mbt_strlen(const char *s)
{
    size_t n = 0;
    while (s[n] != '\0')
        ++n;
    return n;
}

static int mbt_streq(const char *a, const char *b)
{
    size_t i = 0;
    while (a[i] != '\0' && a[i] == b[i])
        ++i;
    return a[i] == b[i];
}

static int mbt_title_valid(const char *s, size_t n)
{
    size_t i;
    if (n != MBT_TITLE_LEN)
        return 0;
    for (i = 0; i < 4; ++i)
        if (s[i] < 'A' || s[i] > 'Z')
            return 0;
    for (i = 4; i < MBT_TITLE_LEN; ++i)
        if (s[i] < '0' || s[i] > '9')
            return 0;
    return 1;
}

/* Whole small file into buf. 0 ok; -1 not openable (absent); -2 read error;
 * -3 larger than cap; -4 a kernel binding is unbound (checked BEFORE any
 * call, the same rule diagnostic_ready() applies to Open/Write/Close).
 * Never follows a final symlink. */
static int mbt_read_file(const char *path, unsigned char *buf, size_t cap,
                         size_t *length_out)
{
    int (*volatile open_binding)(const char *, int, uint16_t) = sceKernelOpen;
    int64_t (*volatile pread_binding)(int, void *, size_t, int64_t) = sceKernelPread;
    int (*volatile close_binding)(int) = sceKernelClose;
    int fd;
    size_t used = 0;
    int rc = 0;
    *length_out = 0;
    if (open_binding == NULL || pread_binding == NULL || close_binding == NULL)
        return -4;
    fd = sceKernelOpen(path, O_RDONLY | O_NOFOLLOW, 0);
    if (fd < 0)
        return -1;
    for (;;) {
        int64_t got;
        if (used == cap) {
            unsigned char extra;
            got = sceKernelPread(fd, &extra, 1, (int64_t)used);
            if (got < 0)
                rc = -2;
            else if (got > 0)
                rc = -3;
            break;
        }
        got = sceKernelPread(fd, buf + used, cap - used, (int64_t)used);
        if (got < 0 || (uint64_t)got > (uint64_t)(cap - used)) {
            rc = -2;
            break;
        }
        if (got == 0)
            break;
        used += (size_t)got;
    }
    (void)sceKernelClose(fd);
    if (rc == 0)
        *length_out = used;
    return rc;
}

/* ---- Source 1: ploader.ini, the loader's own grammar ---- */

/* ploader trim(): skip leading ' '/'\t'; cut trailing ' ' '\t' '\r' '\n'. */
static char *mbt_trim(char *s)
{
    size_t len;
    char *end;
    while (*s == ' ' || *s == '\t')
        ++s;
    len = mbt_strlen(s);
    if (len == 0)
        return s;
    end = s + len - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
        *end-- = '\0';
    return s;
}

/* ploader parse_mode(): atoi() then 0..2 else -1. atoi skips isspace(), takes
 * an optional sign and decimal digits, stops at anything else. */
static int mbt_parse_mode(const char *s)
{
    long value = 0;
    int negative = 0;
    unsigned digits = 0;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\v' || *s == '\f' || *s == '\r')
        ++s;
    if (*s == '+' || *s == '-') {
        negative = (*s == '-');
        ++s;
    }
    while (*s >= '0' && *s <= '9') {
        if (digits < 9u) /* bounded: any longer number is out of 0..2 anyway */
            value = value * 10 + (*s - '0');
        else
            value = 1000;
        ++digits;
        ++s;
    }
    if (negative)
        value = -value;
    if (value < 0 || value > 2)
        return -1;
    return (int)value;
}

/* ploader normalize_path(): trim; empty, >255 chars or ".." -> reject; no
 * '/' -> "/data/plugins/<p>" (snprintf into 256 bytes, so truncated alike). */
static int mbt_normalize_path(char *p, char *out, size_t out_cap)
{
    size_t len, i, used = 0;
    int has_slash = 0;
    p = mbt_trim(p);
    len = mbt_strlen(p);
    if (len == 0 || len > MBT_LOADER_PATH_CAP - 1)
        return 0;
    for (i = 0; i + 1 < len; ++i)
        if (p[i] == '.' && p[i + 1] == '.')
            return 0;
    for (i = 0; i < len; ++i)
        if (p[i] == '/')
            has_slash = 1;
    if (!has_slash) {
        const char *prefix = MODBRIDGE_PLOADER_PLUGIN_DIR;
        for (i = 0; prefix[i] != '\0' && used + 1 < out_cap; ++i)
            out[used++] = prefix[i];
    }
    for (i = 0; i < len && used + 1 < out_cap; ++i)
        out[used++] = p[i];
    out[used] = '\0';
    return 1;
}

static void mbt_set_add(mbt_title_set *set, const char *title)
{
    unsigned i;
    size_t k;
    for (i = 0; i < set->count; ++i)
        if (mbt_streq(set->titles[i], title))
            return;
    if (set->count >= MBT_MAX_TITLES) {
        ++set->overflow;
        return;
    }
    for (k = 0; k < MBT_TITLE_LEN; ++k)
        set->titles[set->count][k] = title[k];
    set->titles[set->count][MBT_TITLE_LEN] = '\0';
    ++set->count;
}

/* Fills set from the ini bytes. 0 or MBT_E_INI_READ (NUL byte). */
static int mbt_ini_titles_for_module(const unsigned char *bytes, size_t length,
                                     const char *module_path, mbt_title_set *set)
{
    char line[MBT_LOADER_LINE_CAP];
    char tid[MBT_LOADER_TID_CAP];
    char normalized[MBT_LOADER_PATH_CAP];
    size_t pos = 0, i;
    tid[0] = '\0';
    for (i = 0; i < length; ++i)
        if (bytes[i] == 0)
            return MBT_E_INI_READ;
    while (pos < length) {
        /* fgets(line, 1024): up to 1023 bytes, through the first '\n'. */
        size_t n = 0;
        char *s, *eq;
        int mode;
        while (pos < length && n < MBT_LOADER_LINE_CAP - 1) {
            char c = (char)bytes[pos++];
            line[n++] = c;
            if (c == '\n')
                break;
        }
        line[n] = '\0';
        s = mbt_trim(line);
        if (*s == '\0' || *s == ';' || *s == '#')
            continue;
        {
            size_t len = mbt_strlen(s);
            if (s[0] == '[' && s[len - 1] == ']') {
                /* snprintf(tid, 17, "%.*s", len - 2, s + 1) */
                size_t inner = len >= 2 ? len - 2 : 0, k;
                for (k = 0; k < inner && k + 1 < sizeof(tid); ++k)
                    tid[k] = s[1 + k];
                tid[k] = '\0';
                continue;
            }
        }
        if (tid[0] == '\0')
            continue;
        eq = s;
        while (*eq != '\0' && *eq != '=')
            ++eq;
        if (*eq != '=')
            continue;
        *eq = '\0';
        mode = mbt_parse_mode(mbt_trim(eq + 1));
        if (mode < -9)
            continue; /* -1 invalid, 0 disabled */
        if (!mbt_normalize_path(s, normalized, sizeof(normalized)))
            continue;
        if (!mbt_streq(normalized, module_path))
            continue;
        if (mbt_title_valid(tid, mbt_strlen(tid)))
            mbt_set_add(set, tid);
        else
            ++set->invalid_sections; /* the loader can never match it */
    }
    return MBT_OK;
}

/* ---- Source 2: strict JSON, root "titleId" ---- */

typedef struct {
    const unsigned char *p;
    size_t n, i;
    unsigned depth;
    unsigned title_hits;            /* root-level "titleId" keys seen     */
    int title_plain;                /* its value: plain short ASCII string */
    char title[MBT_TITLE_LEN + 7];  /* up to 15 chars captured             */
} mbt_json;

static void mbt_json_ws(mbt_json *j)
{
    while (j->i < j->n && (j->p[j->i] == ' ' || j->p[j->i] == '\t' ||
                           j->p[j->i] == '\n' || j->p[j->i] == '\r'))
        ++j->i;
}

static int mbt_hex(unsigned char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* One UTF-8 sequence starting with lead byte c (>= 0x80) at j->i - 1. */
static int mbt_json_utf8(mbt_json *j, unsigned char c)
{
    unsigned need, k;
    unsigned char lo = 0x80, hi = 0xbf;
    if (c >= 0xc2 && c <= 0xdf) need = 1;
    else if (c == 0xe0) { need = 2; lo = 0xa0; }
    else if ((c >= 0xe1 && c <= 0xec) || c == 0xee || c == 0xef) need = 2;
    else if (c == 0xed) { need = 2; hi = 0x9f; }
    else if (c == 0xf0) { need = 3; lo = 0x90; }
    else if (c >= 0xf1 && c <= 0xf3) need = 3;
    else if (c == 0xf4) { need = 3; hi = 0x8f; }
    else return 0;
    for (k = 0; k < need; ++k) {
        unsigned char d;
        if (j->i >= j->n)
            return 0;
        d = j->p[j->i++];
        if (k == 0 ? (d < lo || d > hi) : (d < 0x80 || d > 0xbf))
            return 0;
    }
    return 1;
}

/* String at j->i (must be '"'). Captures up to cap-1 bytes; *plain = 0 when
 * it holds an escape or a non-ASCII byte; *truncated = 1 when it is longer
 * than cap-1 bytes (the capture is then a prefix only). */
static int mbt_json_string(mbt_json *j, char *capture, size_t cap, int *plain,
                           int *truncated)
{
    size_t used = 0;
    if (j->i >= j->n || j->p[j->i] != '"')
        return 0;
    ++j->i;
    if (plain != NULL)
        *plain = 1;
    if (truncated != NULL)
        *truncated = 0;
    for (;;) {
        unsigned char c;
        if (j->i >= j->n)
            return 0;
        c = j->p[j->i++];
        if (c == '"')
            break;
        if (c < 0x20)
            return 0;
        if (c == '\\') {
            unsigned char e;
            if (j->i >= j->n)
                return 0;
            e = j->p[j->i++];
            if (e == 'u') {
                unsigned k;
                for (k = 0; k < 4; ++k) {
                    if (j->i >= j->n || !mbt_hex(j->p[j->i]))
                        return 0;
                    ++j->i;
                }
            } else if (!(e == '"' || e == '\\' || e == '/' || e == 'b' ||
                         e == 'f' || e == 'n' || e == 'r' || e == 't')) {
                return 0;
            }
            if (plain != NULL)
                *plain = 0;
            continue;
        }
        if (c >= 0x80) {
            if (!mbt_json_utf8(j, c))
                return 0;
            if (plain != NULL)
                *plain = 0;
            continue;
        }
        if (capture != NULL) {
            if (used + 1 < cap)
                capture[used++] = (char)c;
            else if (truncated != NULL)
                *truncated = 1;
        }
    }
    if (capture != NULL && cap > 0)
        capture[used < cap ? used : cap - 1] = '\0';
    return 1;
}

static int mbt_json_number(mbt_json *j)
{
    size_t start;
    if (j->i < j->n && j->p[j->i] == '-')
        ++j->i;
    if (j->i >= j->n)
        return 0;
    if (j->p[j->i] == '0') {
        ++j->i;
    } else if (j->p[j->i] >= '1' && j->p[j->i] <= '9') {
        while (j->i < j->n && j->p[j->i] >= '0' && j->p[j->i] <= '9')
            ++j->i;
    } else {
        return 0;
    }
    if (j->i < j->n && j->p[j->i] == '.') {
        ++j->i;
        start = j->i;
        while (j->i < j->n && j->p[j->i] >= '0' && j->p[j->i] <= '9')
            ++j->i;
        if (j->i == start)
            return 0;
    }
    if (j->i < j->n && (j->p[j->i] == 'e' || j->p[j->i] == 'E')) {
        ++j->i;
        if (j->i < j->n && (j->p[j->i] == '+' || j->p[j->i] == '-'))
            ++j->i;
        start = j->i;
        while (j->i < j->n && j->p[j->i] >= '0' && j->p[j->i] <= '9')
            ++j->i;
        if (j->i == start)
            return 0;
    }
    return 1;
}

static int mbt_json_literal(mbt_json *j, const char *word)
{
    size_t k = 0;
    while (word[k] != '\0') {
        if (j->i >= j->n || j->p[j->i] != (unsigned char)word[k])
            return 0;
        ++j->i;
        ++k;
    }
    return 1;
}

static int mbt_json_value(mbt_json *j);

static int mbt_json_object(mbt_json *j, int is_root)
{
    if (++j->depth > MBT_JSON_MAX_DEPTH)
        return 0;
    ++j->i; /* '{' */
    mbt_json_ws(j);
    if (j->i < j->n && j->p[j->i] == '}') {
        ++j->i;
        --j->depth;
        return 1;
    }
    for (;;) {
        char key[16];
        int key_plain = 0, key_truncated = 0;
        mbt_json_ws(j);
        if (!mbt_json_string(j, key, sizeof(key), &key_plain, &key_truncated))
            return 0;
        if (is_root && !key_plain)
            return 0; /* an escaped root key could hide a second "titleId" */
        mbt_json_ws(j);
        if (j->i >= j->n || j->p[j->i] != ':')
            return 0;
        ++j->i;
        mbt_json_ws(j);
        if (is_root && !key_truncated && mbt_streq(key, "titleId")) {
            ++j->title_hits;
            if (j->i < j->n && j->p[j->i] == '"') {
                int plain = 0, truncated = 0;
                if (!mbt_json_string(j, j->title, sizeof(j->title), &plain, &truncated))
                    return 0;
                j->title_plain = plain && !truncated;
            } else {
                j->title_plain = 0;
                if (!mbt_json_value(j))
                    return 0;
            }
        } else if (!mbt_json_value(j)) {
            return 0;
        }
        mbt_json_ws(j);
        if (j->i >= j->n)
            return 0;
        if (j->p[j->i] == ',') {
            ++j->i;
            continue;
        }
        if (j->p[j->i] == '}') {
            ++j->i;
            --j->depth;
            return 1;
        }
        return 0;
    }
}

static int mbt_json_array(mbt_json *j)
{
    if (++j->depth > MBT_JSON_MAX_DEPTH)
        return 0;
    ++j->i; /* '[' */
    mbt_json_ws(j);
    if (j->i < j->n && j->p[j->i] == ']') {
        ++j->i;
        --j->depth;
        return 1;
    }
    for (;;) {
        mbt_json_ws(j);
        if (!mbt_json_value(j))
            return 0;
        mbt_json_ws(j);
        if (j->i >= j->n)
            return 0;
        if (j->p[j->i] == ',') {
            ++j->i;
            continue;
        }
        if (j->p[j->i] == ']') {
            ++j->i;
            --j->depth;
            return 1;
        }
        return 0;
    }
}

static int mbt_json_value(mbt_json *j)
{
    unsigned char c;
    if (j->i >= j->n)
        return 0;
    c = j->p[j->i];
    if (c == '{')
        return mbt_json_object(j, 0);
    if (c == '[')
        return mbt_json_array(j);
    if (c == '"')
        return mbt_json_string(j, NULL, 0, NULL, NULL);
    if (c == 't')
        return mbt_json_literal(j, "true");
    if (c == 'f')
        return mbt_json_literal(j, "false");
    if (c == 'n')
        return mbt_json_literal(j, "null");
    return mbt_json_number(j);
}

/* 1 = the whole buffer is one strict JSON object (optional UTF-8 BOM) with
 * exactly ONE root "titleId" whose value is a plain string of the title-id
 * shape; the title is copied to out (10 bytes). 0 otherwise. */
static int mbt_param_json_title(const unsigned char *bytes, size_t length,
                                char out[MBT_TITLE_LEN + 1])
{
    mbt_json j;
    size_t k;
    for (k = 0; k < sizeof(j); ++k)
        ((unsigned char *)&j)[k] = 0;
    j.p = bytes;
    j.n = length;
    if (length >= 3 && bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf)
        j.i = 3;
    mbt_json_ws(&j);
    if (j.i >= j.n || j.p[j.i] != '{')
        return 0;
    if (!mbt_json_object(&j, 1))
        return 0;
    mbt_json_ws(&j);
    if (j.i != j.n)
        return 0; /* trailing bytes */
    if (j.title_hits != 1 || !j.title_plain)
        return 0;
    if (!mbt_title_valid(j.title, mbt_strlen(j.title)))
        return 0;
    for (k = 0; k <= MBT_TITLE_LEN; ++k)
        out[k] = j.title[k];
    return 1;
}

/* "/mnt/sandbox/<T>_<NNN>/app0/sce_sys/param.json" */
static void mbt_sandbox_path(char *out, size_t cap, const char *title, unsigned instance)
{
    size_t used = 0;
    char suffix[5];
    suffix[0] = '_';
    suffix[1] = (char)('0' + (instance / 100u) % 10u);
    suffix[2] = (char)('0' + (instance / 10u) % 10u);
    suffix[3] = (char)('0' + instance % 10u);
    suffix[4] = '\0';
    log_append(out, &used, cap - 1, MODBRIDGE_SANDBOX_PARENT);
    log_append(out, &used, cap - 1, title);
    log_append(out, &used, cap - 1, suffix);
    log_append(out, &used, cap - 1, MODBRIDGE_PARAM_JSON_TAIL);
    out[used] = '\0';
}

/* Derives resolved_title / resolved_mod_root. MBT_OK or an MBT_E_* code with
 * a printable reason in detail. Called once, inside module_start. */
static int derive_running_title(char *detail, size_t detail_cap)
{
    mbt_title_set set;
    size_t length = 0, k, used = 0;
    unsigned t, instance, probes = 0, files = 0, confirmed_count = 0;
    unsigned confirmed_index = 0;
    unsigned char confirmed[MBT_MAX_TITLES];
    char first_instance[MBT_MAX_TITLES][5];
    char path[96];
    int rc;
    for (k = 0; k < sizeof(set); ++k)
        ((unsigned char *)&set)[k] = 0;
    detail[0] = '\0';

    rc = mbt_read_file(MODBRIDGE_PLOADER_INI_PATH, mbt_buffer, sizeof(mbt_buffer), &length);
    diagnostic_event("title_ini_read", (uint32_t)rc);
    diagnostic_event("title_ini_bytes", (uint32_t)length);
    if (rc == -1) {
        log_append(detail, &used, detail_cap - 1, "cannot open " MODBRIDGE_PLOADER_INI_PATH);
        detail[used] = '\0';
        return MBT_E_INI_MISSING;
    }
    if (rc != 0) {
        log_append(detail, &used, detail_cap - 1,
                   rc == -3 ? "oversized " : (rc == -4 ? "kernel open/pread/close unbound, not reading " : "read error "));
        log_append(detail, &used, detail_cap - 1, MODBRIDGE_PLOADER_INI_PATH);
        detail[used] = '\0';
        return MBT_E_INI_READ;
    }
    if (mbt_ini_titles_for_module(mbt_buffer, length, MODBRIDGE_MODULE_PATH, &set) != MBT_OK) {
        log_append(detail, &used, detail_cap - 1, "NUL byte in " MODBRIDGE_PLOADER_INI_PATH);
        detail[used] = '\0';
        return MBT_E_INI_READ;
    }
    diagnostic_event("title_ini_enabled", set.count);
    diagnostic_event("title_ini_invalid_sections", set.invalid_sections);
    if (set.overflow != 0) {
        log_append(detail, &used, detail_cap - 1, "ploader.ini enables this module for too many titles");
        detail[used] = '\0';
        return MBT_E_INI_TOO_MANY;
    }
    if (set.count == 0) {
        log_append(detail, &used, detail_cap - 1, "ploader.ini enables " MODBRIDGE_MODULE_PATH " for no title");
        detail[used] = '\0';
        return MBT_E_INI_NO_TITLE;
    }

    for (t = 0; t < set.count; ++t) {
        confirmed[t] = 0;
        first_instance[t][0] = '\0';
        for (instance = 0; instance < MBT_SANDBOX_INSTANCES; ++instance) {
            char found[MBT_TITLE_LEN + 1];
            mbt_sandbox_path(path, sizeof(path), set.titles[t], instance);
            ++probes;
            rc = mbt_read_file(path, mbt_buffer, sizeof(mbt_buffer), &length);
            if (rc == -1)
                continue; /* no such live instance */
            ++files;
            if (rc != 0) {
                diagnostic_event("title_sandbox_probes", probes);
                log_append(detail, &used, detail_cap - 1,
                           rc == -3 ? "oversized " : (rc == -4 ? "kernel open/pread/close unbound, not reading " : "read error "));
                log_append_sanitized(detail, &used, detail_cap - 1, path);
                detail[used] = '\0';
                return MBT_E_PARAM_READ;
            }
            if (!mbt_param_json_title(mbt_buffer, length, found)) {
                diagnostic_event("title_sandbox_probes", probes);
                log_append(detail, &used, detail_cap - 1, "no single valid root titleId in ");
                log_append_sanitized(detail, &used, detail_cap - 1, path);
                detail[used] = '\0';
                return MBT_E_PARAM_JSON;
            }
            if (!mbt_streq(found, set.titles[t])) {
                diagnostic_event("title_sandbox_probes", probes);
                log_append_sanitized(detail, &used, detail_cap - 1, path);
                log_append(detail, &used, detail_cap - 1, " says ");
                log_append_sanitized(detail, &used, detail_cap - 1, found);
                log_append(detail, &used, detail_cap - 1, ", ploader.ini section says ");
                log_append_sanitized(detail, &used, detail_cap - 1, set.titles[t]);
                detail[used] = '\0';
                return MBT_E_PARAM_DISAGREE;
            }
            if (!confirmed[t]) {
                size_t q;
                confirmed[t] = 1;
                for (q = 0; q < 4; ++q)
                    first_instance[t][q] = path[mbt_strlen(MODBRIDGE_SANDBOX_PARENT) + MBT_TITLE_LEN + q];
                first_instance[t][4] = '\0';
            }
        }
        if (confirmed[t]) {
            ++confirmed_count;
            confirmed_index = t;
        }
    }
    diagnostic_event("title_sandbox_probes", probes);
    diagnostic_event("title_sandbox_files", files);
    diagnostic_event("title_confirmed", confirmed_count);
    if (confirmed_count == 0) {
        log_append(detail, &used, detail_cap - 1,
                   "no live " MODBRIDGE_SANDBOX_PARENT "<title>_NNN" MODBRIDGE_PARAM_JSON_TAIL
                   " confirms a title ploader.ini enables for this module");
        detail[used] = '\0';
        return MBT_E_NOT_CONFIRMED;
    }
    if (confirmed_count > 1) {
        log_append(detail, &used, detail_cap - 1, "ambiguous: several configured titles have a live sandbox:");
        for (t = 0; t < set.count; ++t) {
            if (!confirmed[t])
                continue;
            log_append(detail, &used, detail_cap - 1, " ");
            log_append(detail, &used, detail_cap - 1, set.titles[t]);
        }
        detail[used] = '\0';
        return MBT_E_AMBIGUOUS;
    }

    for (k = 0; k <= MBT_TITLE_LEN; ++k)
        resolved_title[k] = set.titles[confirmed_index][k];
    used = 0;
    log_append(resolved_mod_root, &used, sizeof(resolved_mod_root) - 1, MODBRIDGE_MOD_ROOTS_PARENT);
    log_append(resolved_mod_root, &used, sizeof(resolved_mod_root) - 1, resolved_title);
    resolved_mod_root[used] = '\0';
    used = 0;
    log_append(detail, &used, detail_cap - 1, "root=");
    log_append(detail, &used, detail_cap - 1, resolved_mod_root);
    log_append(detail, &used, detail_cap - 1, " agree: " MODBRIDGE_PLOADER_INI_PATH " + " MODBRIDGE_SANDBOX_PARENT);
    log_append(detail, &used, detail_cap - 1, resolved_title);
    log_append(detail, &used, detail_cap - 1, first_instance[confirmed_index]);
    log_append(detail, &used, detail_cap - 1, MODBRIDGE_PARAM_JSON_TAIL);
    detail[used] = '\0';
    return MBT_OK;
}

/* The wiring itself. Runs exactly once per process, only after a clean
 * constructor run, while /data is still reachable. */
static void start_composed_runtime(void)
{
    if (__atomic_test_and_set(&runtime_started, __ATOMIC_ACQUIRE))
        return;
    diagnostic_event("runtime_wiring_begin", 0);

    /* Step 0 (r8): derive the running title from two independent sources;
     * any missing/disagreeing source fails closed BEFORE the mod root is
     * touched (no root is guessed, so no status file is written either). */
    {
        char title_detail[192];
        int title_result = derive_running_title(title_detail, sizeof(title_detail));
        diagnostic_event("title_derive_result", (uint32_t)title_result);
        if (title_result != MBT_OK) {
            resolved_title[0] = '\0';
            resolved_mod_root[0] = '\0';
            runtime_failure("title_derive_failed", title_result, title_detail);
            return; /* game continues UNMODDED; module_start returns 0 */
        }
        write_ue4ss_log_text("modbridge title: ", resolved_title, 0, title_detail);
    }

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
    config.root = resolved_mod_root;
    config.title_id = resolved_title;
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
        MODBRIDGE_MODULE_PATH, resolved_mod_root, resolved_title,
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
    /* The owning runtime is not the only possible holder of published code.
       Closing publication and testing residence is unconditional, including
       partial-start/no-owner cases. The in-link provider refuses unknown state. */
    int residence = bridge_native_code_residence_required();
    if (residence != 0) {
        diagnostic_event("module_stop_code_resident", (uint32_t)residence);
        return leave_entry(-22);
    }
    /* The native pad helper owns executable callbacks independently of hooks.
       Closing starts and retaining a running worker precedes every finalizer. */
    int pad_residence = bridge_native_pad_code_residence_required();
    if (pad_residence != 0) {
        diagnostic_event("module_stop_pad_resident", (uint32_t)pad_residence);
        return leave_entry(-23);
    }
    /* Cancel native authority and drain its tickets before any owner/finalizer
       release. Unknown/self-wait refuses and preserves process residence. */
    int thread_retirement = bridge_native_thread_authority_retire();
    if (thread_retirement != 0) {
        diagnostic_event("module_stop_thread_authority_resident", (uint32_t)thread_retirement);
        return leave_entry(-24);
    }
    if (runtime_active) {
        diagnostic_event("runtime_stop_begin", 0);
        int stop_result = modbridge_runtime_stop();
        diagnostic_event("runtime_stop_result", (uint32_t)stop_result);
        if (stop_result != 0) {
            runtime_failure("runtime_stop_failed", stop_result,
                            "owner stop refused; preserving runtime and module residence");
            /* Nonzero positive callback values may not refuse unload. Always return
               a negative entry error. Preserve active ownership and skip every
               fini/input/channel release while code can still be referenced. */
            return leave_entry(-21);
        }
        /* The initializer may have prepared an authority after an absent-owner
           precheck. Stop has now joined it: retire again before finalization. */
        int final_thread_retirement = bridge_native_thread_authority_retire();
        if (final_thread_retirement != 0) {
            diagnostic_event("module_stop_postjoin_authority_resident", (uint32_t)final_thread_retirement);
            return leave_entry(-25);
        }
        runtime_active = 0; /* stop succeeded: ownership has been retired */
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
