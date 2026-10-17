/*
 * multi_mod_lifecycle.h — M2 N-mod lifecycle state machine (prototype).
 *
 * worker-multi-mod, 2026-10-04. Host-testable, dependency-free C99. Pure
 * state machine: no filesystem, no engine calls. The host (modinst.c slice
 * or the console app backend) wraps mm_plan_* / mm_*_commit in the existing
 * journal stage/swap transaction exactly as worker-7-installer does today.
 *
 * Design basis:
 *   - worker-7-installer/src/modinst.c (single-mod transaction: states
 *     downloaded/prepared/installed; TSV records; journal recovery;
 *     priority-then-id load order; requirement + framework blockers).
 *   - worker-product-roadmap/PRODUCT-ROADMAP.md M2 (N-mod install/toggle/
 *     uninstall, conflict classification Supported / Needs conversion /
 *     Unsupported / Failed, per-mod records transitions, mod list UI).
 *
 * Honest scope: this is the M2 state machine + classification DESIGN as
 * working code. It does not compose bundles, write roots, or prove anything
 * on console. Load order is computed here and must be handed to an extended
 * mfx_compose (see INTEGRATION.md); it does not replace the vendored
 * composer's own (priority, id) sort until that lands.
 */
#ifndef MULTI_MOD_LIFECYCLE_H
#define MULTI_MOD_LIFECYCLE_H

#include <stddef.h>

#define MM_MAX_MODS    64
#define MM_MAX_DEPS    16
#define MM_MAX_FILES   64
#define MM_MAX_FW      8
#define MM_MAX_CAPS    32
#define MM_MAX_FAMS    32
#define MM_ID_MAX      128
#define MM_VER_MAX     64
#define MM_PATH_MAX    256
#define MM_REASON_MAX  256
#define MM_TITLE_MAX   64

/* ---- stored per-mod states (records.txt `mod` line, state column) --------
 * Extends the single-mod store {downloaded, prepared, installed} with the
 * M2 states. `running` stays derived-only (never stored), same discipline
 * as modinst.c: the runtime status file is the only source. */
typedef enum {
    MM_DOWNLOADED = 0,     /* archive registered; bytes on disk; not validated */
    MM_PREPARED,           /* validated + analyzed for the title; not in root */
    MM_INSTALLING,         /* transaction in flight; never legal after recovery */
    MM_INSTALLED_ENABLED,  /* files in root AND named in the active load list */
    MM_INSTALLED_DISABLED, /* files in root, NOT in the active load list */
    MM_FAILED              /* evidence-backed failure of the mod's last action */
} mm_state;
const char *mm_state_name(mm_state s);
int         mm_state_from_name(const char *name); /* -1 if unknown */

/* ---- user-facing conflict classification (console-only contract) -------- */
typedef enum {
    MM_CLASS_SUPPORTED = 0,        /* composes as-is, proven backend + evidence */
    MM_CLASS_NEEDS_CONVERSION = 1, /* different container/family; verified
                                      on-console converter required */
    MM_CLASS_UNSUPPORTED = 2,      /* no PS5 backend / undefined semantics */
    MM_CLASS_FAILED = 3            /* runtime-evidenced failure; never static */
} mm_class;
const char *mm_class_name(mm_class c);
mm_class    mm_class_merge(mm_class a, mm_class b); /* worst-of */

/* ---- op results (each refusal carries an actionable reason) -------------- */
typedef enum {
    MM_OK = 0,
    MM_ERR_BAD_REQUEST,      /* unknown mod id, malformed argument */
    MM_ERR_STATE,            /* action illegal in the mod's current state */
    MM_ERR_BLOCKED_DEP,      /* dependency missing / too old / not enabled */
    MM_ERR_BLOCKED_DEPENDENT,/* an enabled (or installed) dependent blocks it */
    MM_ERR_CONFLICT,         /* classification refuses: unsupported/convert */
    MM_ERR_CAPACITY,         /* mod-count capacity gate */
    MM_ERR_ORDER,            /* dependency cycle: no deterministic load order */
    MM_ERR_IO                /* serialization failure (save/load) */
} mm_result;
const char *mm_result_name(mm_result r);

/* ---- declared mod facts (host reads these from the bundle modinfo) ------- */
typedef struct {
    char id[MM_ID_MAX];
    char title[MM_TITLE_MAX];
    char version[MM_VER_MAX];
    long priority;                      /* lower loads earlier */
    int  layout;                        /* 0 = merged container compose,
                                           1 = per-mod directory (enabled.txt) */
    char bundle[MM_PATH_MAX];           /* registered archive path, "-" if none */
    char family[MM_ID_MAX];             /* mod family tag (caps table keyed) */
    int  n_fw;
    char fw[MM_MAX_FW][MM_ID_MAX];      /* required runtime framework ids */
    int  n_files;
    char files[MM_MAX_FILES][MM_PATH_MAX]; /* declared destination paths */
    int  n_deps;
    char dep_id[MM_MAX_DEPS][MM_ID_MAX];
    char dep_min[MM_MAX_DEPS][MM_VER_MAX];
} mm_manifest;

/* ---- one records-store row per mod --------------------------------------- */
typedef struct {
    char   id[MM_ID_MAX];
    mm_state state;
    char   reason[MM_REASON_MAX];       /* last actionable reason, "-" if none */
} mm_record;

/* ---- the per-title store (mirrors records.txt + in-memory manifests) ----- */
typedef struct {
    char title[MM_TITLE_MAX];
    long generation;                    /* whole-root transaction generation */
    long capacity;                      /* max simultaneously installed mods
                                           (<=0 disables the gate) */
    int  n_mods;
    mm_manifest mfs[MM_MAX_MODS];
    mm_record   recs[MM_MAX_MODS];
} mm_store;

/* ---- capability tables (host seeds from the engine facts, like caps.tsv) -- */
typedef struct {
    char id[MM_ID_MAX];
    char maturity[16];   /* "ported" | "integrating" | "planned" | "missing" */
} mm_cap;

typedef struct {
    char family[MM_ID_MAX];
    char verdict[16];    /* "native" | "conversion" | "unsupported" */
    char shared_policy[16]; /* "last-wins" | "refuse" (shared dest paths) */
} mm_family;

typedef struct {
    int n_caps;
    mm_cap  caps[MM_MAX_CAPS];
    int n_fams;
    mm_family fams[MM_MAX_FAMS];
} mm_caps;

/* ---- the install plan (validate first, commit after the root swap) ------- */
typedef struct {
    int  n;                                  /* target enabled set size */
    char ids[MM_MAX_MODS][MM_ID_MAX];        /* target enabled set */
    int  order[MM_MAX_MODS];                 /* load order: indices into ids */
    long new_generation;
} mm_plan;

/* ---- lifecycle ops -------------------------------------------------------- */
mm_result mm_declare(mm_store *st, const mm_manifest *mf);
mm_result mm_prepare(mm_store *st, const char *id, const mm_caps *caps,
                     char *reason, size_t rn);

/* plan = pure validation + ordering (no records mutation). The host runs the
 * plan inside its journal transaction; commit applies records after the
 * rename swap, exactly where modinst.c commits records today. */
mm_result mm_plan_install(mm_store *st, const char *const *new_ids, int n,
                          const mm_caps *caps, mm_plan *out,
                          char *reason, size_t rn);
mm_result mm_install_commit(mm_store *st, const mm_plan *plan);

mm_result mm_enable(mm_store *st, const char *id, const mm_caps *caps,
                    char *reason, size_t rn);
mm_result mm_disable(mm_store *st, const char *id, char *reason, size_t rn);
/* force=0: refuse while any installed mod depends on target.
 * force=1: also uninstall transitively-dependent installed mods, but only if
 * every one of them is disabled; one enabled dependent still refuses. */
mm_result mm_uninstall(mm_store *st, const char *id, int force,
                       char *reason, size_t rn);

/* evidence hooks: Failed is only ever set from a real acknowledgment
 * (runtime status state=failed, or recovery integrity failure), never
 * predicted. clear_failure returns the mod to PREPARED (re-validate first). */
mm_result mm_mark_failed(mm_store *st, const char *id, const char *why);
mm_result mm_clear_failure(mm_store *st, const char *id, char *reason, size_t rn);

/* recovery: reconcile records against the live root truth, mirroring
 * records_commit_from_root(). loadlist = ids named in the active load list;
 * permod_dirs = mod ids whose per-mod directory exists under the root.
 * A mod in the loadlist -> INSTALLED_ENABLED; a mod with a per-mod dir but
 * not in the loadlist -> INSTALLED_DISABLED; otherwise any INSTALLING /
 * FAILED-without-evidence residue drops back to PREPARED (installed mods the
 * root no longer names were rolled back or composed out). */
mm_result mm_recover(mm_store *st, const char *const *loadlist, int n_ll,
                     const char *const *permod_dirs, int n_pd,
                     long generation_seen, char *reason, size_t rn);

/* ---- load order -----------------------------------------------------------
 * Deterministic topological order over the dependency graph (dependencies
 * load before dependents), Kahn's algorithm with (priority, id) tie-break
 * among ready nodes. out_idx receives indices into ids[]. Cycles refuse
 * with MM_ERR_ORDER rather than silently reordering. */
mm_result mm_order_set(const mm_store *st, const char *const *ids, int n,
                       int *out_idx, char *reason, size_t rn);

/* ---- conflict classification ----------------------------------------------
 * Static classification NEVER returns Failed (that requires runtime
 * evidence; merge it in with mm_class_merge after a real ack). */
mm_class mm_classify_mod(const mm_store *st, const mm_manifest *m,
                         const mm_caps *caps, char *reason, size_t rn);
mm_class mm_classify_pair(const mm_manifest *a, const mm_manifest *b,
                          const mm_caps *caps, char *reason, size_t rn);
mm_class mm_classify_against_set(const mm_store *st, const mm_manifest *m,
                                 const mm_caps *caps,
                                 const char *const *also_ids, int n_also,
                                 char *reason, size_t rn);

/* version compare: dot-separated, numeric segments when both parse,
 * missing segments count as 0. Integration note: the host should call the
 * vendored mfx_ver_cmp instead; semantics here match its usage in modinst. */
int mm_ver_cmp(const char *a, const char *b);

/* ---- records serialization (records.txt-compatible, append-only cols) -----
 * Line: mod <TAB> id <TAB> title <TAB> state <TAB> bundle <TAB> version
 *       <TAB> priority <TAB> layout <TAB> reason <TAB> deps
 * deps: "id>=min;id2>=min2" or "-". The first five columns are exactly the
 * single-mod store's, so the old parser reads new files (extra columns
 * ignored) and the new parser reads old files (missing columns defaulted). */
mm_result mm_store_save(const mm_store *st, const char *path);
mm_result mm_store_load(mm_store *st, const char *path);

/* ---- UI state surfacing (mod list row + legal actions) -------------------- */
typedef enum {
    MM_UI_NOT_INSTALLED = 0,  /* downloaded */
    MM_UI_READY,              /* prepared */
    MM_UI_UPDATING,           /* installing (transaction in flight) */
    MM_UI_ENABLED,            /* installed + enabled */
    MM_UI_DISABLED,           /* installed, not in load list */
    MM_UI_FAILED              /* failed (reason shown under the row) */
} mm_ui_state;
const char  *mm_ui_state_name(mm_ui_state u);
mm_ui_state  mm_ui_state_of(const mm_store *st, const char *id);

enum {  /* legal-action bits for the row's buttons */
    MM_ACT_INSTALL   = 1u << 0,
    MM_ACT_ENABLE    = 1u << 1,
    MM_ACT_DISABLE   = 1u << 2,
    MM_ACT_UNINSTALL = 1u << 3,
    MM_ACT_UPDATE    = 1u << 4,
    MM_ACT_RETRY     = 1u << 5
};
unsigned mm_ui_actions_of(const mm_store *st, const char *id);

/* convenience lookups */
int mm_index_of(const mm_store *st, const char *id);
const mm_manifest *mm_manifest_of(const mm_store *st, const char *id);
const mm_record *mm_record_of(const mm_store *st, const char *id);
int mm_is_enabled(const mm_store *st, const char *id);
int mm_is_installed(const mm_store *st, const char *id);

#endif /* MULTI_MOD_LIFECYCLE_H */
