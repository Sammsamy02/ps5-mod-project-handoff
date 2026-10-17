/*
 * modforge_catalog.h — allocation-free reader for the modforge console catalog.
 *
 * The console app's browse screen reads one game's catalog listing (exported
 * on the host by `modforge catalog export`): which mods exist, whether each
 * works on this console ("native" / "converted" / "unsupported") and why not.
 * Format: see modforge/console_catalog.py / docs/FORMATS.md.
 *
 * Single header, C99, no libc calls, no allocation: records are returned as
 * (pointer, length) slices into the caller's buffer. Unknown record kinds are
 * returned as MFC_REC_UNKNOWN so callers can skip them.
 *
 *   mfc_reader r;
 *   mfc_record rec;
 *   int rc;
 *   mfc_reader_init(&r, buf, len);
 *   while ((rc = mfc_next(&r, &rec)) > 0) {
 *       if (rec.kind == MFC_REC_MOD) { ... rec.f[0] id, rec.f[5] name, rec.tier ... }
 *   }
 *   if (rc < 0) { ... malformed at line r.line ... }
 */
#ifndef MODFORGE_CATALOG_H
#define MODFORGE_CATALOG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MFC_MAX_FIELDS 8   /* including the record kind */

typedef enum {
    MFC_REC_GAME = 1,  /* f[0] title id, f[1] name, f[2] engine                      */
    MFC_REC_MOD,       /* f[0] id, f[1] version, f[2] tier, f[3] type, f[4] size,
                          f[5] name; .tier and .size are parsed for you             */
    MFC_REC_WHY,       /* f[0] mod id, f[1] reason                                   */
    MFC_REC_UNKNOWN    /* unrecognised kind: skip it                                 */
} mfc_kind;

typedef enum {
    MFC_TIER_NONE = 0,
    MFC_TIER_NATIVE,       /* installs as-is                                   */
    MFC_TIER_CONVERTED,    /* works; the host converts it before bundling      */
    MFC_TIER_UNSUPPORTED   /* can't work on this console (show greyed out)     */
} mfc_tier;

typedef struct {
    const char *p;
    size_t n;
} mfc_str;

typedef struct {
    mfc_kind kind;
    mfc_str name;                     /* the record kind as written */
    mfc_str f[MFC_MAX_FIELDS - 1];    /* fields after the kind      */
    int nf;
    mfc_tier tier;                    /* MFC_REC_MOD only           */
    uint64_t size;                    /* MFC_REC_MOD only           */
} mfc_record;

typedef struct {
    const char *cur;
    const char *end;
    int line;
    int seen_header;
} mfc_reader;

static inline int mfc_str_eq(mfc_str s, const char *lit) {
    size_t i = 0;
    for (; lit[i]; ++i) {
        if (i >= s.n || s.p[i] != lit[i]) return 0;
    }
    return i == s.n;
}

static inline void mfc_reader_init(mfc_reader *r, const char *buf, size_t len) {
    r->cur = buf;
    r->end = buf + len;
    r->line = 0;
    r->seen_header = 0;
}

/* Strict UTF-8, same rules as the host's decoder. */
static inline int mfc_utf8_ok(const unsigned char *s, size_t n) {
    size_t i = 0, k;
    while (i < n) {
        unsigned char c = s[i], lo = 0x80, hi = 0xBF;
        size_t need;
        if (c < 0x80) { ++i; continue; }
        if (c >= 0xC2 && c <= 0xDF) need = 1;
        else if (c == 0xE0) { need = 2; lo = 0xA0; }
        else if (c == 0xED) { need = 2; hi = 0x9F; }
        else if (c >= 0xE1 && c <= 0xEF) need = 2;
        else if (c == 0xF0) { need = 3; lo = 0x90; }
        else if (c >= 0xF1 && c <= 0xF3) need = 3;
        else if (c == 0xF4) { need = 3; hi = 0x8F; }
        else return 0;
        if (n - i - 1 < need) return 0;
        if (s[i + 1] < lo || s[i + 1] > hi) return 0;
        for (k = 2; k <= need; ++k)
            if (s[i + k] < 0x80 || s[i + k] > 0xBF) return 0;
        i += need + 1;
    }
    return 1;
}

static inline int mfc_parse_u64(mfc_str s, uint64_t *out) {
    uint64_t v = 0;
    size_t i;
    if (s.n == 0 || s.n > 20) return 0;
    for (i = 0; i < s.n; ++i) {
        uint64_t d;
        if (s.p[i] < '0' || s.p[i] > '9') return 0;
        d = (uint64_t)(s.p[i] - '0');
        if (v > (UINT64_MAX - d) / 10) return 0;
        v = v * 10 + d;
    }
    *out = v;
    return 1;
}

/* Returns 1 and fills *rec for each record, 0 at end of input, -1 on a
 * malformed line (r->line says which). The header is consumed internally. */
static inline int mfc_next(mfc_reader *r, mfc_record *rec) {
    while (r->cur < r->end) {
        const char *ls = r->cur, *le = ls, *p;
        mfc_str parts[MFC_MAX_FIELDS];
        int np = 0, i;

        while (le < r->end && *le != '\n') ++le;
        r->cur = (le < r->end) ? le + 1 : le;
        r->line++;
        if (le > ls && le[-1] == '\r') --le;                   /* tolerate CRLF */
        if (!mfc_utf8_ok((const unsigned char *)ls, (size_t)(le - ls))) return -1;
        if (le == ls || *ls == '#') continue;                  /* blank / comment */

        p = ls;
        for (;;) {
            const char *fs = p;
            while (p < le && *p != '\t') {
                unsigned char c = (unsigned char)*p;
                if (c < 0x20 || c == 0x7f) return -1;          /* control character */
                ++p;
            }
            if (np >= MFC_MAX_FIELDS) return -1;               /* too many fields */
            parts[np].p = fs;
            parts[np].n = (size_t)(p - fs);
            np++;
            if (p >= le) break;
            ++p;
        }

        if (!r->seen_header) {
            if (np != 2 || !mfc_str_eq(parts[0], "modforge-catalog") || !mfc_str_eq(parts[1], "1"))
                return -1;
            r->seen_header = 1;
            continue;
        }

        rec->name = parts[0];
        rec->nf = np - 1;
        rec->tier = MFC_TIER_NONE;
        rec->size = 0;
        for (i = 0; i < MFC_MAX_FIELDS - 1; ++i) {
            rec->f[i].p = 0;
            rec->f[i].n = 0;
        }
        for (i = 1; i < np; ++i) rec->f[i - 1] = parts[i];

        if (mfc_str_eq(parts[0], "game")) {
            rec->kind = MFC_REC_GAME;
            if (np != 4 || !parts[1].n || !parts[2].n || !parts[3].n) return -1;
        } else if (mfc_str_eq(parts[0], "mod")) {
            rec->kind = MFC_REC_MOD;
            if (np != 7) return -1;
            for (i = 1; i <= 5; ++i)
                if (!parts[i].n) return -1;
            if (mfc_str_eq(parts[3], "native")) rec->tier = MFC_TIER_NATIVE;
            else if (mfc_str_eq(parts[3], "converted")) rec->tier = MFC_TIER_CONVERTED;
            else if (mfc_str_eq(parts[3], "unsupported")) rec->tier = MFC_TIER_UNSUPPORTED;
            else return -1;
            if (!mfc_parse_u64(parts[5], &rec->size)) return -1;
        } else if (mfc_str_eq(parts[0], "why")) {
            rec->kind = MFC_REC_WHY;
            if (np != 3 || !parts[1].n) return -1;
        } else {
            rec->kind = MFC_REC_UNKNOWN;
        }
        return 1;
    }
    return r->seen_header ? 0 : -1;                            /* empty input is an error */
}

#ifdef __cplusplus
}
#endif

#endif /* MODFORGE_CATALOG_H */
