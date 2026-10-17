/*
 * modforge_bundle.h — console-side installer for modforge PS5 bundles.
 *
 * The host (PC or catalog server) resolves everything — which mods, conflicts,
 * dependencies, load order, conversion — and writes one bundle per game: every
 * file at its final path with its SHA-256, plus loadlist.txt. Format: see
 * modforge/bundle.py. This header installs such a bundle on the console:
 *
 *   - validates the whole header and index before writing anything
 *     (paths relative, no "..", no backslashes or control characters, every
 *     entry's bytes inside the file, exactly one loadlist.txt);
 *   - writes ".modforge-root", then every file, hashing as it goes;
 *   - writes loadlist.txt LAST, so an interrupted or corrupt install never has
 *     a load list and the runtime loads nothing from it.
 *
 * Single header, C99, no allocation, no libc calls. All I/O goes through the
 * callbacks in mfb_io, so on the PS5 they map to sceKernelOpen/Write/Mkdir
 * (or whatever the app uses); the caller supplies a work buffer of any size.
 * The caller must hand in an empty (or freshly cleared) mod root.
 *
 *   mfb_error err;
 *   long n = mfb_install(&io, bundle_size, buf, sizeof buf, &err);
 *   if (n < 0) { ... err.msg says why; clear the mod root ... }
 */
#ifndef MODFORGE_BUNDLE_H
#define MODFORGE_BUNDLE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MFB_HEADER_SIZE 32u
#define MFB_ENTRY_FIXED 50u
#define MFB_MAX_PATH 1024u
#define MFB_MAX_ENTRIES 1000000u
#define MFB_LOADLIST "loadlist.txt"
#define MFB_MODINFO "modinfo.txt"
#define MFB_KIND_GAME 0u
#define MFB_KIND_MOD 1u
#define MFB_MARKER ".modforge-root"

typedef struct {
    void *ctx;
    /* Read exactly n bytes at byte offset off of the bundle. 0 = ok. */
    int (*read_at)(void *ctx, uint64_t off, void *buf, size_t n);
    /* Create every missing directory of rel_dir (relative to the mod root). 0 = ok. */
    int (*make_dirs)(void *ctx, const char *rel_dir);
    /* Create/truncate rel_path for writing. Returns a handle >= 0, or < 0. */
    int (*open_write)(void *ctx, const char *rel_path);
    /* Write all n bytes. 0 = ok. */
    int (*write)(void *ctx, int handle, const void *buf, size_t n);
    /* 0 = ok. */
    int (*close)(void *ctx, int handle);
    /* Optional (may be NULL): delete a file that failed its hash check. */
    int (*remove)(void *ctx, const char *rel_path);
} mfb_io;

typedef struct {
    char msg[192];
} mfb_error;

/* ---- SHA-256 ----------------------------------------------------------- */

typedef struct {
    uint32_t h[8];
    uint64_t len;
    unsigned char block[64];
    size_t fill;
} mfb_sha256;

static const uint32_t mfb_k256[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

#define MFB_ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static inline void mfb_sha256_block(mfb_sha256 *s, const unsigned char *p) {
    uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;
    int i;
    for (i = 0; i < 16; ++i)
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 |
               (uint32_t)p[4 * i + 2] << 8 | (uint32_t)p[4 * i + 3];
    for (i = 16; i < 64; ++i) {
        uint32_t s0 = MFB_ROR(w[i - 15], 7) ^ MFB_ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = MFB_ROR(w[i - 2], 17) ^ MFB_ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3];
    e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; ++i) {
        t1 = h + (MFB_ROR(e, 6) ^ MFB_ROR(e, 11) ^ MFB_ROR(e, 25)) + ((e & f) ^ (~e & g)) +
             mfb_k256[i] + w[i];
        t2 = (MFB_ROR(a, 2) ^ MFB_ROR(a, 13) ^ MFB_ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d;
    s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static inline void mfb_sha256_init(mfb_sha256 *s) {
    static const uint32_t iv[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                   0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    int i;
    for (i = 0; i < 8; ++i) s->h[i] = iv[i];
    s->len = 0;
    s->fill = 0;
}

static inline void mfb_sha256_update(mfb_sha256 *s, const void *data, size_t n) {
    const unsigned char *p = (const unsigned char *)data;
    s->len += (uint64_t)n;
    while (n) {
        size_t take = 64 - s->fill;
        size_t i;
        if (take > n) take = n;
        for (i = 0; i < take; ++i) s->block[s->fill + i] = p[i];
        s->fill += take;
        p += take;
        n -= take;
        if (s->fill == 64) {
            mfb_sha256_block(s, s->block);
            s->fill = 0;
        }
    }
}

static inline void mfb_sha256_final(mfb_sha256 *s, unsigned char out[32]) {
    uint64_t bits = s->len * 8u;
    unsigned char pad = 0x80, zero = 0, lenbuf[8];
    int i;
    mfb_sha256_update(s, &pad, 1);
    while (s->fill != 56) mfb_sha256_update(s, &zero, 1);
    for (i = 0; i < 8; ++i) lenbuf[i] = (unsigned char)(bits >> (56 - 8 * i));
    mfb_sha256_update(s, lenbuf, 8);
    for (i = 0; i < 8; ++i) {
        out[4 * i] = (unsigned char)(s->h[i] >> 24);
        out[4 * i + 1] = (unsigned char)(s->h[i] >> 16);
        out[4 * i + 2] = (unsigned char)(s->h[i] >> 8);
        out[4 * i + 3] = (unsigned char)s->h[i];
    }
}

/* ---- helpers ------------------------------------------------------------ */

static inline uint32_t mfb_le32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline uint64_t mfb_le64(const unsigned char *p) {
    return (uint64_t)mfb_le32(p) | (uint64_t)mfb_le32(p + 4) << 32;
}

/* Strict UTF-8 (no overlong forms, no surrogates, max U+10FFFF), the same
 * rules as the host's decoder, so both sides accept exactly the same paths. */
static inline int mfb_utf8_ok(const unsigned char *s, size_t n) {
    size_t i = 0;
    while (i < n) {
        unsigned char c = s[i];
        size_t need;
        unsigned char lo = 0x80, hi = 0xBF;
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
        for (size_t k = 2; k <= need; ++k)
            if (s[i + k] < 0x80 || s[i + k] > 0xBF) return 0;
        i += need + 1;
    }
    return 1;
}

static inline void mfb_fail(mfb_error *err, const char *what, const char *path) {
    size_t i = 0, j = 0;
    if (!err) return;
    while (what[j] && i + 1 < sizeof err->msg) err->msg[i++] = what[j++];
    if (path) {
        const char *sep = ": ";
        size_t plen = 0;
        int utf8;
        while (path[plen]) ++plen;
        utf8 = mfb_utf8_ok((const unsigned char *)path, plen);
        for (j = 0; sep[j] && i + 1 < sizeof err->msg; ++j) err->msg[i++] = sep[j];
        /* The path may be hostile: the message must stay printable, valid
         * UTF-8, so control characters (and any non-ASCII byte of an invalid
         * UTF-8 path) become '?'. */
        for (j = 0; j < plen && i + 1 < sizeof err->msg; ++j) {
            unsigned char c = (unsigned char)path[j];
            int bad = c < 0x20 || c == 0x7f || (!utf8 && c >= 0x80);
            err->msg[i++] = bad ? '?' : path[j];
        }
        /* If the message was truncated, don't leave a multi-byte character cut
         * in half: back off to the start of the last character. */
        if (j < plen) {
            while (i > 0 && ((unsigned char)err->msg[i - 1] & 0xC0) == 0x80) --i;
            if (i > 0 && ((unsigned char)err->msg[i - 1] & 0xC0) == 0xC0) --i;
        }
    }
    err->msg[i] = 0;
}

static inline void mfb_zero(void *p, size_t n) {
    unsigned char *b = (unsigned char *)p;
    while (n--) *b++ = 0;
}

static inline int mfb_streq(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

/* Relative, '/'-separated, valid UTF-8, no empty/"."/".." segment, no
 * backslash or control character. 1 = valid. */
static inline int mfb_path_ok(const char *p, size_t n) {
    size_t i, seg = 0;
    if (n == 0 || p[0] == '/') return 0;
    if (!mfb_utf8_ok((const unsigned char *)p, n)) return 0;
    for (i = 0; i <= n; ++i) {
        if (i == n || p[i] == '/') {
            size_t len = i - seg;
            if (len == 0) return 0;
            if (len == 1 && p[seg] == '.') return 0;
            if (len == 2 && p[seg] == '.' && p[seg + 1] == '.') return 0;
            seg = i + 1;
        } else {
            unsigned char c = (unsigned char)p[i];
            if (c == '\\' || c < 0x20 || c == 0x7f) return 0;
        }
    }
    return 1;
}

typedef struct {
    uint64_t offset;
    uint64_t size;
    unsigned char sha256[32];
    char path[MFB_MAX_PATH + 1];
} mfb_entry;

/* Read index entry starting at *pos (absolute file offset); advances *pos. */
static inline int mfb_read_entry(const mfb_io *io, uint64_t *pos, uint64_t index_end,
                                 uint64_t data_start, uint64_t bundle_size,
                                 mfb_entry *e, mfb_error *err) {
    unsigned char fixed[MFB_ENTRY_FIXED];
    uint32_t plen, i;
    if (index_end - *pos < MFB_ENTRY_FIXED) { mfb_fail(err, "bundle index is shorter than its entries", 0); return -1; }
    if (io->read_at(io->ctx, *pos, fixed, MFB_ENTRY_FIXED)) { mfb_fail(err, "read error in bundle index", 0); return -1; }
    *pos += MFB_ENTRY_FIXED;
    e->offset = mfb_le64(fixed);
    e->size = mfb_le64(fixed + 8);
    for (i = 0; i < 32; ++i) e->sha256[i] = fixed[16 + i];
    plen = (uint32_t)fixed[48] | (uint32_t)fixed[49] << 8;
    if (plen == 0 || plen > MFB_MAX_PATH || index_end - *pos < plen) {
        mfb_fail(err, "bundle entry has a bad path length", 0);
        return -1;
    }
    if (io->read_at(io->ctx, *pos, e->path, plen)) { mfb_fail(err, "read error in bundle index", 0); return -1; }
    *pos += plen;
    e->path[plen] = 0;
    for (i = 0; i < plen; ++i) {
        if (e->path[i] == 0) { mfb_fail(err, "bundle path contains a NUL byte", 0); return -1; }
    }
    if (!mfb_path_ok(e->path, plen) || mfb_streq(e->path, MFB_MARKER)) {
        mfb_fail(err, "bundle entry has an unsafe path", e->path);
        return -1;
    }
    if (e->offset < data_start || e->offset > bundle_size || e->size > bundle_size - e->offset) {
        mfb_fail(err, "bundle entry lies outside the file", e->path);
        return -1;
    }
    return 0;
}

static inline int mfb_write_entry(const mfb_io *io, const mfb_entry *e,
                                  unsigned char *work, size_t work_size, mfb_error *err) {
    size_t i, slash = 0;
    int handle;
    uint64_t done = 0;
    mfb_sha256 sha;
    unsigned char digest[32];

    for (i = 0; e->path[i]; ++i)
        if (e->path[i] == '/') slash = i;
    if (slash) {
        char dir[MFB_MAX_PATH + 1];
        for (i = 0; i < slash; ++i) dir[i] = e->path[i];
        dir[slash] = 0;
        if (io->make_dirs(io->ctx, dir)) { mfb_fail(err, "cannot create directory for", e->path); return -1; }
    }
    handle = io->open_write(io->ctx, e->path);
    if (handle < 0) { mfb_fail(err, "cannot create", e->path); return -1; }

    mfb_sha256_init(&sha);
    while (done < e->size) {
        size_t n = work_size;
        if ((uint64_t)n > e->size - done) n = (size_t)(e->size - done);
        if (io->read_at(io->ctx, e->offset + done, work, n) || io->write(io->ctx, handle, work, n)) {
            io->close(io->ctx, handle);
            mfb_fail(err, "read/write error while installing", e->path);
            return -1;
        }
        mfb_sha256_update(&sha, work, n);
        done += n;
    }
    if (io->close(io->ctx, handle)) { mfb_fail(err, "cannot finish writing", e->path); return -1; }
    mfb_sha256_final(&sha, digest);
    for (i = 0; i < 32; ++i) {
        if (digest[i] != e->sha256[i]) {
            if (io->remove) io->remove(io->ctx, e->path);
            mfb_fail(err, "hash mismatch (bundle is corrupt)", e->path);
            return -1;
        }
    }
    return 0;
}

/* Install a bundle into the (empty) mod root. Returns the number of files
 * installed, or -1 with err->msg set. */
static inline long mfb_install(const mfb_io *io, uint64_t bundle_size,
                               unsigned char *work, size_t work_size, mfb_error *err) {
    static const char marker_text[] =
        "This directory is managed by modforge; its contents are replaced on each install.\n";
    unsigned char header[MFB_HEADER_SIZE];
    uint32_t count, i, loadlists = 0, modinfos = 0;
    uint64_t index_size, data_start, pos;
    mfb_entry e, ll;
    int handle;
    const char *magic = "MFBUNDL1";

    if (err) err->msg[0] = 0;
    mfb_zero(&ll, sizeof ll);
    if (!io || !io->read_at || !io->make_dirs || !io->open_write || !io->write || !io->close ||
        !work || work_size == 0) {
        mfb_fail(err, "bad arguments", 0);
        return -1;
    }
    if (bundle_size < MFB_HEADER_SIZE || io->read_at(io->ctx, 0, header, MFB_HEADER_SIZE)) {
        mfb_fail(err, "bundle is truncated", 0);
        return -1;
    }
    for (i = 0; i < 8; ++i) {
        if (header[i] != (unsigned char)magic[i]) { mfb_fail(err, "not a modforge bundle", 0); return -1; }
    }
    if (mfb_le32(header + 8) != 1) { mfb_fail(err, "unsupported bundle version", 0); return -1; }
    if (mfb_le32(header + 24) != MFB_KIND_GAME) {
        mfb_fail(err, mfb_le32(header + 24) == MFB_KIND_MOD
                          ? "this is a mod bundle: merge it with the composer"
                          : "unknown bundle kind", 0);
        return -1;
    }
    count = mfb_le32(header + 12);
    index_size = mfb_le64(header + 16);
    if (count > MFB_MAX_ENTRIES || index_size > bundle_size - MFB_HEADER_SIZE) {
        mfb_fail(err, "bundle index is implausible", 0);
        return -1;
    }
    data_start = MFB_HEADER_SIZE + index_size;

    /* Pass 1: validate everything before touching the filesystem. */
    pos = MFB_HEADER_SIZE;
    for (i = 0; i < count; ++i) {
        if (mfb_read_entry(io, &pos, data_start, data_start, bundle_size, &e, err)) return -1;
        if (mfb_streq(e.path, MFB_LOADLIST)) {
            ll = e;
            loadlists++;
        }
        if (mfb_streq(e.path, MFB_MODINFO)) modinfos++;
    }
    if (pos != data_start) { mfb_fail(err, "bundle index size does not match its entries", 0); return -1; }
    if (loadlists != 1 || modinfos) {
        mfb_fail(err, "a game bundle must contain exactly one " MFB_LOADLIST " and no " MFB_MODINFO, 0);
        return -1;
    }

    /* Pass 2: marker, every file, then the load list last. */
    handle = io->open_write(io->ctx, MFB_MARKER);
    if (handle < 0 || io->write(io->ctx, handle, marker_text, sizeof marker_text - 1) ||
        io->close(io->ctx, handle)) {
        mfb_fail(err, "cannot write " MFB_MARKER, 0);
        return -1;
    }
    pos = MFB_HEADER_SIZE;
    for (i = 0; i < count; ++i) {
        if (mfb_read_entry(io, &pos, data_start, data_start, bundle_size, &e, err)) return -1;
        if (mfb_streq(e.path, MFB_LOADLIST)) continue;
        if (mfb_write_entry(io, &e, work, work_size, err)) return -1;
    }
    if (mfb_write_entry(io, &ll, work, work_size, err)) return -1;
    return (long)count;
}

#ifdef __cplusplus
}
#endif

#endif /* MODFORGE_BUNDLE_H */
