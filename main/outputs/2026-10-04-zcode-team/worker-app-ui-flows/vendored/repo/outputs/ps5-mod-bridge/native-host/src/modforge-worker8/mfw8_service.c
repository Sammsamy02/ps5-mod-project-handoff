/* mfw8_service.c — see mfw8_service.h for the contract. */
#include "mfw8_service.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mfw8_actions.h"
#include "mfw8_caps.h"
#include "mfw8_records.h"

#define MFW8_REQ_MAX (256u * 1024u)
#define MFW8_REPLY_INIT 4096
#define MFW8_JOBS_MAX 64
#define MFW8_JSON_NODES 256
#define MFW8_JSON_DEPTH 6

/* ---- tiny strict JSON parser (flat objects/arrays, bounded) ---- */

typedef enum jv_kind { JV_NULL, JV_BOOL, JV_NUM, JV_STR, JV_ARR, JV_OBJ } jv_kind;

typedef struct jv {
    jv_kind kind;
    int boolean;
    long number;
    char *str;                    /* JV_STR: unescaped UTF-8, owned */
    struct jv **children;         /* JV_ARR/JV_OBJ */
    char **keys;                  /* JV_OBJ, parallel to children */
    int nchildren;
} jv;

typedef struct jparse {
    const char *p, *end;
    int nodes, depth;
    const char *error;
} jparse;

static jv *jv_new(jparse *J, jv_kind k) {
    jv *v;
    if (++J->nodes > MFW8_JSON_NODES) { J->error = "json node limit"; return NULL; }
    v = calloc(1, sizeof(jv));
    if (v) v->kind = k;
    return v;
}

static void jv_free(jv *v) {
    int i;
    if (!v) return;
    free(v->str);
    for (i = 0; i < v->nchildren; ++i) {
        jv_free(v->children[i]);
        if (v->keys) free(v->keys[i]);
    }
    free(v->children);
    free(v->keys);
    free(v);
}

static void jskip(jparse *J) {
    while (J->p < J->end && (*J->p == ' ' || *J->p == '\t' || *J->p == '\n' || *J->p == '\r')) ++J->p;
}

static jv *jvalue(jparse *J, int depth);

static int jhex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static char *jstring(jparse *J) {
    char *out;
    size_t used = 0;
    if (J->p >= J->end || *J->p != '"') { J->error = "string expected"; return NULL; }
    ++J->p;
    out = malloc((size_t)(J->end - J->p) + 1);
    if (!out) { J->error = "oom"; return NULL; }
    while (J->p < J->end && *J->p != '"') {
        char c = *J->p;
        if (c == '\\') {
            ++J->p;
            if (J->p >= J->end) break;
            switch (*J->p) {
            case '"': case '\\': case '/': out[used++] = *J->p; break;
            case 'b': out[used++] = '\b'; break;
            case 'f': out[used++] = '\f'; break;
            case 'n': out[used++] = '\n'; break;
            case 'r': out[used++] = '\r'; break;
            case 't': out[used++] = '\t'; break;
            case 'u': {
                int v = 0, i;
                for (i = 0; i < 4; ++i) {
                    int h;
                    ++J->p;
                    if (J->p >= J->end || (h = jhex(*J->p)) < 0) { free(out); J->error = "bad \\u"; return NULL; }
                    v = v * 16 + h;
                }
                if (v < 0x20 || v > 0x7e) { free(out); J->error = "non-ascii escape"; return NULL; }
                out[used++] = (char)v;
                break;
            }
            default: free(out); J->error = "bad escape"; return NULL;
            }
            ++J->p;
        } else {
            out[used++] = c;
            ++J->p;
        }
        if (used > MFW8_TOKEN_MAX * 4) { free(out); J->error = "string too long"; return NULL; }
    }
    if (J->p >= J->end) { free(out); J->error = "unterminated string"; return NULL; }
    ++J->p;
    out[used] = 0;
    return out;
}

static jv *jobject_or_array(jparse *J, int depth, char terminator) {
    jv *v = jv_new(J, terminator == '}' ? JV_OBJ : JV_ARR);
    int cap = 0;
    if (!v) return NULL;
    if (++depth > MFW8_JSON_DEPTH) { J->error = "json depth"; jv_free(v); return NULL; }
    ++J->p; /* consume '{' or '[' */
    jskip(J);
    if (J->p < J->end && *J->p == terminator) { ++J->p; return v; }
    for (;;) {
        jv *child;
        jskip(J);
        if (v->kind == JV_OBJ) {
            char *key = jstring(J);
            if (!key) { jv_free(v); return NULL; }
            jskip(J);
            if (J->p >= J->end || *J->p != ':') { free(key); jv_free(v); J->error = "expected :"; return NULL; }
            ++J->p;
            jskip(J);
            child = jvalue(J, depth);
            if (!child) { free(key); jv_free(v); return NULL; }
            if (v->nchildren == cap) {
                cap = cap ? cap * 2 : 8;
                v->children = realloc(v->children, sizeof(jv *) * (size_t)cap);
                v->keys = realloc(v->keys, sizeof(char *) * (size_t)cap);
                if (!v->children || !v->keys) { free(key); jv_free(v); J->error = "oom"; return NULL; }
            }
            v->keys[v->nchildren] = key;
            v->children[v->nchildren++] = child;
        } else {
            child = jvalue(J, depth);
            if (!child) { jv_free(v); return NULL; }
            if (v->nchildren == cap) {
                cap = cap ? cap * 2 : 8;
                v->children = realloc(v->children, sizeof(jv *) * (size_t)cap);
                if (!v->children) { jv_free(v); J->error = "oom"; return NULL; }
            }
            v->children[v->nchildren++] = child;
        }
        jskip(J);
        if (J->p < J->end && *J->p == ',') { ++J->p; continue; }
        if (J->p < J->end && *J->p == terminator) { ++J->p; return v; }
        J->error = "expected , or close";
        jv_free(v);
        return NULL;
    }
}

static jv *jvalue(jparse *J, int depth) {
    jskip(J);
    if (J->p >= J->end) { J->error = "unexpected end"; return NULL; }
    if (*J->p == '{') return jobject_or_array(J, depth, '}');
    if (*J->p == '[') return jobject_or_array(J, depth, ']');
    if (*J->p == '"') {
        jv *v = jv_new(J, JV_STR);
        if (!v) return NULL;
        v->str = jstring(J);
        if (!v->str) { jv_free(v); return NULL; }
        return v;
    }
    if (!strncmp(J->p, "true", (size_t)(J->end - J->p) < 4 ? (size_t)(J->end - J->p) : 4)) {
        jv *v = jv_new(J, JV_BOOL);
        if (v) { v->boolean = 1; J->p += 4; }
        return v;
    }
    if (!strncmp(J->p, "false", (size_t)(J->end - J->p) < 5 ? (size_t)(J->end - J->p) : 5)) {
        jv *v = jv_new(J, JV_BOOL);
        if (v) { J->p += 5; }
        return v;
    }
    if (!strncmp(J->p, "null", (size_t)(J->end - J->p) < 4 ? (size_t)(J->end - J->p) : 4)) {
        jv *v = jv_new(J, JV_NULL);
        if (v) { J->p += 4; }
        return v;
    }
    {   /* number: bounded integer */
        jv *v = jv_new(J, JV_NUM);
        long value = 0;
        int any = 0, neg = 0;
        if (!v) return NULL;
        if (*J->p == '-') { neg = 1; ++J->p; }
        while (J->p < J->end && *J->p >= '0' && *J->p <= '9') {
            value = value * 10 + (*J->p - '0');
            ++J->p;
            ++any;
            if (value > 4611686018427387903L) { jv_free(v); J->error = "number range"; return NULL; }
        }
        if (!any) { jv_free(v); J->error = "bad value"; return NULL; }
        v->number = neg ? -value : value;
        return v;
    }
}

static const jv *jobj_get(const jv *obj, const char *key) {
    int i;
    if (!obj || obj->kind != JV_OBJ) return NULL;
    for (i = 0; i < obj->nchildren; ++i)
        if (!strcmp(obj->keys[i], key)) return obj->children[i];
    return NULL;
}

static void jfree_all(jv *v) { jv_free(v); }

/* ---- reply construction ---- */

typedef struct mfw8_service {
    pthread_mutex_t lock;
    mfw8_actions_config cfg;
    char state_dir_buf[MFW8_PATH_MAX];
    char roots_buf[MFW8_PATH_MAX];
    char installer_buf[MFW8_PATH_MAX];
    char intake_dir[MFW8_PATH_MAX];
    unsigned job_seq;
    struct {
        char id[16];
        char kind[24];
        char state[16];
        char error[64];
        char detail[256];
    } jobs[MFW8_JOBS_MAX];
    int njobs;
} mfw8_service;

typedef struct sbuf {
    char *p;
    size_t used, cap;
    int truncated;
} sbuf;

static void sput(sbuf *b, const char *fmt, ...) {
    va_list ap;
    int n;
    if (b->truncated || b->used >= b->cap) { b->truncated = 1; return; }
    va_start(ap, fmt);
    n = vsnprintf(b->p + b->used, b->cap - b->used, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= b->cap - b->used) { b->truncated = 1; return; }
    b->used += (size_t)n;
}

static void sput_json_str(sbuf *b, const char *s) {
    sput(b, "\"");
    for (; *s; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') sput(b, "\\%c", c);
        else if (c < 0x20) sput(b, "\\u%04x", c);
        else sput(b, "%c", c);
    }
    sput(b, "\"");
}

static void release_free(void *owner, const char *bytes, size_t length) {
    (void)bytes;
    (void)length;
    free(owner);
}

static mfs_owned reply_take(sbuf *b) {
    mfs_owned owned;
    memset(&owned, 0, sizeof owned);
    if (b->truncated || !b->p) {
        static const char too_large[] = "{\"ok\":false,\"error\":{\"code\":\"io_error\",\"message\":\"reply too large\"}}";
        char *copy;
        free(b->p);
        copy = malloc(sizeof too_large);
        if (copy) {
            memcpy(copy, too_large, sizeof too_large);
            owned.bytes = copy;
            owned.length = sizeof too_large - 1;
            owned.owner = copy;
            owned.release = release_free;
        }
        return owned;
    }
    owned.bytes = b->p;
    owned.length = b->used;
    owned.owner = b->p;
    owned.release = release_free;
    return owned;
}

static mfs_owned error_reply(const char *code, const char *message) {
    sbuf b;
    b.cap = 512;
    b.p = malloc(b.cap);
    b.used = 0;
    b.truncated = 0;
    if (!b.p) { mfs_owned none = {0}; return none; }
    sput(&b, "{\"ok\":false,\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}", code, message ? message : "");
    return reply_take(&b);
}

/* ---- service lifecycle ---- */

int mfw8_mfs_open(const mfs_open_params *params, mfs_service **out, mfs_owned *error) {
    jv *config = NULL;
    mfw8_service *svc;
    jparse J;
    const jv *v;
    int rc = -1;

    if (error) memset(error, 0, sizeof *error);
    if (!params || !out || params->abi_version != MFS_ABI_VERSION ||
        params->struct_size < sizeof(mfs_open_params)) {
        if (error) *error = error_reply("bad_request", "abi mismatch");
        return -1;
    }
    J.p = params->config_json.bytes;
    J.end = params->config_json.bytes + params->config_json.length;
    J.nodes = 0;
    J.depth = 0;
    J.error = NULL;
    config = jvalue(&J, 0);
    if (!config || config->kind != JV_OBJ) {
        char msg[128];
        snprintf(msg, sizeof msg, "config must be a JSON object (%s)",
                 J.error ? J.error : "truncated");
        if (error) *error = error_reply("bad_request", msg);
        jfree_all(config);
        return -1;
    }
    v = jobj_get(config, "state_dir");
    if (!v || v->kind != JV_STR || !v->str[0]) {
        if (error) *error = error_reply("bad_request", "state_dir required");
        jfree_all(config);
        return -1;
    }
    svc = calloc(1, sizeof *svc);
    if (!svc) {
        if (error) *error = error_reply("io_error", "oom");
        jfree_all(config);
        return -1;
    }
    pthread_mutex_init(&svc->lock, NULL);
    {
        v = jobj_get(config, "state_dir");
        snprintf(svc->state_dir_buf, sizeof svc->state_dir_buf, "%s", v->str);
        v = jobj_get(config, "mod_root_base");
        if (v && v->kind == JV_STR && v->str[0])
            snprintf(svc->roots_buf, sizeof svc->roots_buf, "%s", v->str);
        else
            svc->roots_buf[0] = 0;
        v = jobj_get(config, "installer_path");
        if (v && v->kind == JV_STR && v->str[0])
            snprintf(svc->installer_buf, sizeof svc->installer_buf, "%s", v->str);
        else
            snprintf(svc->installer_buf, sizeof svc->installer_buf, "modinst_composed");
        svc->cfg.state_dir = svc->state_dir_buf;
        svc->cfg.roots_base = svc->roots_buf;
        svc->cfg.installer_path = svc->installer_buf;
        v = jobj_get(config, "intake_dir");
        if (v && v->kind == JV_STR && v->str[0])
            snprintf(svc->intake_dir, sizeof svc->intake_dir, "%s", v->str);
        else
            snprintf(svc->intake_dir, sizeof svc->intake_dir, "%.3000s/intake", svc->state_dir_buf);
    }
    jfree_all(config);
    /* SERVICE.md: recover interrupted installs at open. Installer absence is
     * reported honestly by capabilities.query, not fabricated as recovery. */
    if (mfw8_installer_present(&svc->cfg)) {
        mfw8_outcome outcome;
        memset(&outcome, 0, sizeof outcome);
        (void)mfw8_act_recover(&svc->cfg, &outcome);
    }
    *out = (mfs_service *)svc;
    rc = 0;
    (void)rc;
    return 0;
}

void mfw8_mfs_close(mfs_service *s) {
    mfw8_service *svc = (mfw8_service *)s;
    if (!svc) return;
    pthread_mutex_destroy(&svc->lock);
    free(svc);
}

/* ---- job table (process-lifetime, synchronous model) ---- */

static const char *job_record(mfw8_service *svc, const char *kind,
                              const mfw8_outcome *outcome) {
    if (svc->njobs >= MFW8_JOBS_MAX) {
        memmove(&svc->jobs[0], &svc->jobs[1], sizeof svc->jobs[0] * (size_t)(MFW8_JOBS_MAX - 1));
        svc->njobs--;
    }
    {
        char *id = svc->jobs[svc->njobs].id;
        char *k = svc->jobs[svc->njobs].kind;
        char *st = svc->jobs[svc->njobs].state;
        char *er = svc->jobs[svc->njobs].error;
        char *de = svc->jobs[svc->njobs].detail;
        snprintf(id, 16, "j-%06u", ++svc->job_seq);
        snprintf(k, 24, "%s", kind);
        snprintf(st, 16, "%s", outcome ? outcome->state : "failed");
        snprintf(er, 64, "%s", outcome ? outcome->error : "io_error");
        snprintf(de, 256, "%s", outcome ? outcome->detail : "");
        svc->njobs++;
        return id;
    }
}

static void emit_job(sbuf *b, mfw8_service *svc, int i) {
    sput(b, "{\"job_id\":\"%s\",\"kind\":\"%s\",\"state\":\"%s\"",
         svc->jobs[i].id, svc->jobs[i].kind, svc->jobs[i].state);
    if (svc->jobs[i].state[0] == 'f')
        sput(b, ",\"error\":{\"code\":\"%s\",\"message\":\"", svc->jobs[i].error);
    else
        sput(b, ",\"result\":{\"detail\":\"");
    {
        const char *c;
        for (c = svc->jobs[i].detail; *c && *c != '"'; ++c) sput(b, "%c", *c);
    }
    if (svc->jobs[i].state[0] == 'f') sput(b, "\"}");
    else sput(b, "\"}");
    sput(b, "}");
}

/* ---- method handlers ---- */

static int title_is_console_title(const char *t) {
    size_t n, i;
    if (!t) return 0;
    n = strlen(t);
    if (n < 5 || n > 20) return 0;
    if (strncmp(t, "PPSA", 4) && strncmp(t, "CUSA", 4)) return 0;
    for (i = 4; i < n; ++i) if (t[i] < '0' || t[i] > '9') return 0;
    return 1;
}

static mfs_owned handle(mfw8_service *svc, const char *method, const jv *params) {
    sbuf b;
    mfw8_records recs;

    b.cap = MFW8_REPLY_INIT;
    b.p = malloc(b.cap);
    b.used = 0;
    b.truncated = 0;
    if (!b.p) { mfs_owned none = {0}; return none; }
    memset(&recs, 0, sizeof recs);
    (void)mfw8_records_load(&recs, svc->cfg.state_dir);

    if (!strcmp(method, "service.describe")) {
        sput(&b, "{\"ok\":true,\"result\":{");
        sput(&b, "\"contract\":\"" MFS_CONTRACT "\"");
        sput(&b, ",\"methods\":[\"service.describe\",\"records.list\",\"mod.register_download\","
                 "\"mod.prepare\",\"install.apply\",\"mod.uninstall\",\"mod.forget\","
                 "\"job.status\",\"job.cancel\",\"job.list\",\"capabilities.query\",\"game.status\"]");
        sput(&b, ",\"job_model\":\"synchronous\",\"job_history\":\"process-lifetime\"");
        sput(&b, ",\"installer_present\":%s", mfw8_installer_present(&svc->cfg) ? "true" : "false");
        sput(&b, ",\"records_store\":\"proven records.txt lineage (worker-7/e2e-final/zip-to-mfb)\"");
        sput(&b, "}}");
        return reply_take(&b);
    }

    if (!strcmp(method, "capabilities.query")) {
        char report[MFW8_CAPS_OUTPUT_MAX];
        mfw8_caps_input in;
        memset(&in, 0, sizeof in);
        in.state_dir = svc->cfg.state_dir;
        in.roots_base = svc->cfg.roots_base;
        in.installer_path = svc->cfg.installer_path;
        in.console_flavor = 0;   /* the app host process reports its own flavor via build; caller may override */
        in.download_linked = 0;
        in.install_evidence = 0;
        if (mfw8_caps_report(&in, report, sizeof report)) {
            free(b.p);
            return error_reply("io_error", "capability report did not fit");
        }
        sput(&b, "{\"ok\":true,\"result\":%s}", report);
        return reply_take(&b);
    }

    if (!strcmp(method, "records.list")) {
        mfw8_identity ids[MFW8_MODS_MAX];
        int nids = 0, i;
        char err[160];
        sput(&b, "{\"ok\":true,\"result\":{\"mods\":[");
        (void)mfw8_identity_load(svc->cfg.state_dir, ids, MFW8_MODS_MAX, &nids, err, sizeof err);
        {
            int first = 1;
            for (i = 0; i < recs.nmods; ++i) {
                mfw8_identity *found = NULL;
                int k;
                for (k = 0; k < nids; ++k)
                    if (!strcmp(ids[k].download_id, recs.mods[i].id)) { found = &ids[k]; break; }
                if (!found) continue;   /* goes to installer_records below */
                if (!first) sput(&b, ",");
                first = 0;
                sput(&b, "{\"download_id\":");
                sput_json_str(&b, recs.mods[i].id);
                sput(&b, ",\"title_id\":");
                sput_json_str(&b, recs.mods[i].title);
                sput(&b, ",\"state\":\"%s\"", recs.mods[i].state);
                sput(&b, ",\"source\":{\"provider\":");
                sput_json_str(&b, found->provider);
                sput(&b, ",\"domain\":");
                sput_json_str(&b, found->domain);
                sput(&b, ",\"mod_id\":%s,\"file_id\":%s}", found->mod_id, found->file_id);
                sput(&b, "}");
            }
        }
        sput(&b, "],\"installer_records\":[");
        for (i = 0; i < recs.nmods; ++i)
            sput(&b, "%s{\"id\":", i ? "," : ""), sput_json_str(&b, recs.mods[i].id),
            sput(&b, ",\"title\":"),
            sput_json_str(&b, recs.mods[i].title),
            sput(&b, ",\"state\":\"%s\",\"bundle\":", recs.mods[i].state),
            sput_json_str(&b, recs.mods[i].bundle), sput(&b, "}");
        sput(&b, "],\"games\":[");
        for (i = 0; i < recs.ngames; ++i) {
            mfw8_runtime_state rt;
            (void)mfw8_runtime_probe(svc->cfg.roots_base, recs.games[i].title, &rt);
            sput(&b, "%s{\"title_id\":", i ? "," : "");
            sput_json_str(&b, recs.games[i].title);
            sput(&b, ",\"engine\":");
            sput_json_str(&b, recs.games[i].engine);
            sput(&b, ",\"installed\":{\"generation\":%ld,\"load_list_sha256\":", recs.games[i].generation);
            sput_json_str(&b, recs.games[i].loadlist_sha);
            sput(&b, "},\"runtime\":{\"state\":\"%s\",\"running\":%s}}",
                 rt.state, rt.running ? "true" : "false");
        }
        sput(&b, "]}}");
        return reply_take(&b);
    }

    if (!strcmp(method, "mod.register_download")) {
        const jv *id = jobj_get(params, "download_id");
        const jv *title = jobj_get(params, "title_id");
        const jv *source = jobj_get(params, "source");
        const jv *domain, *mid, *fid, *prov;
        mfw8_identity ident;
        const char *ids, *ts;
        if (!id || id->kind != JV_STR || !title || title->kind != JV_STR ||
            !source || source->kind != JV_OBJ) {
            free(b.p);
            return error_reply("bad_request", "download_id, title_id and source required");
        }
        ids = id->str;
        ts = title->str;
        if (strlen(ids) != 67 || strncmp(ids, "dl-", 3) || !title_is_console_title(ts)) {
            free(b.p);
            return error_reply("bad_request", "download_id must be dl-<64hex>; title_id must be a PPSA/CUSA title");
        }
        prov = jobj_get(source, "provider");
        domain = jobj_get(source, "domain");
        mid = jobj_get(source, "mod_id");
        fid = jobj_get(source, "file_id");
        if (!prov || prov->kind != JV_STR || !domain || domain->kind != JV_STR ||
            !mid || mid->kind != JV_NUM || mid->number <= 0 ||
            !fid || fid->kind != JV_NUM || fid->number <= 0) {
            free(b.p);
            return error_reply("bad_request", "source requires provider, domain, mod_id, file_id");
        }
        memset(&ident, 0, sizeof ident);
        snprintf(ident.download_id, sizeof ident.download_id, "%s", ids);
        snprintf(ident.title, sizeof ident.title, "%s", ts);
        snprintf(ident.provider, sizeof ident.provider, "%s", prov->str);
        snprintf(ident.domain, sizeof ident.domain, "%s", domain->str);
        snprintf(ident.mod_id, sizeof ident.mod_id, "%ld", mid->number);
        snprintf(ident.file_id, sizeof ident.file_id, "%ld", fid->number);
        if (strcmp(ident.provider, "nexus")) {
            free(b.p);
            return error_reply("bad_request", "provider must be nexus (the app's authorized source)");
        }
        if (mfw8_register_download(&recs, ids, ts, NULL) ||
            mfw8_records_store(&recs, svc->cfg.state_dir) ||
            mfw8_identity_put(svc->cfg.state_dir, &ident)) {
            free(b.p);
            return error_reply("io_error", "records store write failed");
        }
        sput(&b, "{\"ok\":true,\"result\":{\"download_id\":");
        sput_json_str(&b, ids);
        sput(&b, ",\"title_id\":");
        sput_json_str(&b, ts);
        sput(&b, ",\"state\":\"downloaded\",\"registration\":\"registered\","
                 "\"requirements_status\":\"unknown\",\"source\":{\"provider\":\"nexus\",\"domain\":");
        sput_json_str(&b, ident.domain);
        sput(&b, ",\"mod_id\":%s,\"file_id\":%s}}", ident.mod_id, ident.file_id);
        return reply_take(&b);
    }

    if (!strcmp(method, "mod.prepare")) {
        const jv *mods = jobj_get(params, "mod_ids");
        const char *title = NULL;
        const jv *tj = jobj_get(params, "title_id");
        mfw8_outcome outcome;
        mfw8_identity *ident = NULL;
        mfw8_identity ids[MFW8_MODS_MAX];
        int nids = 0, i, is_archive = 0;
        char err[160], archive_path[MFW8_PATH_MAX], quarantine[MFW8_PATH_MAX], mods_csv[2048];
        size_t used = 0;
        if (tj && tj->kind == JV_STR) title = tj->str;
        if (!mods || mods->kind != JV_ARR || mods->nchildren < 1 || mods->nchildren > 8 || !title) {
            free(b.p);
            return error_reply("bad_request", "title_id and 1..8 mod_ids required");
        }
        if (mods->nchildren > 1) {
            /* The proven intake is one archive per download id; a multi-mod
             * archive prepare would need a source list this vertical does not
             * claim. Refuse specifically instead of guessing. */
            int all_prepared = 1, k;
            for (k = 0; k < mods->nchildren; ++k) {
                const mfw8_mod *r = mfw8_find_mod(&recs, mods->children[k]->str);
                if (!r || strcmp(r->state, "prepared")) { all_prepared = 0; break; }
            }
            if (!all_prepared) {
                free(b.p);
                return error_reply("bad_request", "multi-mod prepare supported only for already-prepared (idempotent) sets; the intake is one archive per download id");
            }
        }
        (void)mfw8_identity_load(svc->cfg.state_dir, ids, MFW8_MODS_MAX, &nids, err, sizeof err);
        for (i = 0; i < mods->nchildren; ++i) {
            const jv *m = mods->children[i];
            const mfw8_mod *row;
            int k;
            if (!m || m->kind != JV_STR) { free(b.p); return error_reply("bad_request", "mod_ids must be strings"); }
            row = mfw8_find_mod(&recs, m->str);
            if (!row) { free(b.p); return error_reply("unknown_download", "unknown mod id"); }
            if (!strcmp(row->state, "prepared")) continue;   /* idempotent re-prepare */
            if (strcmp(row->state, "downloaded")) { free(b.p); return error_reply("not_ready", "mod not downloadable"); }
            for (k = 0; k < nids; ++k)
                if (!strcmp(ids[k].download_id, row->id)) { ident = &ids[k]; break; }
            if (!ident || strcmp(ident->title, title)) {
                free(b.p);
                return error_reply("bad_request", "mod has no app identity for this title; use the installer intake path");
            }
            is_archive = 1;
            if (used && used < sizeof mods_csv) mods_csv[used++] = ',';
            used += (size_t)snprintf(mods_csv + used, sizeof mods_csv - used, "%s", row->id);
        }
        memset(&outcome, 0, sizeof outcome);
        if (is_archive) {
            mfw8_archive_spec spec;
            memset(&spec, 0, sizeof spec);
            spec.title = title;
            snprintf(archive_path, sizeof archive_path, "%.3000s/%.128s.zip", svc->intake_dir, mods_csv);
            snprintf(quarantine, sizeof quarantine, "%.3000s/quarantine", svc->cfg.state_dir);
            spec.archive_path = archive_path;
            spec.quarantine_dir = quarantine;
            spec.mod_id = mods_csv;   /* single-mod vertical: one archive per download id */
            if (mfw8_act_prepare_archive(&svc->cfg, &spec, &outcome)) {
                free(b.p);
                return error_reply("io_error", "installer could not run");
            }
        } else {
            mfw8_prepare_spec spec;
            memset(&spec, 0, sizeof spec);
            spec.title = title;
            spec.mods_csv = mods_csv;
            if (mfw8_act_prepare(&svc->cfg, &spec, &outcome)) {
                free(b.p);
                return error_reply("io_error", "installer could not run");
            }
        }
        {
            const char *id = job_record(svc, "mod.prepare", &outcome);
            if (!strcmp(outcome.state, "succeeded")) {
                sput(&b, "{\"ok\":true,\"result\":{\"job_id\":\"%s\",\"state\":\"succeeded\",\"mod\":", id);
                sput_json_str(&b, mods_csv);
                sput(&b, "}}");
            } else {
                sput(&b, "{\"ok\":true,\"result\":{\"job_id\":\"%s\",\"state\":\"%s\",\"error\":{\"code\":\"%s\"}}",
                     id, outcome.state, outcome.error);
            }
        }
        return reply_take(&b);
    }

    if (!strcmp(method, "install.apply") || !strcmp(method, "mod.uninstall")) {
        const jv *mods = jobj_get(params, "mod_ids");
        const jv *tj = jobj_get(params, "title_id");
        const char *title = tj && tj->kind == JV_STR ? tj->str : NULL;
        mfw8_outcome outcome;
        char mods_csv[2048];
        size_t used = 0;
        int i;
        if (!title) { free(b.p); return error_reply("bad_request", "title_id required"); }
        if (!mods || mods->kind != JV_ARR || mods->nchildren < 1 || mods->nchildren > 64) {
            free(b.p);
            return error_reply("bad_request", "1..64 mod_ids required");
        }
        for (i = 0; i < mods->nchildren; ++i) {
            const jv *m = mods->children[i];
            if (!m || m->kind != JV_STR) { free(b.p); return error_reply("bad_request", "mod_ids must be strings"); }
            if (used && used < sizeof mods_csv) mods_csv[used++] = ',';
            used += (size_t)snprintf(mods_csv + used, sizeof mods_csv - used, "%s", m->str);
        }
        memset(&outcome, 0, sizeof outcome);
        if (!strcmp(method, "install.apply")) {
            mfw8_install_spec spec;
            memset(&spec, 0, sizeof spec);
            spec.title = title;
            spec.mods_csv = mods_csv;
            if (mfw8_act_install(&svc->cfg, &spec, &outcome)) {
                free(b.p);
                return error_reply("io_error", "installer could not run");
            }
        } else {
            if (mfw8_act_uninstall(&svc->cfg, title, mods_csv, &outcome)) {
                free(b.p);
                return error_reply("io_error", "installer could not run");
            }
        }
        {
            const char *id = job_record(svc, !strcmp(method, "install.apply") ? "install.apply" : "mod.uninstall", &outcome);
            if (!strcmp(outcome.state, "succeeded")) {
                /* reload records so the reply reflects the transaction commit */
                (void)mfw8_records_load(&recs, svc->cfg.state_dir);
                {
                    const mfw8_game *g = mfw8_find_game(&recs, title);
                    sput(&b, "{\"ok\":true,\"result\":{\"job_id\":\"%s\",\"state\":\"succeeded\",\"title_id\":", id);
                    sput_json_str(&b, title);
                    if (g) sput(&b, ",\"generation\":%ld,\"load_list_sha256\":", g->generation),
                               sput_json_str(&b, g->loadlist_sha);
                    sput(&b, "}}");
                }
            } else {
                sput(&b, "{\"ok\":true,\"result\":{\"job_id\":\"%s\",\"state\":\"%s\",\"error\":{\"code\":\"%s\"}}",
                     id, outcome.state, outcome.error);
            }
        }
        return reply_take(&b);
    }

    if (!strcmp(method, "mod.forget")) {
        const jv *id = jobj_get(params, "download_id");
        int rc;
        if (!id || id->kind != JV_STR) { free(b.p); return error_reply("bad_request", "download_id required"); }
        rc = mfw8_forget(&recs, id->str);
        if (rc < 0) {
            free(b.p);
            return error_reply(recs.error, recs.error);
        }
        if (mfw8_records_store(&recs, svc->cfg.state_dir)) {
            free(b.p);
            return error_reply("io_error", "records store write failed");
        }
        sput(&b, "{\"ok\":true,\"result\":{\"forgotten\":");
        sput_json_str(&b, id->str);
        sput(&b, "}}");
        return reply_take(&b);
    }

    if (!strcmp(method, "game.status")) {
        const jv *tj = jobj_get(params, "title_id");
        mfw8_runtime_state rt;
        if (!tj || tj->kind != JV_STR) { free(b.p); return error_reply("bad_request", "title_id required"); }
        if (mfw8_runtime_probe(svc->cfg.roots_base, tj->str, &rt)) {
            free(b.p);
            return error_reply("bad_request", "title must be a token");
        }
        sput(&b, "{\"ok\":true,\"result\":{\"title_id\":");
        sput_json_str(&b, tj->str);
        sput(&b, ",\"runtime\":{\"state\":\"%s\",\"running\":%s,\"ack_present\":%s}}",
             rt.state, rt.running ? "true" : "false", rt.ack_present ? "true" : "false");
        return reply_take(&b);
    }

    if (!strcmp(method, "job.status")) {
        const jv *id = jobj_get(params, "job_id");
        int i;
        if (!id || id->kind != JV_STR) { free(b.p); return error_reply("bad_request", "job_id required"); }
        for (i = 0; i < svc->njobs; ++i)
            if (!strcmp(svc->jobs[i].id, id->str)) {
                sput(&b, "{\"ok\":true,\"result\":");
                emit_job(&b, svc, i);
                sput(&b, "}");
                return reply_take(&b);
            }
        free(b.p);
        return error_reply("unknown_job", "no such job in this process");
    }

    if (!strcmp(method, "job.cancel")) {
        const jv *id = jobj_get(params, "job_id");
        int i;
        if (!id || id->kind != JV_STR) { free(b.p); return error_reply("bad_request", "job_id required"); }
        for (i = 0; i < svc->njobs; ++i)
            if (!strcmp(svc->jobs[i].id, id->str)) {
                /* Synchronous model: terminal jobs cannot be cancelled; reply
                 * the current state honestly (documented no-op). */
                sput(&b, "{\"ok\":true,\"result\":");
                emit_job(&b, svc, i);
                sput(&b, ",\"cancel_requested\":false}");
                return reply_take(&b);
            }
        free(b.p);
        return error_reply("unknown_job", "no such job in this process");
    }

    if (!strcmp(method, "job.list")) {
        int i;
        sput(&b, "{\"ok\":true,\"result\":{\"jobs\":[");
        for (i = 0; i < svc->njobs; ++i) {
            if (i) sput(&b, ",");
            emit_job(&b, svc, i);
        }
        sput(&b, "]}}");
        return reply_take(&b);
    }

    free(b.p);
    return error_reply("unknown_method", method);
}

mfs_owned mfw8_mfs_request(mfs_service *s, mfs_view request_json) {
    mfw8_service *svc = (mfw8_service *)s;
    jparse J;
    jv *req = NULL;
    const jv *method, *params;
    mfs_owned reply;
    if (!svc) { mfs_owned none = {0}; return none; }
    if (!request_json.bytes || !request_json.length || request_json.length > MFW8_REQ_MAX)
        return error_reply("bad_request", "request envelope invalid");
    pthread_mutex_lock(&svc->lock);
    J.p = request_json.bytes;
    J.end = request_json.bytes + request_json.length;
    J.nodes = 0;
    J.depth = 0;
    J.error = NULL;
    req = jvalue(&J, 0);
    jskip(&J);
    if (!req || req->kind != JV_OBJ || J.p != J.end || J.error) {
        jfree_all(req);
        pthread_mutex_unlock(&svc->lock);
        return error_reply("bad_request", "request must be one flat JSON object");
    }
    method = jobj_get(req, "method");
    params = jobj_get(req, "params");
    if (!method || method->kind != JV_STR || !method->str[0] ||
        (params && params->kind != JV_OBJ)) {
        jfree_all(req);
        pthread_mutex_unlock(&svc->lock);
        return error_reply("bad_request", "method required, params must be an object");
    }
    reply = handle(svc, method->str, params ? params : NULL);
    jfree_all(req);
    pthread_mutex_unlock(&svc->lock);
    return reply;
}

#ifndef MFW8_NO_MFS_ALIASES
int mfs_open(const mfs_open_params *params, mfs_service **out, mfs_owned *error) {
    return mfw8_mfs_open(params, out, error);
}
mfs_owned mfs_request(mfs_service *svc, mfs_view request_json) {
    return mfw8_mfs_request(svc, request_json);
}
void mfs_close(mfs_service *svc) { mfw8_mfs_close(svc); }
#endif
