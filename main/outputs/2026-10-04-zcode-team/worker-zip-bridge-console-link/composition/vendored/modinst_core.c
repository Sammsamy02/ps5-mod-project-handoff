/*
 * modinst.c — worker-7-installer vertical slice of modforge-service/1.
 *
 * Pipeline implemented here, on top of Claude's imported runtime components
 * (vendored, unmodified; origin commit 43a96969e3bf1f4c9d1e43a55a386328dbac19e6):
 *
 *   select mods -> compose bundle (conflict detection, load order,
 *   FOMOD-style variant choices where representable) -> staged owned output
 *   -> transactional reversible install (journal, commit, rollback, crash
 *   recovery between journal steps) -> durable install records with
 *   downloaded/prepared/installed/running as distinct states.
 *
 * Composition, hashing, bundle validation and load-list writing are done by
 * the vendored composer (modforge_compose.h / modforge_bundle.h). Runtime
 * activation is plumbed through the vendored modforge_activation.c exactly
 * the way the real engine runtime would use it (mfa_activate preload check +
 * mfa_status_* status file); the engine itself is an explicit capability gap
 * and is never faked.
 *
 * This is a host-testable driver for the service slice, not the PS5 app: the
 * same steps would run behind mfs_request() (contract/modforge_service.h).
 * Records are a documented line store (see REPORT.md for the mapping to the
 * SERVICE.md records.json shape); the journal is the SERVICE.md journal with
 * an extended "stage" phase so a crash during staging also recovers.
 *
 * Build:  cc -std=c99 -O2 -Wall -o modinst src/modinst.c vendored/modforge_activation.c
 */
#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "modforge_compose.h"   /* includes modforge_bundle.h */
#include "ps5_io.h"
#include "ps5_io_dir.h"
#include "e2e_status.h"
#include "modinst_archive_prepare.h"
#ifdef MI_HAS_DOWNLOAD
#include "modinst_download.h"   /* worker-e2e-final: stage-1 owner transport */
#endif
#include "modinst_zip_bridge.h"   /* worker-zip-to-mfb: F-R1 quarantine->mfb bridge */
#include "../vendored/modforge_loadlist.h"
#include "../vendored/modforge_activation.h"

#define MAX_MODS 256
#define MAX_VARS 512
#define MAX_GAMES 64
#define MAX_SEL 64
#define MAX_CAPS 64
#define LINE_MAX_ 4096

/* exit codes: 0 ok, 1 internal/io, 2 bad request, 3 not ready (blockers),
 * 4 needs choices, 5 cancelled */
#define EX_OK 0
#define EX_IO 1
#define EX_BAD 2
#define EX_NOTREADY 3
#define EX_CHOICES 4
#define EX_CANCELLED 5

static const char *g_state_dir;
static const char *g_roots;
static const char *g_crash_at;      /* --crash-at POINT: _exit(86) at that step */
static long g_cancel_after = -1;    /* --cancel-after N: refuse the Nth staged file write */
static const char *g_cancel_at;     /* --cancel-at POINT: graceful cancel at a safe point */

static void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(EX_IO);
}

static void say(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fputc('\n', stdout);
}

static void maybe_crash(const char *point) {
    if (!g_crash_at || strcmp(g_crash_at, point) != 0) return;
    fprintf(stderr, "SIMULATED CRASH at %s\n", point);
    _exit(86); /* no cleanup: this is the crash being simulated */
}

static int cancelled_now(const char *where) {
    if (g_cancel_at && strcmp(g_cancel_at, where) == 0) return 1;
    return 0;
}

/* ---- small path/fs helpers ------------------------------------------------ */

static void joinpath(char *out, size_t n, const char *a, const char *b) {
    snprintf(out, n, "%s/%s", a, b);
}

static int fsync_dir(const char *path) {
    return ps5io_fsync_dir(path); /* composition: pinned sceKernelFsync */
}

/* composition: write-all is ps5io_write_full (pinned sceKernelWrite loop) */

/* Atomic durable replace: tmp file, fsync, rename, fsync directory. */
static int atomic_write(const char *path, const void *buf, size_t n) {
    char tmp[LINE_MAX_ + 8];
    int fd, rc = 0;
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    fd = ps5io_open(tmp, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_TRUNC, 0644);
    if (fd < 0) return -1;
    if (ps5io_write_full(fd, buf, n) || ps5io_fsync(fd)) rc = -1;
    if (ps5io_close(fd) && !rc) rc = -1;
    if (!rc && ps5io_rename(tmp, path)) rc = -1;
    if (rc) ps5io_unlink(tmp);
    else {
        char dir[LINE_MAX_];
        snprintf(dir, sizeof dir, "%s", path);
        { char *s = strrchr(dir, '/'); if (s) *s = 0; }
        fsync_dir(dir);
    }
    return rc;
}

static int rm_rf(const char *path) {
    /* composition: enumeration/removal through the owned ps5_io_dir binding
     * (pinned dirent + rmdir providers; sceKernelDopen/Dread/Dclose are not
     * exports of the pinned libs — see ps5_io_dir.h receipts). */
    return ps5io_dir_rm_rf(path);
}

static int mkdirs(const char *path) {
    return ps5io_mkdir_p(path, 0755); /* composition: pinned sceKernelMkdir */
}

static int file_exists(const char *path) {
    ps5io_stat st;
    return ps5io_stat_path(path, &st) == 0 && st.exists;
}

static int read_file(const char *path, char **out, size_t *len) {
    ps5io_stat st;
    char *buf;
    size_t got = 0;
    int fd = ps5io_open(path, PS5IO_RDONLY, 0);
    if (fd < 0) return -1;
    if (ps5io_stat_path(path, &st) || st.is_dir == 1) { ps5io_close(fd); return -1; }
    buf = (char *)malloc((size_t)st.size + 1);
    if (!buf) { ps5io_close(fd); return -1; }
    if (st.size && ps5io_read_full(fd, buf, (size_t)st.size, &got)) {
        free(buf); ps5io_close(fd); return -1;
    }
    ps5io_close(fd);
    buf[got] = 0;
    *out = buf;
    *len = got;
    return 0;
}

static void sha256_file(const char *path, char hex[65]) {
    mfb_sha256 sha;
    unsigned char digest[32], buf[65536];
    FILE *f = fopen(path, "rb");
    size_t r;
    int i;
    mfb_sha256_init(&sha);
    if (f) {
        while ((r = fread(buf, 1, sizeof buf, f)) > 0) mfb_sha256_update(&sha, buf, r);
        fclose(f);
    }
    mfb_sha256_final(&sha, digest);
    for (i = 0; i < 32; ++i) sprintf(hex + 2 * i, "%02x", digest[i]);
    hex[64] = 0;
}

static int is_token(const char *s) { /* ids/titles/groups: no whitespace/control */
    size_t i, n = strlen(s);
    if (!n || n > 128) return 0;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c <= 0x20 || c >= 0x7f) return 0;
    }
    return 1;
}

/* copy an mfx_str to a NUL buffer */
static char *sx(mfx_str s, char *buf, size_t n) {
    size_t k = s.n < n - 1 ? s.n : n - 1;
    memcpy(buf, s.p, k);
    buf[k] = 0;
    return buf;
}

/* ---- durable records -------------------------------------------------------
 * Line store (TSV). Integration contract (see REPORT.md): each line maps to
 * the same facts SERVICE.md keeps in records.json; this slice serializes them
 * as tokens so the host driver needs no JSON parser. */
typedef struct {
    char id[128], title[64], state[16], bundle[LINE_MAX_];
} modrec;

typedef struct {
    char mod[128], group[128], option[128], bundle[LINE_MAX_];
    int is_default;
} varrec;

typedef struct {
    char mod[128], group[128], option[128];
} choicerec;

typedef struct {
    char title[64], engine[64], sdk[64];
    long generation;
    char loadlist_sha[65];
    int present;
} gamerec;

static modrec g_mods[MAX_MODS];    static int g_nmods;
static varrec g_vars[MAX_VARS];    static int g_nvars;
static choicerec g_choices[MAX_VARS]; static int g_nchoices;
static gamerec g_games[MAX_GAMES]; static int g_ngames;

static const char *records_path(void) {
    static char p[LINE_MAX_];
    snprintf(p, sizeof p, "%s/records.txt", g_state_dir);
    return p;
}

static modrec *find_mod(const char *id) {
    int i;
    for (i = 0; i < g_nmods; ++i)
        if (!strcmp(g_mods[i].id, id)) return &g_mods[i];
    return 0;
}

static gamerec *find_game(const char *title) {
    int i;
    for (i = 0; i < g_ngames; ++i)
        if (!strcmp(g_games[i].title, title)) return &g_games[i];
    return 0;
}

static gamerec *game_slot(const char *title) {
    gamerec *g = find_game(title);
    if (g) return g;
    if (g_ngames >= MAX_GAMES) die("too many games");
    g = &g_games[g_ngames++];
    memset(g, 0, sizeof *g);
    snprintf(g->title, sizeof g->title, "%s", title);
    g->present = 1;
    return g;
}

static void records_load(void) {
    char *buf; size_t len; char *line, *save = 0;
    g_nmods = g_nvars = g_nchoices = g_ngames = 0;
    if (read_file(records_path(), &buf, &len)) return;
    for (line = strtok_r(buf, "\n", &save); line; line = strtok_r(0, "\n", &save)) {
        char *f[8]; int nf = 0; char *s = line, *t;
        while (nf < 8) {
            t = strchr(s, '\t');
            f[nf++] = s;
            if (!t) break;
            *t = 0;
            s = t + 1;
        }
        if (nf >= 1 && !strcmp(f[0], "mod") && nf >= 5 && g_nmods < MAX_MODS) {
            modrec *m = &g_mods[g_nmods++];
            snprintf(m->id, sizeof m->id, "%s", f[1]);
            snprintf(m->title, sizeof m->title, "%s", f[2]);
            snprintf(m->state, sizeof m->state, "%s", f[3]);
            snprintf(m->bundle, sizeof m->bundle, "%s", f[4]);
        } else if (nf >= 2 && !strcmp(f[0], "variant") && nf >= 6 && g_nvars < MAX_VARS) {
            varrec *v = &g_vars[g_nvars++];
            snprintf(v->mod, sizeof v->mod, "%s", f[1]);
            snprintf(v->group, sizeof v->group, "%s", f[2]);
            snprintf(v->option, sizeof v->option, "%s", f[3]);
            snprintf(v->bundle, sizeof v->bundle, "%s", f[4]);
            v->is_default = atoi(f[5]);
        } else if (nf >= 2 && !strcmp(f[0], "choice") && nf >= 4 && g_nchoices < MAX_VARS) {
            choicerec *c = &g_choices[g_nchoices++];
            snprintf(c->mod, sizeof c->mod, "%s", f[1]);
            snprintf(c->group, sizeof c->group, "%s", f[2]);
            snprintf(c->option, sizeof c->option, "%s", f[3]);
        } else if (nf >= 2 && !strcmp(f[0], "game") && nf >= 6 && g_ngames < MAX_GAMES) {
            gamerec *g = &g_games[g_ngames++];
            snprintf(g->title, sizeof g->title, "%s", f[1]);
            snprintf(g->engine, sizeof g->engine, "%s", f[2]);
            snprintf(g->sdk, sizeof g->sdk, "%s", f[3]);
            g->generation = atol(f[4]);
            snprintf(g->loadlist_sha, sizeof g->loadlist_sha, "%s", f[5]);
            g->present = 1;
        }
    }
    free(buf);
}

static void records_save(void) {
    char out[LINE_MAX_ * (MAX_MODS + MAX_VARS * 2 + MAX_GAMES + 4)];
    size_t used = 0;
    int i;
    used += (size_t)snprintf(out + used, sizeof out - used, "modforge-records\t1\n");
    for (i = 0; i < g_nmods; ++i)
        used += (size_t)snprintf(out + used, sizeof out - used, "mod\t%s\t%s\t%s\t%s\n",
                                 g_mods[i].id, g_mods[i].title, g_mods[i].state,
                                 g_mods[i].bundle[0] ? g_mods[i].bundle : "-");
    for (i = 0; i < g_nvars; ++i)
        used += (size_t)snprintf(out + used, sizeof out - used, "variant\t%s\t%s\t%s\t%s\t%d\n",
                                 g_vars[i].mod, g_vars[i].group, g_vars[i].option,
                                 g_vars[i].bundle, g_vars[i].is_default);
    for (i = 0; i < g_nchoices; ++i)
        used += (size_t)snprintf(out + used, sizeof out - used, "choice\t%s\t%s\t%s\n",
                                 g_choices[i].mod, g_choices[i].group, g_choices[i].option);
    for (i = 0; i < g_ngames; ++i)
        used += (size_t)snprintf(out + used, sizeof out - used, "game\t%s\t%s\t%s\t%ld\t%s\n",
                                 g_games[i].title, g_games[i].engine, g_games[i].sdk,
                                 g_games[i].generation, g_games[i].loadlist_sha);
    if (used >= sizeof out) die("records too large");
    if (atomic_write(records_path(), out, used)) die("cannot write records: %s", strerror(errno));
}

/* ---- capability table ------------------------------------------------------
 * Seeded from the vendored engine facts (engines/unreal.json: pak-loader
 * available, ue4ss-ps5 integrating). Component ids and maturity statuses use
 * the FORMATS.md vocabulary. */
typedef struct { char id[128], maturity[32]; } caprec;
static caprec g_caps[MAX_CAPS];
static int g_ncaps;

static void caps_load(void) {
    char path[LINE_MAX_], *buf; size_t len; char *line, *save = 0;
    g_ncaps = 0;
    snprintf(path, sizeof path, "%s/caps.tsv", g_state_dir);
    if (read_file(path, &buf, &len)) return;
    for (line = strtok_r(buf, "\n", &save); line; line = strtok_r(0, "\n", &save)) {
        char id[128], mat[32];
        if (sscanf(line, "component %127s %31s", id, mat) == 2 && g_ncaps < MAX_CAPS) {
            snprintf(g_caps[g_ncaps].id, sizeof g_caps[0].id, "%s", id);
            snprintf(g_caps[g_ncaps].maturity, sizeof g_caps[0].maturity, "%s", mat);
            g_ncaps++;
        }
    }
    free(buf);
}

static const caprec *find_cap(const char *id) {
    int i;
    for (i = 0; i < g_ncaps; ++i)
        if (!strcmp(g_caps[i].id, id)) return &g_caps[i];
    return 0;
}

/* ---- mod root paths -------------------------------------------------------- */

static void root_path(char *out, size_t n, const char *title) {
    snprintf(out, n, "%s/%s", g_roots, title);
}
static void root_next(char *out, size_t n, const char *title) {
    snprintf(out, n, "%s/%s.next", g_roots, title);
}
static void root_prev(char *out, size_t n, const char *title) {
    snprintf(out, n, "%s/%s.prev", g_roots, title);
}

/* ---- bundle I/O adapter (same shape as the imported tests/c/compose.c) ---- */

typedef struct {
    int fds[MAX_SEL];
    const char *root;        /* target root for writes (the .next staging dir) */
    long writes;             /* count of staged file writes (for --cancel-after) */
    int cancel_flag;         /* set once cancel-after trips */
} bio_ctx;

static int bio_read_at(void *vc, size_t bundle, uint64_t off, void *buf, size_t n) {
    bio_ctx *c = (bio_ctx *)vc;
    /* composition: exact pread over the pinned sceKernel set (lseek+read) */
    return ps5io_pread_exact(c->fds[bundle], off, buf, n);
}

static int bio_make_dirs(void *vc, const char *rel_dir) {
    char path[LINE_MAX_ * 2];
    bio_ctx *c = (bio_ctx *)vc;
    joinpath(path, sizeof path, c->root, rel_dir);
    return ps5io_mkdir_p(path, 0755) == 0 ? 0 : -1;
}

static int bio_open_write(void *vc, const char *rel) {
    char path[LINE_MAX_ * 2];
    bio_ctx *c = (bio_ctx *)vc;
    if (strcmp(rel, MFB_MARKER) != 0) { /* the marker is not a staged mod file */
        c->writes++;
        if (g_cancel_after >= 0 && c->writes > g_cancel_after) {
            c->cancel_flag = 1;
            return -1; /* stop cooperating: the composer aborts before more writes */
        }
    }
    joinpath(path, sizeof path, c->root, rel);
    return ps5io_open(path, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_TRUNC, 0644);
}

static int bio_write(void *vc, int h, const void *buf, size_t n) {
    (void)vc;
    return ps5io_write_full(h, buf, n) == 0 ? 0 : -1;
}

static int bio_close(void *vc, int h) {
    (void)vc;
    return ps5io_close(h);
}

static int bio_remove(void *vc, const char *rel) {
    char path[LINE_MAX_ * 2];
    joinpath(path, sizeof path, ((bio_ctx *)vc)->root, rel);
    return ps5io_unlink(path);
}

/* ---- selection resolution (variants = FOMOD-style choices) -----------------
 * A mod registered with --variant exposes choice groups; each option is a
 * pre-resolved bundle, exactly the shape the host FOMOD runner produces
 * (modforge/fomod.py runs installers host-side; the console picks variants).
 * Group semantics: SelectExactlyOne — default applies when unchosen. */

typedef struct { char bundle[LINE_MAX_]; } selentry;

static const char *stored_choice(const char *mod, const char *group) {
    int i;
    for (i = 0; i < g_nchoices; ++i)
        if (!strcmp(g_choices[i].mod, mod) && !strcmp(g_choices[i].group, group))
            return g_choices[i].option;
    return 0;
}

static void set_stored_choice(const char *mod, const char *group, const char *option) {
    int i;
    for (i = 0; i < g_nchoices; ++i)
        if (!strcmp(g_choices[i].mod, mod) && !strcmp(g_choices[i].group, group)) {
            snprintf(g_choices[i].option, sizeof g_choices[i].option, "%s", option);
            return;
        }
    if (g_nchoices >= MAX_VARS) die("too many choices");
    snprintf(g_choices[g_nchoices].mod, sizeof g_choices[0].mod, "%s", mod);
    snprintf(g_choices[g_nchoices].group, sizeof g_choices[0].group, "%s", group);
    snprintf(g_choices[g_nchoices].option, sizeof g_choices[0].option, "%s", option);
    g_nchoices++;
}

/* Resolve one mod to a bundle path. Exits with the documented codes on error. */
static void resolve_mod(const char *id, char *out, size_t n, const char **extra_choices) {
    modrec *m = find_mod(id);
    int i, nvars = 0, first = 1;
    char groups[8][128]; int ng = 0;
    if (!m) { say("error bad_request unknown mod '%s'", id); exit(EX_BAD); }
    for (i = 0; i < g_nvars; ++i)
        if (!strcmp(g_vars[i].mod, id)) {
            int g, seen = 0;
            nvars++;
            for (g = 0; g < ng; ++g) if (!strcmp(groups[g], g_vars[i].group)) seen = 1;
            if (!seen && ng < 8) snprintf(groups[ng++], sizeof groups[0], "%s", g_vars[i].group);
        }
    if (!nvars) {
        if (!m->bundle[0] || !strcmp(m->bundle, "-"))
            { say("error bad_request mod '%s' has no bundle", id); exit(EX_BAD); }
        snprintf(out, n, "%s", m->bundle);
        return;
    }
    /* variant mod: exactly one option per group */
    for (i = 0; i < ng; ++i) {
        const char *chosen = 0;
        int j;
        if (extra_choices) {
            for (j = 0; extra_choices[j]; j += 2)
                if (!strcmp(extra_choices[j], groups[i])) { chosen = extra_choices[j + 1]; break; }
        }
        if (!chosen) chosen = stored_choice(id, groups[i]);
        if (chosen) {
            int ok = 0;
            for (j = 0; j < g_nvars; ++j)
                if (!strcmp(g_vars[j].mod, id) && !strcmp(g_vars[j].group, groups[i]) &&
                    !strcmp(g_vars[j].option, chosen)) ok = 1;
            if (!ok) {
                char valid[512];
                size_t u = 0;
                for (j = 0; j < g_nvars; ++j)
                    if (!strcmp(g_vars[j].mod, id) && !strcmp(g_vars[j].group, groups[i]))
                        u += (size_t)snprintf(valid + u, sizeof valid - u, "%s%s", u ? "," : "", g_vars[j].option);
                say("error fomod_invalid_choice mod '%s' group '%s': '%s' is not an option (valid: %s)",
                    id, groups[i], chosen, valid);
                exit(EX_BAD);
            }
        } else {
            for (j = 0; j < g_nvars; ++j)
                if (!strcmp(g_vars[j].mod, id) && !strcmp(g_vars[j].group, groups[i]) && g_vars[j].is_default)
                    chosen = g_vars[j].option;
            if (!chosen) {
                say("needs_choices mod '%s' group '%s' has no default; choose with --choices",
                    id, groups[i]);
                exit(EX_CHOICES);
            }
        }
        set_stored_choice(id, groups[i], chosen);
        say("choice mod='%s' %s=%s%s", id, groups[i], chosen,
            extra_choices && chosen ? "" : " (default)");
    }
    /* the option of the first group (alphabetical record order) selects the bundle;
     * multi-group mods would need the host to pre-combine; we take the option
     * stored for the first group encountered in registration order */
    for (i = 0; i < g_nvars; ++i)
        if (!strcmp(g_vars[i].mod, id)) {
            const char *ch = stored_choice(id, g_vars[i].group);
            if (ch && !strcmp(g_vars[i].option, ch)) {
                snprintf(out, n, "%s", g_vars[i].bundle);
                return;
            }
        }
    (void)first;
    say("error internal cannot resolve variants of '%s'", id);
    exit(EX_IO);
}

static int split_mods(const char *csv, char ids[][128], int max) {
    int n = 0;
    const char *p = csv;
    while (*p && n < max) {
        const char *e = strchr(p, ',');
        size_t k = e ? (size_t)(e - p) : strlen(p);
        if (!k || k >= 128) { say("error bad_request bad mod list"); exit(EX_BAD); }
        memcpy(ids[n], p, k);
        ids[n][k] = 0;
        n++;
        p = e ? e + 1 : p + k;
    }
    if (!n) { say("error bad_request empty mod list"); exit(EX_BAD); }
    return n;
}

/* ---- plan: validate, order, conflicts, requirements, capabilities ---------- */

typedef struct {
    mfx_mod mods[MAX_SEL];
    size_t order[MAX_SEL];   /* load order: priority then id */
    int n;
    char bundles[MAX_SEL][LINE_MAX_];
    char title[64];
} plan_t;

static int plan_mod_less(const plan_t *p, size_t a, size_t b) {
    char ia[128], ib[128];
    if (p->mods[a].priority != p->mods[b].priority)
        return p->mods[a].priority < p->mods[b].priority;
    sx(p->mods[a].id, ia, sizeof ia);
    sx(p->mods[b].id, ib, sizeof ib);
    return strcmp(ia, ib) < 0;
}

/* Loads every selected bundle with the vendored loader (full validation:
 * header, index, hashes' syntax, modinfo agreement, path safety). */
static void plan_load(plan_t *p, char ids[][128], int n, const char **extra_choices) {
    bio_ctx bc;
    mfx_io io;
    mfb_error err;
    static unsigned char arena[8u << 20];
    mfx_arena ar;
    int i;
    memset(p, 0, sizeof *p);
    p->n = n;
    memset(&bc, 0, sizeof bc);
    bc.root = "";
    io.ctx = &bc;
    io.read_at = bio_read_at;
    io.make_dirs = bio_make_dirs;
    io.open_write = bio_open_write;
    io.write = bio_write;
    io.close = bio_close;
    io.remove = bio_remove;
    ar.base = arena;
    ar.size = sizeof arena;
    ar.used = 0;
    for (i = 0; i < n; ++i) {
        ps5io_stat st;
        resolve_mod(ids[i], p->bundles[i], sizeof p->bundles[i], extra_choices);
        bc.fds[i] = ps5io_open(p->bundles[i], PS5IO_RDONLY, 0);
        if (bc.fds[i] < 0 || ps5io_stat_path(p->bundles[i], &st))
            { say("error archive_unreadable cannot open bundle for '%s': %s", ids[i], p->bundles[i]); exit(EX_BAD); }
        memset(&p->mods[i], 0, sizeof(mfx_mod));
        if (mfx_load(&io, (size_t)i, st.size, &p->mods[i], &ar, &err)) {
            char mid[128] = "?";
            if (p->mods[i].have_id) sx(p->mods[i].id, mid, sizeof mid);
            say("error archive_unreadable bundle for mod '%s' rejected: %s", mid, err.msg);
            exit(EX_BAD);
        }
    }
    /* game agreement, same rule as the composer */
    for (i = 1; i < n; ++i) {
        char t0[64], ti[64];
        sx(p->mods[0].title, t0, sizeof t0);
        sx(p->mods[i].title, ti, sizeof ti);
        if (strcmp(t0, ti)) { say("error bad_request mod '%s' was built for another game (%s != %s)", ids[i], ti, t0); exit(EX_BAD); }
    }
    { char t[64]; sx(p->mods[0].title, t, sizeof t); snprintf(p->title, sizeof p->title, "%s", t); }
    /* load order: priority then id (insertion sort, same as mfx_compose) */
    for (i = 0; i < n; ++i) {
        size_t v = (size_t)i;
        int j = i;
        while (j > 0 && !plan_mod_less(p, p->order[j - 1], v)) {
            p->order[j] = p->order[j - 1];
            --j;
        }
        p->order[j] = v;
    }
}

typedef struct { char path[1024]; size_t rank; int mod; } plan_file;
static plan_file g_pfiles[MAX_SEL * 256];
static size_t g_npfiles;

/* Returns blockers count; prints plan, conflicts, warnings. ready = no blockers. */
static int plan_report(plan_t *p, char ids[][128], int n, int verbose) {
    int i, j, blockers = 0;
    size_t k;
    g_npfiles = 0;
    for (i = 0; i < n; ++i) {
        mfx_mod *m = &p->mods[p->order[i]];
        for (j = 0; j < (int)m->n_files && g_npfiles < sizeof g_pfiles / sizeof g_pfiles[0]; ++j) {
            plan_file *pf = &g_pfiles[g_npfiles++];
            sx(m->files[j].path, pf->path, sizeof pf->path);
            pf->rank = g_npfiles - 1;
            pf->mod = (int)p->order[i];
        }
    }
    /* sort by (path, rank) to find overlapping dests */
    for (i = 0; (size_t)i < g_npfiles; ++i)
        for (j = i + 1; (size_t)j < g_npfiles; ++j)
            if (strcmp(g_pfiles[i].path, g_pfiles[j].path) > 0 ||
                (!strcmp(g_pfiles[i].path, g_pfiles[j].path) && g_pfiles[i].rank > g_pfiles[j].rank)) {
                plan_file t = g_pfiles[i]; g_pfiles[i] = g_pfiles[j]; g_pfiles[j] = t;
            }
    if (verbose) {
        char t[64];
        say("plan title=%s mods=%d", (sx(p->mods[0].title, t, sizeof t), t), n);
        for (i = 0; i < n; ++i) {
            mfx_mod *m = &p->mods[p->order[i]];
            char mid[128], mv[64];
            say("load-order %ld %s %s", (long)m->priority, sx(m->id, mid, sizeof mid), sx(m->version, mv, sizeof mv));
        }
    }
    for (k = 0; k < g_npfiles; ++k) {
        size_t e = k;
        char prev[128], win[128];
        while (e + 1 < g_npfiles && !strcmp(g_pfiles[e + 1].path, g_pfiles[k].path)) ++e;
        if (e > k) {
            mfx_mod *a = &p->mods[g_pfiles[k].mod], *b = &p->mods[g_pfiles[e].mod];
            say("conflict dest=%s previous=%s winner=%s", g_pfiles[k].path,
                sx(a->id, prev, sizeof prev), sx(b->id, win, sizeof win));
        }
        k = e;
    }
    /* mod -> mod dependencies */
    for (i = 0; i < n; ++i) {
        mfx_mod *m = &p->mods[i];
        char mid[128];
        sx(m->id, mid, sizeof mid);
        for (k = 0; k < m->n_reqs; ++k) {
            char rid[128], rmin[64], dver[64];
            int found = 0;
            sx(m->reqs[k].id, rid, sizeof rid);
            sx(m->reqs[k].min, rmin, sizeof rmin);
            for (j = 0; j < n; ++j)
                if (mfx_cmp(p->mods[j].id, m->reqs[k].id) == 0) { found = j; break; }
            if (!found) {
                say("blocker requirement-unavailable mod '%s' requires mod '%s' >= %s which is not selected",
                    mid, rid, rmin);
                blockers++;
                continue;
            }
            sx(p->mods[found].version, dver, sizeof dver);
            if (mfx_ver_cmp(p->mods[found].version, m->reqs[k].min) < 0) {
                say("blocker requirement-unavailable mod '%s' requires '%s' >= %s but selected version is %s",
                    mid, rid, rmin, dver);
                blockers++;
            }
        }
        /* runtime components: the engine-side capability table */
        for (k = 0; k < m->n_frameworks; ++k) {
            char fid[128];
            const caprec *c;
            sx(m->frameworks[k], fid, sizeof fid);
            c = find_cap(fid);
            if (!c) {
                say("blocker requirement-unavailable framework '%s' required by mod '%s': no runtime component provides this capability on PS5",
                    fid, mid);
                blockers++;
            } else if (!strcmp(c->maturity, "planned")) {
                say("blocker requirement-unavailable framework '%s' required by mod '%s': runtime component is planned (not ported)",
                    fid, mid);
                blockers++;
            } else if (!strcmp(c->maturity, "integrating")) {
                say("warning framework '%s' (mod '%s') is integrating: files install, in-game execution not proven",
                    fid, mid);
            }
        }
    }
    if (verbose) say("ready %d", blockers == 0);
    return blockers;
}

/* ---- journal ----------------------------------------------------------------
 * SERVICE.md journal/<title>.json, extended with a "stage" phase so a crash
 * during staging (before any rename) also recovers deterministically. */
typedef struct {
    char phase[16];
    long generation;
    char title[64], engine[64], sdk[64], mods[LINE_MAX_], ll_sha[65];
} journal_t;

static void journal_path(char *out, size_t n, const char *title) {
    snprintf(out, n, "%s/journal/%s.json", g_state_dir, title);
}

static void journal_write(const char *title, const char *phase, long gen,
                          const char *engine, const char *sdk, const char *mods_csv,
                          const char *ll_sha) {
    char path[LINE_MAX_], buf[LINE_MAX_ * 2];
    int n;
    journal_path(path, sizeof path, title);
    n = snprintf(buf, sizeof buf,
        "{\"schema\": \"modforge-journal/1\", \"phase\": \"%s\", \"generation\": %ld, "
        "\"title\": \"%s\", \"engine\": \"%s\", \"sdk\": \"%s\", \"mods\": \"%s\", "
        "\"ll_sha\": \"%s\"}\n",
        phase, gen, title, engine, sdk, mods_csv, ll_sha ? ll_sha : "");
    if (atomic_write(path, buf, (size_t)n)) die("cannot write journal: %s", strerror(errno));
}

static void journal_delete(const char *title) {
    char path[LINE_MAX_], dir[LINE_MAX_];
    journal_path(path, sizeof path, title);
    ps5io_unlink(path);
    snprintf(dir, sizeof dir, "%s/journal", g_state_dir);
    fsync_dir(dir);
}

static int journal_read(const char *path, journal_t *j) {
    char *buf; size_t len;
    char *p;
    memset(j, 0, sizeof *j);
    if (read_file(path, &buf, &len)) return -1;
#define JSTR(key, field) \
    if ((p = strstr(buf, "\"" key "\": \""))) { \
        char *e; p += strlen(key) + 5; e = strchr(p, '"'); \
        if (e) { size_t kk = (size_t)(e - p); if (kk >= sizeof j->field) kk = sizeof j->field - 1; \
                 memcpy(j->field, p, kk); j->field[kk] = 0; } }
    JSTR("phase", phase)
    JSTR("title", title)
    JSTR("engine", engine)
    JSTR("sdk", sdk)
    JSTR("mods", mods)
    JSTR("ll_sha", ll_sha)
#undef JSTR
    if ((p = strstr(buf, "\"generation\": "))) j->generation = atol(p + 14);
    free(buf);
    return 0;
}

/* Recompute the installed record facts from the live root (used by recovery,
 * which must repair records after a crash between journal steps). The load
 * list is the post-crash truth: mods it names are installed; previously
 * installed mods it no longer names drop back to prepared; downloaded mods
 * that were never part of the transaction stay downloaded. */
static void records_commit_from_root(const char *title, const journal_t *j) {
    gamerec *g = game_slot(title);
    char root[LINE_MAX_], ll[LINE_MAX_ * 2], sha[65];
    char *buf; size_t len;
    int i;
    root_path(root, sizeof root, title);
    joinpath(ll, sizeof ll, root, MFB_LOADLIST);
    sha256_file(ll, sha);
    g->generation = j->generation;
    snprintf(g->engine, sizeof g->engine, "%s", j->engine);
    snprintf(g->sdk, sizeof g->sdk, "%s", j->sdk);
    snprintf(g->loadlist_sha, sizeof g->loadlist_sha, "%s", sha);
    if (!read_file(ll, &buf, &len)) {
        mf_reader r; mf_record rec;
        char ids[MAX_SEL][128]; int nids = 0;
        mf_reader_init(&r, buf, len);
        while (mf_next(&r, &rec) > 0) {
            if (rec.kind == MF_REC_MOD && nids < MAX_SEL && rec.f[0].n < 128) {
                memcpy(ids[nids], rec.f[0].p, rec.f[0].n);
                ids[nids][rec.f[0].n] = 0;
                nids++;
            }
        }
        free(buf);
        for (i = 0; i < g_nmods; ++i) {
            int k, on = 0;
            if (strcmp(g_mods[i].title, title)) continue;
            for (k = 0; k < nids; ++k)
                if (!strcmp(g_mods[i].id, ids[k])) on = 1;
            if (on) snprintf(g_mods[i].state, sizeof g_mods[i].state, "installed");
            else if (!strcmp(g_mods[i].state, "installed"))
                snprintf(g_mods[i].state, sizeof g_mods[i].state, "prepared");
        }
    }
    records_save();
}

/* Recovery per SERVICE.md: the game sees either the old set or the new set,
 * never a mix.
 *   stage phase: the renames never started -> drop the staging root, roll
 *     back to the pre-transaction state.
 *   swap phase: the transaction is committed to forward completion.
 *     root missing -> finish from .next (or roll back from .prev if .next
 *     was lost); root present -> compare the live load list's hash with the
 *     journal's expected hash: equal means the second rename already
 *     happened (finish cleanup, commit records); different means the root is
 *     still the old set (drop .next, records unchanged). */
static void recover_title(const char *title) {
    char jpath[LINE_MAX_], root[LINE_MAX_], next[LINE_MAX_], prev[LINE_MAX_];
    journal_t j;
    journal_path(jpath, sizeof jpath, title);
    if (journal_read(jpath, &j)) return; /* no journal: nothing in flight */
    root_path(root, sizeof root, title);
    root_next(next, sizeof next, title);
    root_prev(prev, sizeof prev, title);
    say("recover title=%s phase=%s generation=%ld", title, j.phase, j.generation);
    if (!strcmp(j.phase, "stage")) {
        rm_rf(next);
        journal_delete(title);
        say("recover title=%s rolled back staging; previous root untouched", title);
        return;
    }
    if (!strcmp(j.phase, "swap")) {
        if (!file_exists(root)) {
            if (file_exists(next)) {
                if (ps5io_rename(next, root)) die("recovery: cannot finish swap: %s", strerror(errno));
                rm_rf(prev);
                journal_delete(title);
                fsync_dir(g_roots);
                records_commit_from_root(title, &j);
                say("recover title=%s finished swap forward (new set active, generation %ld)", title, j.generation);
                return;
            } else if (file_exists(prev)) {
                if (ps5io_rename(prev, root)) die("recovery: cannot roll back swap: %s", strerror(errno));
                journal_delete(title);
                fsync_dir(g_roots);
                say("recover title=%s rolled back swap (previous set restored byte-for-byte)", title);
                return;
            }
            die("recovery: root missing and neither .next nor .prev exists; refusing to guess");
        }
        {
            char ll[LINE_MAX_ * 2], sha[65];
            joinpath(ll, sizeof ll, root, MFB_LOADLIST);
            sha256_file(ll, sha);
            if (j.ll_sha[0] && !strcmp(sha, j.ll_sha)) {
                /* crash after the second rename: finish cleanup, commit */
                rm_rf(next);
                rm_rf(prev);
                journal_delete(title);
                fsync_dir(g_roots);
                records_commit_from_root(title, &j);
                say("recover title=%s committed (new set already active, generation %ld)", title, j.generation);
                return;
            }
            /* the root is still the old set: the first rename never happened */
            rm_rf(next);
            journal_delete(title);
            fsync_dir(g_roots);
            say("recover title=%s rolled back before swap (previous set still active)", title);
            return;
        }
    }
    die("recovery: unknown journal phase '%s'", j.phase);
}

static void recover_all(void) {
    char jdir[LINE_MAX_];
    snprintf(jdir, sizeof jdir, "%s/journal", g_state_dir);
    mkdirs(jdir);
    /* composition: enumeration through the owned ps5_io_dir binding (pinned
     * dirent providers; the pure sceKernel D-op set is absent — see
     * ps5_io_dir.h). */
    ps5io_dir_each_json(jdir, recover_title);
}

/* ---- the install transaction ------------------------------------------------ */

static void write_empty_root(const char *dir, const char *title, const char *engine, const char *sdk) {
    char path[LINE_MAX_ * 2], buf[512];
    int fd, n;
    static const char marker_text[] =
        "This directory is managed by modforge; its contents are replaced on each install.\n";
    if (mkdirs(dir)) die("cannot create empty root");
    joinpath(path, sizeof path, dir, MFB_MARKER);
    fd = ps5io_open(path, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_TRUNC, 0644);
    if (fd < 0 || ps5io_write_full(fd, marker_text, sizeof marker_text - 1) || ps5io_close(fd))
        die("cannot write marker");
    /* minimal load list: header + title + engine (+ sdk); zero mods, zero files */
    if (*sdk)
        n = snprintf(buf, sizeof buf, "modforge-loadlist\t1\ntitle\t%s\nengine\t%s\nsdk\t%s\n",
                     title, engine, sdk);
    else
        n = snprintf(buf, sizeof buf, "modforge-loadlist\t1\ntitle\t%s\nengine\t%s\n",
                     title, engine);
    joinpath(path, sizeof path, dir, MFB_LOADLIST);
    fd = ps5io_open(path, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_TRUNC, 0644);
    if (fd < 0 || ps5io_write_full(fd, buf, (size_t)n) || ps5io_fsync(fd) || ps5io_close(fd))
        die("cannot write empty load list");
    fsync_dir(dir);
}

static void cmd_install(const char *title, const char *mods_csv, const char *remove_csv,
                        const char **extra_choices) {
    char ids[MAX_SEL][128], rmids[MAX_SEL][128];
    int n = 0, nrm = 0, i, blockers;
    plan_t plan;
    bio_ctx bc;
    mfx_io io;
    mfb_error err;
    static unsigned char arena[8u << 20], work[1u << 20];
    char root[LINE_MAX_], next[LINE_MAX_], prev[LINE_MAX_], mods_out[LINE_MAX_];
    gamerec *g;
    long newgen;
    size_t ml = 0;
    char engine[64] = "", sdk[64] = "";

    if (mods_csv) n = split_mods(mods_csv, ids, MAX_SEL);
    if (remove_csv) nrm = split_mods(remove_csv, rmids, MAX_SEL);
    if (!n && !nrm) { say("error bad_request install needs --mods or --remove"); exit(EX_BAD); }

    /* Which mods end up installed? Current installed set plus selections,
     * minus removals. Re-selection = the explicit list. */
    g = find_game(title);
    if (nrm || !n) {
        /* uninstall path: rebuild from the currently installed mods minus removed */
        int k;
        for (i = 0; i < g_nmods && n < MAX_SEL; ++i) {
            int skip = 0;
            if (strcmp(g_mods[i].title, title)) continue;
            if (strcmp(g_mods[i].state, "installed")) continue;
            for (k = 0; k < nrm; ++k) if (!strcmp(g_mods[i].id, rmids[k])) skip = 1;
            if (!skip) snprintf(ids[n++], sizeof ids[0], "%s", g_mods[i].id);
        }
        mods_csv = 0; /* ids[] now carries the full surviving set */
    }
    for (i = 0; i < n; ++i) ml += (size_t)snprintf(mods_out + ml, sizeof mods_out - ml, "%s%s", i ? "," : "", ids[i]);

    /* Validate + plan BEFORE touching the filesystem. */
    if (n) {
        plan_load(&plan, ids, n, extra_choices);
        if (strcmp(plan.title, title)) {
            say("error bad_request bundles are for title '%s', not '%s'", plan.title, title);
            exit(EX_BAD);
        }
        blockers = plan_report(&plan, ids, n, 1);
        if (blockers) { say("error not_ready install blocked by %d unavailable requirement(s)", blockers); exit(EX_NOTREADY); }
        {
            char e[64], s[64];
            sx(plan.mods[0].engine, e, sizeof e);
            snprintf(engine, sizeof engine, "%s", e);
            if (plan.mods[0].have_sdk) { sx(plan.mods[0].sdk, s, sizeof s); snprintf(sdk, sizeof sdk, "%s", s); }
        }
    } else if (g) {
        snprintf(engine, sizeof engine, "%s", g->engine);
        snprintf(sdk, sizeof sdk, "%s", g->sdk);
    } else { say("error unknown_title '%s' (nothing installed, nothing selected)", title); exit(EX_BAD); }

    root_path(root, sizeof root, title);
    root_next(next, sizeof next, title);
    root_prev(prev, sizeof prev, title);
    newgen = (g ? g->generation : 0) + 1;

    /* ownership guard: never replace a directory modforge did not create */
    if (file_exists(root)) {
        char marker[LINE_MAX_ * 2];
        joinpath(marker, sizeof marker, root, MFB_MARKER);
        if (!file_exists(marker)) {
            say("error bad_request refusing to replace unmanaged mod root %s (missing %s marker)",
                root, MFB_MARKER);
            exit(EX_BAD);
        }
    }

    /* ---- phase stage: build the complete new root under .next ---- */
    if (rm_rf(next)) die("cannot clear stale staging root");
    if (mkdir(next, 0755) && errno != EEXIST) die("cannot create staging root");
    journal_write(title, "stage", newgen, engine, sdk, mods_out, "");
    maybe_crash("stage-journal");
    if (cancelled_now("pre-stage")) {
        rm_rf(next); journal_delete(title);
        say("cancelled before staging; previous root untouched");
        exit(EX_CANCELLED);
    }
    if (n) {
        uint64_t sizes[MAX_SEL];
        long written;
        memset(&bc, 0, sizeof bc);
        bc.root = next;
        for (i = 0; i < n; ++i) {
            ps5io_stat st;
            bc.fds[i] = ps5io_open(plan.bundles[i], PS5IO_RDONLY, 0);
            if (bc.fds[i] < 0 || ps5io_stat_path(plan.bundles[i], &st)) die("cannot reopen bundle");
            sizes[i] = st.size;
        }
        io.ctx = &bc;
        io.read_at = bio_read_at;
        io.make_dirs = bio_make_dirs;
        io.open_write = bio_open_write;
        io.write = bio_write;
        io.close = bio_close;
        io.remove = bio_remove;
        written = mfx_compose(&io, sizes, (size_t)n, title, arena, sizeof arena, work, sizeof work, &err);
        for (i = 0; i < n; ++i) ps5io_close(bc.fds[i]);
        if (written < 0) {
            rm_rf(next); journal_delete(title);
            if (bc.cancel_flag) {
                say("cancelled during staging after %ld file(s); previous root untouched", g_cancel_after);
                exit(EX_CANCELLED);
            }
            say("error internal compose failed: %s", err.msg);
            exit(EX_IO);
        }
        fsync_dir(next);
        say("staged files=%ld root=%s", written, next);
    } else {
        write_empty_root(next, title, engine, sdk);
        say("staged files=0 root=%s (empty mod set)", next);
    }
    maybe_crash("staged");
    if (cancelled_now("post-stage") || cancelled_now("pre-swap")) {
        rm_rf(next); journal_delete(title);
        say("cancelled after staging; previous root untouched");
        exit(EX_CANCELLED);
    }

    /* ---- phase swap: journaled rename sequence (SERVICE.md) ----
     * The journal records the sha256 the staged load list must have once the
     * new root is live; recovery uses it to tell "second rename already
     * happened" from "first rename never happened". */
    {
        char llp[LINE_MAX_ * 2], llsha[65];
        joinpath(llp, sizeof llp, next, MFB_LOADLIST);
        sha256_file(llp, llsha);
        journal_write(title, "swap", newgen, engine, sdk, mods_out, llsha);
    }
    maybe_crash("swap-journal");
    if (cancelled_now("swap-journal")) {
        rm_rf(next); journal_delete(title);
        say("cancelled before swap; previous root untouched");
        exit(EX_CANCELLED);
    }
    if (file_exists(root)) {
        if (ps5io_rename(root, prev)) die("cannot move current root aside: %s", strerror(errno));
        fsync_dir(g_roots);
    }
    maybe_crash("rename1");
    if (cancelled_now("rename1")) {
        /* between the renames: rolling back restores the original bytes */
        if (ps5io_rename(prev, root)) die("cannot roll back first rename: %s", strerror(errno));
        rm_rf(next); journal_delete(title); fsync_dir(g_roots);
        say("cancelled between renames; rolled back, previous root restored");
        exit(EX_CANCELLED);
    }
    if (ps5io_rename(next, root)) die("cannot swap new root in: %s", strerror(errno));
    fsync_dir(g_roots);
    maybe_crash("rename2");
    rm_rf(prev);
    maybe_crash("prevdel");
    journal_delete(title);

    /* ---- commit records ---- */
    {
        char ll[LINE_MAX_ * 2], sha[65];
        joinpath(ll, sizeof ll, root, MFB_LOADLIST);
        sha256_file(ll, sha);
        g = game_slot(title);
        g->generation = newgen;
        snprintf(g->engine, sizeof g->engine, "%s", engine);
        snprintf(g->sdk, sizeof g->sdk, "%s", sdk);
        snprintf(g->loadlist_sha, sizeof g->loadlist_sha, "%s", sha);
        for (i = 0; i < g_nmods; ++i) {
            int k, on = 0;
            if (strcmp(g_mods[i].title, title)) continue;
            for (k = 0; k < n; ++k) if (!strcmp(g_mods[i].id, ids[k])) on = 1;
            if (on) snprintf(g_mods[i].state, sizeof g_mods[i].state, "installed");
            else if (!strcmp(g_mods[i].state, "installed"))
                snprintf(g_mods[i].state, sizeof g_mods[i].state, "prepared");
        }
        records_save();
        say("installed title=%s generation=%ld mods=%s loadlist=%s", title, newgen, mods_out, sha);
    }
}

/* ---- commands -------------------------------------------------------------- */

static void cmd_register(const char *id, const char *title, const char *bundle,
                         int nvar, char vgroup[][64], char vopt[][64], char vbundle[][LINE_MAX_],
                         const char *defaults) {
    modrec *m;
    int i;
    if (!is_token(id) || !is_token(title))
        { say("error bad_request mod id and title must be tokens"); exit(EX_BAD); }
    if (find_mod(id)) { say("error bad_request mod '%s' already registered", id); exit(EX_BAD); }
    if (nvar && bundle) { say("error bad_request a mod has either a base bundle or variants, not both"); exit(EX_BAD); }
    if (!nvar && !bundle) { say("error bad_request a mod needs --bundle or --variant"); exit(EX_BAD); }
    for (i = 0; i < nvar; ++i)
        if (!is_token(vgroup[i]) || !is_token(vopt[i]))
            { say("error bad_request variant group/option must be tokens"); exit(EX_BAD); }
    if (g_nmods >= MAX_MODS) die("too many mods");
    m = &g_mods[g_nmods++];
    memset(m, 0, sizeof *m);
    snprintf(m->id, sizeof m->id, "%s", id);
    snprintf(m->title, sizeof m->title, "%s", title);
    snprintf(m->state, sizeof m->state, "downloaded");
    if (bundle) snprintf(m->bundle, sizeof m->bundle, "%s", bundle);
    for (i = 0; i < nvar; ++i) {
        varrec *v;
        int j, dups = 0;
        for (j = 0; j < i; ++j)
            if (!strcmp(vgroup[j], vgroup[i]) && !strcmp(vopt[j], vopt[i])) dups = 1;
        if (dups) { say("error bad_request duplicate variant %s/%s", vgroup[i], vopt[i]); exit(EX_BAD); }
        if (g_nvars >= MAX_VARS) die("too many variants");
        v = &g_vars[g_nvars++];
        snprintf(v->mod, sizeof v->mod, "%s", id);
        snprintf(v->group, sizeof v->group, "%s", vgroup[i]);
        snprintf(v->option, sizeof v->option, "%s", vopt[i]);
        snprintf(v->bundle, sizeof v->bundle, "%s", vbundle[i]);
        v->is_default = 0;
    }
    /* defaults: "g=o,g2=o2" */
    if (defaults) {
        char buf[512], *pair, *save = 0;
        snprintf(buf, sizeof buf, "%s", defaults);
        for (pair = strtok_r(buf, ",", &save); pair; pair = strtok_r(0, ",", &save)) {
            char *eq = strchr(pair, '=');
            int hit = 0;
            if (!eq) { say("error bad_request bad default '%s' (want group=option)", pair); exit(EX_BAD); }
            *eq = 0;
            for (i = 0; i < g_nvars; ++i)
                if (!strcmp(g_vars[i].mod, id) && !strcmp(g_vars[i].group, pair) &&
                    !strcmp(g_vars[i].option, eq + 1)) { g_vars[i].is_default = 1; hit = 1; }
            if (!hit) { say("error bad_request default '%s=%s' is not a variant of '%s'", pair, eq + 1, id); exit(EX_BAD); }
        }
    }
    records_save();
    say("registered mod=%s title=%s state=downloaded variants=%d", id, title, nvar);
}

static void cmd_prepare(const char *title, const char *mods_csv, const char **extra_choices) {
    char ids[MAX_SEL][128];
    int n = split_mods(mods_csv, ids, MAX_SEL), i, blockers;
    plan_t plan;
    plan_load(&plan, ids, n, extra_choices);
    if (strcmp(plan.title, title))
        { say("error bad_request bundles are for title '%s', not '%s'", plan.title, title); exit(EX_BAD); }
    blockers = plan_report(&plan, ids, n, 1);
    for (i = 0; i < n; ++i) {
        modrec *m = find_mod(ids[i]);
        if (m) snprintf(m->state, sizeof m->state, "prepared");
    }
    { gamerec *g = game_slot(title);
      char e[64], s[64];
      sx(plan.mods[0].engine, e, sizeof e);
      snprintf(g->engine, sizeof g->engine, "%s", e);
      if (plan.mods[0].have_sdk) { sx(plan.mods[0].sdk, s, sizeof s); snprintf(g->sdk, sizeof g->sdk, "%s", s); } }
    records_save();
    if (blockers) exit(EX_NOTREADY);
}

/* worker-e2e-final gap-1: a successful stage-2a prepare-archive is a durable
 * downloaded->prepared transition in this records line store, exactly the
 * transition cmd_prepare performs (worker-7's model: register=downloaded,
 * prepare=prepared, install=installed; the load-list sha256 remains the only
 * install/recovery commit truth and is never touched here). Without --mod the
 * run is reporting-only. The quarantine is never erased on refusal. */
static void records_mark_archive_prepared(int argc, char **argv) {
    const char *mod = 0;
    int i;
    modrec *m;
    for (i = 1; i + 1 < argc; ++i)
        if (!strcmp(argv[i], "--mod") && argv[i + 1][0]) mod = argv[i + 1];
    if (!mod) return;
    m = find_mod(mod);
    if (!m) {
        say("error bad_request unknown mod '%s' for prepare-archive (quarantine retained)", mod);
        exit(EX_BAD);
    }
    if (!strcmp(m->state, "prepared")) { say("records unchanged mod=%s already prepared", mod); return; }
    if (strcmp(m->state, "downloaded")) {
        say("error bad_request mod '%s' is '%s'; only downloaded->prepared is defined (quarantine retained)", mod, m->state);
        exit(EX_BAD);
    }
    snprintf(m->state, sizeof m->state, "prepared");
    records_save();
    say("records prepared mod=%s title=%s", mod, m->title);
}
#ifdef MI_HAS_DOWNLOAD
/* worker-e2e-final gap-2: durable registration for a finished download job.
 * The mod row (state downloaded, bundle = the content-addressed intake
 * archive) is the download command's LAST durable write — the record is the
 * commit point, mirroring the load-list-sha rule for installs. */
static void records_register_download_job(int argc, char **argv) {
    const char *job = 0;
    int i;
    char *buf, *line, *save = 0;
    size_t len;
    char id[128] = "", title[64] = "", intake[LINE_MAX_] = "", final[LINE_MAX_ * 2];
    modrec *m;
    for (i = 1; i + 1 < argc; ++i)
        if (!strcmp(argv[i], "--job") && argv[i + 1][0]) job = argv[i + 1];
    if (!job) { say("error bad_request download needs --job"); exit(EX_BAD); }
    if (read_file(job, &buf, &len)) { say("error bad_request cannot read job file"); exit(EX_BAD); }
    for (line = strtok_r(buf, "\n", &save); line; line = strtok_r(0, "\n", &save)) {
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = 0;
        if (!strcmp(line, "id") && !id[0]) snprintf(id, sizeof id, "%s", tab + 1);
        else if (!strcmp(line, "title") && !title[0]) snprintf(title, sizeof title, "%s", tab + 1);
        else if (!strcmp(line, "intake") && !intake[0]) snprintf(intake, sizeof intake, "%s", tab + 1);
    }
    free(buf);
    if (!id[0] || !title[0] || !intake[0])
        { say("error bad_request job file lacks id/title/intake"); exit(EX_BAD); }
    snprintf(final, sizeof final, "%s/%s.zip", intake, id);
    m = find_mod(id);
    if (m) {
        if (strcmp(m->state, "downloaded") || strcmp(m->bundle, final))
            { say("error bad_request mod '%s' already registered differently", id); exit(EX_BAD); }
        say("records unchanged mod=%s state=downloaded", id);
        return;
    }
    if (g_nmods >= MAX_MODS) die("too many mods");
    m = &g_mods[g_nmods++];
    memset(m, 0, sizeof *m);
    snprintf(m->id, sizeof m->id, "%s", id);
    snprintf(m->title, sizeof m->title, "%s", title);
    snprintf(m->state, sizeof m->state, "downloaded");
    snprintf(m->bundle, sizeof m->bundle, "%s", final);
    records_save();
    say("registered mod=%s title=%s state=downloaded bundle=%s", id, title, final);
}
#endif
/* running is derived, never stored: the runtime's own status file is the only
 * source (SERVICE.md: mark running only from a real runtime acknowledgment). */
static void read_runtime_status(const char *root, char *state, size_t n, char *detail, size_t dn) {
    char path[LINE_MAX_ * 2], *buf, *line, *save = 0;
    size_t len;
    snprintf(state, n, "not-started");
    if (detail) snprintf(detail, dn, "");
    joinpath(path, sizeof path, root, "runtime-status.txt");
    if (read_file(path, &buf, &len)) return;
    for (line = strtok_r(buf, "\n", &save); line; line = strtok_r(0, "\n", &save)) {
        /* state <TAB> seq <TAB> state <TAB> code <TAB> detail  (written by mfa_status_write) */
        char *f[6]; int nf = 0; char *s = line, *t;
        while (nf < 6) {
            t = strchr(s, '\t');
            f[nf++] = s;
            if (!t) break;
            *t = 0; s = t + 1;
        }
        if (nf >= 4 && !strcmp(f[0], "state")) {
            snprintf(state, n, "%s", f[2]);
            if (detail && nf >= 5) snprintf(detail, dn, "%s", f[4]);
        }
    }
    free(buf);
}

static void cmd_records(const char *title) {
    int i;
    for (i = 0; i < g_nmods; ++i) {
        if (title && strcmp(g_mods[i].title, title)) continue;
        say("mod %s %s %s", g_mods[i].id, g_mods[i].title, g_mods[i].state);
    }
    for (i = 0; i < g_ngames; ++i) {
        char root[LINE_MAX_], state[32], detail[160];
        gamerec *g = &g_games[i];
        if (title && strcmp(g->title, title)) continue;
        root_path(root, sizeof root, g->title);
        read_runtime_status(root, state, sizeof state, detail, sizeof detail);
        say("game %s generation=%ld engine=%s loadlist=%s", g->title, g->generation,
            g->engine, g->loadlist_sha);
        say("runtime %s running=%d%s%s", state, !strcmp(state, "mods-started"),
            detail[0] ? " detail=" : "", detail[0] ? detail : "");
    }
}

/* Plumbing for the engine runtime's half of the install. This runs the REAL
 * activation component against the installed root (the same thing the
 * injected runtime does at module_start) and appends a status line through
 * the real mfa_status_* API. It is a host-side simulation of the runtime's
 * bookkeeping, NOT engine execution: "running" below means the runtime
 * acknowledged activation through its status file, nothing more. */
static void cmd_simulate_runtime(const char *title, const char *state, int code, const char *detail) {
    char root[LINE_MAX_];
    mfa_config cfg;
    mfa_activation *act = 0;
    mfa_error err;
    mfa_status st;
    int rc;
    uint64_t now = 0;
    gamerec *g = find_game(title);
    if (!g) { say("error unknown_title '%s'", title); exit(EX_BAD); }
    root_path(root, sizeof root, title);
    memset(&cfg, 0, sizeof cfg);
    cfg.root = root;
    cfg.title_id = (char *)title;
    rc = mfa_activate(&cfg, &act, &err);
    if (rc != MFA_OK) {
        say("activation-check failed code=%d (%s) os=%d path=%s line=%d",
            rc, mfa_result_name(rc), err.os_error, err.path, err.line);
        exit(EX_IO);
    }
    say("activation-check ok title=%s mods=%lu files=%lu preloaded=%lu bytes",
        mfa_title(act), (unsigned long)mfa_mod_count(act), (unsigned long)mfa_file_count(act),
        (unsigned long)mfa_preloaded_bytes(act));
    mfa_release(act);
    if (ps5io_now_ms(&now)) now = 0; /* composition: sceKernelClockGettime */
    rc = mfa_status_open(&st, root, (long)getpid(), now);
    if (rc) { say("error io_error mfa_status_open: %d", rc); exit(EX_IO); }
    rc = mfa_status_write(&st, state, code, detail);
    mfa_status_close(&st);
    if (rc) { say("error io_error mfa_status_write: %d", rc); exit(EX_IO); }
    say("runtime-status state=%s code=%d (written through modforge_activation mfa_status API)", state, code);
}

/* ---- test-fixture builder: kind-1 mod bundles ------------------------------
 * Writes exactly the format modforge_bundle.h reads (FORMATS.md "Mod bundles"):
 * header kind 1, index (offset,size,sha256,u16 path), data, modinfo.txt whose
 * file list equals the entries. Test-only tool; real bundles come from the
 * host builder (modforge bundle mod). */
static void cmd_make_bundle(int argc, char **argv) {
    const char *out = 0, *id = "mod", *ver = "1.0.0", *title = "PPSA33286",
               *engine = "unreal", *sdk = 0, *lo = "manifest_priority";
    long prio = 100;
    const char *fw[32]; int nfw = 0;
    const char *req[32]; int nreq = 0;
    const char *base[32]; int nbase = 0;
    struct { char role[32], path[1024], content[4096]; } files[256]; int nfiles = 0;
    struct { char name[256], flag[8]; } plugs[64]; int nplugs = 0;
    struct { char plug[256], master[256]; } masters[256]; int nmast = 0;
    char modinfo[64 * 1024]; size_t mi = 0;
    FILE *f;
    int i;
    unsigned char *idx = 0, *data = 0;
    size_t idx_len = 0, data_len = 0, pos;
    uint32_t count;
    char **ap = argv + 2;

#define NEXTARG(val) do { ++ap; if (!*ap) { say("error bad_request missing value for %s", ap[-1]); exit(EX_BAD); } val = *ap; } while (0)
    for (; *ap; ++ap) {
        if (!strcmp(*ap, "--id")) NEXTARG(id);
        else if (!strcmp(*ap, "--ver")) NEXTARG(ver);
        else if (!strcmp(*ap, "--prio")) { ++ap; if (!*ap) exit(EX_BAD); prio = atol(*ap); }
        else if (!strcmp(*ap, "--title")) NEXTARG(title);
        else if (!strcmp(*ap, "--engine")) NEXTARG(engine);
        else if (!strcmp(*ap, "--sdk")) NEXTARG(sdk);
        else if (!strcmp(*ap, "--load-order")) NEXTARG(lo);
        else if (!strcmp(*ap, "--framework")) { ++ap; if (!*ap || nfw >= 32) exit(EX_BAD); fw[nfw++] = *ap; }
        else if (!strcmp(*ap, "--requires")) { ++ap; if (!*ap || nreq >= 32) exit(EX_BAD); req[nreq++] = *ap; }
        else if (!strcmp(*ap, "--base")) { ++ap; if (!*ap || nbase >= 32) exit(EX_BAD); base[nbase++] = *ap; }
        else if (!strcmp(*ap, "--file")) {
            char *spec, *eq, *c;
            ++ap; if (!*ap || nfiles >= 256) exit(EX_BAD);
            spec = *ap; eq = strchr(spec, '='); if (!eq) exit(EX_BAD);
            *eq = 0;
            c = strchr(spec, ','); if (!c) exit(EX_BAD);
            *c = 0;
            snprintf(files[nfiles].role, sizeof files[0].role, "%s", spec);
            snprintf(files[nfiles].path, sizeof files[0].path, "%s", c + 1);
            snprintf(files[nfiles].content, sizeof files[0].content, "%s", eq + 1);
            nfiles++;
        }
        else if (!strcmp(*ap, "--plugin")) {
            char *spec, *c;
            ++ap; if (!*ap || nplugs >= 64) exit(EX_BAD);
            spec = *ap; c = strchr(spec, ','); if (!c) exit(EX_BAD);
            *c = 0;
            snprintf(plugs[nplugs].name, sizeof plugs[0].name, "%s", spec);
            snprintf(plugs[nplugs].flag, sizeof plugs[0].flag, "%s", c + 1);
            nplugs++;
        }
        else if (!strcmp(*ap, "--master")) {
            char *spec, *c;
            ++ap; if (!*ap || nmast >= 256) exit(EX_BAD);
            spec = *ap; c = strchr(spec, ','); if (!c) exit(EX_BAD);
            *c = 0;
            snprintf(masters[nmast].plug, sizeof masters[0].plug, "%s", spec);
            snprintf(masters[nmast].master, sizeof masters[0].master, "%s", c + 1);
            nmast++;
        }
        else if (!strcmp(*ap, "-o")) NEXTARG(out);
        else { say("error bad_request unknown make-bundle flag %s", *ap); exit(EX_BAD); }
    }
#undef NEXTARG
    if (!out) { say("error bad_request make-bundle needs -o"); exit(EX_BAD); }
    if (!strcmp(lo, "plugins_txt")) { if (!nplugs) { nplugs = 1; snprintf(plugs[0].name, sizeof plugs[0].name, "Dummy.esp"); snprintf(plugs[0].flag, sizeof plugs[0].flag, "-"); } }

    mi += (size_t)snprintf(modinfo + mi, sizeof modinfo - mi, "modforge-modinfo\t1\n");
    mi += (size_t)snprintf(modinfo + mi, sizeof modinfo - mi, "mod\t%s\t%s\t%ld\n", id, ver, prio);
    mi += (size_t)snprintf(modinfo + mi, sizeof modinfo - mi, "title\t%s\n", title);
    mi += (size_t)snprintf(modinfo + mi, sizeof modinfo - mi, "engine\t%s\n", engine);
    if (sdk) mi += (size_t)snprintf(modinfo + mi, sizeof modinfo - mi, "sdk\t%s\n", sdk);
    mi += (size_t)snprintf(modinfo + mi, sizeof modinfo - mi, "load_order\t%s\n", lo);
    for (i = 0; i < nfw; ++i) mi += (size_t)snprintf(modinfo + mi, sizeof modinfo - mi, "framework\t%s\n", fw[i]);
    for (i = 0; i < nreq; ++i) {
        char *eq = strchr(req[i], '=');
        if (eq) *eq = '\t';
        mi += (size_t)snprintf(modinfo + mi, sizeof modinfo - mi, "requires\t%s\n", req[i]);
    }
    for (i = 0; i < nbase; ++i) mi += (size_t)snprintf(modinfo + mi, sizeof modinfo - mi, "base\t%s\n", base[i]);
    for (i = 0; i < nfiles; ++i) mi += (size_t)snprintf(modinfo + mi, sizeof modinfo - mi, "file\t%s\t%s\n", files[i].role, files[i].path);
    for (i = 0; i < nplugs; ++i) mi += (size_t)snprintf(modinfo + mi, sizeof modinfo - mi, "plugin\t%s\t%s\n", plugs[i].name, plugs[i].flag);
    for (i = 0; i < nmast; ++i) mi += (size_t)snprintf(modinfo + mi, sizeof modinfo - mi, "master\t%s\t%s\n", masters[i].plug, masters[i].master);

    /* entries: files..., modinfo.txt */
    count = (uint32_t)(nfiles + 1);
    idx_len = (size_t)count * 50;
    for (i = 0; i < nfiles; ++i) idx_len += strlen(files[i].path);
    idx_len += strlen(MFB_MODINFO);
    idx = (unsigned char *)calloc(1, idx_len + 16);
    data_len = 0;
    for (i = 0; i < nfiles; ++i) data_len += strlen(files[i].content);
    data_len += mi;
    data = (unsigned char *)calloc(1, data_len + 16);
    if (!idx || !data) die("out of memory");
    pos = 0;
    {
        uint64_t doff = 32 + (uint64_t)idx_len;
        unsigned char *ie = idx;
        unsigned char *dp = data;
        for (i = 0; i < nfiles; ++i) {
            mfb_sha256 sha; unsigned char dg[32]; int k;
            size_t pl = strlen(files[i].path), cl = strlen(files[i].content);
            memcpy(dp, files[i].content, cl); dp += cl;
            mfb_sha256_init(&sha); mfb_sha256_update(&sha, files[i].content, cl); mfb_sha256_final(&sha, dg);
            for (k = 0; k < 8; ++k) ie[k] = (unsigned char)(doff >> (8 * k));
            for (k = 0; k < 8; ++k) ie[8 + k] = (unsigned char)((uint64_t)cl >> (8 * k));
            memcpy(ie + 16, dg, 32);
            ie[48] = (unsigned char)(pl & 0xff); ie[49] = (unsigned char)(pl >> 8);
            memcpy(ie + 50, files[i].path, pl);
            ie += 50 + pl;
            doff += cl;
        }
        {
            mfb_sha256 sha; unsigned char dg[32]; int k;
            size_t pl = strlen(MFB_MODINFO);
            memcpy(dp, modinfo, mi); dp += mi;
            mfb_sha256_init(&sha); mfb_sha256_update(&sha, modinfo, mi); mfb_sha256_final(&sha, dg);
            for (k = 0; k < 8; ++k) ie[k] = (unsigned char)(doff >> (8 * k));
            for (k = 0; k < 8; ++k) ie[8 + k] = (unsigned char)((uint64_t)mi >> (8 * k));
            memcpy(ie + 16, dg, 32);
            ie[48] = (unsigned char)(pl & 0xff); ie[49] = (unsigned char)(pl >> 8);
            memcpy(ie + 50, MFB_MODINFO, pl);
        }
    }
    f = fopen(out, "wb");
    if (!f) die("cannot open %s", out);
    {
        unsigned char header[32];
        memset(header, 0, sizeof header);
        memcpy(header, "MFBUNDL1", 8);
        header[8] = 1;                     /* version 1 */
        header[12] = (unsigned char)(count & 0xff);
        header[13] = (unsigned char)((count >> 8) & 0xff);
        header[14] = (unsigned char)((count >> 16) & 0xff);
        header[15] = (unsigned char)((count >> 24) & 0xff);
        for (i = 0; i < 8; ++i) header[16 + i] = (unsigned char)((uint64_t)idx_len >> (8 * i));
        header[24] = MFB_KIND_MOD;         /* kind 1: mod bundle */
        if (fwrite(header, 1, 32, f) != 32) die("write header");
        if (fwrite(idx, 1, idx_len, f) != idx_len) die("write index");
        if (fwrite(data, 1, data_len, f) != data_len) die("write data");
    }
    fclose(f);
    free(idx); free(data);
    say("bundle %s id=%s files=%d", out, id, nfiles);
}

/* ---- main ------------------------------------------------------------------- */

static void usage(void) {
    say("usage: modinst --state DIR --roots BASE <command> [options]");
    say("  register-mod --id ID --title TITLE [--bundle FILE | --variant GROUP/OPTION=FILE ...] [--defaults g=o,...]");
    say("  prepare --title T --mods a,b [--choices g=o,...]");
    say("  install --title T [--mods a,b] [--remove x,y] [--choices g=o,...]");
    say("          [--cancel-after N | --cancel-at pre-stage|post-stage|swap-journal|rename1]");
    say("          [--crash-at stage-journal|staged|swap-journal|rename1|rename2|prevdel]");
    say("  records [--title T]");
    say("  simulate-runtime --title T --rt-state activated|engine-ready|mods-started|failed|stopped [--code N] [--detail TEXT]");
    say("  make-bundle --id ID ... --file role,path=content ... -o OUT.mfb   (test fixture)");
    say("  download --job JOB.tsv   (stage 1: transport -> intake/<id>.zip + records downloaded)");
    say("  prepare-archive --title T --archive F --quarantine D [--mod ID]   (stage 2a)");
    say("          [--no-auto-bundle]  (F-R1 bridge: with --mod, default composes bundles/<id>.mfb from the quarantine)");
    say("  recover");
}

int main(int argc, char **argv) {
    const char *cmd = 0, *title = 0, *mods = 0, *remove = 0, *id = 0, *bundle = 0,
               *defaults = 0, *choices = 0, *statedir = 0, *rtstate = 0, *detail = 0,
               *crash = 0, *cancelat = 0;
    int code = 0, i;
    char vgroup[32][64], vopt[32][64], vbundle[32][LINE_MAX_];
    int nvar = 0;
    char state_dir[LINE_MAX_], roots[LINE_MAX_];

    /* make-bundle owns its own flag set (fixture builder); bypass the store */
    if (argc > 1 && !strcmp(argv[1], "make-bundle")) {
        cmd_make_bundle(argc, argv);
        return EX_OK;
    }

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--state") && i + 1 < argc) { statedir = argv[++i]; }
        else if (!strcmp(argv[i], "--roots") && i + 1 < argc) { g_roots = argv[++i]; }
        else if (!strcmp(argv[i], "--title") && i + 1 < argc) title = argv[++i];
        else if (!strcmp(argv[i], "--mods") && i + 1 < argc) mods = argv[++i];
        else if (!strcmp(argv[i], "--remove") && i + 1 < argc) remove = argv[++i];
        else if (!strcmp(argv[i], "--id") && i + 1 < argc) id = argv[++i];
        else if (!strcmp(argv[i], "--bundle") && i + 1 < argc) bundle = argv[++i];
        else if (!strcmp(argv[i], "--defaults") && i + 1 < argc) defaults = argv[++i];
        else if (!strcmp(argv[i], "--choices") && i + 1 < argc) choices = argv[++i];
        else if (!strcmp(argv[i], "--archive") && i + 1 < argc) ++i;      /* prepare-archive (owned command parses it) */
        else if (!strcmp(argv[i], "--quarantine") && i + 1 < argc) ++i;   /* prepare-archive */
        else if (!strcmp(argv[i], "--mod") && i + 1 < argc) ++i;          /* prepare-archive records wiring (worker-e2e-final) */
        else if (!strcmp(argv[i], "--job") && i + 1 < argc) ++i;          /* download (owned command parses it) */
        else if (!strcmp(argv[i], "--no-auto-bundle")) ;         /* value-less opt-out (worker-zip-to-mfb bridge parses it) */
        else if (!strcmp(argv[i], "--rt-state") && i + 1 < argc) rtstate = argv[++i];
        else if (!strcmp(argv[i], "--state-name") && i + 1 < argc) rtstate = argv[++i];
        else if (!strcmp(argv[i], "--code") && i + 1 < argc) code = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--detail") && i + 1 < argc) detail = argv[++i];
        else if (!strcmp(argv[i], "--crash-at") && i + 1 < argc) crash = argv[++i];
        else if (!strcmp(argv[i], "--cancel-at") && i + 1 < argc) cancelat = argv[++i];
        else if (!strcmp(argv[i], "--cancel-after") && i + 1 < argc) g_cancel_after = atol(argv[++i]);
        else if (!strcmp(argv[i], "--variant") && i + 1 < argc) {
            char *spec = argv[++i], *slash = strchr(spec, '/'), *eq = strchr(spec, '=');
            if (!slash || !eq || eq < slash || nvar >= 32) { say("error bad_request bad --variant"); exit(EX_BAD); }
            *slash = 0; *eq = 0;
            if (!is_token(spec) || !is_token(slash + 1)) { say("error bad_request variant group/option must be tokens"); exit(EX_BAD); }
            snprintf(vgroup[nvar], sizeof vgroup[0], "%s", spec);
            snprintf(vopt[nvar], sizeof vopt[0], "%s", slash + 1);
            snprintf(vbundle[nvar], sizeof vbundle[0], "%s", eq + 1);
            nvar++;
        }
        else if (!cmd) cmd = argv[i];
        else { usage(); return EX_BAD; }
    }
    if (!cmd || !g_roots) { usage(); return EX_BAD; }
    if (!statedir) statedir = "state";
    if (statedir[0] != '/') {
        if (!getcwd(state_dir, sizeof state_dir)) die("getcwd");
        { size_t l = strlen(state_dir); snprintf(state_dir + l, sizeof state_dir - l, "/%s", statedir); }
    } else snprintf(state_dir, sizeof state_dir, "%s", statedir);
    g_state_dir = state_dir;
    e2e_status_set_dir(g_state_dir);
    snprintf(roots, sizeof roots, "%s", g_roots);
    g_roots = roots;
    g_crash_at = crash;
    g_cancel_at = cancelat;

    if (mkdirs((char *)g_state_dir)) die("cannot create state dir %s", g_state_dir);
    { char jd[LINE_MAX_]; snprintf(jd, sizeof jd, "%s/journal", g_state_dir); mkdirs(jd); }
    if (mkdirs((char *)g_roots)) die("cannot create mod root base %s", g_roots);

    e2e_status_begin(cmd, title);

    /* every open recovers interrupted transactions first (mfs_open semantics) */
    records_load();
    caps_load();
    if (strcmp(cmd, "make-bundle") != 0) recover_all();

    /* choices: "g=o,g2=o2" -> flat array. The parse buffer must outlive the
     * dispatch below: extra[] points into it. */
    {
        static const char *extra[64];
        int nextra = 0;
        char chbuf[512], *pair, *save = 0;
        if (choices) {
            snprintf(chbuf, sizeof chbuf, "%s", choices);
            for (pair = strtok_r(chbuf, ",", &save); pair; pair = strtok_r(0, ",", &save)) {
                char *eq = strchr(pair, '=');
                if (!eq || nextra + 2 >= 64) { say("error bad_request bad --choices"); exit(EX_BAD); }
                *eq = 0;
                extra[nextra++] = pair;
                extra[nextra++] = eq + 1;
            }
            extra[nextra] = 0;
        }
        if (!strcmp(cmd, "register-mod")) {
            if (!id || !title) { usage(); return EX_BAD; }
            cmd_register(id, title, bundle, nvar, vgroup, vopt, vbundle, defaults);
        } else if (!strcmp(cmd, "prepare")) {
            if (!title || !mods) { usage(); return EX_BAD; }
            cmd_prepare(title, mods, extra[0] ? extra : 0);
        } else if (!strcmp(cmd, "install")) {
            if (!title) { usage(); return EX_BAD; }
            cmd_install(title, mods, remove, extra[0] ? extra : 0);
        } else if (!strcmp(cmd, "records")) {
            cmd_records(title);
        } else if (!strcmp(cmd, "recover")) {
            /* recover_all() already ran above */
        } else if (!strcmp(cmd, "prepare-archive")) {
            /* composition: E2E stage 2a — archive -> quarantine via the
             * pinned 3.8.7 intake closure through the vendored bridge. */
            { int e2e_pr = mi_cmd_prepare_archive(argc, argv);
              /* worker-e2e-final gap-1: durable downloaded->prepared records
               * transition on success (records_mark_archive_prepared); the
               * end line is emitted here, not skipped. */
              if (e2e_pr == 0 && mi_bridge_wants_auto(argc, argv)) {
                  /* worker-zip-to-mfb (F-R1): compose a kind-1 mfb from the
                   * quarantine BEFORE the records transition; on success the
                   * record's bundle becomes the bridge output so install
                   * resolves a real modforge bundle. A refusal keeps the
                   * record at downloaded and the quarantine retained. */
                  char bridge_bundle[LINE_MAX_ * 2];
                  e2e_pr = mi_bridge_run(argc, argv, g_state_dir,
                                         bridge_bundle, sizeof bridge_bundle);
                  if (e2e_pr == 0) {
                      modrec *bm = find_mod(mi_bridge_mod_arg(argc, argv));
                      if (bm) snprintf(bm->bundle, sizeof bm->bundle, "%s", bridge_bundle);
                  }
              }
              if (e2e_pr == 0) records_mark_archive_prepared(argc, argv);
              e2e_status_end(e2e_pr);
              return e2e_pr; }
        } else if (!strcmp(cmd, "download")) {
#ifdef MI_HAS_DOWNLOAD
            /* worker-e2e-final gap-2: E2E stage 1 through the exclusive
             * curl-owner transport ABI v1 (mb_curl_runtime_start/download/
             * close); explicit job config only; the registration row is the
             * command's last durable write. */
            { int e2e_dr = mi_cmd_download(argc, argv, g_state_dir);
              if (e2e_dr == 0) records_register_download_job(argc, argv);
              e2e_status_end(e2e_dr);
              return e2e_dr; }
#else
            say("error bad_request download transport not linked into this flavor");
            return EX_BAD;
#endif
        } else if (!strcmp(cmd, "simulate-runtime")) {
            if (!title || !rtstate) { usage(); return EX_BAD; }
            cmd_simulate_runtime(title, rtstate, code, detail ? detail : "");
        } else {
            usage();
            return EX_BAD;
        }
    }
    e2e_status_end(EX_OK);
    return EX_OK;
}
