#include "curl_download.h"
#include "provider_transfer.h"
#include "owner.h"
#include <curl/curl.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#define STARTUP_TIMEOUT_MS 5000u
#define HEADER_BYTES_MAX 65536u
#define HEADER_LINE_MAX 8192u
#define HEADER_FIELDS_MAX 256u
#define INFORMATIONAL_MAX 4u
#define CALLBACK_MAX 2097152u
#define DRIVE_MAX 1048576u
#define CALLBACK_WORK_MAX \
    (MB_DOWNLOAD_FILE_SINK_MAX_BYTES * UINT64_C(4) + UINT64_C(1048576))

/* The public contract serializes all calls, including platform hooks. No
 * global cleanup, PSA free, source reset or source rebinding occurs per job. */
static struct {
    unsigned bound, initialized, closed, busy;
} runtime;

struct response_headers {
    unsigned status, in_block, final, informational, fields;
    unsigned has_length, chunked, has_encoding;
    size_t bytes;
    uint64_t content_length;
};

struct transfer_params {
    const char *url, *resolve;
    size_t url_bytes, resolve_bytes;
    const uint8_t *ca_pem;
    size_t ca_bytes;
    uint32_t timeout_ms, connect_timeout_ms;
    uint64_t body_cap;
    const mb_download_file_sink_params_v1 *sink;
    uint8_t *body;
    uint32_t auth_mode;
    const char *credential;
    size_t credential_bytes;
    void *consume_context;
    int (*consume_complete_body)(void *,const uint8_t *,size_t);
    void *cancel_context;
    mb_curl_cancel_fn_v1 cancelled;
};

struct download_attempt {
    const struct transfer_params *params;
    mb_download_file_sink_v1 sink;
    struct response_headers headers;
    mb_curl_download_status_v1 status;
    uint64_t received, work_bytes;
    unsigned callbacks, drives, tls_context_seen;
    size_t carry_bytes;
    uint8_t carry[MB_DOWNLOAD_FILE_SINK_WORK_BYTES];
};

static void clear_bytes(void *data, size_t bytes)
{
    volatile unsigned char *p = data;
    while(bytes--)
        *p++ = 0;
}

static int overlaps(const void *a, size_t an, const void *b, size_t bn)
{
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    if(!an || !bn)
        return 0;
    if(x > UINTPTR_MAX - an || y > UINTPTR_MAX - bn)
        return 1;
    return x < y + bn && y < x + an;
}

static void runtime_snapshot(mb_curl_runtime_result_v1 *out)
{
    mb_curl_owner_snapshot_v1(out);
    out->struct_size = sizeof(*out);
    out->abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    out->initialized = runtime.initialized;
}

int mb_curl_runtime_status_v1(mb_curl_runtime_result_v1 *out)
{
    if(!out)
        return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    runtime_snapshot(out);
    return MB_CURL_DOWNLOAD_OK;
}

int mb_curl_runtime_start_v1(const mb_curl_runtime_params_v1 *params,
                             mb_curl_runtime_result_v1 *out)
{
    CURLcode rc;
    int window = 0;
    mb_curl_download_status_v1 status = MB_CURL_DOWNLOAD_OK;
    if(!params || !out ||
       overlaps(params, sizeof(*params), out, sizeof(*out)) ||
       params->struct_size != sizeof(*params) ||
       params->abi_version != MB_CURL_DOWNLOAD_ABI_V1 ||
       !params->entropy_read32)
        return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    if(runtime.bound || runtime.closed || runtime.busy) {
        runtime_snapshot(out);
        return MB_CURL_DOWNLOAD_BAD_STATE;
    }
    if(mb_curl_owner_bind_v1(params)) {
        runtime_snapshot(out);
        return MB_CURL_DOWNLOAD_BAD_STATE;
    }
    runtime.bound = runtime.busy = 1;
    if(mb_curl_owner_window_begin_v1(STARTUP_TIMEOUT_MS)) {
        status = MB_CURL_DOWNLOAD_OWNER_FAILED;
        mb_curl_owner_fail_v1();
        goto done;
    }
    window = 1;
    rc = curl_global_init(CURL_GLOBAL_DEFAULT);
    if(rc != CURLE_OK) {
        /* The pinned 8.18 backport performs failed-backend initialization
         * cleanup itself. There is no successful global ownership to free. */
        mb_curl_owner_fail_v1();
        status = MB_CURL_DOWNLOAD_INIT_FAILED;
        goto done;
    }
    runtime.initialized = 1;
    if(!mb_curl_owner_guard_v1()) {
        mb_curl_owner_fail_v1();
        status = MB_CURL_DOWNLOAD_OWNER_FAILED;
    }
done:
    if(status != MB_CURL_DOWNLOAD_OK && runtime.initialized) {
        /* Keep the initialization window through its exceptional cleanup. */
        curl_global_cleanup();
        runtime.initialized = 0;
    }
    if(window && (mb_curl_owner_window_end_v1() ||
                  mb_curl_owner_window_expired_v1())) {
        mb_curl_owner_fail_v1();
        if(status == MB_CURL_DOWNLOAD_OK)
            status = MB_CURL_DOWNLOAD_OWNER_FAILED;
    }
    if(status != MB_CURL_DOWNLOAD_OK) {
        if(runtime.initialized) {
            /* Terminal hooks refuse fresh entropy; global cleanup still owns
             * the successful initialization and must execute exactly once. */
            curl_global_cleanup();
            runtime.initialized = 0;
        }
        mb_curl_owner_close_v1();
        runtime.closed = 1;
    }
    runtime.busy = 0;
    runtime_snapshot(out);
    return status;
}

int mb_curl_runtime_close_v1(mb_curl_runtime_result_v1 *out)
{
    int window = 0;
    if(!out)
        return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    if(!runtime.bound || runtime.closed || runtime.busy) {
        runtime_snapshot(out);
        return MB_CURL_DOWNLOAD_BAD_STATE;
    }
    runtime.busy = 1;
    if(runtime.initialized) {
        if(!mb_curl_owner_window_begin_v1(STARTUP_TIMEOUT_MS))
            window = 1;
        curl_global_cleanup();
        runtime.initialized = 0;
    }
    if(window)
        (void)mb_curl_owner_window_end_v1();
    mb_curl_owner_close_v1();
    runtime.closed = 1;
    runtime.busy = 0;
    runtime_snapshot(out);
    return MB_CURL_DOWNLOAD_OK;
}

static unsigned ascii_lower(unsigned c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

static int ascii_equal(const char *p, size_t n, const char *word)
{
    size_t i, length = strlen(word);
    if(n != length)
        return 0;
    for(i = 0; i < n; ++i)
        if(ascii_lower((unsigned char)p[i]) !=
           ascii_lower((unsigned char)word[i]))
            return 0;
    return 1;
}

static int bounded_ascii(const char *s, size_t maximum, size_t *length)
{
    size_t n;
    if(!s)
        return 0;
    for(n = 0; n <= maximum; ++n) {
        unsigned c = (unsigned char)s[n];
        if(!c) {
            if(!n)
                return 0;
            *length = n;
            return 1;
        }
        if(c < 33u || c > 126u || c == '\\')
            return 0;
    }
    return 0;
}

static int decimal_port(const char *s, size_t n, unsigned *port)
{
    size_t i;
    unsigned value = 0;
    if(!n || n > 5u)
        return 0;
    for(i = 0; i < n; ++i) {
        unsigned c = (unsigned char)s[i];
        if(c < '0' || c > '9')
            return 0;
        value = value * 10u + c - '0';
    }
    if(!value || value > 65535u)
        return 0;
    *port = value;
    return 1;
}

/* A resolver entry can choose addresses for this exact URL host and port.
 * It cannot supply another authority, wildcard or a removal/cache directive.
 * CA and hostname verification remain mandatory on the original URL. */
static int resolve_matches(const char *entry, size_t n,
                           const char *host, unsigned url_port)
{
    size_t h = strlen(host), i, begin;
    unsigned port;
    if(n <= h + 2u || entry[h] != ':' || !ascii_equal(entry, h, host))
        return 0;
    begin = h + 1u;
    for(i = begin; i < n && entry[i] != ':'; ++i)
        ;
    if(i == n || !decimal_port(entry + begin, i - begin, &port) ||
       port != url_port || i + 1u == n)
        return 0;
    /* Curl validates the address-list grammar when the entry is installed.
     * Reject URL-like or credential-bearing text before admitting the list. */
    for(++i; i < n; ++i)
        if(entry[i] == '/' || entry[i] == '@' || entry[i] == '#' ||
           entry[i] == '?' || entry[i] == ';' || entry[i] == '*')
            return 0;
    return 1;
}

static int validate_url(const struct transfer_params *params,
                        size_t url_bytes, size_t resolve_bytes)
{
    CURLU *url = NULL;
    char *scheme = NULL, *host = NULL, *port_text = NULL, *part = NULL;
    char *path = NULL;
    unsigned port = 0;
    int valid = 0;
    size_t i;
    for(i = 0; i < url_bytes; ++i)
        if(params->url[i] == '#' ||
           (params->auth_mode && params->url[i] == '?'))
            return 0;
    url = curl_url();
    if(!url || curl_url_set(url, CURLUPART_URL, params->url, 0) != CURLUE_OK)
        goto done;
    if(curl_url_get(url, CURLUPART_SCHEME, &scheme, 0) != CURLUE_OK ||
       !ascii_equal(scheme, strlen(scheme), "https") ||
       curl_url_get(url, CURLUPART_HOST, &host, 0) != CURLUE_OK || !*host ||
       curl_url_get(url, CURLUPART_PORT, &port_text,
                    CURLU_DEFAULT_PORT) != CURLUE_OK ||
       !decimal_port(port_text, strlen(port_text), &port))
        goto done;
    if(curl_url_get(url, CURLUPART_USER, &part, 0) != CURLUE_NO_USER)
        goto done;
    if(curl_url_get(url, CURLUPART_PASSWORD, &part, 0) != CURLUE_NO_PASSWORD)
        goto done;
    if(curl_url_get(url, CURLUPART_OPTIONS, &part, 0) != CURLUE_NO_OPTIONS)
        goto done;
    if(curl_url_get(url, CURLUPART_FRAGMENT, &part, 0) != CURLUE_NO_FRAGMENT)
        goto done;
    if(params->auth_mode) {
        if(port != 443u ||
           curl_url_get(url, CURLUPART_QUERY, &part, 0) != CURLUE_NO_QUERY ||
           curl_url_get(url, CURLUPART_PATH, &path, 0) != CURLUE_OK)
            goto done;
        if(ascii_equal(host, strlen(host), "api.nexusmods.com")) {
            if(strncmp(path, "/v1/", 4u))
                goto done;
        }
        else if(ascii_equal(host, strlen(host), "users.nexusmods.com")) {
            if(params->auth_mode != 2u || strcmp(path, "/oauth/userinfo"))
                goto done;
        }
        else
            goto done;
    }
    if(params->resolve &&
       !resolve_matches(params->resolve, resolve_bytes, host, port))
        goto done;
    valid = 1;
done:
    curl_free(path);
    curl_free(part);
    curl_free(port_text);
    curl_free(host);
    curl_free(scheme);
    curl_url_cleanup(url);
    return valid;
}

static void reject(struct download_attempt *a, mb_curl_download_status_v1 status)
{
    if(a->status == MB_CURL_DOWNLOAD_OK)
        a->status = status;
}

static int guard(struct download_attempt *a)
{
    if(!mb_curl_owner_guard_v1()) {
        if(mb_curl_owner_failed_v1())
            a->status = MB_CURL_DOWNLOAD_OWNER_FAILED;
        else if(mb_curl_owner_window_expired_v1())
            reject(a, MB_CURL_DOWNLOAD_TIMEOUT);
        else
            reject(a, MB_CURL_DOWNLOAD_LIMIT);
        return 0;
    }
    if(a->status != MB_CURL_DOWNLOAD_OK)
        return 0;
    if(a->params->cancelled &&
       a->params->cancelled(a->params->cancel_context, a->received)) {
        reject(a, MB_CURL_DOWNLOAD_CANCELLED);
        return 0;
    }
    return 1;
}

static int tick(struct download_attempt *a, size_t n, unsigned multiplier)
{
    if(a->callbacks >= CALLBACK_MAX ||
       (uint64_t)n > CALLBACK_WORK_MAX / multiplier ||
       (uint64_t)n * multiplier > CALLBACK_WORK_MAX - a->work_bytes) {
        reject(a, MB_CURL_DOWNLOAD_LIMIT);
        return 0;
    }
    ++a->callbacks;
    a->work_bytes += (uint64_t)n * multiplier;
    return 1;
}

static int header_token(unsigned c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           (c && strchr("!#$%&'*+-.^_`|~", (int)c) != NULL);
}

static int parse_header(struct download_attempt *a, const char *line, size_t n)
{
    struct response_headers *h = &a->headers;
    size_t i, colon, first, last;
    int length, transfer, encoding;
    if(n < 2u || n > HEADER_LINE_MAX ||
       n > HEADER_BYTES_MAX - h->bytes) {
        reject(a, MB_CURL_DOWNLOAD_LIMIT);
        return 0;
    }
    if(line[n - 2u] != '\r' || line[n - 1u] != '\n')
        goto refused;
    h->bytes += n;
    for(i = 0; i < n - 2u; ++i) {
        unsigned c = (unsigned char)line[i];
        if((c < 32u && c != '\t') || c > 126u)
            goto refused;
    }
    if(n >= 5u && !memcmp(line, "HTTP/", 5u)) {
        unsigned status = 0;
        if(h->final || h->in_block || n < 15u ||
           (memcmp(line, "HTTP/1.0 ", 9u) &&
            memcmp(line, "HTTP/1.1 ", 9u)) || line[12] != ' ')
            goto refused;
        for(i = 9u; i < 12u; ++i) {
            if(line[i] < '0' || line[i] > '9')
                goto refused;
            status = status * 10u + (unsigned)(line[i] - '0');
        }
        if(status < 100u || status > 599u || status == 101u)
            goto refused;
        h->status = status;
        h->in_block = 1;
        h->has_length = h->chunked = h->has_encoding = 0;
        h->content_length = 0;
        return 1;
    }
    if(!h->in_block && (!h->final || !h->chunked))
        goto refused;
    if(n == 2u) {
        if(!h->in_block)
            return 1;
        h->in_block = 0;
        if(h->status < 200u) {
            if(h->has_length || h->chunked ||
               ++h->informational > INFORMATIONAL_MAX)
                goto refused;
            return 1;
        }
        if(h->status != 200u || (h->has_length && h->chunked))
            goto refused;
        h->final = 1;
        return 1;
    }
    if(line[0] == ' ' || line[0] == '\t')
        goto refused;
    for(colon = 0; colon < n - 2u && line[colon] != ':'; ++colon)
        if(!header_token((unsigned char)line[colon]))
            goto refused;
    if(!colon || colon == n - 2u)
        goto refused;
    if(++h->fields > HEADER_FIELDS_MAX) {
        reject(a, MB_CURL_DOWNLOAD_LIMIT);
        return 0;
    }
    first = colon + 1u;
    last = n - 2u;
    while(first < last && (line[first] == ' ' || line[first] == '\t'))
        ++first;
    while(last > first && (line[last - 1u] == ' ' || line[last - 1u] == '\t'))
        --last;
    length = ascii_equal(line, colon, "content-length");
    transfer = ascii_equal(line, colon, "transfer-encoding");
    encoding = ascii_equal(line, colon, "content-encoding");
    if(!h->in_block) {
        if(length || transfer || encoding ||
           ascii_equal(line, colon, "content-type") ||
           ascii_equal(line, colon, "host") ||
           ascii_equal(line, colon, "location") ||
           ascii_equal(line, colon, "connection") ||
           ascii_equal(line, colon, "trailer"))
            goto refused;
        return 1;
    }
    if(length) {
        uint64_t value = 0;
        if(h->has_length || first == last || last - first > 20u)
            goto refused;
        for(i = first; i < last; ++i) {
            unsigned c = (unsigned char)line[i];
            if(c < '0' || c > '9' ||
               value > (UINT64_MAX - (c - '0')) / UINT64_C(10))
                goto refused;
            value = value * UINT64_C(10) + c - '0';
        }
        if(value > a->params->body_cap) {
            reject(a, MB_CURL_DOWNLOAD_LIMIT);
            return 0;
        }
        h->content_length = value;
        h->has_length = 1;
    }
    else if(transfer) {
        if(h->chunked || !ascii_equal(line + first, last - first, "chunked"))
            goto refused;
        h->chunked = 1;
    }
    else if(encoding) {
        if(h->has_encoding ||
           !ascii_equal(line + first, last - first, "identity"))
            goto refused;
        h->has_encoding = 1;
    }
    if(h->has_length && h->chunked)
        goto refused;
    return 1;
refused:
    reject(a, MB_CURL_DOWNLOAD_RESPONSE_REFUSED);
    return 0;
}

static size_t header_callback(char *data, size_t size, size_t count, void *user)
{
    struct download_attempt *a = user;
    size_t n;
    if(size && count > SIZE_MAX / size) {
        reject(a, MB_CURL_DOWNLOAD_LIMIT);
        return 0;
    }
    n = size * count;
    if(!tick(a, n, 4u) || !guard(a) || !parse_header(a, data, n))
        return 0;
    return n;
}

static int feed_carry(struct download_attempt *a)
{
    if(!guard(a))
        return 0;
    if(mb_download_file_sink_feed_v1(&a->sink, a->carry, a->carry_bytes)) {
        reject(a, a->sink.status == MB_DOWNLOAD_FILE_SINK_LIMIT ?
                  MB_CURL_DOWNLOAD_LIMIT : MB_CURL_DOWNLOAD_SINK_FAILED);
        return 0;
    }
    clear_bytes(a->carry, a->carry_bytes);
    a->carry_bytes = 0;
    return guard(a);
}

static size_t body_callback(char *data, size_t size, size_t count, void *user)
{
    struct download_attempt *a = user;
    size_t n, offset = 0;
    if(size && count > SIZE_MAX / size) {
        reject(a, MB_CURL_DOWNLOAD_LIMIT);
        return 0;
    }
    n = size * count;
    if(!tick(a, n, 3u) || !guard(a))
        return 0;
    if(!a->headers.final || a->headers.in_block || a->headers.status != 200u) {
        reject(a, MB_CURL_DOWNLOAD_RESPONSE_REFUSED);
        return 0;
    }
    if(a->received > a->params->body_cap ||
       (uint64_t)n > a->params->body_cap - a->received) {
        reject(a, MB_CURL_DOWNLOAD_LIMIT);
        return 0;
    }
    if(a->headers.has_length &&
       (a->received > a->headers.content_length ||
        (uint64_t)n > a->headers.content_length - a->received)) {
        reject(a, MB_CURL_DOWNLOAD_RESPONSE_REFUSED);
        return 0;
    }
    if(!a->params->sink) {
        memcpy(a->params->body + (size_t)a->received, data, n);
        a->received += n;
        return guard(a) ? n : 0;
    }
    a->received += n;
    while(offset < n) {
        size_t room = sizeof(a->carry) - a->carry_bytes;
        size_t take = n - offset < room ? n - offset : room;
        memcpy(a->carry + a->carry_bytes, data + offset, take);
        a->carry_bytes += take;
        offset += take;
        if(a->carry_bytes == sizeof(a->carry) && !feed_carry(a))
            return 0;
    }
    return guard(a) ? n : 0;
}

static int progress_callback(void *user, curl_off_t total, curl_off_t now,
                             curl_off_t upload_total, curl_off_t uploaded)
{
    struct download_attempt *a = user;
    (void)total;
    (void)now;
    (void)upload_total;
    (void)uploaded;
    return !tick(a, 0, 1u) || !guard(a);
}

static const uint16_t tls_groups[] = { 23, 24, 0 };
static const uint16_t tls_signatures[] = { 0x0403, 0x0503, 0x0401, 0 };
static const mbedtls_x509_crt_profile certificate_profile = {
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256) |
        MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA384),
    MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_RSA) |
        MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_ECKEY) |
        MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_ECDSA),
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP256R1) |
        MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP384R1),
    2048
};

static CURLcode ssl_context_callback(CURL *easy, void *ctx, void *user)
{
    struct download_attempt *a = user;
    (void)easy;
    if(!ctx || a->tls_context_seen || !tick(a, 0, 1u) || !guard(a))
        return CURLE_ABORTED_BY_CALLBACK;
    ++a->tls_context_seen;
    /* Borrow only within this callback. Trust, hostname verification, entropy
     * and lifetime remain owned by the configured curl/MbedTLS runtime. */
    mbedtls_ssl_conf_cert_profile(ctx, &certificate_profile);
    mbedtls_ssl_conf_groups(ctx, tls_groups);
    mbedtls_ssl_conf_sig_algs(ctx, tls_signatures);
    return CURLE_OK;
}

static void failed_file_result(const struct download_attempt *a,
                               mb_download_file_sink_result_v1 *out)
{
    memset(out, 0, sizeof(*out));
    out->struct_size = sizeof(*out);
    out->abi_version = MB_DOWNLOAD_FILE_SINK_ABI_V1;
    out->status = a->sink.phase == MB_DOWNLOAD_FILE_SINK_FAILED ?
                  a->sink.status : MB_DOWNLOAD_FILE_SINK_TRANSFER_INCOMPLETE;
    out->bytes_received = a->sink.bytes_received;
    out->bytes_written = a->sink.bytes_written;
    out->feed_calls = a->sink.feed_calls;
    out->write_calls = a->sink.write_calls;
    out->read_calls = a->sink.read_calls;
}

static int append_header(struct curl_slist **headers, const char *value)
{
    struct curl_slist *next = curl_slist_append(*headers, value);
    if(!next)
        return 0;
    *headers = next;
    return 1;
}

static int request_headers(const struct transfer_params *params,
                           struct curl_slist **headers)
{
    char secret[MB_CURL_PROVIDER_SECRET_MAX + sizeof("Authorization: Bearer ")];
    const char *prefix;
    size_t prefix_bytes;
    int ok;
    if(!append_header(headers, "Accept-Encoding: identity"))
        return 0;
    if(!params->auth_mode)
        return 1;
    if(!append_header(headers, "Accept: application/json") ||
       !append_header(headers, "Application-Name: PS5ModBridge") ||
       !append_header(headers, "Application-Version: 0.1.0"))
        return 0;
    prefix = params->auth_mode == 1u ? "apikey: " : "Authorization: Bearer ";
    prefix_bytes = strlen(prefix);
    memcpy(secret, prefix, prefix_bytes);
    memcpy(secret + prefix_bytes, params->credential, params->credential_bytes);
    secret[prefix_bytes + params->credential_bytes] = 0;
    ok = append_header(headers, secret);
    clear_bytes(secret, sizeof(secret));
    return ok;
}

static void free_headers(struct curl_slist *headers)
{
    struct curl_slist *entry;
    for(entry = headers; entry; entry = entry->next)
        if(entry->data)
            clear_bytes(entry->data, strlen(entry->data));
    curl_slist_free_all(headers);
}

static int transfer(const struct transfer_params *params,
                    mb_curl_download_result_v1 *out)
{
    struct download_attempt a;
    mb_curl_download_result_v1 result;
    mb_download_file_sink_result_v1 identity;
    CURL *easy = NULL;
    CURLM *multi = NULL;
    struct curl_slist *resolve = NULL, *headers = NULL;
    struct curl_blob ca;
    CURLcode transfer = CURLE_FAILED_INIT, option = CURLE_OK;
    CURLMcode multi_rc = CURLM_OK;
    long actual_status = 0, verify_result = -1;
    unsigned window = 0, added = 0, done = 0, identity_ready = 0;
    memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    result.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    result.file.struct_size = sizeof(result.file);
    result.file.abi_version = MB_DOWNLOAD_FILE_SINK_ABI_V1;
    result.file.status = MB_DOWNLOAD_FILE_SINK_TRANSFER_INCOMPLETE;
    if(!runtime.initialized || runtime.closed || runtime.busy) {
        result.status = mb_curl_owner_failed_v1() ?
                        MB_CURL_DOWNLOAD_OWNER_FAILED : MB_CURL_DOWNLOAD_BAD_STATE;
        if(params->body)
            clear_bytes(params->body, (size_t)params->body_cap);
        *out = result;
        return result.status;
    }
    memset(&a, 0, sizeof(a));
    memset(&identity, 0, sizeof(identity));
    a.params = params;
    runtime.busy = 1;
    if(mb_curl_owner_window_begin_v1(params->timeout_ms)) {
        reject(&a, mb_curl_owner_failed_v1() ?
                   MB_CURL_DOWNLOAD_OWNER_FAILED : MB_CURL_DOWNLOAD_BAD_STATE);
        goto cleanup;
    }
    window = 1;
    if(!guard(&a))
        goto cleanup;
    if(!validate_url(params, params->url_bytes, params->resolve_bytes)) {
        reject(&a, MB_CURL_DOWNLOAD_BAD_ARGUMENT);
        goto cleanup;
    }
    if(params->sink && mb_download_file_sink_start_v1(&a.sink, params->sink)) {
        reject(&a, MB_CURL_DOWNLOAD_SINK_FAILED);
        goto cleanup;
    }
    easy = curl_easy_init();
    multi = curl_multi_init();
    if(!easy || !multi) {
        reject(&a, MB_CURL_DOWNLOAD_SETUP_FAILED);
        goto cleanup;
    }
    if(!request_headers(params, &headers)) {
        reject(&a, MB_CURL_DOWNLOAD_SETUP_FAILED);
        goto cleanup;
    }
    if(params->resolve)
        resolve = curl_slist_append(NULL, params->resolve);
    if(!headers || (params->resolve && !resolve)) {
        reject(&a, MB_CURL_DOWNLOAD_SETUP_FAILED);
        goto cleanup;
    }
    ca.data = (void *)params->ca_pem;
    ca.len = params->ca_bytes;
    ca.flags = CURL_BLOB_COPY;
#define SET(option_name, value) do { \
    option = curl_easy_setopt(easy, (option_name), (value)); \
    if(option != CURLE_OK) { \
        reject(&a, MB_CURL_DOWNLOAD_SETUP_FAILED); \
        goto cleanup; \
    } \
} while(0)
    SET(CURLOPT_URL, params->url);
    SET(CURLOPT_HTTPGET, 1L);
    SET(CURLOPT_PROTOCOLS_STR, "https");
    SET(CURLOPT_REDIR_PROTOCOLS_STR, "https");
    SET(CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);
    SET(CURLOPT_SSLVERSION, (long)(CURL_SSLVERSION_TLSv1_2 |
                                 CURL_SSLVERSION_MAX_TLSv1_2));
    SET(CURLOPT_SSL_VERIFYPEER, 1L);
    SET(CURLOPT_SSL_VERIFYHOST, 2L);
    SET(CURLOPT_CAINFO, NULL);
    SET(CURLOPT_CAPATH, NULL);
    SET(CURLOPT_CAINFO_BLOB, &ca);
    SET(CURLOPT_RESOLVE, resolve);
    SET(CURLOPT_PROXY, "");
    /* The admitted host/native profile compiles netrc support out. Asking for
     * even NETRC_IGNORED is UNKNOWN_OPTION; there is no netrc reader to enable. */
    SET(CURLOPT_FOLLOWLOCATION, 0L);
    SET(CURLOPT_UNRESTRICTED_AUTH, 0L);
    SET(CURLOPT_FORBID_REUSE, 1L);
    SET(CURLOPT_SSL_SESSIONID_CACHE, 0L);
    SET(CURLOPT_ACCEPT_ENCODING, NULL);
    SET(CURLOPT_HTTPHEADER, headers);
    SET(CURLOPT_HTTP_CONTENT_DECODING, 0L);
    SET(CURLOPT_HTTP_TRANSFER_DECODING, 1L);
    SET(CURLOPT_HEADERFUNCTION, header_callback);
    SET(CURLOPT_HEADERDATA, &a);
    SET(CURLOPT_WRITEFUNCTION, body_callback);
    SET(CURLOPT_WRITEDATA, &a);
    SET(CURLOPT_XFERINFOFUNCTION, progress_callback);
    SET(CURLOPT_XFERINFODATA, &a);
    SET(CURLOPT_NOPROGRESS, 0L);
    SET(CURLOPT_VERBOSE, 0L);
    SET(CURLOPT_SSL_CTX_FUNCTION, ssl_context_callback);
    SET(CURLOPT_SSL_CTX_DATA, &a);
    SET(CURLOPT_MAXFILESIZE_LARGE, (curl_off_t)params->body_cap);
    SET(CURLOPT_TIMEOUT_MS, (long)params->timeout_ms);
    SET(CURLOPT_CONNECTTIMEOUT_MS, (long)params->connect_timeout_ms);
    SET(CURLOPT_NOSIGNAL, 1L);
#undef SET
    if(!guard(&a))
        goto cleanup;
    multi_rc = curl_multi_add_handle(multi, easy);
    if(multi_rc != CURLM_OK) {
        reject(&a, MB_CURL_DOWNLOAD_SETUP_FAILED);
        goto cleanup;
    }
    added = 1;
    while(!done) {
        int running = 0, left = 0, fds = 0;
        CURLMsg *message;
        if(a.drives >= DRIVE_MAX) {
            reject(&a, MB_CURL_DOWNLOAD_LIMIT);
            break;
        }
        if(!guard(&a))
            break;
        ++a.drives;
        multi_rc = curl_multi_perform(multi, &running);
        if(multi_rc != CURLM_OK) {
            reject(&a, MB_CURL_DOWNLOAD_TRANSPORT_FAILED);
            break;
        }
        while((message = curl_multi_info_read(multi, &left)) != NULL) {
            if(message->msg != CURLMSG_DONE || message->easy_handle != easy || done) {
                reject(&a, MB_CURL_DOWNLOAD_TRANSPORT_FAILED);
                break;
            }
            done = 1;
            transfer = message->data.result;
        }
        if(done || !guard(&a))
            break;
        if(!running) {
            reject(&a, MB_CURL_DOWNLOAD_TRANSPORT_FAILED);
            break;
        }
        multi_rc = curl_multi_poll(multi, NULL, 0, 100, &fds);
        if(multi_rc != CURLM_OK) {
            reject(&a, MB_CURL_DOWNLOAD_TRANSPORT_FAILED);
            break;
        }
    }
    if(curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &actual_status) != CURLE_OK ||
       curl_easy_getinfo(easy, CURLINFO_SSL_VERIFYRESULT, &verify_result) != CURLE_OK) {
        reject(&a, MB_CURL_DOWNLOAD_TRANSPORT_FAILED);
        goto cleanup;
    }
    if(transfer != CURLE_OK) {
        reject(&a, transfer == CURLE_OPERATION_TIMEDOUT ? MB_CURL_DOWNLOAD_TIMEOUT :
                   transfer == CURLE_FILESIZE_EXCEEDED ? MB_CURL_DOWNLOAD_LIMIT :
                   MB_CURL_DOWNLOAD_TRANSPORT_FAILED);
        goto cleanup;
    }
    if(!done || !guard(&a)) {
        reject(&a, MB_CURL_DOWNLOAD_TRANSPORT_FAILED);
        goto cleanup;
    }
    if(actual_status != 200 || !a.headers.final || a.headers.in_block ||
       a.headers.status != 200u || a.tls_context_seen != 1u || verify_result ||
       (a.headers.has_length && a.received != a.headers.content_length)) {
        reject(&a, MB_CURL_DOWNLOAD_RESPONSE_REFUSED);
        goto cleanup;
    }
    if(params->sink) {
        /* The last partial carry is never flushed after a curl error,
         * cancellation, rejected framing or a missing DONE message. */
        if(a.carry_bytes && !feed_carry(&a))
            goto cleanup;
        if(!guard(&a))
            goto cleanup;
        if(fsync(params->sink->fd)) {
            reject(&a, MB_CURL_DOWNLOAD_SYNC_FAILED);
            goto cleanup;
        }
        if(!guard(&a))
            goto cleanup;
        if(mb_download_file_sink_finish_v1(&a.sink, 1, a.headers.has_length,
                                          a.headers.content_length, &identity)) {
            reject(&a, MB_CURL_DOWNLOAD_SINK_FAILED);
            goto cleanup;
        }
    }
    else {
        if(!guard(&a))
            goto cleanup;
        if(params->consume_complete_body(params->consume_context, params->body,
                                          (size_t)a.received)) {
            reject(&a, MB_CURL_DOWNLOAD_CONSUMER_FAILED);
            goto cleanup;
        }
    }
    identity_ready = 1;
    (void)guard(&a);
cleanup:
    if(added) {
        multi_rc = curl_multi_remove_handle(multi, easy);
        if(multi_rc == CURLM_OK)
            added = 0;
        else {
            /* Ownership cannot be safely reused after a failed detachment.
             * Free the multi first rather than free an attached easy handle. */
            mb_curl_owner_fail_v1();
            a.status = MB_CURL_DOWNLOAD_OWNER_FAILED;
        }
    }
    if(added && multi) {
        (void)curl_multi_cleanup(multi);
        multi = NULL;
        added = 0;
    }
    if(easy)
        curl_easy_cleanup(easy);
    if(multi && curl_multi_cleanup(multi) != CURLM_OK) {
        mb_curl_owner_fail_v1();
        a.status = MB_CURL_DOWNLOAD_OWNER_FAILED;
    }
    curl_slist_free_all(resolve);
    free_headers(headers);
    if(params->body)
        clear_bytes(params->body, (size_t)params->body_cap);
    if(window) {
        (void)guard(&a);
        if(mb_curl_owner_window_end_v1()) {
            if(mb_curl_owner_failed_v1())
                a.status = MB_CURL_DOWNLOAD_OWNER_FAILED;
            else if(mb_curl_owner_window_expired_v1())
                reject(&a, MB_CURL_DOWNLOAD_TIMEOUT);
            else
                reject(&a, MB_CURL_DOWNLOAD_BAD_STATE);
        }
        /* End samples the clock itself; it can discover an ordinary deadline
         * expiry after the last guard without reporting a terminal fault. */
        if(mb_curl_owner_window_expired_v1())
            reject(&a, MB_CURL_DOWNLOAD_TIMEOUT);
    }
    if(mb_curl_owner_failed_v1())
        a.status = MB_CURL_DOWNLOAD_OWNER_FAILED;
    if(a.status == MB_CURL_DOWNLOAD_OK && !identity_ready)
        reject(&a, MB_CURL_DOWNLOAD_TRANSPORT_FAILED);
    result.status = a.status;
    result.http_status = actual_status >= 100 && actual_status <= 599 ?
                         (uint32_t)actual_status : a.headers.status;
    result.curl_code = (int32_t)(option != CURLE_OK ? option : transfer);
    result.received_bytes = a.received;
    if(a.status == MB_CURL_DOWNLOAD_OK) {
        result.complete = 1;
        result.file = identity;
    }
    else if(params->sink)
        failed_file_result(&a, &result.file);
    if(params->sink)
        mb_download_file_sink_dispose_v1(&a.sink);
    clear_bytes(&identity, sizeof(identity));
    clear_bytes(&a, sizeof(a));
    runtime.busy = 0;
    *out = result;
    return result.status;
}

int mb_curl_download_v1(const mb_curl_download_params_v1 *params,
                       mb_curl_download_result_v1 *out)
{
    struct transfer_params request;
    size_t url_bytes = 0, resolve_bytes = 0;
    int status;
    if(!params || !out ||
       overlaps(params, sizeof(*params), out, sizeof(*out)) ||
       params->struct_size != sizeof(*params) ||
       params->abi_version != MB_CURL_DOWNLOAD_ABI_V1 ||
       !bounded_ascii(params->url, MB_CURL_DOWNLOAD_URL_MAX, &url_bytes) ||
       !params->ca_pem || !params->ca_bytes ||
       params->ca_bytes > MB_CURL_DOWNLOAD_CA_MAX ||
       !params->timeout_ms ||
       params->timeout_ms > MB_CURL_DOWNLOAD_TIMEOUT_MAX_MS ||
       !params->connect_timeout_ms ||
       params->connect_timeout_ms > params->timeout_ms ||
       params->sink.struct_size != sizeof(params->sink) ||
       params->sink.abi_version != MB_DOWNLOAD_FILE_SINK_ABI_V1 ||
       params->sink.reserved || params->sink.fd < 0 ||
       !params->sink.cap_bytes ||
       params->sink.cap_bytes > MB_DOWNLOAD_FILE_SINK_MAX_BYTES ||
       !params->sink.read_at || !params->sink.write_at ||
       (params->resolve &&
        !bounded_ascii(params->resolve, MB_CURL_DOWNLOAD_URL_MAX, &resolve_bytes)))
        return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    if(overlaps(params->url, url_bytes + 1u, out, sizeof(*out)) ||
       overlaps(params->ca_pem, params->ca_bytes, out, sizeof(*out)) ||
       (params->resolve &&
        overlaps(params->resolve, resolve_bytes + 1u, out, sizeof(*out))))
        return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    memset(&request, 0, sizeof(request));
    request.url = params->url;
    request.url_bytes = url_bytes;
    request.ca_pem = params->ca_pem;
    request.ca_bytes = params->ca_bytes;
    request.resolve = params->resolve;
    request.resolve_bytes = resolve_bytes;
    request.timeout_ms = params->timeout_ms;
    request.connect_timeout_ms = params->connect_timeout_ms;
    request.body_cap = params->sink.cap_bytes;
    request.sink = &params->sink;
    request.cancel_context = params->cancel_context;
    request.cancelled = params->cancelled;
    status = transfer(&request, out);
    clear_bytes(&request, sizeof(request));
    return status;
}

/* Measure storage independently of ASCII policy, so a safely bounded but
 * rejected URL/credential cannot leave bytes in an independent raw buffer. */
static int bounded_string(const char *s, size_t maximum, size_t *length)
{
    size_t n;
    if(!s) {
        *length = 0;
        return 1;
    }
    for(n = 0; n <= maximum; ++n)
        if(!s[n]) {
            *length = n;
            return 1;
        }
    return 0;
}

static int provider_storage(const mb_curl_provider_transfer_params_v1 *params,
                            mb_curl_provider_transfer_result_v1 *out,
                            size_t *url_bytes, size_t *resolve_bytes)
{
    struct region { const void *data; size_t bytes; } regions[7];
    size_t i, j;
    if(!params || !params->body || !params->body_capacity ||
       params->body_capacity > MB_CURL_PROVIDER_BODY_MAX ||
       params->credential_bytes > MB_CURL_PROVIDER_SECRET_MAX ||
       params->ca_bytes > MB_CURL_DOWNLOAD_CA_MAX ||
       !bounded_string(params->url, MB_CURL_DOWNLOAD_URL_MAX, url_bytes) ||
       !bounded_string(params->resolve, MB_CURL_DOWNLOAD_URL_MAX, resolve_bytes))
        return 0;
    regions[0] = (struct region){ params, sizeof(*params) };
    regions[1] = (struct region){ out, out ? sizeof(*out) : 0u };
    regions[2] = (struct region){ params->body, params->body_capacity };
    regions[3] = (struct region){ params->url, params->url ? *url_bytes + 1u : 0u };
    regions[4] = (struct region){ params->credential, params->credential_bytes };
    regions[5] = (struct region){ params->ca_pem, params->ca_bytes };
    regions[6] = (struct region){ params->resolve,
                                params->resolve ? *resolve_bytes + 1u : 0u };
    for(i = 0; i < sizeof(regions) / sizeof(regions[0]); ++i)
        for(j = i + 1u; j < sizeof(regions) / sizeof(regions[0]); ++j)
            if(overlaps(regions[i].data, regions[i].bytes,
                        regions[j].data, regions[j].bytes))
                return 0;
    return 1;
}

int mb_curl_provider_transfer_v1(const mb_curl_provider_transfer_params_v1 *params,
                                mb_curl_provider_transfer_result_v1 *out)
{
    struct transfer_params request;
    mb_curl_provider_transfer_result_v1 result;
    mb_curl_download_result_v1 transferred;
    size_t url_bytes = 0, resolve_bytes = 0, measured = 0, i;
    int valid = 1;
    if(!params || params->struct_size != sizeof(*params) ||
       params->abi_version != MB_CURL_DOWNLOAD_ABI_V1)
        return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    if(!provider_storage(params, out, &url_bytes, &resolve_bytes))
        return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    if(!out) {
        clear_bytes(params->body, params->body_capacity);
        return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    }
    memset(&request, 0, sizeof(request));
    memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    result.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    result.status = MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    result.curl_code = (int32_t)CURLE_FAILED_INIT;
    if(!bounded_ascii(params->url, MB_CURL_DOWNLOAD_URL_MAX, &measured) ||
       (params->resolve &&
        !bounded_ascii(params->resolve, MB_CURL_DOWNLOAD_URL_MAX, &measured)) ||
       (params->auth_mode != 1u && params->auth_mode != 2u) ||
       !params->credential || !params->credential_bytes ||
       !params->ca_pem || !params->ca_bytes ||
       !params->timeout_ms ||
       params->timeout_ms > MB_CURL_DOWNLOAD_TIMEOUT_MAX_MS ||
       !params->connect_timeout_ms ||
       params->connect_timeout_ms > params->timeout_ms ||
       !params->consume_complete_body)
        valid = 0;
    if(valid)
        for(i = 0; i < params->credential_bytes; ++i) {
            unsigned c = (unsigned char)params->credential[i];
            if(c < 33u || c > 126u) {
                valid = 0;
                break;
            }
        }
    request.url = params->url;
    request.url_bytes = url_bytes;
    request.ca_pem = params->ca_pem;
    request.ca_bytes = params->ca_bytes;
    request.resolve = params->resolve;
    request.resolve_bytes = resolve_bytes;
    request.timeout_ms = params->timeout_ms;
    request.connect_timeout_ms = params->connect_timeout_ms;
    request.body_cap = params->body_capacity;
    request.body = params->body;
    request.auth_mode = params->auth_mode;
    request.credential = params->credential;
    request.credential_bytes = params->credential_bytes;
    request.consume_context = params->consume_context;
    request.consume_complete_body = params->consume_complete_body;
    request.cancel_context = params->cancel_context;
    request.cancelled = params->cancelled;
    if(valid && !validate_url(&request, url_bytes, resolve_bytes))
        valid = 0;
    if(valid) {
        (void)transfer(&request, &transferred);
        result.status = transferred.status;
        result.complete = transferred.complete;
        result.http_status = transferred.http_status;
        result.curl_code = transferred.curl_code;
        result.received_bytes = transferred.received_bytes;
        clear_bytes(&transferred, sizeof(transferred));
    }
    else
        clear_bytes(params->body, params->body_capacity);
    clear_bytes(&request, sizeof(request));
    *out = result;
    return result.status;
}
