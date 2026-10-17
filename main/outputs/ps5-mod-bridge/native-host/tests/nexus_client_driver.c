#define _POSIX_C_SOURCE 200809L
#include "../nexus_client.h"
#include "../download/curl_download.h"
#include "../download/provider_transfer.h"
#include <arpa/inet.h>
#include <assert.h>
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

struct shim_state {
    unsigned fixture_port;
    unsigned installs;
    unsigned open_calls;
    unsigned connected;
    unsigned refused;
    curl_socket_t connected_fd;
};

static struct shim_state socket_shim;
#ifndef MB_NEXUS_LATE_ONLY
static uint8_t raw_body[MB_NEXUS_INPUT_LIMIT_BYTES];
static char decoded_report[MB_NEXUS_OUTPUT_LIMIT_BYTES];
#endif

static int entropy(void *context, uint8_t output[32]) {
    (void)context;
    errno = 0;
    if (getentropy(output, 32) != 0 || errno != 0) {
        volatile uint8_t *bytes = output;
        for (size_t i = 0; i < 32; ++i) bytes[i] = 0;
        return -1;
    }
    return 0;
}

static int destination_allowed(curlsocktype purpose,
                              const struct curl_sockaddr *address) {
    if (!address || purpose != CURLSOCKTYPE_IPCXN ||
        address->family != AF_INET || address->addrlen < sizeof(struct sockaddr_in))
        return 0;
    const struct sockaddr_in *remote = (const struct sockaddr_in *)&address->addr;
    return remote->sin_family == AF_INET &&
           remote->sin_addr.s_addr == htonl(0x7f000001u) &&
           remote->sin_port == htons(443);
}

static curl_socket_t fixture_open_socket(void *opaque, curlsocktype purpose,
                                         struct curl_sockaddr *address) {
    struct shim_state *state = opaque;
    ++state->open_calls;
    if (!destination_allowed(purpose, address) || !state->fixture_port ||
        state->fixture_port > 65535u) {
        ++state->refused;
        return CURL_SOCKET_BAD;
    }

    struct sockaddr_in destination = *(struct sockaddr_in *)&address->addr;
    destination.sin_port = htons((uint16_t)state->fixture_port);
    curl_socket_t fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == CURL_SOCKET_BAD ||
        connect(fd, (const struct sockaddr *)&destination, sizeof(destination)) != 0) {
        if (fd != CURL_SOCKET_BAD) close(fd);
        ++state->refused;
        return CURL_SOCKET_BAD;
    }
    state->connected_fd = fd;
    ++state->connected;
    return fd;
}

static int fixture_sockopt(void *opaque, curl_socket_t fd, curlsocktype purpose) {
    struct shim_state *state = opaque;
    if (purpose != CURLSOCKTYPE_IPCXN || fd != state->connected_fd ||
        !state->connected)
        return CURL_SOCKOPT_ERROR;
    return CURL_SOCKOPT_ALREADY_CONNECTED;
}

/* This private link-time hook only replaces curl_download.c's call site.
 * The real curl_multi_add_handle remains linked and is called after the
 * socket callbacks are installed on the actual easy handle. */
CURLMcode fixture_curl_multi_add_handle(CURLM *multi, CURL *easy) {
    ++socket_shim.installs;
    if (curl_easy_setopt(easy, CURLOPT_OPENSOCKETFUNCTION, fixture_open_socket) != CURLE_OK ||
        curl_easy_setopt(easy, CURLOPT_OPENSOCKETDATA, &socket_shim) != CURLE_OK ||
        curl_easy_setopt(easy, CURLOPT_SOCKOPTFUNCTION, fixture_sockopt) != CURLE_OK ||
        curl_easy_setopt(easy, CURLOPT_SOCKOPTDATA, &socket_shim) != CURLE_OK)
        return CURLM_INTERNAL_ERROR;
    return curl_multi_add_handle(multi, easy);
}

static int all_zero(const void *buffer, size_t size) {
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < size; ++i)
        if (bytes[i] != 0) return 0;
    return 1;
}

#ifndef MB_NEXUS_LATE_ONLY
static int report_has(const char *text) {
    return strstr(decoded_report, text) != NULL;
}

static const char *expected_marker(int kind) {
    switch (kind) {
    case MB_NEXUS_GAMES: return "Fixture Game";
    case MB_NEXUS_MODS: return "Fixture Mod";
    case MB_NEXUS_MOD: return "Fixture Mod";
    case MB_NEXUS_FILES: return "Fixture File";
    case MB_NEXUS_OAUTH_USER: return "Fixture User";
    case MB_NEXUS_API_KEY_USER: return "Fixture User";
    default: return "";
    }
}

static int run_one(int kind, unsigned auth_mode, const char *credential,
                   size_t credential_bytes, const char *domain,
                   uint64_t mod_id, const uint8_t *ca_pem, size_t ca_bytes,
                   uint32_t timeout_ms, uint32_t connect_timeout_ms,
                   const char *label) {
    char resolve[96];
    const char *host = kind == MB_NEXUS_OAUTH_USER ? "users.nexusmods.com" : "api.nexusmods.com";
    const int resolve_size = snprintf(resolve, sizeof(resolve), "%s:443:127.0.0.1", host);
    if (resolve_size < 0 || (size_t)resolve_size >= sizeof(resolve)) return 2;

    memset(raw_body, 0xa5, sizeof(raw_body));
    memset(decoded_report, 0x52, sizeof(decoded_report));
    mb_nexus_client_params_v1 params = {0};
    params.struct_size = sizeof(params);
    params.abi_version = MB_NEXUS_CLIENT_ABI_V1;
    params.kind = kind;
    params.auth_mode = auth_mode;
    params.credential = credential;
    params.credential_bytes = credential_bytes;
    params.domain = domain;
    params.domain_bytes = domain ? strlen(domain) : 0;
    params.mod_id = mod_id;
    params.ca_pem = ca_pem;
    params.ca_bytes = ca_bytes;
    params.resolve = resolve;
    params.timeout_ms = timeout_ms;
    params.connect_timeout_ms = connect_timeout_ms;
    params.raw_body = raw_body;
    params.raw_capacity = sizeof(raw_body);
    params.report = decoded_report;
    params.report_capacity = sizeof(decoded_report);

    mb_nexus_client_result_v1 result = {0};
    result.struct_size = sizeof(result);
    result.abi_version = MB_NEXUS_CLIENT_ABI_V1;
    const int status = mb_nexus_client_request_v1(&params, &result);
    const int raw_wiped = all_zero(raw_body, sizeof(raw_body));
    const int has_marker = report_has(expected_marker(kind));
    printf("{\"case\":\"%s\",\"return\":%d,\"status\":%u,\"complete\":%u,"
           "\"decoded\":%u,\"http\":%u,\"curl\":%d,\"decode\":%d,"
           "\"received\":%llu,\"report_bytes\":%zu,\"raw_wiped\":%s,"
           "\"expected_marker\":%s}\n",
           label, status, (unsigned)result.status, result.complete,
           result.metadata_decoded, result.http_status, result.curl_code,
           result.decode_status, (unsigned long long)result.received_bytes,
           result.report_bytes, raw_wiped ? "true" : "false",
           has_marker ? "true" : "false");
    return status == MB_NEXUS_CLIENT_OK && result.complete == 1 &&
           result.metadata_decoded == 1 && result.http_status == 200 &&
           raw_wiped && has_marker ? 0 : 1;
}

static int request_metadata(int kind, unsigned auth_mode, const char *credential,
                            size_t credential_bytes, const char *domain, uint64_t mod_id,
                            const uint8_t *ca_pem, size_t ca_bytes,
                            size_t report_capacity, mb_nexus_client_result_v1 *result) {
    char resolve[96];
    const char *host = kind == MB_NEXUS_OAUTH_USER ? "users.nexusmods.com" : "api.nexusmods.com";
    const int resolve_size = snprintf(resolve, sizeof(resolve), "%s:443:127.0.0.1", host);
    if (resolve_size < 0 || (size_t)resolve_size >= sizeof(resolve)) return MB_NEXUS_CLIENT_BAD_ARGUMENT;
    memset(raw_body, 0xa5, sizeof(raw_body));
    memset(decoded_report, 0x52, sizeof(decoded_report));
    mb_nexus_client_params_v1 params = {0};
    params.struct_size = sizeof(params);
    params.abi_version = MB_NEXUS_CLIENT_ABI_V1;
    params.kind = kind;
    params.auth_mode = auth_mode;
    params.credential = credential;
    params.credential_bytes = credential_bytes;
    params.domain = domain;
    params.domain_bytes = domain ? strlen(domain) : 0;
    params.mod_id = mod_id;
    params.ca_pem = ca_pem;
    params.ca_bytes = ca_bytes;
    params.resolve = resolve;
    params.timeout_ms = 10000;
    params.connect_timeout_ms = 3000;
    params.raw_body = raw_body;
    params.raw_capacity = sizeof(raw_body);
    params.report = decoded_report;
    params.report_capacity = report_capacity;
    memset(result, 0, sizeof(*result));
    result->struct_size = sizeof(*result);
    result->abi_version = MB_NEXUS_CLIENT_ABI_V1;
    return mb_nexus_client_request_v1(&params, result);
}

static int run_short_report(const uint8_t *ca_pem, size_t ca_bytes) {
    static const char key[] = "fixture-only-api-key";
    mb_nexus_client_result_v1 result;
    const int status = request_metadata(MB_NEXUS_MOD, MB_NEXUS_CLIENT_API_KEY,
        key, sizeof(key) - 1, "newgame2028", 17, ca_pem, ca_bytes, 1, &result);
    const int raw_wiped = all_zero(raw_body, sizeof(raw_body));
    const int report_wiped = decoded_report[0] == 0;
    printf("{\"case\":\"short-report-after-decode\",\"return\":%d,\"status\":%u,"
           "\"complete\":%u,\"http\":%u,\"decode\":%d,\"required\":%zu,"
           "\"raw_wiped\":%s,\"report_wiped\":%s}\n",
           status, (unsigned)result.status, result.complete, result.http_status,
           result.decode_status, result.report_bytes, raw_wiped ? "true" : "false",
           report_wiped ? "true" : "false");
    return status == MB_NEXUS_CLIENT_OUTPUT_TOO_SMALL &&
           result.status == MB_NEXUS_CLIENT_OUTPUT_TOO_SMALL && !result.complete &&
           result.http_status == 200 && result.report_bytes > 1 && raw_wiped && report_wiped ? 0 : 1;
}

static int run_malformed_final(const uint8_t *ca_pem, size_t ca_bytes) {
    static const char bearer[] = "fixture-only-bearer";
    mb_nexus_client_result_v1 result;
    const int status = request_metadata(MB_NEXUS_MODS, MB_NEXUS_CLIENT_BEARER,
        bearer, sizeof(bearer) - 1, "newgame2028", 0, ca_pem, ca_bytes,
        sizeof(decoded_report), &result);
    const int raw_wiped = all_zero(raw_body, sizeof(raw_body));
    const int malformed_only = report_has("nexus_metadata_malformed") &&
                               !report_has("Fixture Mod") && !report_has("records");
    printf("{\"case\":\"malformed-final-record\",\"return\":%d,\"status\":%u,"
           "\"complete\":%u,\"metadata_decoded\":%u,\"http\":%u,"
           "\"raw_wiped\":%s,\"no_prefix_published\":%s}\n",
           status, (unsigned)result.status, result.complete, result.metadata_decoded,
           result.http_status, raw_wiped ? "true" : "false", malformed_only ? "true" : "false");
    return status == MB_NEXUS_CLIENT_OK && result.complete && !result.metadata_decoded &&
           result.http_status == 200 && raw_wiped && malformed_only ? 0 : 1;
}

static int run_http401(const uint8_t *ca_pem, size_t ca_bytes) {
    static const char key[] = "fixture-only-api-key";
    mb_nexus_client_result_v1 result;
    const int status = request_metadata(MB_NEXUS_MOD, MB_NEXUS_CLIENT_API_KEY,
        key, sizeof(key) - 1, "newgame2028", 17, ca_pem, ca_bytes,
        sizeof(decoded_report), &result);
    const int raw_wiped = all_zero(raw_body, sizeof(raw_body));
    const int report_wiped = all_zero(decoded_report, sizeof(decoded_report));
    printf("{\"case\":\"http-401\",\"return\":%d,\"status\":%u,"
           "\"complete\":%u,\"http\":%u,\"raw_wiped\":%s,\"report_wiped\":%s}\n",
           status, (unsigned)result.status, result.complete, result.http_status,
           raw_wiped ? "true" : "false", report_wiped ? "true" : "false");
    return status == MB_NEXUS_CLIENT_TRANSPORT_FAILED && !result.complete &&
           result.http_status == 401 && raw_wiped && report_wiped ? 0 : 1;
}

static int run_oversize_length(const uint8_t *ca_pem, size_t ca_bytes) {
    static const char bearer[] = "fixture-only-bearer";
    mb_nexus_client_result_v1 result;
    const int status = request_metadata(MB_NEXUS_GAMES, MB_NEXUS_CLIENT_BEARER,
        bearer, sizeof(bearer) - 1, NULL, 0, ca_pem, ca_bytes,
        sizeof(decoded_report), &result);
    const int raw_wiped = all_zero(raw_body, sizeof(raw_body));
    const int report_wiped = all_zero(decoded_report, sizeof(decoded_report));
    printf("{\"case\":\"body-length-over-limit\",\"return\":%d,\"status\":%u,"
           "\"complete\":%u,\"http\":%u,\"curl\":%d,\"received\":%llu,"
           "\"raw_wiped\":%s,\"report_wiped\":%s}\n",
           status, (unsigned)result.status, result.complete, result.http_status,
           result.curl_code, (unsigned long long)result.received_bytes,
           raw_wiped ? "true" : "false", report_wiped ? "true" : "false");
    return status == MB_NEXUS_CLIENT_TRANSPORT_FAILED && !result.complete &&
           result.http_status == 200 && raw_wiped && report_wiped ? 0 : 1;
}

static int sink_read_at(void *context, int fd, uint64_t offset,
                        uint8_t *output, uint32_t bytes, uint32_t *actual) {
    (void)context;
    if (offset > (uint64_t)INT64_MAX) return -1;
    const ssize_t count = pread(fd, output, bytes, (off_t)offset);
    if (count < 0) return -1;
    *actual = (uint32_t)count;
    return 0;
}

static int sink_write_at(void *context, int fd, uint64_t offset,
                         const uint8_t *input, uint32_t bytes, uint32_t *actual) {
    (void)context;
    if (offset > (uint64_t)INT64_MAX) return -1;
    const ssize_t count = pwrite(fd, input, bytes, (off_t)offset);
    if (count < 0) return -1;
    *actual = (uint32_t)count;
    return 0;
}

static int run_opaque_archive(const uint8_t *ca_pem, size_t ca_bytes) {
    static const uint8_t expected[] = {
        0x50, 0x4b, 0x03, 0x04, 0x6f, 0x70, 0x61, 0x71,
        0x75, 0x65, 0x00, 0xff, 0x01, 0x42
    };
    char path[] = "/tmp/modbridge-nexus-fixture-XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0 || unlink(path) != 0) {
        if (fd >= 0) close(fd);
        puts("{\"case\":\"opaque-archive\",\"setup_ok\":false}");
        return 1;
    }
    mb_curl_download_params_v1 params = {0};
    params.struct_size = sizeof(params);
    params.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    params.url = "https://api.nexusmods.com/owned-archive";
    params.ca_pem = ca_pem;
    params.ca_bytes = ca_bytes;
    params.resolve = "api.nexusmods.com:443:127.0.0.1";
    params.timeout_ms = 10000;
    params.connect_timeout_ms = 3000;
    params.sink.struct_size = sizeof(params.sink);
    params.sink.abi_version = MB_DOWNLOAD_FILE_SINK_ABI_V1;
    params.sink.fd = fd;
    params.sink.cap_bytes = sizeof(expected);
    params.sink.read_at = sink_read_at;
    params.sink.write_at = sink_write_at;
    mb_curl_download_result_v1 result = {0};
    result.struct_size = sizeof(result);
    result.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    const int status = mb_curl_download_v1(&params, &result);
    uint8_t recovered[sizeof(expected)];
    const ssize_t recovered_bytes = pread(fd, recovered, sizeof(recovered), 0);
    const int bytes_match = recovered_bytes == (ssize_t)sizeof(expected) &&
                            memcmp(recovered, expected, sizeof(expected)) == 0;
    printf("{\"case\":\"opaque-archive\",\"return\":%d,\"status\":%u,"
           "\"complete\":%u,\"http\":%u,\"received\":%llu,\"sink_complete\":%u,"
           "\"readback_verified\":%u,\"bytes_match\":%s}\n",
           status, (unsigned)result.status, result.complete, result.http_status,
           (unsigned long long)result.received_bytes, result.file.complete,
           result.file.readback_verified, bytes_match ? "true" : "false");
    close(fd);
    return status == MB_CURL_DOWNLOAD_OK && result.complete && result.http_status == 200 &&
           result.received_bytes == sizeof(expected) && result.file.complete &&
           result.file.readback_verified && bytes_match ? 0 : 1;
}
#endif

struct late_consumer_state { unsigned calls; uint32_t delay_ms; unsigned body_matches; };

static int late_consumer(void *opaque, const uint8_t *bytes, size_t size) {
    struct late_consumer_state *state = opaque;
    ++state->calls;
    if (!bytes || size != 2 || memcmp(bytes, "[]", 2) != 0) return 1;
    state->body_matches = 1;
    struct timespec delay = {(time_t)(state->delay_ms / 1000u),
                             (long)(state->delay_ms % 1000u) * 1000000L};
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
    return 1;
}

static int read_ca(const char *path, uint8_t **bytes, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0) { if (file) fclose(file); return -1; }
    long length = ftell(file);
    if (length <= 0 || (size_t)length > MB_NEXUS_CLIENT_CA_LIMIT_BYTES || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file); return -1;
    }
    uint8_t *result = malloc((size_t)length + 1u);
    if (!result) { fclose(file); return -1; }
    const int read_failed = fread(result, 1, (size_t)length, file) != (size_t)length;
    const int close_failed = fclose(file) != 0;
    if (read_failed || close_failed) {
        memset(result, 0, (size_t)length + 1u); free(result); return -1;
    }
    result[length] = 0;
    *bytes = result;
    *size = (size_t)length;
    return 0;
}

static int run_late_consumer(const char *resolve,
                             uint8_t *ca_pem, size_t ca_bytes) {
    static const char token[] = "fixture-only-bearer";
    struct late_consumer_state consumer = {0, 400, 0};
    uint8_t body[128];
    memset(body, 0x6b, sizeof(body));
    mb_curl_provider_transfer_params_v1 params = {0};
    params.struct_size = sizeof(params);
    params.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    params.url = "https://api.nexusmods.com/v1/games";
    params.auth_mode = MB_NEXUS_CLIENT_BEARER;
    params.credential = token;
    params.credential_bytes = sizeof(token) - 1;
    params.ca_pem = ca_pem;
    params.ca_bytes = ca_bytes;
    params.resolve = resolve;
    params.timeout_ms = 150;
    params.connect_timeout_ms = 100;
    params.body = body;
    params.body_capacity = sizeof(body);
    params.consume_context = &consumer;
    params.consume_complete_body = late_consumer;

    mb_curl_provider_transfer_result_v1 result = {0};
    result.struct_size = sizeof(result);
    result.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    const int status = mb_curl_provider_transfer_v1(&params, &result);
    printf("{\"case\":\"late-consumer-expiry\",\"return\":%d,\"status\":%u,"
           "\"complete\":%u,\"http\":%u,\"curl\":%d,\"received\":%llu,\"consumer_calls\":%u,"
           "\"body_matches\":%s,\"body_wiped\":%s}\n",
           status, (unsigned)result.status, result.complete, result.http_status, result.curl_code,
           (unsigned long long)result.received_bytes, consumer.calls,
           consumer.body_matches ? "true" : "false",
           all_zero(body, sizeof(body)) ? "true" : "false");
#if defined(MB_EXPECT_TIMEOUT_PRIORITY) && MB_EXPECT_TIMEOUT_PRIORITY
    const int expected_status = MB_CURL_DOWNLOAD_TIMEOUT;
#else
    const int expected_status = MB_CURL_DOWNLOAD_CONSUMER_FAILED;
#endif
    return status == expected_status && result.status == expected_status &&
           !result.complete && result.http_status == 200 && consumer.calls == 1 && consumer.body_matches &&
           all_zero(body, sizeof(body)) ? 0 : 1;
}

#ifndef MB_NEXUS_LATE_ONLY
static int run_shim_checks(void) {
    struct curl_sockaddr test = {0};
    struct sockaddr_in *address = (struct sockaddr_in *)&test.addr;
    test.family = AF_INET;
    test.socktype = SOCK_STREAM;
    test.protocol = IPPROTO_TCP;
    test.addrlen = sizeof(*address);
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = htonl(0x7f000001u);
    address->sin_port = htons(443);
    const int accepts_target = destination_allowed(CURLSOCKTYPE_IPCXN, &test);
    address->sin_port = htons(444);
    const int rejects_other_port = !destination_allowed(CURLSOCKTYPE_IPCXN, &test);
    address->sin_port = htons(443);
    address->sin_addr.s_addr = htonl(0x08080808u);
    const int rejects_non_loopback = !destination_allowed(CURLSOCKTYPE_IPCXN, &test);
    const int rejects_wrong_purpose = !destination_allowed(CURLSOCKTYPE_ACCEPT, &test);
    printf("{\"case\":\"socket-shim-controls\",\"accepts_loopback443\":%s,"
           "\"rejects_port444\":%s,\"rejects_non_loopback\":%s,"
           "\"rejects_wrong_purpose\":%s}\n",
           accepts_target ? "true" : "false", rejects_other_port ? "true" : "false",
           rejects_non_loopback ? "true" : "false", rejects_wrong_purpose ? "true" : "false");
    return accepts_target && rejects_other_port && rejects_non_loopback && rejects_wrong_purpose ? 0 : 1;
}
#endif

int main(int argc, char **argv) {
#ifdef MB_NEXUS_LATE_ONLY
    if (argc != 4 || strcmp(argv[1], "late-consumer") != 0) return 2;
#else
    if (argc == 2 && strcmp(argv[1], "--shim-check") == 0)
        return run_shim_checks();
    if (argc != 4 || (strcmp(argv[1], "suite") && strcmp(argv[1], "typed-six") &&
                      strcmp(argv[1], "late-consumer"))) return 2;
#endif
    char *end = NULL;
    unsigned long port = strtoul(argv[2], &end, 10);
    if (!end || *end || port == 0 || port > 65535u) return 2;
    socket_shim.fixture_port = (unsigned)port;
    uint8_t *ca_pem = NULL;
    size_t ca_bytes = 0;
    if (read_ca(argv[3], &ca_pem, &ca_bytes)) return 2;

    mb_curl_runtime_params_v1 start = {0};
    start.struct_size = sizeof(start);
    start.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    start.entropy_read32 = entropy;
    mb_curl_runtime_result_v1 runtime = {0};
    runtime.struct_size = sizeof(runtime);
    runtime.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    const int initialized = mb_curl_runtime_start_v1(&start, &runtime);
    if (initialized != MB_CURL_DOWNLOAD_OK || !runtime.initialized) {
        printf("{\"case\":\"runtime-start\",\"status\":%d,\"initialized\":%u}\n",
               initialized, runtime.initialized);
        free(ca_pem);
        return 3;
    }
    printf("{\"case\":\"runtime-start\",\"status\":%d,\"initialized\":%u}\n",
           initialized, runtime.initialized);

    int failed = 0;
#ifdef MB_NEXUS_LATE_ONLY
    failed = run_late_consumer("api.nexusmods.com:443:127.0.0.1", ca_pem, ca_bytes);
    printf("{\"case\":\"socket-shim\",\"installs\":%u,\"opens\":%u,"
           "\"connected\":%u,\"refused\":%u}\n",
           socket_shim.installs, socket_shim.open_calls, socket_shim.connected,
           socket_shim.refused);
    if (socket_shim.installs != 1 || socket_shim.open_calls != 1 ||
        socket_shim.connected != 1 || socket_shim.refused != 0)
        failed = 1;
#else
    if (!strcmp(argv[1], "late-consumer")) {
        failed = run_late_consumer("api.nexusmods.com:443:127.0.0.1", ca_pem, ca_bytes);
    } else {
        static const char api_key[] = "fixture-only-api-key";
        static const char bearer[] = "fixture-only-bearer";
        const struct { int kind; unsigned auth; const char *credential; size_t credential_bytes; const char *domain; uint64_t mod_id; const char *name; } cases[] = {
            {MB_NEXUS_GAMES, MB_NEXUS_CLIENT_API_KEY, api_key, sizeof(api_key)-1, NULL, 0, "games"},
            {MB_NEXUS_MODS, MB_NEXUS_CLIENT_BEARER, bearer, sizeof(bearer)-1, "newgame2028", 0, "mods"},
            {MB_NEXUS_MOD, MB_NEXUS_CLIENT_API_KEY, api_key, sizeof(api_key)-1, "newgame2028", 17, "mod"},
            {MB_NEXUS_FILES, MB_NEXUS_CLIENT_BEARER, bearer, sizeof(bearer)-1, "newgame2028", 17, "files"},
            {MB_NEXUS_OAUTH_USER, MB_NEXUS_CLIENT_BEARER, bearer, sizeof(bearer)-1, NULL, 0, "oauth-user"},
            {MB_NEXUS_API_KEY_USER, MB_NEXUS_CLIENT_API_KEY, api_key, sizeof(api_key)-1, NULL, 0, "api-key-user"}
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
            failed |= run_one(cases[i].kind, cases[i].auth, cases[i].credential,
                              cases[i].credential_bytes, cases[i].domain,
                              cases[i].mod_id, ca_pem, ca_bytes, 10000, 3000,
                              cases[i].name);
        unsigned expected_calls = 6;
        if (!strcmp(argv[1], "suite")) {
            failed |= run_one(MB_NEXUS_MOD, MB_NEXUS_CLIENT_API_KEY,
                              api_key, sizeof(api_key) - 1, "othergame_2049",
                              UINT64_C(9223372036854775807), ca_pem, ca_bytes,
                              10000, 3000, "other-domain-max-id");
            failed |= run_short_report(ca_pem, ca_bytes);
            failed |= run_malformed_final(ca_pem, ca_bytes);
            failed |= run_http401(ca_pem, ca_bytes);
            failed |= run_oversize_length(ca_pem, ca_bytes);
            failed |= run_one(MB_NEXUS_GAMES, MB_NEXUS_CLIENT_API_KEY,
                              api_key, sizeof(api_key) - 1, NULL, 0, ca_pem, ca_bytes,
                              10000, 3000, "metadata-before-archive");
            failed |= run_opaque_archive(ca_pem, ca_bytes);
            failed |= run_one(MB_NEXUS_GAMES, MB_NEXUS_CLIENT_API_KEY,
                              api_key, sizeof(api_key) - 1, NULL, 0, ca_pem, ca_bytes,
                              10000, 3000, "final-metadata-after-archive");
            expected_calls = 14;
        }
        printf("{\"case\":\"socket-shim\",\"installs\":%u,\"opens\":%u,"
               "\"connected\":%u,\"refused\":%u,\"targets_loopback443\":%s}\n",
               socket_shim.installs, socket_shim.open_calls, socket_shim.connected,
               socket_shim.refused,
               socket_shim.open_calls == expected_calls && socket_shim.connected == expected_calls &&
               socket_shim.refused == 0 ? "true" : "false");
        if (socket_shim.installs != expected_calls || socket_shim.open_calls != expected_calls ||
            socket_shim.connected != expected_calls || socket_shim.refused != 0)
            failed = 1;
    }
#endif

    const int close_status = mb_curl_runtime_close_v1(&runtime);
    printf("{\"case\":\"runtime-close\",\"status\":%d,\"initialized\":%u,"
           "\"terminal\":%u,\"windows\":%llu,\"entropy_callbacks\":%llu}\n",
           close_status, runtime.initialized, runtime.terminal,
           (unsigned long long)runtime.entropy_windows,
           (unsigned long long)runtime.entropy_callbacks);
#ifdef MB_NEXUS_LATE_ONLY
    const uint64_t minimum_windows = 3;
#else
    const uint64_t minimum_windows = !strcmp(argv[1], "suite") ? 16 : 8;
#endif
    if (close_status != MB_CURL_DOWNLOAD_OK || runtime.initialized || runtime.terminal ||
        runtime.entropy_windows < minimum_windows || runtime.entropy_callbacks < minimum_windows)
        failed = 1;
    memset(ca_pem, 0, ca_bytes + 1u);
    free(ca_pem);
    return failed ? 1 : 0;
}
