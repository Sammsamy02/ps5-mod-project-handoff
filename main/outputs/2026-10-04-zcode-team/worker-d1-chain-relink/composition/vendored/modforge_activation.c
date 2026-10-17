/*
 * modforge_activation.c — see modforge_activation.h.
 */
#include "modforge_activation.h"
#include "modforge_loadlist.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#define MFA_PATH_MAX 1024
#define MFA_CHUNK (256u * 1024u)

struct mfa_activation {
    char *arena;               /* every string, NUL-terminated               */
    unsigned char *blob;       /* every preloaded file, each followed by NUL */
    const char *title, *engine, *sdk;
    const char **frameworks;
    size_t n_frameworks;
    mfa_mod *mods;
    size_t n_mods;
    mfa_file *files;
    size_t n_files;
    const char **order;
    size_t n_order;
    uint64_t preloaded;
};

/* --- default POSIX I/O ---------------------------------------------------- */

static int posix_open(void *ctx, const char *path, int *os_error) {
    int fd;
    (void)ctx;
    errno = 0;
    fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) *os_error = errno;
    return fd;
}

static long posix_read(void *ctx, int fd, void *buf, size_t n, int *os_error) {
    unsigned tries = 0;
    (void)ctx;
    for (;;) {
        ssize_t got;
        errno = 0;
        got = read(fd, buf, n);
        if (got < 0 && errno == EINTR && tries++ < 8) continue;
        if (got < 0) *os_error = errno;
        return (long)got;
    }
}

static int posix_close(void *ctx, int fd) {
    (void)ctx;
    return close(fd);
}

static uint64_t posix_now_ms(void *ctx) {
    struct timespec ts;
    (void)ctx;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static const mfa_io mfa_posix = {0, posix_open, posix_read, posix_close, posix_now_ms};

/* --- errors ---------------------------------------------------------------- */

static void copy_printable(char *dst, size_t cap, const char *src, size_t n) {
    size_t i, o = 0;
    if (cap == 0) return;
    for (i = 0; i < n && src[i] && o + 1 < cap; ++i) {
        unsigned char c = (unsigned char)src[i];
        dst[o++] = (c >= 0x20 && c < 0x7f) ? (char)c : '?';
    }
    dst[o] = 0;
}

static int fail(mfa_error *err, int code, int os_error, const char *msg,
                const char *path, size_t path_n, int line) {
    if (err) {
        err->code = code;
        err->os_error = os_error;
        err->line = line;
        copy_printable(err->message, sizeof err->message, msg, strlen(msg));
        if (path) copy_printable(err->path, sizeof err->path, path, path_n);
        else err->path[0] = 0;
    }
    return code;
}

const char *mfa_result_name(int code) {
    switch (code) {
    case MFA_OK: return "ok";
    case MFA_E_INVALID: return "invalid";
    case MFA_E_NO_LOADLIST: return "no-loadlist";
    case MFA_E_LOADLIST: return "loadlist";
    case MFA_E_TITLE: return "title";
    case MFA_E_LIMIT: return "limit";
    case MFA_E_OPEN: return "open";
    case MFA_E_READ: return "read";
    case MFA_E_SIZE: return "size";
    case MFA_E_MEMORY: return "memory";
    case MFA_E_CANCELLED: return "cancelled";
    case MFA_E_DEADLINE: return "deadline";
    case MFA_E_DUPLICATE: return "duplicate";
    default: return "unknown";
    }
}

/* --- helpers --------------------------------------------------------------- */

typedef struct {
    const mfa_config *cfg;
    const mfa_io *io;
    mfa_limits lim;
} mfa_ctx;

static int stop_requested(const mfa_ctx *c, mfa_error *err, const char *path, size_t n) {
    if (c->cfg->cancel && *c->cfg->cancel)
        return fail(err, MFA_E_CANCELLED, 0, "activation cancelled", path, n, 0);
    if (c->cfg->deadline_ms && c->io->now_ms(c->io->ctx) >= c->cfg->deadline_ms)
        return fail(err, MFA_E_DEADLINE, 0, "activation deadline passed", path, n, 0);
    return 0;
}

static int field_ok(mf_str s) {
    size_t i;
    for (i = 0; i < s.n; ++i) {
        unsigned char c = (unsigned char)s.p[i];
        if (c < 0x20 || c == 0x7f) return 0;
    }
    return 1;
}

static int str_eq_c(mf_str s, const char *c) {
    size_t n = strlen(c);
    return s.n == n && memcmp(s.p, c, n) == 0;
}

static int join_path(char *out, const char *root, const char *rel, size_t rel_n) {
    size_t rn = strlen(root);
    while (rn > 1 && root[rn - 1] == '/') --rn;
    if (rn + 1 + rel_n + 1 > MFA_PATH_MAX) return -1;
    memcpy(out, root, rn);
    out[rn] = '/';
    memcpy(out + rn + 1, rel, rel_n);
    out[rn + 1 + rel_n] = 0;
    return 0;
}

/* Read exactly `size` bytes (and confirm EOF) from root/rel into dst. */
static int read_exact(const mfa_ctx *c, const char *rel, size_t rel_n, unsigned char *dst,
                      uint64_t size, uint64_t max_bytes, mfa_error *err) {
    char path[MFA_PATH_MAX];
    int fd, os = 0, rc = 0;
    uint64_t used = 0;
    if (join_path(path, c->cfg->root, rel, rel_n) < 0)
        return fail(err, MFA_E_LIMIT, 0, "path too long", rel, rel_n, 0);
    fd = c->io->open_read(c->io->ctx, path, &os);
    if (fd < 0)
        return fail(err, MFA_E_OPEN, os, max_bytes ? "cannot open listed file"
                                                   : "cannot open loadlist.txt",
                    rel, rel_n, 0);
    while (used < size) {
        size_t want = (size - used) > MFA_CHUNK ? MFA_CHUNK : (size_t)(size - used);
        long got;
        if ((rc = stop_requested(c, err, rel, rel_n)) != 0) break;
        os = 0;
        got = c->io->read(c->io->ctx, fd, dst + used, want, &os);
        if (got < 0) {
            rc = fail(err, MFA_E_READ, os, "read failed", rel, rel_n, 0);
            break;
        }
        if (got == 0) {
            rc = fail(err, MFA_E_SIZE, 0, "file is shorter than the load list says", rel,
                      rel_n, 0);
            break;
        }
        if ((size_t)got > want) {
            rc = fail(err, MFA_E_READ, 0, "read returned more than asked", rel, rel_n, 0);
            break;
        }
        used += (uint64_t)got;
    }
    if (rc == 0) {
        unsigned char extra;
        long got;
        os = 0;
        got = c->io->read(c->io->ctx, fd, &extra, 1, &os);
        if (got < 0) rc = fail(err, MFA_E_READ, os, "read failed", rel, rel_n, 0);
        else if (got != 0)
            rc = fail(err, MFA_E_SIZE, 0, "file is longer than the load list says", rel,
                      rel_n, 0);
    }
    c->io->close(c->io->ctx, fd);
    return rc;
}

/* Read loadlist.txt into a fresh buffer (NUL-terminated). */
static int read_loadlist(const mfa_ctx *c, char **out, size_t *out_n, mfa_error *err) {
    static const char name[] = "loadlist.txt";
    char path[MFA_PATH_MAX];
    int fd, os = 0, rc = 0;
    size_t cap = 4096, used = 0;
    char *buf;
    if (join_path(path, c->cfg->root, name, sizeof name - 1) < 0)
        return fail(err, MFA_E_LIMIT, 0, "root path too long", name, sizeof name - 1, 0);
    fd = c->io->open_read(c->io->ctx, path, &os);
    if (fd < 0) {
        if (os == ENOENT)
            return fail(err, MFA_E_NO_LOADLIST, os, "no loadlist.txt (nothing installed)",
                        name, sizeof name - 1, 0);
        return fail(err, MFA_E_OPEN, os, "cannot open loadlist.txt", name, sizeof name - 1, 0);
    }
    buf = (char *)malloc(cap + 1);
    if (!buf) {
        c->io->close(c->io->ctx, fd);
        return fail(err, MFA_E_MEMORY, 0, "out of memory", name, sizeof name - 1, 0);
    }
    for (;;) {
        long got;
        if (used == cap) {
            char *grown;
            if ((uint64_t)cap >= c->lim.max_loadlist_bytes + 1) {
                rc = fail(err, MFA_E_LIMIT, 0, "loadlist.txt is too large", name,
                          sizeof name - 1, 0);
                break;
            }
            cap = cap * 2;
            if ((uint64_t)cap > c->lim.max_loadlist_bytes + 1)
                cap = (size_t)c->lim.max_loadlist_bytes + 1;
            grown = (char *)realloc(buf, cap + 1);
            if (!grown) {
                rc = fail(err, MFA_E_MEMORY, 0, "out of memory", name, sizeof name - 1, 0);
                break;
            }
            buf = grown;
        }
        if ((rc = stop_requested(c, err, name, sizeof name - 1)) != 0) break;
        os = 0;
        got = c->io->read(c->io->ctx, fd, buf + used, cap - used, &os);
        if (got < 0) {
            rc = fail(err, MFA_E_READ, os, "read failed", name, sizeof name - 1, 0);
            break;
        }
        if (got == 0) break;
        if ((size_t)got > cap - used) {
            rc = fail(err, MFA_E_READ, 0, "read returned more than asked", name,
                      sizeof name - 1, 0);
            break;
        }
        used += (size_t)got;
        if ((uint64_t)used > c->lim.max_loadlist_bytes) {
            rc = fail(err, MFA_E_LIMIT, 0, "loadlist.txt is too large", name, sizeof name - 1,
                      0);
            break;
        }
    }
    c->io->close(c->io->ctx, fd);
    if (rc) {
        free(buf);
        return rc;
    }
    buf[used] = 0;
    *out = buf;
    *out_n = used;
    return 0;
}

static const char *const mfa_default_roles[] = {"lua_mod", "config", "script", 0};

static int role_preloaded(const mfa_config *cfg, mf_str role) {
    const char *const *r = cfg->preload_roles ? cfg->preload_roles : mfa_default_roles;
    for (; *r; ++r)
        if (str_eq_c(role, *r)) return 1;
    return 0;
}

static int role_preloaded_c(const mfa_config *cfg, const char *role) {
    mf_str s;
    s.p = role;
    s.n = strlen(role);
    return role_preloaded(cfg, s);
}

static int cmp_files(const void *a, const void *b) {
    return strcmp(((const mfa_file *)a)->path, ((const mfa_file *)b)->path);
}

static char *put(char **cursor, mf_str s) {
    char *at = *cursor;
    memcpy(at, s.p, s.n);
    at[s.n] = 0;
    *cursor = at + s.n + 1;
    return at;
}

/* --- activation ------------------------------------------------------------ */

int mfa_activate(const mfa_config *cfg, mfa_activation **out, mfa_error *err) {
    mfa_ctx c;
    char *text = 0;
    size_t text_n = 0, arena_bytes = 0;
    size_t n_fw = 0, n_mods = 0, n_files = 0, n_order = 0;
    int n_title = 0, n_engine = 0, n_sdk = 0;
    uint64_t preload_total = 0;
    mf_reader r;
    mf_record rec;
    int rc;
    mfa_activation *a = 0;

    if (err) {
        err->code = 0;
        err->os_error = 0;
        err->line = 0;
        err->path[0] = 0;
        err->message[0] = 0;
    }
    if (!out) return fail(err, MFA_E_INVALID, 0, "no output pointer", 0, 0, 0);
    *out = 0;
    if (!cfg || !cfg->root || !cfg->root[0] || !cfg->title_id)
        return fail(err, MFA_E_INVALID, 0, "root and title_id are required", 0, 0, 0);
    c.cfg = cfg;
    c.io = cfg->io ? cfg->io : &mfa_posix;
    c.lim = cfg->limits;
    if (!c.lim.max_files) c.lim.max_files = 4096;
    if (!c.lim.max_file_bytes) c.lim.max_file_bytes = 8u << 20;
    if (!c.lim.max_total_bytes) c.lim.max_total_bytes = 64u << 20;
    if (!c.lim.max_loadlist_bytes) c.lim.max_loadlist_bytes = 1u << 20;

    if ((rc = stop_requested(&c, err, 0, 0)) != 0) return rc;
    if ((rc = read_loadlist(&c, &text, &text_n, err)) != 0) return rc;

    /* Pass 1: validate everything and size the arena. */
    mf_reader_init(&r, text, text_n);
    while ((rc = mf_next(&r, &rec)) > 0) {
        int i;
        if (rec.kind == MF_REC_UNKNOWN) continue;
        for (i = 0; i < rec.nf; ++i) {
            if (!field_ok(rec.f[i])) {
                rc = fail(err, MFA_E_LOADLIST, 0, "control character in a field", 0, 0, r.line);
                free(text);
                return rc;
            }
            arena_bytes += rec.f[i].n + 1;
        }
        switch (rec.kind) {
        case MF_REC_TITLE: n_title++; break;
        case MF_REC_ENGINE: n_engine++; break;
        case MF_REC_SDK: n_sdk++; break;
        case MF_REC_FRAMEWORK: n_fw++; break;
        case MF_REC_MOD: n_mods++; break;
        case MF_REC_ORDER: n_order++; break;
        case MF_REC_FILE:
            if (++n_files > c.lim.max_files) {
                rc = fail(err, MFA_E_LIMIT, 0, "too many files", 0, 0, r.line);
                free(text);
                return rc;
            }
            if (rec.f[1].n + 2 > MFA_PATH_MAX) {
                rc = fail(err, MFA_E_LIMIT, 0, "path too long", rec.f[1].p, rec.f[1].n, r.line);
                free(text);
                return rc;
            }
            if (role_preloaded(cfg, rec.f[0])) {
                if (rec.size > c.lim.max_file_bytes) {
                    rc = fail(err, MFA_E_LIMIT, 0, "file too large to preload", rec.f[1].p,
                                rec.f[1].n, r.line);
                    free(text);
                    return rc;
                }
                preload_total += rec.size + 1;
                if (preload_total > c.lim.max_total_bytes) {
                    rc = fail(err, MFA_E_LIMIT, 0, "preloaded files exceed the total limit",
                                rec.f[1].p, rec.f[1].n, r.line);
                    free(text);
                    return rc;
                }
            }
            break;
        default: break;
        }
    }
    if (rc < 0) {
        int line = r.line;
        rc = fail(err, MFA_E_LOADLIST, 0, "malformed loadlist.txt", 0, 0, line);
        free(text);
        return rc;
    }
    if (n_title != 1 || n_engine != 1 || n_sdk > 1) {
        rc = fail(err, MFA_E_LOADLIST, 0, "loadlist.txt needs one title and one engine",
                    0, 0, 0);
        free(text);
        return rc;
    }

    a = (mfa_activation *)calloc(1, sizeof *a);
    if (a) a->arena = (char *)malloc(arena_bytes + 1);
    if (a && n_fw) a->frameworks = (const char **)calloc(n_fw, sizeof(char *));
    if (a && n_mods) a->mods = (mfa_mod *)calloc(n_mods, sizeof(mfa_mod));
    if (a && n_files) a->files = (mfa_file *)calloc(n_files, sizeof(mfa_file));
    if (a && n_order) a->order = (const char **)calloc(n_order, sizeof(char *));
    if (a && preload_total) a->blob = (unsigned char *)malloc((size_t)preload_total);
    if (!a || !a->arena || (n_fw && !a->frameworks) || (n_mods && !a->mods) ||
        (n_files && !a->files) || (n_order && !a->order) || (preload_total && !a->blob)) {
        mfa_release(a);
        rc = fail(err, MFA_E_MEMORY, 0, "out of memory", 0, 0, 0);
        free(text);
        return rc;
    }

    /* Pass 2: copy the strings. */
    {
        char *cur = a->arena;
        a->sdk = "";
        mf_reader_init(&r, text, text_n);
        while (mf_next(&r, &rec) > 0) {
            switch (rec.kind) {
            case MF_REC_TITLE: a->title = put(&cur, rec.f[0]); break;
            case MF_REC_ENGINE: a->engine = put(&cur, rec.f[0]); break;
            case MF_REC_SDK: a->sdk = put(&cur, rec.f[0]); break;
            case MF_REC_FRAMEWORK: a->frameworks[a->n_frameworks++] = put(&cur, rec.f[0]); break;
            case MF_REC_MOD:
                a->mods[a->n_mods].id = put(&cur, rec.f[0]);
                a->mods[a->n_mods].version = put(&cur, rec.f[1]);
                a->n_mods++;
                break;
            case MF_REC_ORDER: a->order[a->n_order++] = put(&cur, rec.f[0]); break;
            case MF_REC_FILE: {
                mfa_file *f = &a->files[a->n_files++];
                f->role = put(&cur, rec.f[0]);
                f->path = put(&cur, rec.f[1]);
                f->size = rec.size;
                f->data = 0;   /* set in pass 3 for preloaded roles */
                break;
            }
            default: break;
            }
        }
    }
    free(text);

    if (strcmp(a->title, cfg->title_id) != 0) {
        rc = fail(err, MFA_E_TITLE, 0, "load list is for a different game", a->title,
                  strlen(a->title), 0);
        mfa_release(a);
        return rc;
    }

    if (a->n_files > 1) qsort(a->files, a->n_files, sizeof(mfa_file), cmp_files);
    {
        size_t i;
        for (i = 1; i < a->n_files; ++i) {
            if (strcmp(a->files[i - 1].path, a->files[i].path) == 0) {
                rc = fail(err, MFA_E_DUPLICATE, 0, "path listed twice", a->files[i].path,
                          strlen(a->files[i].path), 0);
                mfa_release(a);
                return rc;
            }
        }
    }

    /* Pass 3: read the preloaded files, in path order. */
    {
        size_t i;
        uint64_t off = 0;
        for (i = 0; i < a->n_files; ++i) {
            mfa_file *f = &a->files[i];
            if (!role_preloaded_c(cfg, f->role)) continue;
            rc = read_exact(&c, f->path, strlen(f->path), a->blob + off, f->size,
                            c.lim.max_file_bytes, err);
            if (rc) {
                mfa_release(a);
                return rc;
            }
            a->blob[off + f->size] = 0;
            f->data = a->blob + off;
            off += f->size + 1;
        }
        a->preloaded = off;
    }
    *out = a;
    return MFA_OK;
}

void mfa_release(mfa_activation *a) {
    if (!a) return;
    free(a->arena);
    free(a->blob);
    free((void *)a->frameworks);
    free(a->mods);
    free(a->files);
    free((void *)a->order);
    free(a);
}

const char *mfa_title(const mfa_activation *a) { return a->title; }
const char *mfa_engine(const mfa_activation *a) { return a->engine; }
const char *mfa_sdk(const mfa_activation *a) { return a->sdk; }
uint64_t mfa_preloaded_bytes(const mfa_activation *a) { return a->preloaded; }
size_t mfa_framework_count(const mfa_activation *a) { return a->n_frameworks; }
const char *mfa_framework_at(const mfa_activation *a, size_t i) {
    return i < a->n_frameworks ? a->frameworks[i] : 0;
}
int mfa_wants_framework(const mfa_activation *a, const char *id) {
    size_t i;
    for (i = 0; i < a->n_frameworks; ++i)
        if (strcmp(a->frameworks[i], id) == 0) return 1;
    return 0;
}
size_t mfa_mod_count(const mfa_activation *a) { return a->n_mods; }
const mfa_mod *mfa_mod_at(const mfa_activation *a, size_t i) {
    return i < a->n_mods ? &a->mods[i] : 0;
}
size_t mfa_file_count(const mfa_activation *a) { return a->n_files; }
const mfa_file *mfa_file_at(const mfa_activation *a, size_t i) {
    return i < a->n_files ? &a->files[i] : 0;
}
size_t mfa_lower_bound(const mfa_activation *a, const char *prefix) {
    size_t lo = 0, hi = a->n_files;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (strcmp(a->files[mid].path, prefix) < 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}
const mfa_file *mfa_find(const mfa_activation *a, const char *path) {
    size_t i = mfa_lower_bound(a, path);
    return (i < a->n_files && strcmp(a->files[i].path, path) == 0) ? &a->files[i] : 0;
}
size_t mfa_order_count(const mfa_activation *a) { return a->n_order; }
const char *mfa_order_at(const mfa_activation *a, size_t i) {
    return i < a->n_order ? a->order[i] : 0;
}

/* --- status file ----------------------------------------------------------- */

static int write_all(int fd, const char *p, size_t n) {
    while (n) {
        ssize_t w;
        errno = 0;
        w = write(fd, p, n);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) return -(errno ? errno : EIO);
        p += w;
        n -= (size_t)w;
    }
    return 0;
}

static size_t put_u64(char *out, uint64_t v) {
    char tmp[24];
    size_t n = 0, i;
    do {
        tmp[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    for (i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
    return n;
}

/* Copy UTF-8 text, replacing control characters and invalid sequences with
 * '?', never splitting a character, within cap bytes. */
static size_t put_text(char *out, size_t cap, const char *s) {
    size_t i = 0, o = 0, n = s ? strlen(s) : 0;
    while (i < n) {
        const unsigned char *u = (const unsigned char *)s + i;
        size_t len = 1;
        int ok = 1;
        if (u[0] < 0x20 || u[0] == 0x7f) ok = 0;
        else if (u[0] >= 0x80) {
            if (u[0] >= 0xC2 && u[0] <= 0xDF) len = 2;
            else if (u[0] >= 0xE0 && u[0] <= 0xEF) len = 3;
            else if (u[0] >= 0xF0 && u[0] <= 0xF4) len = 4;
            else len = 1, ok = 0;
            if (ok && (i + len > n || !mf_utf8_ok(u, len))) len = 1, ok = 0;
        }
        if (o + (ok ? len : 1) > cap) break;
        if (ok) memcpy(out + o, u, len);
        else out[o] = '?';
        o += ok ? len : 1;
        i += len;
    }
    return o;
}

int mfa_status_open(mfa_status *s, const char *root, long pid, uint64_t now_ms) {
    char path[MFA_PATH_MAX], line[96];
    static const char name[] = "runtime-status.txt";
    static const char head[] = "modforge-runtime-status\t1\nsession\t";
    size_t n = 0;
    int rc;
    s->fd = -1;
    s->seq = 0;
    if (!root || join_path(path, root, name, sizeof name - 1) < 0) return -EINVAL;
    errno = 0;
    s->fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0644);
    if (s->fd < 0) return -(errno ? errno : EIO);
    memcpy(line, head, sizeof head - 1);
    n = sizeof head - 1;
    if (pid < 0) pid = 0;
    n += put_u64(line + n, (uint64_t)pid);
    line[n++] = '\t';
    n += put_u64(line + n, now_ms);
    line[n++] = '\n';
    rc = write_all(s->fd, line, n);
    if (rc) {
        close(s->fd);
        s->fd = -1;
    }
    return rc;
}

int mfa_status_write(mfa_status *s, const char *state, int code, const char *detail) {
    char line[512];
    size_t n = 0;
    if (!s || s->fd < 0 || !state) return -EINVAL;
    memcpy(line, "state\t", 6);
    n = 6;
    n += put_u64(line + n, ++s->seq);
    line[n++] = '\t';
    n += put_text(line + n, 32, state);
    line[n++] = '\t';
    if (code < 0) {
        line[n++] = '-';
        n += put_u64(line + n, (uint64_t)(-(int64_t)code));
    } else {
        n += put_u64(line + n, (uint64_t)code);
    }
    line[n++] = '\t';
    n += put_text(line + n, sizeof line - n - 1, detail);
    line[n++] = '\n';
    return write_all(s->fd, line, n);
}

int mfa_status_note(mfa_status *s, const char *key, const char *text) {
    char line[1024];
    size_t n = 0;
    if (!s || s->fd < 0 || !key) return -EINVAL;
    memcpy(line, "note\t", 5);
    n = 5;
    n += put_u64(line + n, ++s->seq);
    line[n++] = '\t';
    n += put_text(line + n, 64, key);
    line[n++] = '\t';
    n += put_text(line + n, sizeof line - n - 1, text);
    line[n++] = '\n';
    return write_all(s->fd, line, n);
}

void mfa_status_close(mfa_status *s) {
    if (s && s->fd >= 0) {
        close(s->fd);
        s->fd = -1;
    }
}
