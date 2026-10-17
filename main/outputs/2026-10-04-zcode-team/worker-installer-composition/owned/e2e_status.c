/*
 * e2e_status.c — see e2e_status.h. Owned by worker-installer-composition.
 *
 * I/O goes through the vendored ps5_io adapter (sceKernel set on console,
 * POSIX on host) so the console payload adds no new provider beyond the
 * nm-verified set. Imports: ps5io_* plus libc snprintf/strlen/strcmp only.
 */
#include "e2e_status.h"

#include <stdio.h>
#include <string.h>

#include "ps5_io.h"

#define E2E_MAX_LINES 512
#define E2E_MAX_BYTES 262144
#define E2E_LINE 512

static char g_dir[4096];
static int g_lines;
static long g_bytes;
static int g_truncated;

static void sanitize(char *out, size_t cap, const char *in) {
    size_t i = 0;
    if (!in) in = "";
    for (; in[i] && i + 1 < cap; ++i) {
        unsigned char c = (unsigned char)in[i];
        out[i] = (c < 32 || c > 126 || c == '"' || c == '\\') ? '_' : (char)c;
    }
    out[i] = 0;
}

static void emit(const char *line) {
    char path[4096 + 32];
    int fd, n;
    size_t len = strlen(line);
    if (g_truncated) return;
    if (g_lines >= E2E_MAX_LINES || g_bytes + (long)len + 1 > E2E_MAX_BYTES) {
        g_truncated = 1;
        /* one last line, then silence forever */
        if (g_lines < E2E_MAX_LINES) {
            const char t[] = "{\"event\":\"truncated\"}\n";
            n = snprintf(path, sizeof path, "%s/e2e-status.jsonl", g_dir);
            if (n > 0 && (size_t)n < sizeof path) {
                fd = ps5io_open(path, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_APPEND, 0644);
                if (fd >= 0) { ps5io_write_full(fd, t, sizeof t - 1); ps5io_close(fd); }
            }
        }
        return;
    }
    n = snprintf(path, sizeof path, "%s/e2e-status.jsonl", g_dir);
    if (n <= 0 || (size_t)n >= sizeof path) return;
    fd = ps5io_open(path, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_APPEND, 0644);
    if (fd < 0) return;
    if (ps5io_write_full(fd, line, len) == 0) { g_lines++; g_bytes += (long)len + 1; }
    ps5io_close(fd);
}

static const char *stage_for_cmd(const char *cmd) {
    if (!cmd) return "unknown";
    if (!strcmp(cmd, "records")) return "stage-0-baseline";
    if (!strcmp(cmd, "register-mod")) return "stage-1-download";
    if (!strcmp(cmd, "prepare") || !strcmp(cmd, "prepare-archive")) return "stage-2-prepare";
    if (!strcmp(cmd, "install")) return "stage-3-install";
    if (!strcmp(cmd, "simulate-runtime")) return "stage-4-5-runtime-ack";
    if (!strcmp(cmd, "recover")) return "recovery";
    return "unknown";
}

void e2e_status_set_dir(const char *state_dir) {
    snprintf(g_dir, sizeof g_dir, "%s", state_dir ? state_dir : "");
}

void e2e_status_begin(const char *cmd, const char *title) {
    char line[E2E_LINE], c[32], t[64];
    sanitize(c, sizeof c, cmd);
    sanitize(t, sizeof t, title);
    snprintf(line, sizeof line,
             "{\"event\":\"begin\",\"cmd\":\"%s\",\"title\":\"%s\",\"stage\":\"%s\"}\n",
             c, t, stage_for_cmd(cmd));
    emit(line);
}

void e2e_status_end(int exit_code) {
    char line[E2E_LINE];
    snprintf(line, sizeof line, "{\"event\":\"end\",\"exit\":%d}\n", exit_code);
    emit(line);
}

void e2e_status_event(const char *stage, const char *outcome, const char *detail) {
    char line[E2E_LINE], s[48], o[32], d[192];
    sanitize(s, sizeof s, stage);
    sanitize(o, sizeof o, outcome);
    sanitize(d, sizeof d, detail);
    snprintf(line, sizeof line,
             "{\"event\":\"status\",\"stage\":\"%s\",\"outcome\":\"%s\",\"detail\":\"%s\"}\n",
             s, o, d);
    emit(line);
}
