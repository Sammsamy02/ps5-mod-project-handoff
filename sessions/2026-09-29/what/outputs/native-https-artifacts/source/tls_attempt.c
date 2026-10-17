/* Ignored integration experiment. Native code uses the reviewed real collector;
   development injection belongs only to the separate host boundary unit. */
#define _POSIX_C_SOURCE 200809L
#include "tls_attempt.h"
#include "tls_effective.h"
#include "tls_seed_source.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define EXP_FAIL (-0x7100)
#define TOTAL_NS INT64_C(20000000000)
#define CLOCK_LIMIT 4096u
#define TLS_LIMIT 512u
#define BIO_LIMIT 512u
#define POLL_LIMIT 256u
#define RNG_LIMIT 96u
#define RNG_BYTES_LIMIT 6144u

static const unsigned char request_prefix[] = "GET /native-https-v1 HTTP/1.1\r\nHost: ";
static const unsigned char request_suffix[] = "\r\nConnection: close\r\nAccept-Encoding: identity\r\n\r\n";
static const unsigned char response[] = "HTTP/1.1 200 OK\r\nContent-Length: 24\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nnative-https-fixture-v1\n";
static const int suites[] = {MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0};
static const uint16_t groups[] = {23, 24, 0};
static const uint16_t sigs[] = {0x0403, 0x0503, 0x0401, 0};
static const mbedtls_x509_crt_profile profile = {
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256) | MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA384),
    MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_ECKEY) | MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_ECDSA) | MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_RSA),
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP256R1) | MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP384R1), 2048
};

struct operation {
    struct tls_seed_operation seed;
    mbedtls_ssl_config conf;
    mbedtls_ssl_context ssl;
    mbedtls_x509_crt root;
    unsigned initialized, in_library, clock_failed, have_anchor;
    int fd, root_parse_rc, setup_rc, handshake_rc, close_rc, last_tls_rc, clock_errno;
    const char *phase, *reason, *clock_reason;
    unsigned clock_attempts, monotonic_calls, wall_calls, library_reads, utc_calls, cert_utc_calls;
    unsigned rng_calls, rng_bytes, tls_calls, bio_calls, polls;
    unsigned wire_send, wire_recv, request_bytes, response_bytes, clean_peer, body_accepted;
    unsigned certificate_verified, framed_complete, local_close;
    unsigned have_utc;
    int utc_year, utc_month, utc_day, utc_hour, utc_minute, utc_second;
    time_t anchor, last_wall;
    int64_t anchor_a, anchor_b, last_mono, last_b;
    uint32_t address;
    unsigned port;
    size_t root_length, request_length;
    char hostname[TLS_ATTEMPT_MAX_HOSTNAME + 1];
    unsigned char root_der[TLS_ATTEMPT_MAX_ROOT_DER], request[512], output[256];
};
/* Mbed's public time macro / UTC hook have no context argument. The existing
   status-host caller must serialize this one operation and all hook consumers. */
static struct operation *owner;

static void wipe(void *storage, size_t capacity) {
    volatile unsigned char *bytes = storage;
    for (size_t i = 0; i < capacity; ++i) bytes[i] = 0;
}
static void text_copy(char *out, size_t capacity, const char *text) {
    size_t i = 0;
    if (text) while (i + 1 < capacity && text[i]) { out[i] = text[i]; ++i; }
    out[i] = 0;
}
static void fail(struct operation *o, const char *reason) {
    if (!o->reason) o->reason = reason;
}
static void clock_fail(struct operation *o, const char *reason) {
    if (!o->clock_failed) o->clock_reason = reason;
    o->clock_failed = 1;
    fail(o, reason);
}
static int mono(struct operation *o, int64_t *out) {
    struct timespec value = {0};
    errno = 0;
    ++o->monotonic_calls;
    int rc = clock_gettime(CLOCK_MONOTONIC, &value);
    int e = errno;
    /* Leave arithmetic room for all bounded deadlines and tv_nsec addition. */
    if (rc || e || value.tv_sec < 0 ||
        value.tv_sec > (INT64_MAX - 999999999 - TOTAL_NS) / INT64_C(1000000000) ||
        value.tv_nsec < 0 || value.tv_nsec >= 1000000000) {
        o->clock_errno = e;
        clock_fail(o, "monotonic_unavailable");
        return -1;
    }
    int64_t v = (int64_t)value.tv_sec * INT64_C(1000000000) + value.tv_nsec;
    if (v < o->last_mono) { clock_fail(o, "monotonic_backward"); return -1; }
    o->last_mono = v;
    *out = v;
    return 0;
}
static time_t sample(struct operation *o) {
    if (!o || o->clock_failed) return (time_t)-1;
    if (o->clock_attempts >= CLOCK_LIMIT) { clock_fail(o, "clock_budget"); return (time_t)-1; }
    ++o->clock_attempts;
    int64_t a, b;
    if (mono(o, &a)) return (time_t)-1;
    errno = 0;
    ++o->wall_calls;
    time_t w = time(NULL);
    int e = errno;
    o->clock_errno = e;
    if (mono(o, &b)) return (time_t)-1;
    o->last_b = b;
    if (w == (time_t)-1) { clock_fail(o, "time_unavailable"); return (time_t)-1; }
    if (e) { clock_fail(o, "time_errno_nonzero"); return (time_t)-1; }
    if (w < 0 || w > INT64_C(253402300799)) { clock_fail(o, "time_out_of_range"); return (time_t)-1; }
    if (b - a > INT64_C(100000000)) { clock_fail(o, "clock_sample_late"); return (time_t)-1; }
    if (o->have_anchor) {
        if (w < o->last_wall) { clock_fail(o, "time_backward"); return (time_t)-1; }
        if (b - o->anchor_b > TOTAL_NS) { clock_fail(o, "operation_deadline"); return (time_t)-1; }
        int64_t lo = (a - o->anchor_b) / INT64_C(1000000000);
        int64_t delta = b - o->anchor_a;
        int64_t hi = delta / INT64_C(1000000000) + (delta % INT64_C(1000000000) != 0);
        if (w < o->anchor + lo || w > o->anchor + hi) { clock_fail(o, "time_changed"); return (time_t)-1; }
    } else {
        o->anchor = w;
        o->anchor_a = a;
        o->anchor_b = b;
        o->have_anchor = 1;
    }
    o->last_wall = w;
    return w;
}
time_t exp_clock_time(time_t *out) {
    time_t value = (time_t)-1;
    if (owner) { ++owner->library_reads; value = sample(owner); }
    if (out) *out = value;
    return value;
}
struct tm *mbedtls_platform_gmtime_r(const mbedtls_time_t *time_value, struct tm *out) {
    struct operation *o = owner;
    if (o) { ++o->utc_calls; if (o->in_library) ++o->cert_utc_calls; }
    if (!o || !time_value || !out || o->clock_failed || *time_value == (mbedtls_time_t)-1) {
        if (out) wipe(out, sizeof(*out));
        return NULL;
    }
    errno = 0;
    struct tm *converted = gmtime(time_value);
    int e = errno;
    if (!converted || e) {
        o->clock_errno = e;
        clock_fail(o, "utc_unavailable");
        wipe(out, sizeof(*out));
        return NULL;
    }
    struct tm value = *converted; /* Copy shared libc storage while exclusively owned. */
    if (value.tm_year < -1899 || value.tm_year > 8099 || value.tm_mon < 0 || value.tm_mon > 11 ||
        value.tm_mday < 1 || value.tm_mday > 31 || value.tm_hour < 0 || value.tm_hour > 23 ||
        value.tm_min < 0 || value.tm_min > 59 || value.tm_sec < 0 || value.tm_sec > 59 ||
        value.tm_wday < 0 || value.tm_wday > 6) {
        clock_fail(o, "utc_invalid");
        wipe(out, sizeof(*out));
        return NULL;
    }
    static const unsigned days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    unsigned year = (unsigned)(value.tm_year + 1900);
    unsigned max_day = days[value.tm_mon] +
        (value.tm_mon == 1 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    if ((unsigned)value.tm_mday > max_day) {
        clock_fail(o, "utc_invalid");
        wipe(out, sizeof(*out));
        return NULL;
    }
    *out = value;
    o->have_utc = 1;
    o->utc_year = value.tm_year + 1900;
    o->utc_month = value.tm_mon + 1;
    o->utc_day = value.tm_mday;
    o->utc_hour = value.tm_hour;
    o->utc_minute = value.tm_min;
    o->utc_second = value.tm_sec;
    return out;
}
static int guard(struct operation *o) {
    return !o->reason && !o->clock_failed && sample(o) != (time_t)-1;
}
static int utc_guard(struct operation *o) {
    mbedtls_x509_time converted;
    time_t w = sample(o);
    return !o->reason && w != (time_t)-1 && mbedtls_x509_time_gmtime(w, &converted) == 0;
}

static int checked_inputs(struct operation *o, const struct tls_attempt_input *in) {
    if (!in || !in->numeric_address || !in->expected_hostname || !in->root_der ||
        in->numeric_address_length < 7 || in->numeric_address_length > TLS_ATTEMPT_MAX_ADDRESS ||
        !in->expected_hostname_length || in->expected_hostname_length > TLS_ATTEMPT_MAX_HOSTNAME ||
        !in->port || in->port > 65535 || !in->root_der_length || in->root_der_length > TLS_ATTEMPT_MAX_ROOT_DER)
        return -1;
    uint32_t address = 0;
    size_t cursor = 0;
    for (unsigned part = 0; part < 4; ++part) {
        size_t start = cursor;
        unsigned octet = 0;
        while (cursor < in->numeric_address_length && in->numeric_address[cursor] >= '0' && in->numeric_address[cursor] <= '9') {
            if (cursor - start >= 3) return -1;
            octet = octet * 10 + (unsigned)(in->numeric_address[cursor++] - '0');
        }
        if (cursor == start || octet > 255 ||
            (cursor - start > 1 && in->numeric_address[start] == '0')) return -1;
        address = (address << 8) | octet;
        if (part < 3) {
            if (cursor >= in->numeric_address_length || in->numeric_address[cursor++] != '.') return -1;
        } else if (cursor != in->numeric_address_length) return -1;
    }
    if (!address || address == UINT32_MAX || (address & UINT32_C(0xf0000000)) == UINT32_C(0xe0000000)) return -1;
    size_t label = 0;
    for (size_t i = 0; i < in->expected_hostname_length; ++i) {
        unsigned char c = (unsigned char)in->expected_hostname[i];
        if (c == '.') {
            if (!label || in->expected_hostname[i - 1] == '-') return -1;
            label = 0;
        } else {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || (c == '-' && label))) return -1;
            if (++label > 63) return -1;
        }
    }
    if (!label || in->expected_hostname[in->expected_hostname_length - 1] == '-') return -1;
    o->address = address;
    o->port = in->port;
    memcpy(o->hostname, in->expected_hostname, in->expected_hostname_length);
    o->hostname[in->expected_hostname_length] = 0;
    memcpy(o->root_der, in->root_der, in->root_der_length);
    o->root_length = in->root_der_length;
    size_t n = sizeof(request_prefix) - 1;
    memcpy(o->request, request_prefix, n);
    memcpy(o->request + n, o->hostname, in->expected_hostname_length);
    n += in->expected_hostname_length;
    memcpy(o->request + n, request_suffix, sizeof(request_suffix) - 1);
    o->request_length = n + sizeof(request_suffix) - 1;
    return 0;
}

static int rng(void *state, unsigned char *output, size_t capacity) {
    struct operation *o = state;
    int bounded = output && capacity && capacity <= 64;
    if (!o || !bounded) return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    if (!guard(o) || o->seed.source.failed || o->rng_calls >= RNG_LIMIT ||
        capacity > RNG_BYTES_LIMIT - o->rng_bytes) {
        wipe(output, capacity);
        fail(o, "rng_policy");
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }
    ++o->rng_calls;
    o->rng_bytes += (unsigned)capacity;
    int rc = mbedtls_ctr_drbg_random(&o->seed.ctr, output, capacity);
    if (rc || !guard(o)) {
        wipe(output, capacity);
        fail(o, "rng_failed");
        return rc ? rc : MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }
    return 0;
}
static int bio_send(void *state, const unsigned char *buffer, size_t capacity) {
    struct operation *o = state;
    if (!guard(o) || o->bio_calls >= BIO_LIMIT || capacity > 32768u - o->wire_send) {
        fail(o, "send_policy"); return EXP_FAIL;
    }
    ++o->bio_calls;
    errno = 0;
    ssize_t rc = send(o->fd, buffer, capacity, 0);
    int e = errno;
    if (rc > 0 && (size_t)rc <= capacity) o->wire_send += (unsigned)rc;
    if (!guard(o)) return EXP_FAIL;
    if (rc > 0 && (size_t)rc <= capacity) return (int)rc;
    if (rc < 0 && (e == EINTR || e == EAGAIN || e == EWOULDBLOCK)) return MBEDTLS_ERR_SSL_WANT_WRITE;
    fail(o, "send_failed"); return EXP_FAIL;
}
static int bio_recv(void *state, unsigned char *buffer, size_t capacity) {
    struct operation *o = state;
    if (!guard(o) || o->bio_calls >= BIO_LIMIT || capacity > 65536u - o->wire_recv) {
        fail(o, "recv_policy"); return EXP_FAIL;
    }
    ++o->bio_calls;
    errno = 0;
    ssize_t rc = recv(o->fd, buffer, capacity, 0);
    int e = errno;
    if (rc > 0 && (size_t)rc <= capacity) o->wire_recv += (unsigned)rc;
    if (!guard(o)) return EXP_FAIL;
    if (rc >= 0 && (size_t)rc <= capacity) return (int)rc;
    if (rc < 0 && (e == EINTR || e == EAGAIN || e == EWOULDBLOCK)) return MBEDTLS_ERR_SSL_WANT_READ;
    fail(o, "recv_failed"); return EXP_FAIL;
}
static int wait_ready(struct operation *o, int want, int64_t until) {
    if (!guard(o) || o->polls >= POLL_LIMIT || o->last_b >= until) {
        fail(o, "poll_limit_or_deadline"); return -1;
    }
    int64_t remain = until - o->last_b;
    int ms = remain < INT64_C(100000000) ? (int)(remain / 1000000) : 100;
    if (ms < 1) ms = 1;
    struct pollfd descriptor = {o->fd, want == MBEDTLS_ERR_SSL_WANT_WRITE ? POLLOUT : POLLIN, 0};
    ++o->polls;
    errno = 0;
    int rc = poll(&descriptor, 1, ms);
    int e = errno;
    if (!guard(o)) return -1;
    if (rc < 0 && e != EINTR) { fail(o, "poll_failed"); return -1; }
    return rc > 0 ? 1 : 0;
}
static int step(struct operation *o, int action, const unsigned char *write_buffer,
                 size_t capacity, unsigned char *read_buffer) {
    if (!guard(o) || o->tls_calls >= TLS_LIMIT) { fail(o, "tls_entry_limit"); return EXP_FAIL; }
    ++o->tls_calls;
    o->in_library = 1;
    int rc;
    if (action == 0) rc = mbedtls_ssl_handshake(&o->ssl);
    else if (action == 1) rc = mbedtls_ssl_write(&o->ssl, write_buffer, capacity);
    else if (action == 2) rc = mbedtls_ssl_read(&o->ssl, read_buffer, capacity);
    else rc = mbedtls_ssl_close_notify(&o->ssl);
    o->in_library = 0;
    o->last_tls_rc = rc;
    return guard(o) ? rc : EXP_FAIL;
}
static int connect_once(struct operation *o) {
    o->phase = "connect";
    if (!guard(o)) return -1;
    o->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (o->fd < 0) { fail(o, "socket_failed"); return -1; }
#ifdef SO_NOSIGPIPE
    int one = 1;
    if (setsockopt(o->fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one)) < 0) {
        fail(o, "socket_option_failed"); return -1;
    }
#endif
    int flags = fcntl(o->fd, F_GETFL, 0);
    if (flags < 0 || fcntl(o->fd, F_SETFL, flags | O_NONBLOCK) < 0) { fail(o, "nonblocking_failed"); return -1; }
    struct sockaddr_in address = {0};
    address.sin_len = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)o->port);
    address.sin_addr.s_addr = htonl(o->address);
    errno = 0;
    int rc = connect(o->fd, (struct sockaddr *)&address, sizeof(address));
    int e = errno;
    if (!guard(o)) return -1;
    if (rc < 0 && e != EINPROGRESS) { fail(o, "connect_failed"); return -1; }
    if (rc < 0) {
        int64_t until = o->last_b + INT64_C(3000000000);
        if (until > o->anchor_b + TOTAL_NS) until = o->anchor_b + TOTAL_NS;
        for (;;) {
            int ready = wait_ready(o, MBEDTLS_ERR_SSL_WANT_WRITE, until);
            if (ready < 0) return -1;
            if (!ready) continue;
            int error = 0;
            socklen_t length = sizeof(error);
            errno = 0;
            rc = getsockopt(o->fd, SOL_SOCKET, SO_ERROR, &error, &length);
            if (!guard(o)) return -1;
            if (rc || error || length != sizeof(error)) { fail(o, "connect_result_failed"); return -1; }
            break;
        }
    }
    return 0;
}

static void snapshot(struct tls_attempt_result *result, struct operation *o) {
    text_copy(result->phase, sizeof(result->phase), o->phase);
    text_copy(result->reason, sizeof(result->reason), o->reason);
    text_copy(result->clock_failure, sizeof(result->clock_failure), o->clock_reason);
    text_copy(result->source_failure, sizeof(result->source_failure), o->seed.source.failure);
    result->body_accepted = o->body_accepted;
    result->clean_peer_close = o->clean_peer;
    result->certificate_verified = o->certificate_verified;
    result->framed_response_complete = o->framed_complete;
    result->local_close_notify_confirmed = o->local_close;
    result->root_parse_rc = o->root_parse_rc;
    result->last_tls_rc = o->last_tls_rc;
    result->verify_flags = o->initialized ? mbedtls_ssl_get_verify_result(&o->ssl) : UINT32_MAX;
    result->verify_available = result->verify_flags != UINT32_MAX;
    if (o->seed.seed_attempted) {
        result->add_source_rc = o->seed.add_source_rc;
        result->seed_rc = o->seed.seed_rc;
    }
    result->setup_rc = o->setup_rc;
    result->handshake_rc = o->handshake_rc;
    result->close_notify_rc = o->close_rc;
    result->source_callback_count = o->seed.source.callback_count;
    result->source_os_call_count = o->seed.source.os_call_count;
    result->shared_api_count = o->seed.budget.api_count;
    result->source_closed = o->seed.source.closed;
    result->source_failed = o->seed.source.failed;
    result->has_last_api = o->seed.source.has_last;
    if (result->has_last_api) {
        const struct mb_entropy_result *actual = &o->seed.source.last_actual;
        text_copy(result->last_api_status, sizeof(result->last_api_status), actual->status);
        text_copy(result->last_api_reason, sizeof(result->last_api_reason), actual->reason);
        result->last_api_return_code = actual->return_code;
        result->last_api_errno = actual->error_number;
        result->last_api_returned_length = actual->returned_length;
        result->last_api_elapsed_ms = actual->elapsed_ms;
    }
    result->rng_calls = o->rng_calls;
    result->rng_requested_bytes = o->rng_bytes;
    result->tls_entries = o->tls_calls;
    result->bio_entries = o->bio_calls;
    result->poll_calls = o->polls;
    result->wire_sent_bytes = o->wire_send;
    result->wire_received_bytes = o->wire_recv;
    result->http_request_bytes = o->request_bytes;
    result->response_bytes = o->response_bytes;
}

int tls_attempt(const struct tls_attempt_input *input, struct tls_attempt_result *result) {
    if (!result) return -1;
    memset(result, 0, sizeof(*result));
    result->verify_flags = UINT32_MAX;
    result->add_source_rc = result->seed_rc = result->setup_rc = INT_MAX;
    result->handshake_rc = result->close_notify_rc = INT_MAX;
    result->root_parse_rc = result->last_tls_rc = INT_MAX;
    result->elapsed_ms = result->last_api_elapsed_ms = -1;
    if (owner) {
        text_copy(result->phase, sizeof(result->phase), "entry");
        text_copy(result->reason, sizeof(result->reason), "operation_busy");
        return -1;
    }
    struct {
#ifdef TLS_ATTEMPT_TESTING
        uint64_t before;
#endif
        struct operation op;
#ifdef TLS_ATTEMPT_TESTING
        uint64_t after;
#endif
    } storage = {0};
#ifdef TLS_ATTEMPT_TESTING
    storage.before = storage.after = UINT64_C(0x7eac19b4d62fb081);
#endif
    struct operation *o = &storage.op;
    o->fd = -1;
    o->phase = "inputs";
    o->root_parse_rc = o->setup_rc = o->handshake_rc = o->close_rc = o->last_tls_rc = INT_MAX;
    if (checked_inputs(o, input)) { fail(o, "invalid_input"); goto cleanup; }
    owner = o;
    mbedtls_ssl_config_init(&o->conf);
    mbedtls_ssl_init(&o->ssl);
    mbedtls_x509_crt_init(&o->root);
    o->initialized = 1;
    o->phase = "clock_precheck";
    if (!utc_guard(o)) goto cleanup;
    o->phase = "root_parse";
    int rc = mbedtls_x509_crt_parse_der(&o->root, o->root_der, o->root_length);
    o->root_parse_rc = rc;
    if (rc || o->root.next || o->root.raw.len != o->root_length) { fail(o, "root_parse_failed"); goto cleanup; }
    o->phase = "seed";
    if (!guard(o)) goto cleanup;
    rc = tls_seed_once(&o->seed);
    if (rc || o->seed.source.os_call_count != 2 || o->seed.budget.api_count != 2) {
        fail(o, "seed_failed"); goto cleanup;
    }
    if (!guard(o)) goto cleanup;
    o->phase = "setup";
    o->setup_rc = mbedtls_ssl_config_defaults(&o->conf, MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (o->setup_rc) { fail(o, "config_defaults_failed"); goto cleanup; }
    mbedtls_ssl_conf_authmode(&o->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_rng(&o->conf, rng, o);
    mbedtls_ssl_conf_ca_chain(&o->conf, &o->root, NULL);
    mbedtls_ssl_conf_cert_profile(&o->conf, &profile);
    mbedtls_ssl_conf_min_tls_version(&o->conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&o->conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_ciphersuites(&o->conf, suites);
    mbedtls_ssl_conf_groups(&o->conf, groups);
    mbedtls_ssl_conf_sig_algs(&o->conf, sigs);
    o->setup_rc = mbedtls_ssl_setup(&o->ssl, &o->conf);
    if (o->setup_rc) { fail(o, "ssl_setup_failed"); goto cleanup; }
    o->setup_rc = mbedtls_ssl_set_hostname(&o->ssl, o->hostname);
    if (o->setup_rc) { fail(o, "hostname_setup_failed"); goto cleanup; }
    if (connect_once(o)) goto cleanup;
    mbedtls_ssl_set_bio(&o->ssl, o, bio_send, bio_recv, NULL);
    o->phase = "handshake";
    for (;;) {
        rc = step(o, 0, NULL, 0, NULL);
        o->handshake_rc = o->last_tls_rc;
        if (!rc) break;
        if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE) { fail(o, "handshake_failed"); goto cleanup; }
        if (wait_ready(o, rc, o->anchor_b + TOTAL_NS) < 0) goto cleanup;
    }
    o->phase = "verify_gate";
    if (!utc_guard(o) || mbedtls_ssl_get_verify_result(&o->ssl) ||
        mbedtls_ssl_get_version_number(&o->ssl) != MBEDTLS_SSL_VERSION_TLS1_2 ||
        mbedtls_ssl_get_ciphersuite_id_from_ssl(&o->ssl) != suites[0]) {
        fail(o, "verify_gate_failed"); goto cleanup;
    }
    o->certificate_verified = 1;
    o->phase = "write";
    while (o->request_bytes < o->request_length) {
        rc = step(o, 1, o->request + o->request_bytes, o->request_length - o->request_bytes, NULL);
        if (rc > 0) { o->request_bytes += (unsigned)rc; continue; }
        if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE) { fail(o, "http_write_failed"); goto cleanup; }
        if (wait_ready(o, rc, o->anchor_b + TOTAL_NS) < 0) goto cleanup;
    }
    o->phase = "read";
    for (;;) {
        if (o->response_bytes == sizeof(o->output)) { fail(o, "response_capacity"); goto cleanup; }
        rc = step(o, 2, NULL, sizeof(o->output) - o->response_bytes, o->output + o->response_bytes);
        if (rc > 0) {
            unsigned n = (unsigned)rc;
            if (n > sizeof(response) - 1 - o->response_bytes ||
                memcmp(o->output + o->response_bytes, response + o->response_bytes, n)) {
                fail(o, "transcript_mismatch"); goto cleanup;
            }
            o->response_bytes += n;
            if (o->response_bytes == sizeof(response) - 1) o->framed_complete = 1;
            continue;
        }
        if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) { o->clean_peer = 1; break; }
        if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE) { fail(o, "tls_eof_or_read_failed"); goto cleanup; }
        if (wait_ready(o, rc, o->anchor_b + TOTAL_NS) < 0) goto cleanup;
    }
    if (o->response_bytes != sizeof(response) - 1) { fail(o, "transcript_incomplete"); goto cleanup; }
    o->phase = "close";
    int64_t until = o->last_b + INT64_C(500000000);
    if (until > o->anchor_b + TOTAL_NS) until = o->anchor_b + TOTAL_NS;
    for (;;) {
        rc = step(o, 3, NULL, 0, NULL);
        o->close_rc = o->last_tls_rc;
        if (!rc) { o->local_close = 1; break; }
        if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE) { fail(o, "close_notify_failed"); goto cleanup; }
        if (wait_ready(o, rc, until) < 0) goto cleanup;
    }
    if (!utc_guard(o)) goto cleanup;
    o->body_accepted = 1;
cleanup:
    snapshot(result, o);
    if (o->initialized) {
        mbedtls_ssl_free(&o->ssl);
        mbedtls_ssl_config_free(&o->conf);
        mbedtls_x509_crt_free(&o->root);
    }
    tls_seed_cleanup(&o->seed);
    if (o->fd >= 0) {
        result->descriptor_close_attempts = 1;
        errno = 0;
        int close_rc = close(o->fd);
        int e = errno;
        if (close_rc) {
            result->descriptor_close_failed = 1;
            result->descriptor_close_errno = e;
            fail(o, "descriptor_close_failed");
        }
        o->fd = -1; /* Do not retry a possibly already-consumed descriptor. */
    }
    if (owner == o && !o->reason) (void)guard(o);
    text_copy(result->reason, sizeof(result->reason), o->reason);
    text_copy(result->clock_failure, sizeof(result->clock_failure), o->clock_reason);
    result->clock_attempts = o->clock_attempts;
    result->monotonic_calls = o->monotonic_calls;
    result->wall_calls = o->wall_calls;
    result->library_time_reads = o->library_reads;
    result->utc_calls = o->utc_calls;
    result->certificate_utc_calls = o->cert_utc_calls;
    result->clock_errno = o->clock_errno;
    result->wall_value_observed = o->have_anchor;
    result->utc_value_observed = o->have_utc;
    result->initial_wall_epoch = (int64_t)o->anchor;
    result->last_wall_epoch = (int64_t)o->last_wall;
    result->utc_year = o->utc_year;
    result->utc_month = o->utc_month;
    result->utc_day = o->utc_day;
    result->utc_hour = o->utc_hour;
    result->utc_minute = o->utc_minute;
    result->utc_second = o->utc_second;
    if (o->have_anchor && o->last_b >= o->anchor_b) result->elapsed_ms = (o->last_b - o->anchor_b) / 1000000;
    result->passed = o->body_accepted && !o->reason;
    if (owner == o) owner = NULL;
    wipe(o, sizeof(*o));
#ifdef TLS_ATTEMPT_TESTING
    extern void tls_attempt_test_cleanup(const void *, size_t, unsigned, unsigned);
    tls_attempt_test_cleanup(o, sizeof(*o),
        storage.before == UINT64_C(0x7eac19b4d62fb081), storage.after == UINT64_C(0x7eac19b4d62fb081));
#endif
    wipe(&storage, sizeof(storage));
    result->cleanup_completed = 1;
    return result->passed ? 0 : -1;
}
