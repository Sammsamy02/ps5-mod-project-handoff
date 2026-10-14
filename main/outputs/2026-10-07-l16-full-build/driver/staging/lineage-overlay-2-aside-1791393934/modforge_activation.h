/*
 * modforge_activation.h — the game-side half of an install.
 *
 * The console app (or the composer/installer it calls) leaves a mod root such
 * as /data/modbridge/PPSA33286 containing the mod files and loadlist.txt. The
 * runtime injected into the game reads it once, during module_start: that is
 * the only moment /data is readable from the game process (a fresh open
 * afterwards fails with ENOENT). So activation:
 *
 *   1. reads and fully validates loadlist.txt (title must match the game),
 *   2. reads every file whose role the runtime consumes at startup (by
 *      default lua_mod, config and script) into owned memory, checking each
 *      against the size the load list records and that nothing follows it,
 *   3. lists every other file (paks, plugins) by path and size without
 *      reading it,
 *   4. returns one immutable activation the runtime keeps for the process
 *      lifetime and hands to its frameworks (UE4SS reads its Lua mods and
 *      settings from it instead of from /data).
 *
 * All or nothing: any failure (missing or changed file, limit, cancellation,
 * deadline) releases everything and returns an error, so the game runs
 * unmodded rather than half-modded. Errors carry a code, the OS error and the
 * file involved.
 *
 * Optionally, mfa_status_* keep a small append-only status file in the mod
 * root (opened during module_start and kept open, which is what still works
 * later) so the app can tell "installed" from "running" and show errors.
 *
 * C99. Uses malloc/free and, through the default I/O, open/read/close and
 * clock_gettime; everything else is passed in. Compiles with the PS5 payload
 * SDK and on the host (tests/test_activation.py runs it against roots staged
 * by modforge).
 */
#ifndef MODFORGE_ACTIVATION_H
#define MODFORGE_ACTIVATION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MFA_VERSION 1

typedef enum {
    MFA_OK = 0,
    MFA_E_INVALID = -1,     /* bad arguments                                   */
    MFA_E_NO_LOADLIST = -2, /* loadlist.txt missing: nothing installed         */
    MFA_E_LOADLIST = -3,    /* loadlist.txt malformed                          */
    MFA_E_TITLE = -4,       /* installed for a different game                  */
    MFA_E_LIMIT = -5,       /* too many files / bytes, path too long           */
    MFA_E_OPEN = -6,        /* a listed file could not be opened               */
    MFA_E_READ = -7,        /* read error                                      */
    MFA_E_SIZE = -8,        /* a file is shorter or longer than listed         */
    MFA_E_MEMORY = -9,      /* allocation failed                               */
    MFA_E_CANCELLED = -10,  /* the cancel flag was raised                      */
    MFA_E_DEADLINE = -11,   /* the deadline passed                             */
    MFA_E_DUPLICATE = -12   /* the same path is listed twice                   */
} mfa_result;

typedef struct {
    int code;            /* mfa_result                                         */
    int os_error;        /* errno from the failing call, or 0                  */
    int line;            /* load-list line, for MFA_E_LOADLIST                 */
    char path[256];      /* file involved (relative to the root), may be ""    */
    char message[160];   /* printable ASCII                                    */
} mfa_error;

typedef struct {
    uint32_t max_files;          /* default 4096                               */
    uint64_t max_file_bytes;     /* per preloaded file, default 8 MiB          */
    uint64_t max_total_bytes;    /* all preloaded files, default 64 MiB        */
    uint64_t max_loadlist_bytes; /* default 1 MiB                              */
} mfa_limits;

typedef struct {
    void *ctx;
    /* Return a descriptor, or -1 with *os_error set. */
    int (*open_read)(void *ctx, const char *path, int *os_error);
    /* Return bytes read, 0 at end of file, or -1 with *os_error set. */
    long (*read)(void *ctx, int fd, void *buf, size_t n, int *os_error);
    int (*close)(void *ctx, int fd);
    uint64_t (*now_ms)(void *ctx);           /* monotonic milliseconds       */
} mfa_io;

typedef struct {
    const char *root;                 /* mod root, e.g. /data/modbridge/PPSA33286 */
    const char *title_id;             /* must equal the load list's title         */
    const char *const *preload_roles; /* NULL-terminated; NULL = lua_mod, config, script */
    mfa_limits limits;                /* zero fields take the defaults            */
    uint64_t deadline_ms;             /* absolute io->now_ms() value; 0 = none     */
    const volatile int *cancel;       /* checked between reads; may be NULL        */
    const mfa_io *io;                 /* NULL = POSIX                              */
} mfa_config;

typedef struct {
    const char *path;            /* relative to the root, NUL-terminated        */
    const char *role;            /* e.g. "lua_mod", "pak"                        */
    uint64_t size;               /* as listed (and as read, when preloaded)      */
    const unsigned char *data;   /* the bytes when preloaded, else NULL; data[size] == 0 */
} mfa_file;

typedef struct {
    const char *id;
    const char *version;
} mfa_mod;

typedef struct mfa_activation mfa_activation;

/* Read the mod root. On success *out owns everything and must be released
 * with mfa_release. On failure *out is NULL and err (if given) says why. */
int mfa_activate(const mfa_config *cfg, mfa_activation **out, mfa_error *err);
void mfa_release(mfa_activation *a);

const char *mfa_title(const mfa_activation *a);
const char *mfa_engine(const mfa_activation *a);
const char *mfa_sdk(const mfa_activation *a);          /* "" when not listed     */
uint64_t mfa_preloaded_bytes(const mfa_activation *a);

size_t mfa_framework_count(const mfa_activation *a);
const char *mfa_framework_at(const mfa_activation *a, size_t i);
int mfa_wants_framework(const mfa_activation *a, const char *id);

size_t mfa_mod_count(const mfa_activation *a);          /* load order, later wins */
const mfa_mod *mfa_mod_at(const mfa_activation *a, size_t i);

size_t mfa_file_count(const mfa_activation *a);         /* sorted by path (bytewise) */
const mfa_file *mfa_file_at(const mfa_activation *a, size_t i);
const mfa_file *mfa_find(const mfa_activation *a, const char *path);
/* Index of the first file whose path starts with prefix (count if none). */
size_t mfa_lower_bound(const mfa_activation *a, const char *prefix);

size_t mfa_order_count(const mfa_activation *a);        /* data-plugin load order */
const char *mfa_order_at(const mfa_activation *a, size_t i);

const char *mfa_result_name(int code);

/* --- runtime status file ---------------------------------------------------
 * <root>/runtime-status.txt, UTF-8, TAB-separated, append-only:
 *   modforge-runtime-status <TAB> 1
 *   session <TAB> <pid> <TAB> <monotonic ms at open>
 *   state <TAB> <seq> <TAB> <state> <TAB> <code> <TAB> <detail>
 *   note  <TAB> <seq> <TAB> <key> <TAB> <text>        (diagnostics, optional)
 * States the runtime writes: activated, engine-ready, mods-started, failed,
 * stopped. The last state line is current; a new session replaces the file.
 * Details are sanitised (control characters and invalid UTF-8 become '?').
 */
typedef struct {
    int fd;
    uint32_t seq;
} mfa_status;

/* Create/replace the status file (call during module_start). 0 or -errno. */
int mfa_status_open(mfa_status *s, const char *root, long pid, uint64_t now_ms);
/* Append one state line through the retained descriptor. 0 or -errno. */
int mfa_status_write(mfa_status *s, const char *state, int code, const char *detail);
/* Append a free-form diagnostic line: note <TAB> <seq> <TAB> <key> <TAB> <text>.
 * Readers that don't know notes skip them. 0 or -errno. */
int mfa_status_note(mfa_status *s, const char *key, const char *text);
void mfa_status_close(mfa_status *s);

#ifdef __cplusplus
}
#endif

#endif /* MODFORGE_ACTIVATION_H */
