/*
 * modforge_loadlist.h — allocation-free reader for the modforge load list.
 *
 * The engine runtime (e.g. UE4SS-PS5) reads the load list during
 * module_start, the only time /data is readable, and uses it to know exactly
 * which mod files to buffer. Format: see modforge/loadlist.py / docs/FORMATS.md.
 *
 * Single header, C99, no libc calls, no allocation: records are returned as
 * (pointer, length) slices into the caller's buffer, which must stay alive
 * while the slices are used. Unknown record kinds are returned as
 * MF_REC_UNKNOWN so callers can skip them.
 *
 *   mf_reader r;
 *   mf_record rec;
 *   int rc;
 *   mf_reader_init(&r, buf, len);
 *   while ((rc = mf_next(&r, &rec)) > 0) {
 *       if (rec.kind == MF_REC_FILE) { ... rec.f[1] is the path, rec.size ... }
 *   }
 *   if (rc < 0) { ... malformed at line r.line ... }
 */
#ifndef MODFORGE_LOADLIST_H
#define MODFORGE_LOADLIST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MF_MAX_FIELDS 4

typedef enum {
    MF_REC_TITLE = 1,     /* f[0] = title id                              */
    MF_REC_ENGINE,        /* f[0] = engine id                             */
    MF_REC_SDK,           /* f[0] = sdk version, e.g. 0x11000000          */
    MF_REC_FRAMEWORK,     /* f[0] = framework id                          */
    MF_REC_MOD,           /* f[0] = mod id, f[1] = version                */
    MF_REC_FILE,          /* f[0] = role, f[1] = relative path, size set  */
    MF_REC_ORDER,         /* f[0] = plugin file name                      */
    MF_REC_UNKNOWN        /* unrecognised kind: skip it                   */
} mf_kind;

typedef struct {
    const char *p;
    size_t n;
} mf_str;

typedef struct {
    mf_kind kind;
    mf_str name;               /* the record kind as written            */
    mf_str f[MF_MAX_FIELDS];   /* fields after the kind                 */
    int nf;                    /* number of fields after the kind       */
    uint64_t size;             /* MF_REC_FILE only                      */
} mf_record;

typedef struct {
    const char *cur;
    const char *end;
    int line;          /* 1-based line of the last record (or error) */
    int seen_header;
} mf_reader;

static inline int mf_str_eq(mf_str s, const char *lit) {
    size_t i = 0;
    for (; lit[i]; ++i) {
        if (i >= s.n || s.p[i] != lit[i]) return 0;
    }
    return i == s.n;
}

static inline void mf_reader_init(mf_reader *r, const char *buf, size_t len) {
    r->cur = buf;
    r->end = buf + len;
    r->line = 0;
    r->seen_header = 0;
}

/* Strict UTF-8 (no overlong forms, no surrogates, max U+10FFFF): the format
 * is UTF-8 text, and the host rejects anything else, so the console does too. */
static inline int mf_utf8_ok(const unsigned char *s, size_t n) {
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

/* A path is valid if it is non-empty, relative, uses '/', has no backslash or
 * control character (NUL, TAB, CR...), and has no empty, "." or ".." segment.
 * Returns 1 if valid. Callers should still join it to their own mod root
 * rather than trusting it blindly. */
static inline int mf_path_ok(mf_str s) {
    size_t i, seg = 0;
    if (s.n == 0 || s.p[0] == '/') return 0;
    for (i = 0; i <= s.n; ++i) {
        if (i == s.n || s.p[i] == '/') {
            size_t len = i - seg;
            if (len == 0) return 0;
            if (len == 1 && s.p[seg] == '.') return 0;
            if (len == 2 && s.p[seg] == '.' && s.p[seg + 1] == '.') return 0;
            seg = i + 1;
        } else {
            unsigned char c = (unsigned char)s.p[i];
            if (c == '\\' || c < 0x20 || c == 0x7f) return 0;  /* backslash or control char */
        }
    }
    return 1;
}

static inline int mf_parse_u64(mf_str s, uint64_t *out) {
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
static inline int mf_next(mf_reader *r, mf_record *rec) {
    while (r->cur < r->end) {
        const char *ls = r->cur, *le = ls, *p;
        mf_str parts[MF_MAX_FIELDS + 1];
        int np = 0;

        while (le < r->end && *le != '\n') ++le;
        r->cur = (le < r->end) ? le + 1 : le;
        r->line++;
        if (le > ls && le[-1] == '\r') --le;           /* tolerate CRLF */
        if (!mf_utf8_ok((const unsigned char *)ls, (size_t)(le - ls))) return -1;
        if (le == ls || *ls == '#') continue;          /* blank / comment */

        /* split on TAB */
        p = ls;
        for (;;) {
            const char *fs = p;
            while (p < le && *p != '\t') ++p;
            if (np > MF_MAX_FIELDS) return -1;          /* too many fields */
            parts[np].p = fs;
            parts[np].n = (size_t)(p - fs);
            np++;
            if (p >= le) break;
            ++p;                                        /* skip the TAB */
        }

        if (!r->seen_header) {
            if (np != 2 || !mf_str_eq(parts[0], "modforge-loadlist") ||
                !mf_str_eq(parts[1], "1"))
                return -1;
            r->seen_header = 1;
            continue;
        }

        rec->name = parts[0];
        rec->nf = np - 1;
        rec->size = 0;
        {
            int i;
            for (i = 0; i < MF_MAX_FIELDS; ++i) {
                rec->f[i].p = 0;
                rec->f[i].n = 0;
            }
            for (i = 1; i < np; ++i) rec->f[i - 1] = parts[i];
        }

        if (mf_str_eq(parts[0], "title"))          { rec->kind = MF_REC_TITLE;     if (np != 2) return -1; }
        else if (mf_str_eq(parts[0], "engine"))    { rec->kind = MF_REC_ENGINE;    if (np != 2) return -1; }
        else if (mf_str_eq(parts[0], "sdk"))       { rec->kind = MF_REC_SDK;       if (np != 2) return -1; }
        else if (mf_str_eq(parts[0], "framework")) { rec->kind = MF_REC_FRAMEWORK; if (np != 2) return -1; }
        else if (mf_str_eq(parts[0], "mod"))       { rec->kind = MF_REC_MOD;       if (np != 3) return -1; }
        else if (mf_str_eq(parts[0], "order"))     { rec->kind = MF_REC_ORDER;     if (np != 2) return -1; }
        else if (mf_str_eq(parts[0], "file")) {
            rec->kind = MF_REC_FILE;
            if (np != 4 || !mf_path_ok(parts[2]) || !mf_parse_u64(parts[3], &rec->size))
                return -1;
        } else {
            rec->kind = MF_REC_UNKNOWN;
        }
        return 1;
    }
    return r->seen_header ? 0 : -1;                    /* empty input is an error */
}

#ifdef __cplusplus
}
#endif

#endif /* MODFORGE_LOADLIST_H */
