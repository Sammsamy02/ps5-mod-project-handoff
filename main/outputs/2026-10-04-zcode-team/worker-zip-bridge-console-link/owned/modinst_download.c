/*
 * modinst_download.c — see modinst_download.h.
 * Origin: worker-e2e-final, 2026-10-04 (owned).
 *
 * Job file format (explicit TSV, one "key\tvalue" per line; unknown key =
 * bad_request — explicit-config-only):
 *   id        <token, [A-Za-z0-9._-]{1,127}>          (required)
 *   title     <game title token>                      (required)
 *   url       <provider-authorized resource URI>      (required; scheme
 *              policy belongs to the transport owner: the real owner admits
 *              HTTPS only, the host test stub admits file:// only)
 *   intake    <intake directory, e.g. /data/modbridge/intake>  (required)
 *   sha256    <64 hex, optional expected content hash; mismatch = failure>
 *   timeout_ms        <1..300000, default 300000>
 *   connect_timeout_ms <1..timeout_ms, default 10000>
 *   max_bytes  <1..1073741824, default 1073741824 (sink ABI cap)>
 *
 * Flow (job -> sink -> record), one serialized owner per process:
 *   1. parse + validate the job;
 *   2. open <intake>/<id>.zip.tmp O_RDWR|CREAT|TRUNC 0600 (the sink ABI's
 *      fresh unpublished regular nonappend FD) plus a second read-only
 *      descriptor as the readback channel (ps5_io has no exported positional
 *      write, so writes ride the write descriptor's sequential position);
 *   3. mb_curl_runtime_start_v1 (real platform entropy binding);
 *   4. mb_curl_download_v1 with the borrowed sink params (the transport owns
 *      the sink lifecycle: start/feed/finish, sha256, sync, readback);
 *   5. mb_curl_runtime_close_v1 (always);
 *   6. success: verify expected sha256 if given, fsync, close, rename
 *      .tmp -> <id>.zip, fsync the intake dir, write the content-addressed
 *      sidecar <state>/downloads/<id>.json (never the URL), emit the e2e
 *      stage-1 event; the composed main then registers the mod row
 *      `downloaded` — that record is the command's commit point.
 *   7. any failure: the unpublished prefix stays private (unlinked), the
 *      owner is closed, no sidecar, no record, honest error + exit 1.
 */
#include "modinst_download.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>

#include "ps5_io.h"
#include "e2e_status.h"
#include "download/curl_download.h"   /* vendored owner ABI v1 (+ runtime.h, file_sink.h) */

#define MI_EX_OK 0
#define MI_EX_IO 1
#define MI_EX_BAD 2

#define JD_URL_MAX 8192u               /* MB_CURL_DOWNLOAD_URL_MAX */
#define JD_TIMEOUT_MAX_MS 300000u      /* MB_CURL_DOWNLOAD_TIMEOUT_MAX_MS */
#define JD_JOB_MAX_BYTES (64u * 1024u)
#define JD_PATH_MAX 4608u

/* ---------------------------- platform entropy ---------------------------- */
#ifdef PS5IO_HOST
#include <sys/random.h>
static int mi_entropy_read32(void *ctx, uint8_t out[32]) {
    (void)ctx;
    return getentropy(out, 32) == 0 ? 0 : -1;  /* host: real OS source */
}
#else
/* console: the pinned native download closure's real platform source
 * (TRANSPORT_CHECKPOINT native_app_download_build retained_api_symbols:
 * mb_curl_ps5_entropy_read32_v1). This TU is not linked into the console
 * payload yet — see REPORT.md for the exact controller link step. */
int mb_curl_ps5_entropy_read32_v1(void *ctx, uint8_t out[32]);
static int (*const mi_entropy_read32)(void *, uint8_t *) = mb_curl_ps5_entropy_read32_v1;
#endif

/* ------------------------------ sink I/O glue ------------------------------ */
/* The real file-sink ABI admits only a fresh unpublished O_RDWR regular
 * nonappend fd (fcntl F_GETFL check in archive/private/file_sink.c). The
 * ps5_io adapter's exported flag set has no RDWR member yet (upstream ask,
 * see REPORT.md), so the sink descriptor is opened directly: POSIX open on
 * the host flavor; the console binding must extend ps5_io with an RDWR flag
 * before this TU links into the console payload. */
#ifdef PS5IO_HOST
#include <fcntl.h>
static int mi_open_sink_rdwr(const char *path) {
    return open(path, O_RDWR | O_CREAT | O_TRUNC, 0600);
}
#else
static int mi_open_sink_rdwr(const char *path) {
    return ps5io_open(path, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_TRUNC, 0600);
}
#endif

typedef struct {
    int fd_w;        /* unpublished tmp, sequential writes only */
    int fd_r;        /* same inode, readback channel (positional reads) */
    uint64_t next_off;
} mi_sink_io;

/* Serialized exact positional I/O per the file-sink ABI. The transport only
 * streams received bytes sequentially from offset 0, so the write descriptor
 * carries the stream position; readback is positional (lseek+read loop under
 * ps5io_pread_exact) and never disturbs it. */
static int mi_sink_write_at(void *vc, int fd, uint64_t off, const uint8_t *buf,
                            uint32_t len, uint32_t *written) {
    mi_sink_io *c = (mi_sink_io *)vc;
    (void)fd; /* the borrowed params.fd is c->fd_w; io_context is authoritative */
    if (off != c->next_off) return -1;
    if (len && ps5io_write_full(c->fd_w, buf, len)) return -1;
    c->next_off += len;
    *written = len;
    return 0;
}

static int mi_sink_read_at(void *vc, int fd, uint64_t off, uint8_t *buf,
                           uint32_t len, uint32_t *read) {
    mi_sink_io *c = (mi_sink_io *)vc;
    (void)fd;
    if (ps5io_pread_exact(c->fd_r, off, buf, len)) return -1;
    *read = len;
    return 0;
}

/* ------------------------------- job parsing ------------------------------- */
typedef struct {
    char id[128], title[64], url[JD_URL_MAX + 1], intake[PS5IO_MAX_PATH];
    char sha_exp[65];
    uint64_t max_bytes;
    uint32_t timeout_ms, connect_ms;
} mi_job;

static int mi_is_token(const char *s, size_t max) {
    size_t n = strlen(s), i;
    if (!n || n >= max) return 0;
    for (i = 0; i < n; ++i) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return 0;
    }
    return 1;
}

static int mi_is_hex64(const char *s) {
    size_t i;
    if (strlen(s) != 64) return 0;
    for (i = 0; i < 64; ++i) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return 0;
    }
    return 1;
}

static int mi_hex_eq(const char *hex, const uint8_t raw[32]) {
    static const char dc[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < 32; ++i) {
        char lo = dc[raw[i] & 0xf], hi = dc[(raw[i] >> 4) & 0xf], a = hex[2 * i], b = hex[2 * i + 1];
        if (a >= 'A' && a <= 'F') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'F') b = (char)(b - 'A' + 'a');
        if (a != hi || b != lo) return 0;
    }
    return 1;
}

static int mi_load_job(const char *path, mi_job *j, char *err, size_t errn) {
    char *buf;
    char *line, *save = 0;
    ps5io_stat st;
    int fd = ps5io_open(path, PS5IO_RDONLY, 0);
    memset(j, 0, sizeof *j);
    j->timeout_ms = JD_TIMEOUT_MAX_MS;
    j->connect_ms = 10000;
    j->max_bytes = 1073741824u; /* MB_DOWNLOAD_FILE_SINK_MAX_BYTES */
    if (fd < 0 || ps5io_stat_path(path, &st)) {
        if (fd >= 0) ps5io_close(fd);
        snprintf(err, errn, "cannot open job file %s", path);
        return -1;
    }
    if (st.size > JD_JOB_MAX_BYTES) { ps5io_close(fd); snprintf(err, errn, "job file too large"); return -1; }
    buf = (char *)malloc((size_t)st.size + 1);
    if (!buf) { ps5io_close(fd); snprintf(err, errn, "out of memory"); return -1; }
    {
        size_t got = 0;
        if (ps5io_read_full(fd, buf, (size_t)st.size, &got)) { free(buf); ps5io_close(fd); snprintf(err, errn, "cannot read job file"); return -1; }
        buf[got] = 0;
    }
    ps5io_close(fd);
    for (line = strtok_r(buf, "\n", &save); line; line = strtok_r(0, "\n", &save)) {
        char *tab = strchr(line, '\t');
        const char *k, *v;
        if (!tab) { snprintf(err, errn, "bad job line (want key\\tvalue): %.32s", line); free(buf); return -1; }
        *tab = 0; k = line; v = tab + 1;
        if (!strcmp(k, "id")) { if (strlen(v) >= sizeof j->id) goto toolong; snprintf(j->id, sizeof j->id, "%s", v); }
        else if (!strcmp(k, "title")) { if (strlen(v) >= sizeof j->title) goto toolong; snprintf(j->title, sizeof j->title, "%s", v); }
        else if (!strcmp(k, "url")) { if (strlen(v) >= sizeof j->url) goto toolong; snprintf(j->url, sizeof j->url, "%s", v); }
        else if (!strcmp(k, "intake")) { if (strlen(v) >= sizeof j->intake) goto toolong; snprintf(j->intake, sizeof j->intake, "%s", v); }
        else if (!strcmp(k, "sha256")) { if (!mi_is_hex64(v)) { snprintf(err, errn, "sha256 must be 64 hex chars"); free(buf); return -1; } snprintf(j->sha_exp, sizeof j->sha_exp, "%s", v); }
        else if (!strcmp(k, "timeout_ms")) { long t = atol(v); if (t < 1 || t > (long)JD_TIMEOUT_MAX_MS) { free(buf); snprintf(err, errn, "timeout_ms out of 1..300000"); return -1; } j->timeout_ms = (uint32_t)t; }
        else if (!strcmp(k, "connect_timeout_ms")) { long t = atol(v); if (t < 1 || t > (long)j->timeout_ms) { free(buf); snprintf(err, errn, "connect_timeout_ms out of 1..timeout_ms"); return -1; } j->connect_ms = (uint32_t)t; }
        else if (!strcmp(k, "max_bytes")) { unsigned long long t = strtoull(v, 0, 10); if (!t || t > 1073741824ull) { free(buf); snprintf(err, errn, "max_bytes out of 1..1073741824"); return -1; } j->max_bytes = t; }
        else { snprintf(err, errn, "unknown job key '%s' (explicit-config-only)", k); free(buf); return -1; }
        continue;
    toolong:
        snprintf(err, errn, "value too long for key '%s'", k); free(buf); return -1;
    }
    free(buf);
    if (!mi_is_token(j->id, sizeof j->id)) { snprintf(err, errn, "id must be a token"); return -1; }
    if (!mi_is_token(j->title, sizeof j->title)) { snprintf(err, errn, "title must be a token"); return -1; }
    if (!j->url[0]) { snprintf(err, errn, "missing url"); return -1; }
    if (strlen(j->url) >= JD_URL_MAX) { snprintf(err, errn, "url too long"); return -1; }
    if (!j->intake[0]) { snprintf(err, errn, "missing intake dir"); return -1; }
    if (j->connect_ms > j->timeout_ms) { snprintf(err, errn, "connect_timeout_ms exceeds timeout_ms"); return -1; }
    return 0;
}

static const char *mi_transport_status_name(uint32_t s) {
    switch (s) {
    case 0: return "ok";
    case 1: return "bad-argument";
    case 2: return "bad-state";
    case 3: return "owner-failed";
    case 4: return "init-failed";
    case 5: return "setup-failed";
    case 6: return "transport-failed";
    case 7: return "response-refused";
    case 8: return "limit";
    case 9: return "cancelled";
    case 10: return "sink-failed";
    case 11: return "sync-failed";
    case 12: return "timeout";
    case 13: return "consumer-failed";
    default: return "unknown";
    }
}

/* --------------------------------- command --------------------------------- */
int mi_cmd_download(int argc, char **argv, const char *state_dir) {
    const char *job = 0;
    int i, fd_w = -1, fd_r = -1;
    mi_job j;
    char err[160], tmp[JD_PATH_MAX], final[JD_PATH_MAX], sha_hex[65], sidecar[JD_PATH_MAX];
    mi_sink_io io;
    mb_curl_runtime_params_v1 rp;
    mb_curl_runtime_result_v1 rr;
    mb_curl_download_params_v1 dp;
    mb_curl_download_result_v1 dr;
    static const char hexd[] = "0123456789abcdef";

    for (i = 1; i < argc; ++i)
        if (!strcmp(argv[i], "--job") && i + 1 < argc) job = argv[++i];
    if (!job) {
        printf("error bad_request download needs --job FILE\n");
        e2e_status_event("stage-1-download", "bad_request", "missing --job");
        return MI_EX_BAD;
    }
    if (mi_load_job(job, &j, err, sizeof err)) {
        printf("error bad_request job: %s\n", err);
        e2e_status_event("stage-1-download", "bad_request", err);
        return MI_EX_BAD;
    }
    (void)state_dir;

    if (ps5io_mkdir_p(j.intake, 0755)) {
        printf("error io_error cannot create intake dir %s\n", j.intake);
        e2e_status_event("stage-1-download", "fail", "intake mkdir");
        return MI_EX_IO;
    }
    snprintf(final, sizeof final, "%s/%s.zip", j.intake, j.id);
    snprintf(tmp, sizeof tmp, "%s.tmp", final);
    {
        ps5io_stat st;
        if (!ps5io_stat_path(final, &st) && st.exists) {
            printf("error bad_request intake target already exists: %s\n", final);
            e2e_status_event("stage-1-download", "bad_request", "intake target exists");
            return MI_EX_BAD;
        }
    }
    /* the unpublished sink tmp: fresh O_RDWR regular nonappend (see
     * mi_open_sink_rdwr) plus a read-only descriptor on the same inode as
     * the readback channel; the transport drives all I/O through the
     * borrowed positional callbacks below. */
    fd_w = mi_open_sink_rdwr(tmp);
    if (fd_w < 0) {
        printf("error io_error cannot create sink file %s\n", tmp);
        e2e_status_event("stage-1-download", "fail", "sink create");
        return MI_EX_IO;
    }
    fd_r = ps5io_open(tmp, PS5IO_RDONLY, 0);
    if (fd_r < 0) { ps5io_close(fd_w); ps5io_unlink(tmp);
        printf("error io_error cannot open readback channel\n"); e2e_status_event("stage-1-download", "fail", "readback open"); return MI_EX_IO; }

    /* owner start (one serialized exclusive curl/PSA owner per process) */
    memset(&rp, 0, sizeof rp);
    rp.struct_size = (uint32_t)sizeof rp; rp.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    rp.entropy_context = 0; rp.entropy_read32 = mi_entropy_read32;
    memset(&rr, 0, sizeof rr);
    if (mb_curl_runtime_start_v1(&rp, &rr)) {
        ps5io_close(fd_r); ps5io_close(fd_w); ps5io_unlink(tmp);
        printf("error io_error owner start failed\n");
        e2e_status_event("stage-1-download", "fail", "owner start");
        return MI_EX_IO;
    }

    memset(&io, 0, sizeof io);
    io.fd_w = fd_w; io.fd_r = fd_r; io.next_off = 0;
    memset(&dp, 0, sizeof dp);
    dp.struct_size = (uint32_t)sizeof dp; dp.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    dp.url = j.url;
    dp.ca_pem = 0; dp.ca_bytes = 0;      /* explicit config only: none supplied */
    dp.resolve = 0;
    dp.timeout_ms = j.timeout_ms; dp.connect_timeout_ms = j.connect_ms;
    dp.sink.struct_size = (uint32_t)sizeof dp.sink;
    dp.sink.abi_version = MB_DOWNLOAD_FILE_SINK_ABI_V1;
    dp.sink.fd = fd_w; dp.sink.cap_bytes = j.max_bytes;
    dp.sink.io_context = &io;
    dp.sink.read_at = mi_sink_read_at; dp.sink.write_at = mi_sink_write_at;
    dp.cancel_context = 0; dp.cancelled = 0;
    memset(&dr, 0, sizeof dr);
    i = mb_curl_download_v1(&dp, &dr);
    mb_curl_runtime_close_v1(&rr);       /* always: no reset/retry ABI */

    if (i != 0 || !dr.complete || !dr.file.complete || !dr.file.readback_verified) {
        ps5io_close(fd_r); ps5io_close(fd_w); ps5io_unlink(tmp);   /* failed prefix stays private */
        printf("error transport status=%u(%s) complete=%u sink_complete=%u readback=%u\n",
               (unsigned)dr.status, mi_transport_status_name(dr.status),
               dr.complete, dr.file.complete, dr.file.readback_verified);
        { char d[96]; snprintf(d, sizeof d, "transport %s", mi_transport_status_name(dr.status));
          e2e_status_event("stage-1-download", "fail", d); }
        return MI_EX_IO;
    }
    if (j.sha_exp[0] && !mi_hex_eq(j.sha_exp, dr.file.sha256)) {
        ps5io_close(fd_r); ps5io_close(fd_w); ps5io_unlink(tmp);
        printf("error payload sha256 mismatch (job sha256 does not match received bytes)\n");
        e2e_status_event("stage-1-download", "fail", "sha256 mismatch");
        return MI_EX_IO;
    }
    for (i = 0; i < 32; ++i) {
        sha_hex[2 * i] = hexd[(dr.file.sha256[i] >> 4) & 0xf];
        sha_hex[2 * i + 1] = hexd[dr.file.sha256[i] & 0xf];
    }
    sha_hex[64] = 0;

    /* durability, then publish, then record */
    if (ps5io_fsync(fd_w)) { ps5io_close(fd_r); ps5io_close(fd_w); ps5io_unlink(tmp);
        printf("error io_error sink fsync failed\n"); e2e_status_event("stage-1-download", "fail", "fsync"); return MI_EX_IO; }
    ps5io_close(fd_r); ps5io_close(fd_w); fd_w = fd_r = -1;
    if (ps5io_rename(tmp, final) || ps5io_fsync_dir(j.intake)) {
        ps5io_unlink(tmp);
        printf("error io_error cannot publish %s\n", final);
        e2e_status_event("stage-1-download", "fail", "publish rename");
        return MI_EX_IO;
    }
    snprintf(sidecar, sizeof sidecar, "%s/downloads", state_dir);
    if (ps5io_mkdir_p(sidecar, 0755)) {
        printf("error io_error cannot create downloads record dir\n");
        e2e_status_event("stage-1-download", "fail", "records dir");
        return MI_EX_IO;
    }
    snprintf(sidecar, sizeof sidecar, "%s/downloads/%s.json", state_dir, j.id);
    {
        /* content-addressed truth; never the URL, never credentials */
        char line[512];
        int sfd;
        snprintf(line, sizeof line,
                 "{\"id\": \"%s\", \"title\": \"%s\", \"bytes\": %llu, \"received\": %llu, "
                 "\"sha256\": \"%s\", \"intake\": \"%s\"}\n",
                 j.id, j.title, (unsigned long long)dr.file.bytes_written,
                 (unsigned long long)dr.received_bytes, sha_hex, final);
        sfd = ps5io_open(sidecar, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_TRUNC, 0644);
        if (sfd < 0 || ps5io_write_full(sfd, line, strlen(line)) || ps5io_fsync(sfd) || ps5io_close(sfd)) {
            if (sfd >= 0) ps5io_close(sfd);
            printf("error io_error cannot write download record\n");
            e2e_status_event("stage-1-download", "fail", "record write");
            return MI_EX_IO;
        }
    }
    printf("downloaded mod=%s title=%s state=downloaded bytes=%llu sha256=%s intake=%s\n",
           j.id, j.title, (unsigned long long)dr.file.bytes_written, sha_hex, final);
    e2e_status_event("stage-1-download", "ok", sha_hex);
    return MI_EX_OK;
}
