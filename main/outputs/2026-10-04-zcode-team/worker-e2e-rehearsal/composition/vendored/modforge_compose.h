/*
 * modforge_compose.h — merge per-mod bundles on the console.
 *
 * The catalog host pre-builds one mod bundle per mod per game (header kind 1:
 * the mod's files at their final paths plus modinfo.txt). When the user picks
 * several mods, the console app calls mfx_compose(), which does on the PS5
 * what modforge's deploy does on the host:
 *
 *   - validates every bundle completely before writing anything
 *     (header, index, paths, ranges, modinfo hash and syntax, and that
 *     modinfo's file list matches the bundle's entries exactly);
 *   - checks the bundles were built for this game and agree with each other;
 *   - orders mods by load-order priority, then id; a later mod wins a file;
 *   - checks mod dependencies and minimum versions;
 *   - collects the console components the mods need (load-list "framework");
 *   - for Bethesda games, builds the master-aware plugin load order
 *     (masters first, every plugin after its masters, otherwise priority) and
 *     refuses missing masters, masters depending on regular plugins, and cycles;
 *   - installs every winning file with its SHA-256 checked, and writes
 *     loadlist.txt LAST.
 *
 * Its output is byte-identical to the host path (deploy + game bundle); the
 * test suite checks that. Format details: modforge/modbundle.py.
 *
 * Single header, C99 (and C++), no libc calls, no allocation: the caller
 * passes an arena (scratch memory for the tables, e.g. a few MB; the error
 * message says if it's too small) and a work buffer for file copies. The mod
 * root must be empty or freshly cleared.
 */
#ifndef MODFORGE_COMPOSE_H
#define MODFORGE_COMPOSE_H

#include "modforge_bundle.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MFX_MAX_MODINFO (1u << 20)
#define MFX_MAX_FIELDS 4

typedef struct {
    void *ctx;
    /* Read exactly n bytes at offset off of bundle number `bundle`. 0 = ok. */
    int (*read_at)(void *ctx, size_t bundle, uint64_t off, void *buf, size_t n);
    /* The rest act on the mod root, exactly as in mfb_io. */
    int (*make_dirs)(void *ctx, const char *rel_dir);
    int (*open_write)(void *ctx, const char *rel_path);
    int (*write)(void *ctx, int handle, const void *buf, size_t n);
    int (*close)(void *ctx, int handle);
    int (*remove)(void *ctx, const char *rel_path); /* optional */
} mfx_io;

/* ---- small utilities --------------------------------------------------- */

typedef struct {
    const char *p;
    size_t n;
} mfx_str;

typedef struct {
    unsigned char *base;
    size_t size, used;
} mfx_arena;

static inline void *mfx_alloc(mfx_arena *a, size_t n) {
    size_t off = (a->used + 7u) & ~(size_t)7u;
    if (off > a->size || n > a->size - off) return 0;
    a->used = off + n;
    return a->base + off;
}

static inline int mfx_cmp(mfx_str a, mfx_str b) {
    size_t i, n = a.n < b.n ? a.n : b.n;
    for (i = 0; i < n; ++i) {
        unsigned char x = (unsigned char)a.p[i], y = (unsigned char)b.p[i];
        if (x != y) return x < y ? -1 : 1;
    }
    return a.n == b.n ? 0 : (a.n < b.n ? -1 : 1);
}

static inline int mfx_eq_lit(mfx_str s, const char *lit) {
    size_t i = 0;
    for (; lit[i]; ++i)
        if (i >= s.n || s.p[i] != lit[i]) return 0;
    return i == s.n;
}

static inline unsigned char mfx_lc(unsigned char c) { return (c >= 'A' && c <= 'Z') ? (unsigned char)(c + 32) : c; }

static inline int mfx_eq_ci(mfx_str a, mfx_str b) {
    size_t i;
    if (a.n != b.n) return 0;
    for (i = 0; i < a.n; ++i)
        if (mfx_lc((unsigned char)a.p[i]) != mfx_lc((unsigned char)b.p[i])) return 0;
    return 1;
}

/* ASCII-case-insensitive glob with '*' and '?' (same as the host's). */
static inline int mfx_glob(mfx_str pat, mfx_str name) {
    size_t pi = 0, ni = 0, star_p = (size_t)-1, star_n = 0;
    while (ni < name.n) {
        if (pi < pat.n && (pat.p[pi] == '?' ||
                           mfx_lc((unsigned char)pat.p[pi]) == mfx_lc((unsigned char)name.p[ni]))) {
            ++pi;
            ++ni;
        } else if (pi < pat.n && pat.p[pi] == '*') {
            star_p = pi++;
            star_n = ni;
        } else if (star_p != (size_t)-1) {
            pi = star_p + 1;
            ni = ++star_n;
        } else {
            return 0;
        }
    }
    while (pi < pat.n && pat.p[pi] == '*') ++pi;
    return pi == pat.n;
}

/* One dot-separated part of a version: [*beg, *end); advances *pos past it. */
static inline void mfx_ver_part(mfx_str s, size_t *pos, size_t *beg, size_t *end) {
    *beg = *pos;
    while (*pos < s.n && s.p[*pos] != '.') ++*pos;
    *end = *pos;
    ++*pos;
}

/* Number of significant digits in a part (ASCII digits only, leading zeros
 * dropped); *first is the index of the first significant digit. */
static inline size_t mfx_ver_digits(mfx_str s, size_t beg, size_t end, size_t *first) {
    size_t i, n = 0;
    *first = end;
    for (i = beg; i < end; ++i) {
        if (s.p[i] < '0' || s.p[i] > '9') continue;
        if (!n && s.p[i] == '0') continue;
        if (!n) *first = i;
        ++n;
    }
    return n;
}

/* Compare dotted versions exactly like the host: per dot-separated part keep
 * only ASCII digits and compare them as numbers (of any length, no overflow);
 * missing parts count as 0. Returns <0, 0, >0. */
static inline int mfx_ver_cmp(mfx_str a, mfx_str b) {
    size_t pa = 0, pb = 0;
    while (pa <= a.n || pb <= b.n) {
        size_t ab = 0, ae = 0, bb = 0, be = 0, fa = 0, fb = 0, na, nb, k;
        if (pa <= a.n) mfx_ver_part(a, &pa, &ab, &ae);
        if (pb <= b.n) mfx_ver_part(b, &pb, &bb, &be);
        na = (ab < ae) ? mfx_ver_digits(a, ab, ae, &fa) : 0;
        nb = (bb < be) ? mfx_ver_digits(b, bb, be, &fb) : 0;
        if (na != nb) return na < nb ? -1 : 1;
        for (k = 0; k < na; ++k, ++fa, ++fb) {
            while (a.p[fa] < '0' || a.p[fa] > '9') ++fa;
            while (b.p[fb] < '0' || b.p[fb] > '9') ++fb;
            if (a.p[fa] != b.p[fb]) return a.p[fa] < b.p[fb] ? -1 : 1;
        }
    }
    return 0;
}

typedef int (*mfx_less_fn)(const void *ctx, size_t a, size_t b);

/* In-place heapsort of an index array by a strict ordering (no ties expected). */
static inline void mfx_sift(size_t *v, size_t start, size_t end, mfx_less_fn less, const void *ctx) {
    size_t root = start;
    while (2 * root + 1 < end) {
        size_t child = 2 * root + 1, swap = root;
        if (less(ctx, v[swap], v[child])) swap = child;
        if (child + 1 < end && less(ctx, v[swap], v[child + 1])) swap = child + 1;
        if (swap == root) return;
        {
            size_t t = v[root];
            v[root] = v[swap];
            v[swap] = t;
        }
        root = swap;
    }
}

static inline void mfx_sort(size_t *v, size_t n, mfx_less_fn less, const void *ctx) {
    size_t start, end;
    if (n < 2) return;
    for (start = n / 2; start-- > 0;) mfx_sift(v, start, n, less, ctx);
    for (end = n - 1; end > 0; --end) {
        size_t t = v[0];
        v[0] = v[end];
        v[end] = t;
        mfx_sift(v, 0, end, less, ctx);
    }
}

/* ---- per-bundle data ---------------------------------------------------- */

typedef struct {
    uint64_t offset, size;
    unsigned char sha256[32];
    mfx_str path; /* NUL-terminated in the arena */
} mfx_entry;

typedef struct { mfx_str id, min; } mfx_req;
typedef struct { mfx_str role, path; const mfx_entry *entry; } mfx_file;
typedef struct { mfx_str name; char flag; } mfx_plugin;
typedef struct { size_t plugin; mfx_str name; } mfx_master;

typedef struct {
    size_t bundle;
    mfx_entry *entries;
    size_t n_entries;          /* excluding modinfo.txt */
    size_t *entry_order;       /* entries sorted by path */
    char *modinfo;
    size_t modinfo_len;
    mfx_str id, version, title, engine, sdk, load_order;
    int64_t priority;
    int have_id, have_title, have_engine, have_sdk, have_lo;
    mfx_str *frameworks; size_t n_frameworks;
    mfx_req *reqs; size_t n_reqs;
    mfx_str *base; size_t n_base;
    mfx_file *files; size_t n_files;
    mfx_plugin *plugins; size_t n_plugins;
    mfx_master *masters; size_t n_masters;
} mfx_mod;

/* Adapter so the installer's validated helpers can read from one bundle. */
typedef struct {
    const mfx_io *io;
    size_t bundle;
} mfx_adapter;

static inline int mfx_ad_read(void *c, uint64_t off, void *buf, size_t n) {
    mfx_adapter *a = (mfx_adapter *)c;
    return a->io->read_at(a->io->ctx, a->bundle, off, buf, n);
}
static inline int mfx_ad_mkdirs(void *c, const char *d) {
    mfx_adapter *a = (mfx_adapter *)c;
    return a->io->make_dirs(a->io->ctx, d);
}
static inline int mfx_ad_open(void *c, const char *p) {
    mfx_adapter *a = (mfx_adapter *)c;
    return a->io->open_write(a->io->ctx, p);
}
static inline int mfx_ad_write(void *c, int h, const void *b, size_t n) {
    mfx_adapter *a = (mfx_adapter *)c;
    return a->io->write(a->io->ctx, h, b, n);
}
static inline int mfx_ad_close(void *c, int h) {
    mfx_adapter *a = (mfx_adapter *)c;
    return a->io->close(a->io->ctx, h);
}
static inline int mfx_ad_remove(void *c, const char *p) {
    mfx_adapter *a = (mfx_adapter *)c;
    return a->io->remove(a->io->ctx, p);
}
static inline void mfx_adapt(mfb_io *out, mfx_adapter *a) {
    out->ctx = a;
    out->read_at = mfx_ad_read;
    out->make_dirs = mfx_ad_mkdirs;
    out->open_write = mfx_ad_open;
    out->write = mfx_ad_write;
    out->close = mfx_ad_close;
    out->remove = a->io->remove ? mfx_ad_remove : 0;
}

static inline int mfx_entry_less(const void *ctx, size_t a, size_t b) {
    const mfx_entry *e = (const mfx_entry *)ctx;
    return mfx_cmp(e[a].path, e[b].path) < 0;
}

static inline int mfx_file_less(const void *ctx, size_t a, size_t b) {
    const mfx_file *f = (const mfx_file *)ctx;
    return mfx_cmp(f[a].path, f[b].path) < 0;
}

/* ---- modinfo parsing ---------------------------------------------------- */

typedef struct {
    const char *cur, *end;
} mfx_lines;

/* Next line (without '\n' and a trailing '\r'); 0 at end. */
static inline int mfx_next_line(mfx_lines *it, mfx_str *line) {
    const char *ls, *le;
    if (it->cur >= it->end) return 0;
    ls = le = it->cur;
    while (le < it->end && *le != '\n') ++le;
    it->cur = (le < it->end) ? le + 1 : le;
    if (le > ls && le[-1] == '\r') --le;
    line->p = ls;
    line->n = (size_t)(le - ls);
    return 1;
}

/* Split on TAB; returns number of fields, or -1 if more than max. */
static inline int mfx_split(mfx_str line, mfx_str *f, int max) {
    int n = 0;
    size_t i, start = 0;
    for (i = 0; i <= line.n; ++i) {
        if (i == line.n || line.p[i] == '\t') {
            if (n >= max) return -1;
            f[n].p = line.p + start;
            f[n].n = i - start;
            ++n;
            start = i + 1;
        }
    }
    return n;
}

static inline int mfx_is_role(mfx_str s) {
    return mfx_eq_lit(s, "lua_mod") || mfx_eq_lit(s, "script") || mfx_eq_lit(s, "pak") ||
           mfx_eq_lit(s, "native_plugin") || mfx_eq_lit(s, "data_plugin") ||
           mfx_eq_lit(s, "config") || mfx_eq_lit(s, "asset_override");
}

static inline int mfx_parse_i64(mfx_str s, int64_t *out) {
    size_t i = 0, digits;
    int neg = 0;
    int64_t v = 0;
    if (s.n && s.p[0] == '-') { neg = 1; i = 1; }
    digits = s.n - i;
    if (digits == 0 || digits > 18) return 0;
    for (; i < s.n; ++i) {
        if (s.p[i] < '0' || s.p[i] > '9') return 0;
        v = v * 10 + (s.p[i] - '0');
    }
    *out = neg ? -v : v;
    return 1;
}

static inline int mfx_parse_modinfo(mfx_mod *m, mfx_arena *ar, mfb_error *err) {
    mfx_lines it;
    mfx_str line, f[MFX_MAX_FIELDS + 1];
    size_t c_fw = 0, c_req = 0, c_base = 0, c_file = 0, c_plug = 0, c_mast = 0, i, j;
    int seen = 0, nf;

    /* Pass 1: count records by kind, to size the tables exactly. */
    it.cur = m->modinfo;
    it.end = m->modinfo + m->modinfo_len;
    while (mfx_next_line(&it, &line)) {
        size_t k = 0;
        if (!line.n || line.p[0] == '#') continue;
        while (k < line.n && line.p[k] != '\t') ++k;
        {
            mfx_str kind;
            kind.p = line.p;
            kind.n = k;
            if (mfx_eq_lit(kind, "framework")) ++c_fw;
            else if (mfx_eq_lit(kind, "requires")) ++c_req;
            else if (mfx_eq_lit(kind, "base")) ++c_base;
            else if (mfx_eq_lit(kind, "file")) ++c_file;
            else if (mfx_eq_lit(kind, "plugin")) ++c_plug;
            else if (mfx_eq_lit(kind, "master")) ++c_mast;
        }
    }
    m->frameworks = (mfx_str *)mfx_alloc(ar, (c_fw + 1) * sizeof(mfx_str));
    m->reqs = (mfx_req *)mfx_alloc(ar, (c_req + 1) * sizeof(mfx_req));
    m->base = (mfx_str *)mfx_alloc(ar, (c_base + 1) * sizeof(mfx_str));
    m->files = (mfx_file *)mfx_alloc(ar, (c_file + 1) * sizeof(mfx_file));
    m->plugins = (mfx_plugin *)mfx_alloc(ar, (c_plug + 1) * sizeof(mfx_plugin));
    m->masters = (mfx_master *)mfx_alloc(ar, (c_mast + 1) * sizeof(mfx_master));
    if (!m->frameworks || !m->reqs || !m->base || !m->files || !m->plugins || !m->masters) {
        mfb_fail(err, "compose arena is too small (modinfo tables)", 0);
        return -1;
    }

    /* Pass 2: validate and fill. */
    it.cur = m->modinfo;
    while (mfx_next_line(&it, &line)) {
        if (!line.n || line.p[0] == '#') continue;
        nf = mfx_split(line, f, MFX_MAX_FIELDS);
        if (nf < 0) { mfb_fail(err, "modinfo: too many fields", 0); return -1; }
        for (i = 0; i < (size_t)nf; ++i) {
            if (!f[i].n) { mfb_fail(err, "modinfo: empty field", 0); return -1; }
            for (j = 0; j < f[i].n; ++j) {
                unsigned char c = (unsigned char)f[i].p[j];
                if (c < 0x20 || c == 0x7f) { mfb_fail(err, "modinfo: control character", 0); return -1; }
            }
        }
        if (!seen) {
            if (nf != 2 || !mfx_eq_lit(f[0], "modforge-modinfo") || !mfx_eq_lit(f[1], "1")) {
                mfb_fail(err, "modinfo: bad header", 0);
                return -1;
            }
            seen = 1;
            continue;
        }
#define MFX_NEED(cnt) do { if (nf != (cnt)) { mfb_fail(err, "modinfo: wrong field count", 0); return -1; } } while (0)
#define MFX_ONCE(flag) do { if (flag) { mfb_fail(err, "modinfo: duplicate record", 0); return -1; } (flag) = 1; } while (0)
        if (mfx_eq_lit(f[0], "mod")) {
            MFX_NEED(4);
            MFX_ONCE(m->have_id);
            if (!mfx_parse_i64(f[3], &m->priority)) { mfb_fail(err, "modinfo: bad priority", 0); return -1; }
            m->id = f[1];
            m->version = f[2];
        } else if (mfx_eq_lit(f[0], "title")) {
            MFX_NEED(2); MFX_ONCE(m->have_title); m->title = f[1];
        } else if (mfx_eq_lit(f[0], "engine")) {
            MFX_NEED(2); MFX_ONCE(m->have_engine); m->engine = f[1];
        } else if (mfx_eq_lit(f[0], "sdk")) {
            MFX_NEED(2); MFX_ONCE(m->have_sdk); m->sdk = f[1];
        } else if (mfx_eq_lit(f[0], "load_order")) {
            MFX_NEED(2);
            if (!mfx_eq_lit(f[1], "manifest_priority") && !mfx_eq_lit(f[1], "plugins_txt")) {
                mfb_fail(err, "modinfo: bad load order", 0);
                return -1;
            }
            MFX_ONCE(m->have_lo);
            m->load_order = f[1];
        } else if (mfx_eq_lit(f[0], "framework")) {
            MFX_NEED(2); m->frameworks[m->n_frameworks++] = f[1];
        } else if (mfx_eq_lit(f[0], "requires")) {
            MFX_NEED(3);
            m->reqs[m->n_reqs].id = f[1];
            m->reqs[m->n_reqs].min = f[2];
            m->n_reqs++;
        } else if (mfx_eq_lit(f[0], "base")) {
            MFX_NEED(2); m->base[m->n_base++] = f[1];
        } else if (mfx_eq_lit(f[0], "file")) {
            MFX_NEED(3);
            if (!mfx_is_role(f[1])) { mfb_fail(err, "modinfo: unknown role", 0); return -1; }
            if (f[2].n > MFB_MAX_PATH || !mfb_path_ok(f[2].p, f[2].n)) {
                mfb_fail(err, "modinfo: unsafe file path", 0);
                return -1;
            }
            {
                mfx_str marker;
                marker.p = MFB_MARKER;
                marker.n = sizeof(MFB_MARKER) - 1;
                if (mfx_cmp(f[2], marker) == 0) { mfb_fail(err, "modinfo: unsafe file path", 0); return -1; }
            }
            m->files[m->n_files].role = f[1];
            m->files[m->n_files].path = f[2];
            m->files[m->n_files].entry = 0;
            m->n_files++;
        } else if (mfx_eq_lit(f[0], "plugin")) {
            MFX_NEED(3);
            if (f[2].n != 1 || (f[2].p[0] != 'm' && f[2].p[0] != '-' && f[2].p[0] != '?')) {
                mfb_fail(err, "modinfo: bad plugin flags", 0);
                return -1;
            }
            for (i = 0; i < m->n_plugins; ++i)
                if (mfx_eq_ci(m->plugins[i].name, f[1])) { mfb_fail(err, "modinfo: duplicate plugin", 0); return -1; }
            m->plugins[m->n_plugins].name = f[1];
            m->plugins[m->n_plugins].flag = f[2].p[0];
            m->n_plugins++;
        } else if (mfx_eq_lit(f[0], "master")) {
            size_t pi = (size_t)-1;
            MFX_NEED(3);
            for (i = 0; i < m->n_plugins; ++i)
                if (mfx_eq_ci(m->plugins[i].name, f[1])) { pi = i; break; }
            if (pi == (size_t)-1 || m->plugins[pi].flag == '?') {
                mfb_fail(err, "modinfo: master for an unknown plugin", 0);
                return -1;
            }
            m->masters[m->n_masters].plugin = pi;
            m->masters[m->n_masters].name = f[2];
            m->n_masters++;
        }
        /* unknown kinds are skipped */
#undef MFX_NEED
#undef MFX_ONCE
    }
    if (!seen) { mfb_fail(err, "modinfo is empty", 0); return -1; }
    if (!m->have_id || !m->have_title || !m->have_engine || !m->have_lo) {
        mfb_fail(err, "modinfo is missing a required record", 0);
        return -1;
    }
    if (!m->have_sdk) {
        m->sdk.p = "";
        m->sdk.n = 0;
    }
    return 0;
}

/* Load one mod bundle: header, index, modinfo; check they agree. */
static inline int mfx_load(const mfx_io *io, size_t b, uint64_t size, mfx_mod *m,
                           mfx_arena *ar, mfb_error *err) {
    unsigned char header[MFB_HEADER_SIZE];
    const char *magic = "MFBUNDL1";
    uint32_t count, i;
    uint64_t index_size, data_start, pos;
    mfx_adapter ad;
    mfb_io bio;
    mfb_entry e;
    mfb_entry mi;
    size_t modinfos = 0, k;
    size_t *file_order;

    ad.io = io;
    ad.bundle = b;
    mfx_adapt(&bio, &ad);
    m->bundle = b;
    mfb_zero(&mi, sizeof mi);

    if (size < MFB_HEADER_SIZE || io->read_at(io->ctx, b, 0, header, MFB_HEADER_SIZE)) {
        mfb_fail(err, "mod bundle is truncated", 0);
        return -1;
    }
    for (i = 0; i < 8; ++i)
        if (header[i] != (unsigned char)magic[i]) { mfb_fail(err, "not a modforge bundle", 0); return -1; }
    if (mfb_le32(header + 8) != 1) { mfb_fail(err, "unsupported bundle version", 0); return -1; }
    if (mfb_le32(header + 24) != MFB_KIND_MOD) {
        mfb_fail(err, mfb_le32(header + 24) == MFB_KIND_GAME
                          ? "this is a game bundle: install it with mfb_install"
                          : "unknown bundle kind", 0);
        return -1;
    }
    count = mfb_le32(header + 12);
    index_size = mfb_le64(header + 16);
    if (count > MFB_MAX_ENTRIES || index_size > size - MFB_HEADER_SIZE) {
        mfb_fail(err, "bundle index is implausible", 0);
        return -1;
    }
    data_start = MFB_HEADER_SIZE + index_size;

    m->entries = (mfx_entry *)mfx_alloc(ar, ((size_t)count + 1) * sizeof(mfx_entry));
    if (!m->entries) { mfb_fail(err, "compose arena is too small (bundle index)", 0); return -1; }
    m->n_entries = 0;
    pos = MFB_HEADER_SIZE;
    for (i = 0; i < count; ++i) {
        if (mfb_read_entry(&bio, &pos, data_start, data_start, size, &e, err)) return -1;
        if (mfb_streq(e.path, MFB_LOADLIST)) { mfb_fail(err, "a mod bundle must not contain " MFB_LOADLIST, 0); return -1; }
        if (mfb_streq(e.path, MFB_MODINFO)) {
            mi = e;
            modinfos++;
            continue;
        }
        {
            mfx_entry *x = &m->entries[m->n_entries++];
            size_t n = 0;
            char *copy;
            while (e.path[n]) ++n;
            copy = (char *)mfx_alloc(ar, n + 1);
            if (!copy) { mfb_fail(err, "compose arena is too small (paths)", 0); return -1; }
            for (k = 0; k <= n; ++k) copy[k] = e.path[k];
            x->offset = e.offset;
            x->size = e.size;
            for (k = 0; k < 32; ++k) x->sha256[k] = e.sha256[k];
            x->path.p = copy;
            x->path.n = n;
        }
    }
    if (pos != data_start) { mfb_fail(err, "bundle index size does not match its entries", 0); return -1; }
    if (modinfos != 1) { mfb_fail(err, "a mod bundle must contain exactly one " MFB_MODINFO, 0); return -1; }
    if (mi.size > MFX_MAX_MODINFO) { mfb_fail(err, "modinfo is too large", 0); return -1; }

    /* Read modinfo, check its hash and UTF-8, parse it. */
    m->modinfo_len = (size_t)mi.size;
    m->modinfo = (char *)mfx_alloc(ar, m->modinfo_len + 1);
    if (!m->modinfo) { mfb_fail(err, "compose arena is too small (modinfo)", 0); return -1; }
    if (m->modinfo_len && io->read_at(io->ctx, b, mi.offset, m->modinfo, m->modinfo_len)) {
        mfb_fail(err, "read error in modinfo", 0);
        return -1;
    }
    {
        mfb_sha256 sha;
        unsigned char digest[32];
        mfb_sha256_init(&sha);
        mfb_sha256_update(&sha, m->modinfo, m->modinfo_len);
        mfb_sha256_final(&sha, digest);
        for (k = 0; k < 32; ++k)
            if (digest[k] != mi.sha256[k]) { mfb_fail(err, "modinfo hash mismatch (bundle is corrupt)", 0); return -1; }
    }
    if (!mfb_utf8_ok((const unsigned char *)m->modinfo, m->modinfo_len)) {
        mfb_fail(err, "modinfo is not UTF-8", 0);
        return -1;
    }
    if (mfx_parse_modinfo(m, ar, err)) return -1;

    /* modinfo's file list must be exactly the bundle's entries, no repeats. */
    if (m->n_files != m->n_entries) { mfb_fail(err, "modinfo file list does not match the bundle's entries", 0); return -1; }
    m->entry_order = (size_t *)mfx_alloc(ar, (m->n_entries + 1) * sizeof(size_t));
    file_order = (size_t *)mfx_alloc(ar, (m->n_files + 1) * sizeof(size_t));
    if (!m->entry_order || !file_order) { mfb_fail(err, "compose arena is too small (sort)", 0); return -1; }
    for (k = 0; k < m->n_entries; ++k) m->entry_order[k] = k;
    for (k = 0; k < m->n_files; ++k) file_order[k] = k;
    mfx_sort(m->entry_order, m->n_entries, mfx_entry_less, m->entries);
    mfx_sort(file_order, m->n_files, mfx_file_less, m->files);
    for (k = 0; k < m->n_files; ++k) {
        mfx_file *fl = &m->files[file_order[k]];
        mfx_entry *en = &m->entries[m->entry_order[k]];
        if (k && mfx_cmp(fl->path, m->files[file_order[k - 1]].path) == 0) {
            mfb_fail(err, "modinfo lists a file twice", 0);
            return -1;
        }
        if (mfx_cmp(fl->path, en->path) != 0) {
            mfb_fail(err, "modinfo file list does not match the bundle's entries", 0);
            return -1;
        }
        fl->entry = en;
    }
    return 0;
}

/* ---- composition -------------------------------------------------------- */

typedef struct {
    const mfx_mod *mods;
    const size_t *rank_of;
} mfx_order_ctx;

typedef struct {
    mfx_str path;
    size_t rank;      /* position of its mod in load order */
    const mfx_file *file;
    const mfx_mod *mod;
} mfx_global_file;

static inline int mfx_gfile_less(const void *ctx, size_t a, size_t b) {
    const mfx_global_file *g = (const mfx_global_file *)ctx;
    int c = mfx_cmp(g[a].path, g[b].path);
    if (c) return c < 0;
    return g[a].rank < g[b].rank;
}

typedef struct {
    mfx_str *v;
} mfx_strs_ctx;

static inline int mfx_str_less(const void *ctx, size_t a, size_t b) {
    const mfx_str *v = (const mfx_str *)ctx;
    return mfx_cmp(v[a], v[b]) < 0;
}

/* Text builder for the load list, in the arena. */
typedef struct {
    char *p;
    size_t n, cap;
    int overflow;
} mfx_text;

static inline void mfx_put(mfx_text *t, const char *s, size_t n) {
    size_t i;
    if (t->overflow || n > t->cap - t->n) { t->overflow = 1; return; }
    for (i = 0; i < n; ++i) t->p[t->n + i] = s[i];
    t->n += n;
}
static inline void mfx_puts(mfx_text *t, const char *s) {
    size_t n = 0;
    while (s[n]) ++n;
    mfx_put(t, s, n);
}
static inline void mfx_putstr(mfx_text *t, mfx_str s) { mfx_put(t, s.p, s.n); }
static inline void mfx_putu64(mfx_text *t, uint64_t v) {
    char buf[24];
    int i = 24;
    do { buf[--i] = (char)('0' + v % 10); v /= 10; } while (v);
    mfx_put(t, buf + i, (size_t)(24 - i));
}

static inline int mfx_loads_as_master(mfx_str name, char flag) {
    if (name.n >= 4) {
        mfx_str ext;
        ext.p = name.p + name.n - 4;
        ext.n = 4;
        if (ext.p[0] == '.') {
            mfx_str esm, esl;
            esm.p = ".esm"; esm.n = 4;
            esl.p = ".esl"; esl.n = 4;
            if (mfx_eq_ci(ext, esm) || mfx_eq_ci(ext, esl)) return 1;
        }
    }
    return flag == 'm';
}

/* Merge mod bundles into the (empty) mod root. Returns the number of files
 * installed (not counting loadlist.txt), or -1 with err->msg set. */
static inline long mfx_compose(const mfx_io *io, const uint64_t *bundle_sizes, size_t n_bundles,
                               const char *title_id, unsigned char *arena, size_t arena_size,
                               unsigned char *work, size_t work_size, mfb_error *err) {
    mfx_arena ar;
    mfx_mod *mods;
    size_t *order, i, j, k, n_files_total = 0, n_winners = 0, n_fw_total = 0;
    mfx_global_file *gf;
    size_t *gorder;
    mfx_str title, *fws, *plugin_order = 0;
    size_t *fw_order, n_plugin_order = 0;
    mfx_text text;
    int handle;
    static const char marker_text[] =
        "This directory is managed by modforge; its contents are replaced on each install.\n";

    if (err) err->msg[0] = 0;
    if (!io || !io->read_at || !io->make_dirs || !io->open_write || !io->write || !io->close ||
        !bundle_sizes || !title_id || !arena || !work || work_size == 0) {
        mfb_fail(err, "bad arguments", 0);
        return -1;
    }
    if (n_bundles == 0) { mfb_fail(err, "no mod bundles to compose", 0); return -1; }
    ar.base = arena;
    ar.size = arena_size;
    ar.used = 0;
    title.p = title_id;
    title.n = 0;
    while (title_id[title.n]) ++title.n;

    mods = (mfx_mod *)mfx_alloc(&ar, n_bundles * sizeof(mfx_mod));
    order = (size_t *)mfx_alloc(&ar, n_bundles * sizeof(size_t));
    if (!mods || !order) { mfb_fail(err, "compose arena is too small", 0); return -1; }
    mfb_zero(mods, n_bundles * sizeof(mfx_mod));

    /* 1. Load and validate every bundle. */
    for (i = 0; i < n_bundles; ++i)
        if (mfx_load(io, i, bundle_sizes[i], &mods[i], &ar, err)) return -1;

    /* 2. They must be for this game and agree with each other. */
    for (i = 0; i < n_bundles; ++i) {
        const mfx_mod *m = &mods[i], *f0 = &mods[0];
        if (mfx_cmp(m->title, title) != 0) { mfb_fail(err, "a mod bundle was built for another game", 0); return -1; }
        if (mfx_cmp(m->engine, f0->engine) || mfx_cmp(m->sdk, f0->sdk) ||
            mfx_cmp(m->load_order, f0->load_order) || m->n_base != f0->n_base) {
            mfb_fail(err, "mod bundles disagree about the game (engine/sdk/load order)", 0);
            return -1;
        }
        for (k = 0; k < m->n_base; ++k)
            if (mfx_cmp(m->base[k], f0->base[k])) { mfb_fail(err, "mod bundles disagree about base plugins", 0); return -1; }
        for (j = 0; j < i; ++j)
            if (mfx_cmp(m->id, mods[j].id) == 0) { mfb_fail(err, "the same mod was given twice", 0); return -1; }
    }

    /* 3. Load order of mods: priority, then id (insertion sort; few mods). */
    for (i = 0; i < n_bundles; ++i) {
        size_t v = i;
        j = i;
        while (j > 0) {
            const mfx_mod *a = &mods[order[j - 1]], *b = &mods[v];
            if (a->priority < b->priority ||
                (a->priority == b->priority && mfx_cmp(a->id, b->id) < 0)) break;
            order[j] = order[j - 1];
            --j;
        }
        order[j] = v;
    }

    /* 4. Dependencies. */
    for (i = 0; i < n_bundles; ++i) {
        const mfx_mod *m = &mods[i];
        for (k = 0; k < m->n_reqs; ++k) {
            const mfx_mod *dep = 0;
            for (j = 0; j < n_bundles; ++j)
                if (mfx_cmp(mods[j].id, m->reqs[k].id) == 0) dep = &mods[j];
            if (!dep) { mfb_fail(err, "a selected mod requires a mod that isn't selected", 0); return -1; }
            if (mfx_ver_cmp(dep->version, m->reqs[k].min) < 0) {
                mfb_fail(err, "a selected mod requires a newer version of another mod", 0);
                return -1;
            }
        }
    }

    /* 5. Every file of every mod; the highest-ranked mod wins each path. */
    for (i = 0; i < n_bundles; ++i) {
        n_files_total += mods[i].n_files;
        n_fw_total += mods[i].n_frameworks;
    }
    gf = (mfx_global_file *)mfx_alloc(&ar, (n_files_total + 1) * sizeof(mfx_global_file));
    gorder = (size_t *)mfx_alloc(&ar, (n_files_total + 1) * sizeof(size_t));
    fws = (mfx_str *)mfx_alloc(&ar, (n_fw_total + 1) * sizeof(mfx_str));
    fw_order = (size_t *)mfx_alloc(&ar, (n_fw_total + 1) * sizeof(size_t));
    if (!gf || !gorder || !fws || !fw_order) { mfb_fail(err, "compose arena is too small (files)", 0); return -1; }
    k = 0;
    for (i = 0; i < n_bundles; ++i) {
        const mfx_mod *m = &mods[order[i]];
        for (j = 0; j < m->n_files; ++j, ++k) {
            gf[k].path = m->files[j].path;
            gf[k].rank = i;
            gf[k].file = &m->files[j];
            gf[k].mod = m;
            gorder[k] = k;
        }
    }
    mfx_sort(gorder, n_files_total, mfx_gfile_less, gf);
    /* keep the last (highest rank) of each run of equal paths, in path order */
    for (i = 0; i < n_files_total; ++i) {
        if (i + 1 < n_files_total && mfx_cmp(gf[gorder[i]].path, gf[gorder[i + 1]].path) == 0) continue;
        gorder[n_winners++] = gorder[i];
    }

    /* 6. Components to load: union of every mod's, sorted, unique. */
    k = 0;
    for (i = 0; i < n_bundles; ++i)
        for (j = 0; j < mods[i].n_frameworks; ++j) {
            fws[k] = mods[i].frameworks[j];
            fw_order[k] = k;
            ++k;
        }
    mfx_sort(fw_order, n_fw_total, mfx_str_less, fws);

    /* 7. Bethesda-style plugin load order (computed before the load-list
     *    text, which is rendered at the top of the arena). */
    if (mfx_eq_lit(mods[0].load_order, "plugins_txt")) {
        size_t n_all = 0, n_pl = 0, step;
        typedef struct {
            mfx_str name;
            char flag;
            const mfx_mod *mod;
            size_t plugin;     /* index into mod->plugins */
            size_t rank;       /* position in masters-first base sequence */
            size_t indeg;
            int done;
        } mfx_node;
        mfx_node *nodes;
        size_t *deps, *ndeps, max_deps = 0;
        for (i = 0; i < n_bundles; ++i) {
            n_all += mods[i].n_plugins;
            max_deps += mods[i].n_masters;
        }
        nodes = (mfx_node *)mfx_alloc(&ar, (n_all + 1) * sizeof(mfx_node));
        ndeps = (size_t *)mfx_alloc(&ar, (n_all + 1) * sizeof(size_t));
        plugin_order = (mfx_str *)mfx_alloc(&ar, (n_all + 1) * sizeof(mfx_str));
        if (!nodes || !ndeps || !plugin_order) { mfb_fail(err, "compose arena is too small (plugins)", 0); return -1; }
        /* plugins in mod order, keeping the LAST occurrence of each name */
        for (i = 0; i < n_bundles; ++i) {
            const mfx_mod *m = &mods[order[i]];
            for (j = 0; j < m->n_plugins; ++j) {
                for (k = 0; k < n_pl; ++k)
                    if (mfx_eq_ci(nodes[k].name, m->plugins[j].name)) break;
                if (k < n_pl) { /* drop the earlier one, keep relative order */
                    for (; k + 1 < n_pl; ++k) nodes[k] = nodes[k + 1];
                    --n_pl;
                }
                nodes[n_pl].name = m->plugins[j].name;
                nodes[n_pl].flag = m->plugins[j].flag;
                nodes[n_pl].mod = m;
                nodes[n_pl].plugin = j;
                nodes[n_pl].indeg = 0;
                nodes[n_pl].done = 0;
                ++n_pl;
            }
        }
        /* rank: masters block first, then regular plugins, priority within */
        k = 0;
        for (step = 0; step < 2; ++step)
            for (i = 0; i < n_pl; ++i)
                if (mfx_loads_as_master(nodes[i].name, nodes[i].flag) == (step == 0))
                    nodes[i].rank = k++;
        /* edges: dependent -> its in-set masters (deduplicated) */
        deps = (size_t *)mfx_alloc(&ar, (max_deps + 1) * sizeof(size_t));
        if (!deps) { mfb_fail(err, "compose arena is too small (plugin edges)", 0); return -1; }
        {
            size_t used = 0;
            size_t *start = (size_t *)mfx_alloc(&ar, (n_pl + 1) * sizeof(size_t));
            if (!start) { mfb_fail(err, "compose arena is too small (plugin edges)", 0); return -1; }
            for (i = 0; i < n_pl; ++i) {
                const mfx_mod *m = nodes[i].mod;
                int is_master = mfx_loads_as_master(nodes[i].name, nodes[i].flag);
                start[i] = used;
                ndeps[i] = 0;
                if (nodes[i].flag == '?') continue;
                for (j = 0; j < m->n_masters; ++j) {
                    mfx_str mname;
                    size_t target = (size_t)-1, d;
                    if (m->masters[j].plugin != nodes[i].plugin) continue;
                    mname = m->masters[j].name;
                    for (k = 0; k < n_pl; ++k)
                        if (mfx_eq_ci(nodes[k].name, mname)) { target = k; break; }
                    if (target == (size_t)-1) {
                        int base = 0;
                        for (k = 0; k < mods[0].n_base; ++k)
                            if (mfx_glob(mods[0].base[k], mname)) { base = 1; break; }
                        if (!base) { mfb_fail(err, "load order: a plugin needs a master that is not installed", 0); return -1; }
                        continue;
                    }
                    if (is_master && !mfx_loads_as_master(nodes[target].name, nodes[target].flag)) {
                        mfb_fail(err, "load order: a master depends on a regular plugin", 0);
                        return -1;
                    }
                    for (d = 0; d < ndeps[i]; ++d)
                        if (deps[start[i] + d] == target) break;
                    if (d == ndeps[i]) {
                        deps[used++] = target;
                        ndeps[i]++;
                    }
                }
                nodes[i].indeg = ndeps[i];
            }
            /* Kahn: repeatedly take the available plugin with the best rank. */
            for (step = 0; step < n_pl; ++step) {
                size_t best = (size_t)-1;
                for (i = 0; i < n_pl; ++i)
                    if (!nodes[i].done && nodes[i].indeg == 0 &&
                        (best == (size_t)-1 || nodes[i].rank < nodes[best].rank)) best = i;
                if (best == (size_t)-1) { mfb_fail(err, "load order: plugins depend on each other in a loop", 0); return -1; }
                nodes[best].done = 1;
                plugin_order[n_plugin_order++] = nodes[best].name;
                for (i = 0; i < n_pl; ++i) {
                    if (nodes[i].done) continue;
                    for (j = 0; j < ndeps[i]; ++j)
                        if (deps[start[i] + j] == best) nodes[i].indeg--;
                }
            }
        }
    }
    /* 8. Render the load list (header, components, mods, files, order). */
    text.p = (char *)(ar.base + ((ar.used + 7u) & ~(size_t)7u));
    {
        size_t off = (ar.used + 7u) & ~(size_t)7u;
        text.cap = off <= ar.size ? ar.size - off : 0;
        if (off > ar.size) text.p = 0;
    }
    text.n = 0;
    text.overflow = text.p ? 0 : 1;
    mfx_puts(&text, "modforge-loadlist\t1\ntitle\t");
    mfx_putstr(&text, title);
    mfx_puts(&text, "\nengine\t");
    mfx_putstr(&text, mods[0].engine);
    mfx_puts(&text, "\n");
    if (mods[0].sdk.n) {
        mfx_puts(&text, "sdk\t");
        mfx_putstr(&text, mods[0].sdk);
        mfx_puts(&text, "\n");
    }
    for (i = 0; i < n_fw_total; ++i) {
        if (i && mfx_cmp(fws[fw_order[i]], fws[fw_order[i - 1]]) == 0) continue;
        mfx_puts(&text, "framework\t");
        mfx_putstr(&text, fws[fw_order[i]]);
        mfx_puts(&text, "\n");
    }
    for (i = 0; i < n_bundles; ++i) {
        const mfx_mod *m = &mods[order[i]];
        mfx_puts(&text, "mod\t");
        mfx_putstr(&text, m->id);
        mfx_puts(&text, "\t");
        mfx_putstr(&text, m->version);
        mfx_puts(&text, "\n");
    }
    for (i = 0; i < n_winners; ++i) {
        const mfx_global_file *g = &gf[gorder[i]];
        mfx_puts(&text, "file\t");
        mfx_putstr(&text, g->file->role);
        mfx_puts(&text, "\t");
        mfx_putstr(&text, g->path);
        mfx_puts(&text, "\t");
        mfx_putu64(&text, g->file->entry->size);
        mfx_puts(&text, "\n");
    }
    for (i = 0; i < n_plugin_order; ++i) {
        mfx_puts(&text, "order\t");
        mfx_putstr(&text, plugin_order[i]);
        mfx_puts(&text, "\n");
    }

    if (text.overflow) { mfb_fail(err, "compose arena is too small (load list)", 0); return -1; }

    /* 9. Install: marker, every winning file (hash-checked), load list last. */
    handle = io->open_write(io->ctx, MFB_MARKER);
    if (handle < 0 || io->write(io->ctx, handle, marker_text, sizeof marker_text - 1) ||
        io->close(io->ctx, handle)) {
        mfb_fail(err, "cannot write " MFB_MARKER, 0);
        return -1;
    }
    for (i = 0; i < n_winners; ++i) {
        const mfx_global_file *g = &gf[gorder[i]];
        mfx_adapter ad;
        mfb_io bio;
        mfb_entry e;
        ad.io = io;
        ad.bundle = g->mod->bundle;
        mfx_adapt(&bio, &ad);
        e.offset = g->file->entry->offset;
        e.size = g->file->entry->size;
        for (k = 0; k < 32; ++k) e.sha256[k] = g->file->entry->sha256[k];
        for (k = 0; k < g->path.n; ++k) e.path[k] = g->path.p[k];
        e.path[g->path.n] = 0;
        if (mfb_write_entry(&bio, &e, work, work_size, err)) return -1;
    }
    handle = io->open_write(io->ctx, MFB_LOADLIST);
    if (handle < 0 || io->write(io->ctx, handle, text.p, text.n) || io->close(io->ctx, handle)) {
        mfb_fail(err, "cannot write " MFB_LOADLIST, 0);
        return -1;
    }
    return (long)n_winners;
}

#ifdef __cplusplus
}
#endif

#endif /* MODFORGE_COMPOSE_H */
