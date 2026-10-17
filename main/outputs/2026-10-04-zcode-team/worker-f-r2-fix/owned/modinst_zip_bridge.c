/*
 * modinst_zip_bridge.c — see modinst_zip_bridge.h.
 * Origin: worker-zip-to-mfb, 2026-10-04 (owned).
 *
 * Reuses, unmodified: the intake's quarantine + manifest-v1.bin (parsed
 * here exactly as the vendored intake.c emit_manifest writes it), the mfb
 * kind-1 bundle layout (byte-identical to worker-7 cmd_make_bundle and
 * mfb_install/mfx_load readers), mfb_sha256 and mfb_path_ok from the
 * vendored modforge_bundle.h, and ps5io for all I/O (host+console
 * identical semantics). Scratch is fixed static like the sibling owned
 * modules; every bound is refused honestly, never truncated silently.
 *
 * v1 bounds (documented in BRIDGE-DESIGN.md §6): <=256 members, <=64 MiB
 * total member bytes, <=1 MiB manifest, member names <=31 bytes (the
 * intake writes member-%08u.bin = 18). Identity defaults: version 1.0.0,
 * priority 100, engine unreal, load_order manifest_priority.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "modforge_bundle.h"   /* mfb format, sha256, path validator */
#include "ps5_io.h"            /* vendored I/O (both flavors) */
#include "e2e_status.h"

#include "modinst_zip_bridge.h"

#define ZB_MAX_MEMBERS 256
#define ZB_MAX_TOTAL (64u * 1024u * 1024u)
#define ZB_MAX_MANIFEST (1024u * 1024u)
#define ZB_CHUNK (64u * 1024u)
#define ZB_MAX_SEGS 24
#define ZB_EX_OK 0
#define ZB_EX_IO 1
#define ZB_EX_BAD 2

typedef struct {
    char final_path[MFB_MAX_PATH + 1];
    char qname[32];             /* quarantine member-%08u.bin */
    const char *role;           /* static literal */
    uint64_t size;
    unsigned char sha[32];
} zb_member;

static zb_member zb_m[ZB_MAX_MEMBERS];
static int zb_n;
static unsigned char zb_manifest[ZB_MAX_MANIFEST];
static unsigned char zb_index[ZB_MAX_MEMBERS * (50u + MFB_MAX_PATH) + 128];
static char zb_modinfo[256u * 1024u];
static unsigned char zb_chunk[ZB_CHUNK];

/* ---- small helpers -------------------------------------------------------- */

static uint64_t zb_le64(const unsigned char *p) {
    return (uint64_t)p[0] | (uint64_t)p[1] << 8 | (uint64_t)p[2] << 16 |
           (uint64_t)p[3] << 24 | (uint64_t)p[4] << 32 | (uint64_t)p[5] << 40 |
           (uint64_t)p[6] << 48 | (uint64_t)p[7] << 56;
}

static void zb_put32(unsigned char *p, uint32_t v) {
    int i;
    for (i = 0; i < 4; ++i) p[i] = (unsigned char)(v >> (8 * i));
}
static void zb_put64(unsigned char *p, uint64_t v) {
    int i;
    for (i = 0; i < 8; ++i) p[i] = (unsigned char)(v >> (8 * i));
}

static char zb_lc(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

/* segment == literal, case-insensitive (seg is not NUL-terminated) */
static int zb_seg_eq(const char *s, size_t n, const char *lit) {
    size_t i = 0;
    for (; lit[i]; ++i) {
        if (i >= n || zb_lc(s[i]) != zb_lc(lit[i])) return 0;
    }
    return i == n;
}

static int zb_ends_ci(const char *s, const char *suf) {
    size_t ls = strlen(s), lx = strlen(suf), i;
    if (ls < lx) return 0;
    for (i = 0; i < lx; ++i)
        if (zb_lc(s[ls - lx + i]) != zb_lc(suf[i])) return 0;
    return 1;
}

static const char *zb_basename(const char *s) {
    const char *r = s, *p;
    for (p = s; *p; ++p)
        if (*p == '/') r = p + 1;
    return r;
}

static int zb_refuse(const char *reason, const char *detail) {
    if (detail) printf("error bad_request auto-bundle refused: %s (%s); quarantine retained\n", reason, detail);
    else printf("error bad_request auto-bundle refused: %s; quarantine retained\n", reason);
    e2e_status_event("stage-2-prepare", "fail", reason);
    return ZB_EX_BAD;
}

static int zb_io_fail(const char *what, const char *path) {
    printf("error io_error auto-bundle %s%s%s\n", what, path ? ": " : "", path ? path : "");
    e2e_status_event("stage-2-prepare", "fail", what);
    return ZB_EX_IO;
}

/* ---- classification (BRIDGE-DESIGN.md §3) --------------------------------- */

/* Returns role literal and fills final_out, or 0 = skip (metadata). */
static const char *zb_classify(const char *path, char *final_out, size_t final_n) {
    size_t so[ZB_MAX_SEGS], sl[ZB_MAX_SEGS];
    int ns = 0, i;
    const char *base = zb_basename(path);
    int is_lua = zb_ends_ci(base, ".lua");
    int is_cfg_txt = zb_seg_eq(base, strlen(base), "enabled.txt") ||
                     zb_seg_eq(base, strlen(base), "load_order.txt");

    { /* split into segments */
        const char *p = path;
        so[0] = 0;
        for (;;) {
            const char *q = strchr(p, '/');
            if (ns >= ZB_MAX_SEGS) return 0;
            if (q) {
                sl[ns] = (size_t)(q - p);
                ++ns;
                if (ns < ZB_MAX_SEGS) so[ns] = (size_t)(q + 1 - path);
                p = q + 1;
            } else {
                sl[ns] = strlen(p);
                ++ns;
                break;
            }
        }
    }
#define ZB_SEG(idx, lit) zb_seg_eq(path + so[idx], sl[idx], lit)

    /* Rule I: ini fragment -> config/<tail after Saved/Config/> */
    if (zb_ends_ci(base, ".ini")) {
        for (i = 0; i + 2 < ns; ++i)
            if (ZB_SEG(i, "Saved") && ZB_SEG(i + 1, "Config")) {
                if (snprintf(final_out, final_n, "config/%s", path + so[i + 2]) >= (int)final_n) return 0;
                return "config";
            }
        if (snprintf(final_out, final_n, "config/%s", base) >= (int)final_n) return 0;
        return "config";
    }

    /* Rule P: .../Content/Paks/<tail> -> mods/<tail> */
    for (i = 0; i + 2 < ns; ++i) {
        if (ZB_SEG(i, "Content") && ZB_SEG(i + 1, "Paks")) {
            if (zb_ends_ci(base, ".pak")) {
                if (snprintf(final_out, final_n, "mods/%s", path + so[i + 2]) >= (int)final_n) return 0;
                return "pak";
            }
            if (zb_ends_ci(base, ".ucas") || zb_ends_ci(base, ".utoc") || zb_ends_ci(base, ".sig")) {
                if (snprintf(final_out, final_n, "mods/%s", path + so[i + 2]) >= (int)final_n) return 0;
                return "asset_override";
            }
            return 0; /* non-asset inside Paks: metadata for this v1 */
        }
    }

    /* Rule L: ue4ss/Mods/... kept as-is */
    if (ns >= 2 && ZB_SEG(0, "ue4ss") && ZB_SEG(1, "Mods")) {
        if (is_lua) { if (snprintf(final_out, final_n, "%s", path) >= (int)final_n) return 0; return "lua_mod"; }
        if (is_cfg_txt) { if (snprintf(final_out, final_n, "%s", path) >= (int)final_n) return 0; return "config"; }
        return 0;
    }
    /* Rule M: Mods/<X>/... -> ue4ss/Mods/<X>/... */
    if (ns >= 2 && ZB_SEG(0, "Mods")) {
        if (is_lua || is_cfg_txt) {
            if (snprintf(final_out, final_n, "ue4ss/%s", path) >= (int)final_n) return 0;
            return is_lua ? "lua_mod" : "config";
        }
        return 0;
    }
    /* Rule W: single wrapper dir: <X>/Scripts/... or <X>/enabled.txt (a UE4SS
     * mod package, the corpus shape) -> ue4ss/Mods/<X>/... */
    if (ns >= 2 && (ZB_SEG(1, "Scripts") || (ns == 2 && ZB_SEG(1, "enabled.txt")))) {
        if (is_lua || is_cfg_txt) {
            if (snprintf(final_out, final_n, "ue4ss/Mods/%s", path) >= (int)final_n) return 0;
            return is_lua ? "lua_mod" : "config";
        }
        return 0;
    }
#undef ZB_SEG
    return 0; /* ReadMe, dlls, screenshots, everything else: metadata */
}

/* ---- manifest-v1.bin parsing (layout: vendored intake.c emit_manifest) ----- */

typedef struct { const unsigned char *p, *end; } zb_cur;

static const unsigned char *zb_take(zb_cur *c, uint64_t n) {
    const unsigned char *q;
    if (n > (uint64_t)(c->end - c->p)) return 0;
    q = c->p;
    c->p += n;
    return q;
}

static int zb_read_exact(const char *path, unsigned char *buf, uint64_t want) {
    ps5io_stat st;
    int fd;
    size_t g = 0;
    fd = ps5io_open(path, PS5IO_RDONLY, 0);
    if (fd < 0) return -1;
    if (ps5io_stat_path(path, &st) || (uint64_t)st.size != want) { ps5io_close(fd); return -1; }
    if (ps5io_read_full(fd, buf, want, &g) || (uint64_t)g != want) { ps5io_close(fd); return -1; }
    return ps5io_close(fd) ? -1 : 0;
}

/* ---- mfb kind-1 writer (same bytes cmd_make_bundle emits) ----------------- */

typedef struct { unsigned char *p, *end; } zb_out;

static int zb_emit(zb_out *o, const void *b, size_t n) {
    if (n > (size_t)(o->end - o->p)) return -1;
    memcpy(o->p, b, n);
    o->p += n;
    return 0;
}

static int zb_copy_member(const char *qdir, const zb_member *m, int out_fd, uint64_t *total_out) {
    char path[4096];
    ps5io_stat st;
    mfb_sha256 sha;
    unsigned char dg[32];
    uint64_t done = 0;
    int fd, i;
    if (snprintf(path, sizeof path, "%s/%s", qdir, m->qname) >= (int)sizeof path) return -1;
    fd = ps5io_open(path, PS5IO_RDONLY, 0);
    if (fd < 0) return -1;
    if (ps5io_stat_path(path, &st) || (uint64_t)st.size != m->size) { ps5io_close(fd); return -1; }
    mfb_sha256_init(&sha);
    while (done < m->size) {
        size_t want = m->size - done > ZB_CHUNK ? ZB_CHUNK : (size_t)(m->size - done);
        size_t got = 0;
        if (ps5io_read_full(fd, zb_chunk, want, &got) || got != want ||
            ps5io_write_full(out_fd, zb_chunk, want)) { ps5io_close(fd); return -1; }
        mfb_sha256_update(&sha, zb_chunk, want);
        done += want;
    }
    if (ps5io_close(fd)) return -1;
    mfb_sha256_final(&sha, dg);
    for (i = 0; i < 32; ++i)
        if (dg[i] != m->sha[i]) return -1; /* quarantine changed after prepare */
    *total_out += m->size;
    return 0;
}

/* ---- public API ------------------------------------------------------------ */

int mi_bridge_wants_auto(int argc, char **argv) {
    int i, mod = 0, off = 0;
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--mod") && i + 1 < argc) mod = 1;
        else if (!strcmp(argv[i], "--no-auto-bundle")) off = 1;
    }
    return mod && !off;
}

const char *mi_bridge_mod_arg(int argc, char **argv) {
    int i;
    for (i = 1; i + 1 < argc; ++i)
        if (!strcmp(argv[i], "--mod") && argv[i + 1][0]) return argv[i + 1];
    return 0;
}

int mi_bridge_run(int argc, char **argv, const char *state_dir,
                  char *bundle_out, size_t bundle_out_n) {
    const char *mod = 0, *title = 0, *quarantine = 0;
    char mpath[4096], bdir[4096], bpath[4096], btmp[4096 + 8];
    zb_cur c;
    uint64_t count = 0, total = 0, idx_len = 0, doff;
    size_t mi_len = 0;
    int i, j, fd;
    zb_out io;

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--mod") && i + 1 < argc) mod = argv[++i];
        else if (!strcmp(argv[i], "--title") && i + 1 < argc) title = argv[++i];
        else if (!strcmp(argv[i], "--quarantine") && i + 1 < argc) quarantine = argv[++i];
        else if (!strcmp(argv[i], "--archive") && i + 1 < argc) ++i;
        else if (!strcmp(argv[i], "--no-auto-bundle")) { /* parsed by wants_auto */ }
    }
    if (!mod || !title || !quarantine || !state_dir)
        return zb_refuse("auto-bundle needs --mod, --title and --quarantine", 0);
    {
        const char *v;
        for (v = mod; *v; ++v)
            if ((unsigned char)*v < 0x20 || *v == '\t') return zb_refuse("mod id has control characters", mod);
        for (v = title; *v; ++v)
            if ((unsigned char)*v < 0x20 || *v == '\t') return zb_refuse("title has control characters", title);
    }

    /* 1. Parse the intake's durable manifest (member map). */
    if (snprintf(mpath, sizeof mpath, "%s/manifest-v1.bin", quarantine) >= (int)sizeof mpath)
        return zb_refuse("quarantine path too long", quarantine);
    {
        ps5io_stat st;
        int mfd = ps5io_open(mpath, PS5IO_RDONLY, 0);
        if (mfd < 0 || ps5io_stat_path(mpath, &st)) {
            if (mfd >= 0) ps5io_close(mfd);
            return zb_refuse("quarantine has no readable manifest-v1.bin (run prepare intake first)", quarantine);
        }
        ps5io_close(mfd);
        if ((uint64_t)st.size > ZB_MAX_MANIFEST)
            return zb_refuse("manifest exceeds bridge budget (1 MiB)", quarantine);
        if (zb_read_exact(mpath, zb_manifest, (uint64_t)st.size))
            return zb_io_fail("cannot read quarantine manifest", mpath);
        c.p = zb_manifest;
        c.end = zb_manifest + (size_t)st.size;
    }
    {
        static const unsigned char magic[8] = {'M', 'B', 'A', 'R', 'M', 'A', 'N', '1'};
        const unsigned char *p;
        if (!(p = zb_take(&c, 8)) || memcmp(p, magic, 8) ||
            !zb_take(&c, 32) || !zb_take(&c, 32) || !zb_take(&c, 32))
            return zb_refuse("manifest is not MBARMAN1", mpath);
        p = zb_take(&c, 8);
        if (!p) return zb_refuse("manifest truncated (count)", mpath);
        count = zb_le64(p);
        if (count > ZB_MAX_MEMBERS)
            return zb_refuse("archive has more members than the bridge budget (256)", quarantine);
    }

    /* 2. Classify every member (sorted, deduplicated). */
    zb_n = 0;
    for (i = 0; (uint64_t)i < count; ++i) {
        const unsigned char *sz, *sha, *nm, *or, *pa;
        uint64_t nlen, olen, plen, msize;
        char safe[MFB_MAX_PATH + 1];
        const char *role;
        /* per emit_manifest: row_index u64, size u64, sha256 32, member name,
         * original name, safe path (each u64-length prefixed) */
        if (!zb_take(&c, 8)) return zb_refuse("manifest truncated (row index)", mpath);
        sz = zb_take(&c, 8); sha = zb_take(&c, 32);
        if (!sz || !sha) return zb_refuse("manifest truncated (row head)", mpath);
        msize = zb_le64(sz);
        {
            const unsigned char *l = zb_take(&c, 8);
            if (!l) return zb_refuse("manifest truncated (name length)", mpath);
            nlen = zb_le64(l);
        }
        nm = zb_take(&c, nlen);
        if (!nm) return zb_refuse("manifest truncated (member name)", mpath);
        {
            const unsigned char *l = zb_take(&c, 8);
            if (!l) return zb_refuse("manifest truncated (original length)", mpath);
            olen = zb_le64(l);
        }
        or = zb_take(&c, olen);
        if (!or) return zb_refuse("manifest truncated (original name)", mpath);
        {
            const unsigned char *l = zb_take(&c, 8);
            if (!l) return zb_refuse("manifest truncated (path length)", mpath);
            plen = zb_le64(l);
        }
        pa = zb_take(&c, plen);
        if (!pa) return zb_refuse("manifest truncated (safe path)", mpath);
        if (nlen == 0 || nlen >= 32 || plen == 0 || plen > MFB_MAX_PATH)
            return zb_refuse("manifest row has an unusable name/path", quarantine);
        if (total + msize > ZB_MAX_TOTAL)
            return zb_refuse("member bytes exceed bridge budget (64 MiB)", quarantine);
        memcpy(safe, pa, plen);
        safe[plen] = 0;
        role = zb_classify(safe, zb_m[zb_n].final_path, sizeof zb_m[0].final_path);
        if (!role) continue; /* metadata member: skipped, not an error */
        memcpy(zb_m[zb_n].qname, nm, (size_t)nlen);
        zb_m[zb_n].qname[nlen] = 0;
        zb_m[zb_n].role = role;
        zb_m[zb_n].size = msize;
        memcpy(zb_m[zb_n].sha, sha, 32);
        ++zb_n;
        total += msize;
    }
    if (c.p != c.end) return zb_refuse("manifest has trailing bytes", mpath);
    if (zb_n == 0)
        return zb_refuse("no mod content recognized (no ue4ss/Mods or Scripts lua, "
                         "no Content/Paks asset, no ini fragment)", quarantine);
    /* insertion sort by final path; adjacent duplicates refuse */
    for (i = 1; i < zb_n; ++i) {
        zb_member t = zb_m[i];
        j = i - 1;
        while (j >= 0 && strcmp(zb_m[j].final_path, t.final_path) > 0) { zb_m[j + 1] = zb_m[j]; --j; }
        zb_m[j + 1] = t;
    }
    for (i = 1; i < zb_n; ++i)
        if (!strcmp(zb_m[i - 1].final_path, zb_m[i].final_path))
            return zb_refuse("two members map to one final path", zb_m[i].final_path);
    for (i = 0; i < zb_n; ++i) {
        size_t l = strlen(zb_m[i].final_path);
        if (!l || l > MFB_MAX_PATH || !mfb_path_ok(zb_m[i].final_path, l) ||
            !strcmp(zb_m[i].final_path, MFB_MODINFO) || !strcmp(zb_m[i].final_path, MFB_LOADLIST))
            return zb_refuse("mapped final path is unsafe", zb_m[i].final_path);
    }

    /* 3. modinfo.txt (fixed archive-sourced defaults; BRIDGE-DESIGN.md §2). */
    mi_len = (size_t)snprintf(zb_modinfo, sizeof zb_modinfo,
                              "modforge-modinfo\t1\n"
                              "mod\t%s\t1.0.0\t100\n"
                              "title\t%s\n"
                              "engine\tunreal\n"
                              "load_order\tmanifest_priority\n",
                              mod, title);
    for (i = 0; i < zb_n; ++i) {
        size_t need = strlen(zb_m[i].role) + strlen(zb_m[i].final_path) + 8;
        if (mi_len + need > sizeof zb_modinfo)
            return zb_refuse("modinfo exceeds bridge budget", 0);
        mi_len += (size_t)snprintf(zb_modinfo + mi_len, sizeof zb_modinfo - mi_len,
                                   "file\t%s\t%s\n", zb_m[i].role, zb_m[i].final_path);
    }

    /* 4. Write the bundle: header + index + (hash-checked) member data. */
    if (snprintf(bdir, sizeof bdir, "%s/bundles", state_dir) >= (int)sizeof bdir ||
        snprintf(bpath, sizeof bpath, "%s/bundles/%s.mfb", state_dir, mod) >= (int)sizeof bpath ||
        snprintf(btmp, sizeof btmp, "%s.tmp", bpath) >= (int)sizeof btmp)
        return zb_refuse("bundle path too long for state dir", state_dir);
    if (ps5io_mkdir_p(bdir, 0755)) return zb_io_fail("cannot create bundles dir", bdir);
    idx_len = 0;
    for (i = 0; i < zb_n; ++i) idx_len += 50 + strlen(zb_m[i].final_path);
    idx_len += 50 + strlen(MFB_MODINFO);
    io.p = zb_index;
    io.end = zb_index + sizeof zb_index;
    doff = 32 + idx_len;
    for (i = 0; i < zb_n; ++i) {
        unsigned char head[50];
        size_t pl = strlen(zb_m[i].final_path);
        zb_put64(head, doff);
        zb_put64(head + 8, zb_m[i].size);
        memcpy(head + 16, zb_m[i].sha, 32);
        head[48] = (unsigned char)(pl & 0xff);
        head[49] = (unsigned char)(pl >> 8);
        if (zb_emit(&io, head, 50) || zb_emit(&io, zb_m[i].final_path, pl)) return zb_refuse("index budget", 0);
        doff += zb_m[i].size;
    }
    {
        unsigned char head[50];
        mfb_sha256 sha;
        unsigned char dg[32];
        size_t pl = strlen(MFB_MODINFO);
        zb_put64(head, doff);
        zb_put64(head + 8, (uint64_t)mi_len);
        mfb_sha256_init(&sha);
        mfb_sha256_update(&sha, zb_modinfo, mi_len);
        mfb_sha256_final(&sha, dg);
        memcpy(head + 16, dg, 32);
        head[48] = (unsigned char)(pl & 0xff);
        head[49] = (unsigned char)(pl >> 8);
        if (zb_emit(&io, head, 50) || zb_emit(&io, MFB_MODINFO, pl)) return zb_refuse("index budget", 0);
    }

    fd = ps5io_open(btmp, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_TRUNC, 0644);
    if (fd < 0) return zb_io_fail("cannot create bundle tmp", btmp);
    {
        unsigned char header[32];
        uint64_t copied = 0;
        memset(header, 0, sizeof header);
        memcpy(header, "MFBUNDL1", 8);
        header[8] = 1;
        zb_put32(header + 12, (uint32_t)(zb_n + 1));
        zb_put64(header + 16, idx_len);
        header[24] = MFB_KIND_MOD;
        if (ps5io_write_full(fd, header, 32) ||
            ps5io_write_full(fd, zb_index, (size_t)idx_len)) {
            ps5io_close(fd);
            ps5io_unlink(btmp);
            return zb_io_fail("cannot write bundle header/index", btmp);
        }
        for (i = 0; i < zb_n; ++i)
            if (zb_copy_member(quarantine, &zb_m[i], fd, &copied)) {
                ps5io_close(fd);
                ps5io_unlink(btmp);
                return zb_refuse("quarantine member failed size/hash check (changed after prepare?)",
                                 zb_m[i].qname);
            }
        if (ps5io_write_full(fd, zb_modinfo, mi_len) || ps5io_fsync(fd)) {
            ps5io_close(fd);
            ps5io_unlink(btmp);
            return zb_io_fail("cannot finish bundle", btmp);
        }
        if (ps5io_close(fd)) { ps5io_unlink(btmp); return zb_io_fail("close bundle failed", btmp); }
    }
    if (ps5io_rename(btmp, bpath)) { ps5io_unlink(btmp); return zb_io_fail("cannot publish bundle", bpath); }
    ps5io_fsync_dir(bdir);

    if (bundle_out && bundle_out_n) snprintf(bundle_out, bundle_out_n, "%s", bpath);
    printf("bridge bundle %s id=%s files=%d bytes=%llu (archive->mfb, roles+paths per BRIDGE-DESIGN)\n",
           bpath, mod, zb_n, (unsigned long long)total);
    e2e_status_event("stage-2-prepare", "ok", "auto-bundle composed mfb");
    return ZB_EX_OK;
}
