/* mfw8_records.c — see mfw8_records.h for the format + provenance contract. */
#include "mfw8_records.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MFW8_LINE_MAX 4200
#define MFW8_IDENTITY_HEADER "mfw8-identity\t1"

static void set_error(mfw8_records *recs, const char *msg) {
    if (recs) snprintf(recs->error, sizeof recs->error, "%s", msg);
}

int mfw8_token_valid(const char *s) {
    size_t n;
    size_t i;
    if (!s) return 0;
    n = strlen(s);
    if (!n || n > MFW8_TOKEN_MAX) return 0;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c <= 0x20 || c >= 0x7f) return 0;   /* proven is_token, modinst.c:218-226 */
    }
    return 1;
}

int mfw8_state_valid(const char *s) {
    return s && (!strcmp(s, "downloaded") || !strcmp(s, "prepared") ||
                 !strcmp(s, "installed"));
}

int mfw8_state_enum(const char *s) {
    if (!s) return -1;
    if (!strcmp(s, "downloaded")) return MFW8_DOWNLOADED;
    if (!strcmp(s, "prepared")) return MFW8_PREPARED;
    if (!strcmp(s, "installed")) return MFW8_INSTALLED;
    return -1;
}

const char *mfw8_state_name(const char *s) { return mfw8_state_valid(s) ? s : "invalid"; }

static int decimal_valid(const char *s, size_t cap) {
    size_t n = strlen(s);
    size_t i;
    if (!n || n >= cap || (n > 1 && s[0] == '0')) return 0;
    for (i = 0; i < n; ++i) if (s[i] < '0' || s[i] > '9') return 0;
    return 1;
}

/* split one line into at most maxf tab fields (in place) */
static int split_fields(char *line, char *f[], int maxf) {
    int nf = 0;
    char *s = line, *t;
    while (nf < maxf) {
        t = strchr(s, '\t');
        f[nf++] = s;
        if (!t) break;
        *t = 0;
        s = t + 1;
    }
    return nf;
}

static int read_whole_file(const char *path, char **out, size_t *len) {
    FILE *f = fopen(path, "rb");
    long n;
    char *buf;
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END)) { fclose(f); return -1; }
    n = ftell(f);
    if (n < 0 || (unsigned long)n > 64u * 1024u * 1024u) { fclose(f); return -1; }
    if (fseek(f, 0, SEEK_SET)) { fclose(f); return -1; }
    buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return -1; }
    if (n && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return -1; }
    fclose(f);
    buf[n] = 0;
    *out = buf;
    *len = (size_t)n;
    return 0;
}

static void copy_token(char *dst, size_t cap, const char *src) {
    snprintf(dst, cap, "%s", src);
}

int mfw8_records_load(mfw8_records *recs, const char *state_dir) {
    char path[MFW8_PATH_MAX];
    char *buf = NULL;
    size_t len = 0;
    char *line, *save = NULL;
    int first = 1;

    memset(recs, 0, sizeof *recs);
    if (!state_dir || !*state_dir) { set_error(recs, "state_dir required"); return -1; }
    if (snprintf(path, sizeof path, "%.4000s/records.txt", state_dir) >= (int)sizeof path) {
        set_error(recs, "state_dir too long");
        return -1;
    }
    errno = 0;
    if (read_whole_file(path, &buf, &len)) {
        if (errno == ENOENT) return 0;          /* missing = empty store */
        set_error(recs, "records.txt unreadable");
        return -1;
    }
    for (line = strtok_r(buf, "\n", &save); line; line = strtok_r(0, "\n", &save)) {
        char *f[8];
        int nf = split_fields(line, f, 8);
        if (first) {
            first = 0;
            if (strcmp(f[0], "modforge-records") || nf < 2 || strcmp(f[1], "1")) {
                set_error(recs, "unknown records header");
                free(buf);
                return -2;
            }
            continue;
        }
        if (nf >= 5 && !strcmp(f[0], "mod")) {
            mfw8_mod *m;
            if (recs->nmods >= MFW8_MODS_MAX) { set_error(recs, "too many mods"); break; }
            if (!mfw8_token_valid(f[1]) || !mfw8_token_valid(f[2]) ||
                !mfw8_state_valid(f[3]) || !*f[4] || strlen(f[4]) >= MFW8_PATH_MAX) {
                set_error(recs, "malformed mod row");
                free(buf);
                return -2;
            }
            m = &recs->mods[recs->nmods++];
            copy_token(m->id, sizeof m->id, f[1]);
            copy_token(m->title, sizeof m->title, f[2]);
            copy_token(m->state, sizeof m->state, f[3]);
            copy_token(m->bundle, sizeof m->bundle, f[4]);
        } else if (nf >= 6 && !strcmp(f[0], "variant")) {
            mfw8_var *v;
            if (recs->nvars >= MFW8_VARS_MAX) { set_error(recs, "too many variants"); break; }
            if (!mfw8_token_valid(f[1]) || !mfw8_token_valid(f[2]) ||
                !mfw8_token_valid(f[3]) || !*f[4] || strlen(f[4]) >= MFW8_PATH_MAX ||
                (strcmp(f[5], "0") && strcmp(f[5], "1"))) {
                set_error(recs, "malformed variant row");
                free(buf);
                return -2;
            }
            v = &recs->vars[recs->nvars++];
            copy_token(v->mod, sizeof v->mod, f[1]);
            copy_token(v->group, sizeof v->group, f[2]);
            copy_token(v->option, sizeof v->option, f[3]);
            copy_token(v->bundle, sizeof v->bundle, f[4]);
            v->is_default = atoi(f[5]);
        } else if (nf >= 4 && !strcmp(f[0], "choice")) {
            mfw8_choice *c;
            if (recs->nchoices >= MFW8_VARS_MAX) { set_error(recs, "too many choices"); break; }
            if (!mfw8_token_valid(f[1]) || !mfw8_token_valid(f[2]) || !mfw8_token_valid(f[3])) {
                set_error(recs, "malformed choice row");
                free(buf);
                return -2;
            }
            c = &recs->choices[recs->nchoices++];
            copy_token(c->mod, sizeof c->mod, f[1]);
            copy_token(c->group, sizeof c->group, f[2]);
            copy_token(c->option, sizeof c->option, f[3]);
        } else if (nf >= 6 && !strcmp(f[0], "game")) {
            mfw8_game *g;
            /* sdk (f[3]) may be empty: the proven installer writes it only
             * when the bundle carries one (records.txt from the real binary:
             * "game\t<title>\tunreal\t\t2\t<sha>"). */
            size_t sdk_len = strlen(f[3]);
            if (!mfw8_token_valid(f[1]) || !mfw8_token_valid(f[2]) ||
                (sdk_len && !mfw8_token_valid(f[3])) || !decimal_valid(f[4], 32) ||
                strlen(f[5]) != 64) {
                set_error(recs, "malformed game row");
                free(buf);
                return -2;
            }
            g = &recs->games[recs->ngames++];
            copy_token(g->title, sizeof g->title, f[1]);
            copy_token(g->engine, sizeof g->engine, f[2]);
            copy_token(g->sdk, sizeof g->sdk, f[3]);
            g->generation = atol(f[4]);
            copy_token(g->loadlist_sha, sizeof g->loadlist_sha, f[5]);
        } else {
            set_error(recs, "unknown records row");
            free(buf);
            return -2;
        }
    }
    free(buf);
    recs->error[0] = 0;
    return 0;
}

static int write_file_atomic(const char *dir, const char *name, const char *data, size_t len) {
    char tmp[MFW8_PATH_MAX], dst[MFW8_PATH_MAX];
    int fd, rc = -1;
    if (snprintf(tmp, sizeof tmp, "%.3000s/%s.tmp", dir, name) >= (int)sizeof tmp) return -1;
    if (snprintf(dst, sizeof dst, "%.3000s/%s", dir, name) >= (int)sizeof dst) return -1;
    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd >= 0) {
        size_t off = 0;
        int bad = 0;
        while (off < len) {
            ssize_t w = write(fd, data + off, len - off);
            if (w <= 0) { bad = 1; break; }
            off += (size_t)w;
        }
        if (!bad && fsync(fd)) bad = 1;
        if (close(fd)) bad = 1;
        if (!bad && rename(tmp, dst)) bad = 1;
        if (!bad) {
            int dfd = open(dir, O_RDONLY | O_DIRECTORY);
            if (dfd >= 0) {
                if (fsync(dfd)) { /* best effort directory durability */ }
                close(dfd);
            }
            rc = 0;
        }
        if (bad) unlink(tmp);
    }
    return rc;
}

int mfw8_records_store(const mfw8_records *recs, const char *state_dir) {
    char *out;
    size_t cap, used = 0;
    int i, rc;
    if (!recs || !state_dir || !*state_dir) return -1;
    cap = 64 + (size_t)recs->nmods * (MFW8_PATH_MAX + 300) +
          (size_t)recs->nvars * (MFW8_PATH_MAX + 300) +
          (size_t)recs->nchoices * 420 + (size_t)recs->ngames * 420;
    out = malloc(cap);
    if (!out) return -1;
    used += (size_t)snprintf(out + used, cap - used, "%s\n", MFW8_RECORDS_HEADER);
    for (i = 0; i < recs->nmods; ++i)
        used += (size_t)snprintf(out + used, cap - used, "mod\t%s\t%s\t%s\t%s\n",
            recs->mods[i].id, recs->mods[i].title, recs->mods[i].state, recs->mods[i].bundle);
    for (i = 0; i < recs->nvars; ++i)
        used += (size_t)snprintf(out + used, cap - used, "variant\t%s\t%s\t%s\t%s\t%d\n",
            recs->vars[i].mod, recs->vars[i].group, recs->vars[i].option,
            recs->vars[i].bundle, recs->vars[i].is_default);
    for (i = 0; i < recs->nchoices; ++i)
        used += (size_t)snprintf(out + used, cap - used, "choice\t%s\t%s\t%s\n",
            recs->choices[i].mod, recs->choices[i].group, recs->choices[i].option);
    for (i = 0; i < recs->ngames; ++i)
        used += (size_t)snprintf(out + used, cap - used, "game\t%s\t%s\t%s\t%ld\t%s\n",
            recs->games[i].title, recs->games[i].engine, recs->games[i].sdk,
            recs->games[i].generation, recs->games[i].loadlist_sha);
    if (used >= cap) { free(out); return -1; }
    rc = write_file_atomic(state_dir, "records.txt", out, used);
    free(out);
    return rc;
}

const mfw8_mod *mfw8_find_mod(const mfw8_records *recs, const char *id) {
    int i;
    for (i = 0; i < recs->nmods; ++i)
        if (!strcmp(recs->mods[i].id, id)) return &recs->mods[i];
    return NULL;
}

const mfw8_game *mfw8_find_game(const mfw8_records *recs, const char *title) {
    int i;
    for (i = 0; i < recs->ngames; ++i)
        if (!strcmp(recs->games[i].title, title)) return &recs->games[i];
    return NULL;
}

static mfw8_game *game_slot(mfw8_records *recs, const char *title) {
    mfw8_game *g;
    int i;
    for (i = 0; i < recs->ngames; ++i)
        if (!strcmp(recs->games[i].title, title)) return &recs->games[i];
    if (recs->ngames >= MFW8_GAMES_MAX) return NULL;
    g = &recs->games[recs->ngames++];
    memset(g, 0, sizeof *g);
    snprintf(g->title, sizeof g->title, "%s", title);
    return g;
}

int mfw8_register_download(mfw8_records *recs, const char *id, const char *title,
                           const char *bundle_or_null) {
    mfw8_mod *m;
    if (!mfw8_token_valid(id) || !mfw8_token_valid(title)) { set_error(recs, "bad id/title token"); return -1; }
    if (mfw8_find_mod(recs, id)) { set_error(recs, "mod already registered"); return -1; }  /* modinst.c:1160 */
    if (recs->nmods >= MFW8_MODS_MAX) { set_error(recs, "too many mods"); return -1; }
    m = &recs->mods[recs->nmods++];
    memset(m, 0, sizeof *m);
    copy_token(m->id, sizeof m->id, id);
    copy_token(m->title, sizeof m->title, title);
    snprintf(m->state, sizeof m->state, "downloaded");
    snprintf(m->bundle, sizeof m->bundle, "%s", bundle_or_null ? bundle_or_null : "-");
    return 0;
}

int mfw8_mark_prepared(mfw8_records *recs, const char *id, const char *engine, const char *sdk) {
    mfw8_mod *m = NULL;
    int i;
    for (i = 0; i < recs->nmods; ++i)
        if (!strcmp(recs->mods[i].id, id)) { m = &recs->mods[i]; break; }
    if (!m) { set_error(recs, "unknown mod"); return -1; }                 /* e2e-final B4 rule */
    if (!strcmp(m->state, "prepared")) {
        /* idempotent re-prepare is a proven no-op (e2e-final B3) */
        if (engine && mfw8_token_valid(engine)) {
            mfw8_game *g = game_slot(recs, m->title);
            if (g) snprintf(g->engine, sizeof g->engine, "%s", engine);
        }
        if (sdk && mfw8_token_valid(sdk)) {
            mfw8_game *g = game_slot(recs, m->title);
            if (g) snprintf(g->sdk, sizeof g->sdk, "%s", sdk);
        }
        return 1;
    }
    if (strcmp(m->state, "downloaded")) { set_error(recs, "wrong source state"); return -1; }
    snprintf(m->state, sizeof m->state, "prepared");
    return 0;
}

int mfw8_mark_installed(mfw8_records *recs, const char *const *ids, int nids,
                        const char *title, long generation, const char *loadlist_sha) {
    mfw8_game *g;
    int i, changed = 0;
    if (!mfw8_token_valid(title) || !loadlist_sha || strlen(loadlist_sha) != 64) {
        set_error(recs, "bad game/commit facts");
        return -1;
    }
    for (i = 0; i < nids; ++i) {
        mfw8_mod *m = NULL;
        int k;
        for (k = 0; k < recs->nmods; ++k)
            if (!strcmp(recs->mods[k].id, ids[i])) { m = &recs->mods[k]; break; }
        if (!m) { set_error(recs, "unknown mod"); return -1; }
        if (strcmp(m->state, "prepared") && strcmp(m->state, "installed")) {
            set_error(recs, "mod not prepared");
            return -1;
        }
    }
    g = game_slot(recs, title);
    if (!g) { set_error(recs, "too many games"); return -1; }
    for (i = 0; i < nids; ++i) {
        mfw8_mod *m = NULL;
        int k;
        for (k = 0; k < recs->nmods; ++k)
            if (!strcmp(recs->mods[k].id, ids[i])) { m = &recs->mods[k]; break; }
        if (strcmp(m->state, "installed")) {
            snprintf(m->state, sizeof m->state, "installed");
            changed = 1;
        }
    }
    g->generation = generation > 0 ? generation : g->generation + (changed ? 1 : 0);
    copy_token(g->loadlist_sha, sizeof g->loadlist_sha, loadlist_sha);
    return changed ? 0 : 1;
}

int mfw8_mark_uninstalled(mfw8_records *recs, const char *id) {
    mfw8_mod *m = NULL;
    int i;
    for (i = 0; i < recs->nmods; ++i)
        if (!strcmp(recs->mods[i].id, id)) { m = &recs->mods[i]; break; }
    if (!m) { set_error(recs, "unknown mod"); return -1; }
    if (strcmp(m->state, "installed")) { set_error(recs, "mod not installed"); return -1; }
    snprintf(m->state, sizeof m->state, "prepared");   /* modinst.c:844-846 rule */
    return 0;
}

int mfw8_forget(mfw8_records *recs, const char *id) {
    int i;
    const mfw8_mod *m = mfw8_find_mod(recs, id);
    if (!m) { set_error(recs, "unknown mod"); return -1; }
    if (!strcmp(m->state, "installed")) { set_error(recs, "in_use"); return -1; }  /* SERVICE.md:173 */
    for (i = 0; i < recs->nmods; ++i) {
        if (strcmp(recs->mods[i].id, id)) continue;
        memmove(&recs->mods[i], &recs->mods[i + 1],
                sizeof(mfw8_mod) * (size_t)(recs->nmods - i - 1));
        recs->nmods--;
        break;
    }
    for (i = recs->nvars - 1; i >= 0; --i)
        if (!strcmp(recs->vars[i].mod, id)) {
            memmove(&recs->vars[i], &recs->vars[i + 1],
                    sizeof(mfw8_var) * (size_t)(recs->nvars - i - 1));
            recs->nvars--;
        }
    return 0;
}

/* ---- identity sidecar ---- */

static int identity_row_valid(const mfw8_identity *id) {
    return mfw8_token_valid(id->download_id) && mfw8_token_valid(id->title) &&
           mfw8_token_valid(id->provider) && mfw8_token_valid(id->domain) &&
           decimal_valid(id->mod_id, sizeof id->mod_id) &&
           decimal_valid(id->file_id, sizeof id->file_id);
}

int mfw8_identity_load(const char *state_dir, mfw8_identity *out, int max, int *nout,
                       char *error, size_t error_cap) {
    char path[MFW8_PATH_MAX];
    char *buf = NULL;
    size_t len = 0;
    char *line, *save = NULL;
    int first = 1, count = 0;
    if (error_cap) error[0] = 0;
    *nout = 0;
    if (snprintf(path, sizeof path, "%.4000s/identity.tsv", state_dir) >= (int)sizeof path) return -1;
    errno = 0;
    if (read_whole_file(path, &buf, &len)) {
        if (errno == ENOENT) return 0;
        snprintf(error, error_cap, "identity.tsv unreadable");
        return -1;
    }
    for (line = strtok_r(buf, "\n", &save); line; line = strtok_r(0, "\n", &save)) {
        char *f[8];
        int nf;
        mfw8_identity *row;
        if (first) {
            first = 0;
            nf = split_fields(line, f, 8);
            if (strcmp(f[0], "mfw8-identity") || nf < 2 || strcmp(f[1], "1")) {
                snprintf(error, error_cap, "unknown identity header");
                free(buf);
                return -2;
            }
            continue;
        }
        nf = split_fields(line, f, 8);
        if (nf < 6 || count >= max) {
            snprintf(error, error_cap, "malformed identity row");
            free(buf);
            return -2;
        }
        row = &out[count++];
        memset(row, 0, sizeof *row);
        copy_token(row->download_id, sizeof row->download_id, f[0]);
        copy_token(row->title, sizeof row->title, f[1]);
        copy_token(row->provider, sizeof row->provider, f[2]);
        copy_token(row->domain, sizeof row->domain, f[3]);
        copy_token(row->mod_id, sizeof row->mod_id, f[4]);
        copy_token(row->file_id, sizeof row->file_id, f[5]);
        if (!identity_row_valid(row)) {
            snprintf(error, error_cap, "invalid identity row");
            free(buf);
            return -2;
        }
    }
    free(buf);
    *nout = count;
    return 0;
}

static void identity_escape_tabs(char *dst, size_t cap, const char *src) {
    size_t i, n = strlen(src), used = 0;
    for (i = 0; i < n && used + 2 < cap; ++i) {
        if (src[i] == '\t' || src[i] == '\n' || src[i] == '\r') continue; /* cannot occur: tokens */
        dst[used++] = src[i];
    }
    dst[used] = 0;
}

int mfw8_identity_put(const char *state_dir, const mfw8_identity *entry) {
    mfw8_identity rows[MFW8_MODS_MAX];
    int n = 0, i, found = 0;
    char *out;
    size_t cap, used = 0;
    char err[160];
    if (!identity_row_valid(entry)) return -1;
    if (mfw8_identity_load(state_dir, rows, MFW8_MODS_MAX, &n, err, sizeof err)) return -1;
    for (i = 0; i < n; ++i)
        if (!strcmp(rows[i].download_id, entry->download_id)) { rows[i] = *entry; found = 1; }
    if (!found) {
        if (n >= MFW8_MODS_MAX) return -1;
        rows[n++] = *entry;
    }
    cap = 64 + (size_t)n * (MFW8_ID_MAX + 2 * MFW8_TOKEN_MAX + 128);
    out = malloc(cap);
    if (!out) return -1;
    used += (size_t)snprintf(out + used, cap - used, "%s\n", MFW8_IDENTITY_HEADER);
    for (i = 0; i < n; ++i) {
        char a[MFW8_TOKEN_MAX + 1], b[MFW8_TOKEN_MAX + 1];
        identity_escape_tabs(a, sizeof a, rows[i].provider);
        identity_escape_tabs(b, sizeof b, rows[i].domain);
        used += (size_t)snprintf(out + used, cap - used, "%s\t%s\t%s\t%s\t%s\t%s\n",
            rows[i].download_id, rows[i].title, a, b, rows[i].mod_id, rows[i].file_id);
    }
    if (used >= cap) { free(out); return -1; }
    return write_file_atomic(state_dir, "identity.tsv", out, used);
}

int mfw8_identity_get(const char *state_dir, const char *download_id, mfw8_identity *out) {
    mfw8_identity rows[MFW8_MODS_MAX];
    int n = 0, i, rc;
    char err[160];
    rc = mfw8_identity_load(state_dir, rows, MFW8_MODS_MAX, &n, err, sizeof err);
    if (rc) return rc;
    for (i = 0; i < n; ++i)
        if (!strcmp(rows[i].download_id, download_id)) { *out = rows[i]; return 0; }
    return 1;
}
