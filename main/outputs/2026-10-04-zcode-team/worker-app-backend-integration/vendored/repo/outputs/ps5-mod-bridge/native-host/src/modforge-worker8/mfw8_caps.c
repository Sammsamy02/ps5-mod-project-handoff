/* mfw8_caps.c — see mfw8_caps.h. Honest states only; running only from ack. */
#include "mfw8_caps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mfw8_actions.h"

#define MFW8_LINE_MAX 4200

static int read_file_bounded(const char *path, char **out, size_t *len) {
    FILE *f = fopen(path, "rb");
    long n;
    char *buf;
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END)) { fclose(f); return -1; }
    n = ftell(f);
    if (n < 0 || (unsigned long)n > 4u * 1024u * 1024u) { fclose(f); return -1; }
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

static const char *ack_states[] = {
    "not-started", "activated", "engine-ready", "mods-started", "failed", "stopped"
};

static int ack_state_known(const char *s) {
    size_t i;
    for (i = 0; i < sizeof ack_states / sizeof ack_states[0]; ++i)
        if (!strcmp(s, ack_states[i])) return 1;
    return 0;
}

int mfw8_runtime_probe(const char *roots_base, const char *title, mfw8_runtime_state *out) {
    char path[MFW8_PATH_MAX * 2];
    char *buf = NULL;
    size_t len = 0;
    char *line, *save = NULL;
    memset(out, 0, sizeof *out);
    snprintf(out->state, sizeof out->state, "not-started");   /* proven default (modinst.c:1249) */
    if (!roots_base || !title || !mfw8_token_valid(title)) return -1;
    if (snprintf(path, sizeof path, "%.4000s/%.128s/runtime-status.txt", roots_base, title)
        >= (int)sizeof path) return -1;
    if (read_file_bounded(path, &buf, &len)) return 0;        /* missing = not-started */
    out->ack_present = 1;
    for (line = strtok_r(buf, "\n", &save); line; line = strtok_r(0, "\n", &save)) {
        char *f[6];
        int nf = 0;
        char *s = line, *t;
        while (nf < 6) {
            t = strchr(s, '\t');
            f[nf++] = s;
            if (!t) break;
            *t = 0;
            s = t + 1;
        }
        /* ack line: "state\t<seq>\t<state>\t<code>\t<detail>" (mfa_status_write) */
        if (nf >= 4 && !strcmp(f[0], "state") && ack_state_known(f[2])) {
            snprintf(out->state, sizeof out->state, "%s", f[2]);
            if (nf >= 5) snprintf(out->detail, sizeof out->detail, "%s", f[4]);
        }
    }
    free(buf);
    out->running = !strcmp(out->state, "mods-started");       /* the ONLY running source */
    return 0;
}

int mfw8_caps_table_load(const char *state_dir, mfw8_caps_framework *out, int max, int *nout) {
    char path[MFW8_PATH_MAX];
    char *buf = NULL;
    size_t len = 0;
    char *line, *save = NULL;
    int count = 0;
    *nout = 0;
    if (!state_dir || !out || max <= 0) return -1;
    if (snprintf(path, sizeof path, "%.4000s/caps.tsv", state_dir) >= (int)sizeof path) return -1;
    if (read_file_bounded(path, &buf, &len)) return 0;        /* missing = empty table */
    for (line = strtok_r(buf, "\n", &save); line; line = strtok_r(0, "\n", &save)) {
        char *tab;
        mfw8_caps_framework *row;
        if (count >= max) break;
        if (!*line || *line == '#') continue;
        tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = 0;
        row = &out[count];
        memset(row, 0, sizeof *row);
        snprintf(row->id, sizeof row->id, "%s", line);
        snprintf(row->maturity, sizeof row->maturity, "%s", tab + 1);
        if (!mfw8_token_valid(row->id)) continue;
        if (strcmp(row->maturity, "available") && strcmp(row->maturity, "integrating") &&
            strcmp(row->maturity, "planned")) continue;
        ++count;
    }
    free(buf);
    *nout = count;
    return 0;
}

static int json_escape(char *dst, size_t cap, const char *src) {
    size_t used = 0;
    for (; *src && used + 7 < cap; ++src) {
        unsigned char c = (unsigned char)*src;
        if (c == '"' || c == '\\') {
            dst[used++] = '\\';
            dst[used++] = (char)c;
        } else if (c < 0x20) {
            used += (size_t)snprintf(dst + used, cap - used, "\\u%04x", c);
        } else {
            dst[used++] = (char)c;
        }
    }
    dst[used] = 0;
    return 0;
}

int mfw8_caps_report(const mfw8_caps_input *in, char *output, size_t cap) {
    mfw8_records recs;
    mfw8_caps_framework frameworks[MFW8_CAPS_FRAMEWORKS_MAX];
    mfw8_runtime_state rt;
    mfw8_actions_config cfg;
    size_t used = 0;
    int nfw = 0, i;
    int counts[3] = {0, 0, 0};
    char esc[MFW8_PATH_MAX + 64];
    int rc_store;
    int have_title;

    if (!in || !in->state_dir || !in->roots_base || !output || cap < 256) return -1;
    memset(&recs, 0, sizeof recs);
    rc_store = mfw8_records_load(&recs, in->state_dir);
    for (i = 0; i < recs.nmods; ++i) {
        int e = mfw8_state_enum(recs.mods[i].state);
        if (e >= 0) counts[e]++;
    }
    mfw8_caps_table_load(in->state_dir, frameworks, MFW8_CAPS_FRAMEWORKS_MAX, &nfw);
    cfg.installer_path = in->installer_path;
    cfg.state_dir = in->state_dir;
    cfg.roots_base = in->roots_base;
    have_title = in->title && mfw8_token_valid(in->title);
    if (have_title) mfw8_runtime_probe(in->roots_base, in->title, &rt);
    else {
        memset(&rt, 0, sizeof rt);
        snprintf(rt.state, sizeof rt.state, "not-started");
    }

#define EMIT(...) used += (size_t)snprintf(output + used, cap - used, __VA_ARGS__)
    EMIT("{");
    EMIT("\"contract\":\"modforge-service/1\"");
    EMIT(",\"report_scope\":\"worker-8 drop-in vertical (records+actions+caps); host-proven, console integration pending\"");
    EMIT(",\"records\":{");
    if (rc_store == 0) {
        EMIT("\"state\":\"ok\",\"mods\":%d", recs.nmods);
        EMIT(",\"downloaded\":%d,\"prepared\":%d,\"installed\":%d",
             counts[MFW8_DOWNLOADED], counts[MFW8_PREPARED], counts[MFW8_INSTALLED]);
        EMIT(",\"games\":%d", recs.ngames);
    } else {
        json_escape(esc, sizeof esc, rc_store == -2 ? recs.error : "records.txt unreadable");
        EMIT("\"state\":\"%s\",\"error\":\"%s\"", rc_store == -2 ? "corrupt" : "unavailable", esc);
    }
    EMIT("}");
    EMIT(",\"installer\":{");
    json_escape(esc, sizeof esc, in->installer_path ? in->installer_path : "");
    EMIT("\"path\":\"%s\",\"present\":%s", esc,
         mfw8_installer_present(&cfg) ? "true" : "false");
    EMIT(",\"lineage\":\"worker-7 -> installer-composition -> e2e-final -> zip-to-mfb\"");
    EMIT("}");
    EMIT(",\"download\":{");
    EMIT("\"linked_in_this_flavor\":%s", in->download_linked ? "true" : "false");
    EMIT(",\"flavor\":\"%s\"", in->console_flavor ? "console" : "host");
    EMIT(",\"note\":\"stage-1 download TU link is flavor-dependent (e2e-final F1, ps5io-rdwr relink)\"");
    EMIT("}");
    EMIT(",\"runtime\":{");
    EMIT("\"ack_source\":\"<roots>/%s/runtime-status.txt\"", have_title ? in->title : "<title>");
    EMIT(",\"state\":\"%s\",\"running\":%s,\"ack_present\":%s",
         rt.state, rt.running ? "true" : "false", rt.ack_present ? "true" : "false");
    EMIT(",\"rule\":\"running=true derives only from a real runtime mods-started ack; never stored, never inferred\"");
    EMIT("}");
    EMIT(",\"frameworks\":[");
    for (i = 0; i < nfw; ++i) {
        json_escape(esc, sizeof esc, frameworks[i].id);
        EMIT("%s{\"id\":\"%s\",\"maturity\":\"%s\"}", i ? "," : "", esc, frameworks[i].maturity);
    }
    EMIT("]");
    EMIT(",\"installation_supported\":%s", in->install_evidence ? "true" : "false");
    EMIT(",\"job_model\":\"synchronous-actions (serialized); cooperative cancel at proven safe points\"");
    EMIT("}");
#undef EMIT
    if (used >= cap) return -1;
    return 0;
}
