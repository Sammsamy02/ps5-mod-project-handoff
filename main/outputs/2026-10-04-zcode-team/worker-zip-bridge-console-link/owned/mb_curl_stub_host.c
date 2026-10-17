/*
 * mb_curl_stub_host.c — HOST TEST TRANSPORT STUB for the composed installer's
 * E2E stage-1 download command. Origin: worker-e2e-final, 2026-10-04 (owned).
 *
 * HOST FLAVOR ONLY. Never link this into a console payload: the console link
 * must provide the pinned native download closure (outputs/ps5-mod-bridge/
 * native-host/download, receipt callable_archive_download_transport).
 *
 * It implements the exclusive-owner transport ABI v1 (vendored
 * download/runtime.h + download/curl_download.h) as a contract-faithful local
 * stand-in: start-once/close-once serialization, BAD_ARGUMENT/BAD_STATE
 * admission, an https-only-equivalent scheme policy (this stub admits ONLY
 * file:// local resources — its "provider"), a fixed 64 KiB carry, and body
 * streaming through the REAL mb_download_file_sink API (real sha256, sync,
 * readback verification), so the job->sink->record path under test exercises
 * the genuine sink code. Failure fixtures are encoded in the file basename:
 *   *.stub-refuse*  -> RESPONSE_REFUSED before any transfer
 *   *.stub-timeout* -> TIMEOUT (no transfer)
 *   *.stub-cut*     -> half the bytes, then TRANSPORT_FAILED mid-stream
 * No URL, query or path is ever logged or returned beyond scalar facts.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "download/runtime.h"                 /* ABI + status enums */
#include "download/curl_download.h"           /* params/result */
#include "archive/private/file_sink.h"        /* REAL sink API */

#define STUB_CARRY 65536u

static int st_started = 0, st_closed = 0;
static uint64_t st_jobs = 0, st_entropy_calls = 0, st_entropy_bytes = 0;

int mb_curl_runtime_start_v1(const mb_curl_runtime_params_v1 *p,
                             mb_curl_runtime_result_v1 *r) {
    if (!p || !r) return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    if (p->struct_size != sizeof *p || p->abi_version != MB_CURL_DOWNLOAD_ABI_V1)
        return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    if (p->struct_size != (uint32_t)sizeof(mb_curl_runtime_params_v1))
        return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    if (!p->entropy_read32) return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    if (st_started || st_closed) return MB_CURL_DOWNLOAD_BAD_STATE;
    {   /* one real entropy window at start (real owner: per-operation windows) */
        uint8_t seed[32];
        if (p->entropy_read32(p->entropy_context, seed) != 0)
            return MB_CURL_DOWNLOAD_INIT_FAILED;
        st_entropy_calls++; st_entropy_bytes += 32;
    }
    st_started = 1;
    memset(r, 0, sizeof *r);
    r->struct_size = (uint32_t)sizeof *r; r->abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    r->initialized = 1; r->terminal = 0; r->window_active = 1;
    r->entropy_windows = 1; r->entropy_callbacks = st_entropy_calls;
    r->entropy_bytes = st_entropy_bytes; r->clock_samples = 1;
    return MB_CURL_DOWNLOAD_OK;
}

int mb_curl_runtime_status_v1(mb_curl_runtime_result_v1 *r) {
    if (!r) return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    memset(r, 0, sizeof *r);
    r->struct_size = (uint32_t)sizeof *r; r->abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    r->initialized = (uint32_t)st_started;
    r->terminal = (uint32_t)st_closed;
    r->entropy_windows = st_started ? 1 : 0;
    r->entropy_callbacks = st_entropy_calls;
    r->entropy_bytes = st_entropy_bytes;
    r->clock_samples = st_started;
    return MB_CURL_DOWNLOAD_OK;
}

int mb_curl_runtime_close_v1(mb_curl_runtime_result_v1 *r) {
    if (!st_started || st_closed) return MB_CURL_DOWNLOAD_BAD_STATE;
    st_closed = 1;
    if (r) {
        memset(r, 0, sizeof *r);
        r->struct_size = (uint32_t)sizeof *r; r->abi_version = MB_CURL_DOWNLOAD_ABI_V1;
        r->initialized = 1; r->terminal = 1; r->window_active = 0;
        r->entropy_windows = 1; r->entropy_callbacks = st_entropy_calls;
        r->entropy_bytes = st_entropy_bytes; r->clock_samples = 1;
    }
    return MB_CURL_DOWNLOAD_OK;
}

static size_t stub_url_len(const char *s) { return s ? strlen(s) : 0; }

int mb_curl_download_v1(const mb_curl_download_params_v1 *p,
                        mb_curl_download_result_v1 *r) {
    static uint8_t carry[STUB_CARRY];
    const char *path;
    FILE *f;
    long size;
    mb_download_file_sink_v1 sink;
    uint64_t sent = 0;

    if (!p || !r) return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    if (p->struct_size != (uint32_t)sizeof *p || p->abi_version != MB_CURL_DOWNLOAD_ABI_V1)
        return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    if (!st_started || st_closed) return MB_CURL_DOWNLOAD_BAD_STATE;
    if (!p->url || !p->url[0] || stub_url_len(p->url) >= MB_CURL_DOWNLOAD_URL_MAX)
        return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    if (p->timeout_ms == 0 || p->timeout_ms > MB_CURL_DOWNLOAD_TIMEOUT_MAX_MS)
        return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    if (p->connect_timeout_ms == 0 || p->connect_timeout_ms > p->timeout_ms)
        return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    if (p->sink.struct_size != (uint32_t)sizeof(mb_download_file_sink_params_v1) ||
        p->sink.abi_version != MB_DOWNLOAD_FILE_SINK_ABI_V1 || p->sink.fd < 0 ||
        !p->sink.write_at || !p->sink.read_at)
        return MB_CURL_DOWNLOAD_BAD_ARGUMENT;

    memset(r, 0, sizeof *r);
    r->struct_size = (uint32_t)sizeof *r; r->abi_version = MB_CURL_DOWNLOAD_ABI_V1;

    /* scheme policy: the stub's only admitted provider resource is a local
     * file (the real owner admits provider-authorized HTTPS only) */
    if (strncmp(p->url, "file://", 7) != 0)
        { r->status = MB_CURL_DOWNLOAD_RESPONSE_REFUSED; return MB_CURL_DOWNLOAD_OK; }
    path = p->url + 7;

    if (strstr(path, ".stub-refuse"))
        { r->status = MB_CURL_DOWNLOAD_RESPONSE_REFUSED; return MB_CURL_DOWNLOAD_OK; }
    if (strstr(path, ".stub-timeout"))
        { r->status = MB_CURL_DOWNLOAD_TIMEOUT; return MB_CURL_DOWNLOAD_OK; }

    f = fopen(path, "rb");
    if (!f) { r->status = MB_CURL_DOWNLOAD_RESPONSE_REFUSED; return MB_CURL_DOWNLOAD_OK; }
    fseek(f, 0, SEEK_END); size = ftell(f); fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); r->status = MB_CURL_DOWNLOAD_TRANSPORT_FAILED; return MB_CURL_DOWNLOAD_OK; }
    if ((uint64_t)size > p->sink.cap_bytes)
        { fclose(f); r->status = MB_CURL_DOWNLOAD_LIMIT; return MB_CURL_DOWNLOAD_OK; }

    memset(&sink, 0, sizeof sink);
    if (mb_download_file_sink_start_v1(&sink, &p->sink)) {
        fclose(f);
        r->status = MB_CURL_DOWNLOAD_BAD_ARGUMENT;   /* sink refused admission */
        return MB_CURL_DOWNLOAD_OK;
    }
    {
        long limit = strstr(path, ".stub-cut") ? size / 2 : size;
        int failed = 0;
        while (sent < (uint64_t)limit) {
            size_t want = (uint64_t)limit - sent > STUB_CARRY ? STUB_CARRY
                         : (size_t)((uint64_t)limit - sent);
            if (fread(carry, 1, want, f) != want) { failed = 1; break; }
            if (mb_download_file_sink_feed_v1(&sink, carry, want)) { failed = 2; break; }
            sent += want;
        }
        if (!failed && sent < (uint64_t)size) failed = 1;   /* .stub-cut prefix */
        if (failed == 2) {
            mb_download_file_sink_dispose_v1(&sink);
            fclose(f);
            r->status = MB_CURL_DOWNLOAD_SINK_FAILED;
            r->received_bytes = sent;
            return MB_CURL_DOWNLOAD_OK;
        }
        if (failed) {
            (void)mb_download_file_sink_finish_v1(&sink, 0, 0, 0, &r->file);
            mb_download_file_sink_dispose_v1(&sink);
            fclose(f);
            r->status = MB_CURL_DOWNLOAD_TRANSPORT_FAILED;
            r->received_bytes = sent;
            return MB_CURL_DOWNLOAD_OK;
        }
        if (mb_download_file_sink_finish_v1(&sink, 1, 1, (uint64_t)size, &r->file)) {
            mb_download_file_sink_dispose_v1(&sink);
            fclose(f);
            r->status = MB_CURL_DOWNLOAD_SINK_FAILED;
            r->received_bytes = sent;
            return MB_CURL_DOWNLOAD_OK;
        }
        mb_download_file_sink_dispose_v1(&sink);
    }
    fclose(f);
    st_jobs++;
    r->status = MB_CURL_DOWNLOAD_OK;
    r->complete = 1;
    r->http_status = 200;
    r->received_bytes = (uint64_t)size;
    /* r->file filled by the real sink finish: complete, readback_verified,
     * sha256, counters */
    return MB_CURL_DOWNLOAD_OK;
}
