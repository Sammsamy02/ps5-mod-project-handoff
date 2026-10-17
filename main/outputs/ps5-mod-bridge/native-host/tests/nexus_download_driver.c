#define _POSIX_C_SOURCE 200809L
#include "../nexus_download.h"
#include "../download/curl_download.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define RAW_CAPACITY MB_NEXUS_DOWNLOAD_RAW_LIMIT_BYTES
#define REPORT_CAPACITY 65536u
#define URI_CAPACITY MB_NEXUS_DOWNLOAD_URI_LIMIT_BYTES

static const char archive_uri[] =
    "https://owned-archive.fixture.test/signed/archive.zip?sig=fixture-only-signature";
static const unsigned char archive_body[] = {
    0x50, 0x4b, 0x03, 0x04, 'o', 'w', 'n', 'e', 'd', '-', 'a', 'r', 'c', 'h', 'i', 'v', 'e', 0, 0xff
};

struct socket_shim_state {
    unsigned fixture_port, installs, opens, connected, refused;
    curl_socket_t connected_fd;
};
static struct socket_shim_state socket_shim;

static int entropy(void *context, uint8_t output[32]) {
    (void)context;
    errno = 0;
    if (getentropy(output, 32) == 0 && errno == 0) return 0;
    volatile uint8_t *bytes = output;
    for (size_t i = 0; i < 32; ++i) bytes[i] = 0;
    return -1;
}

static int destination_allowed(curlsocktype purpose, const struct curl_sockaddr *address) {
    if (!address || purpose != CURLSOCKTYPE_IPCXN || address->family != AF_INET ||
        address->addrlen < sizeof(struct sockaddr_in)) return 0;
    const struct sockaddr_in *remote = (const struct sockaddr_in *)&address->addr;
    return remote->sin_family == AF_INET &&
           remote->sin_addr.s_addr == htonl(0x7f000001u) &&
           remote->sin_port == htons(443);
}

static curl_socket_t fixture_open_socket(void *opaque, curlsocktype purpose,
                                         struct curl_sockaddr *address) {
    struct socket_shim_state *state = opaque;
    ++state->opens;
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
    struct socket_shim_state *state = opaque;
    if (purpose != CURLSOCKTYPE_IPCXN || fd != state->connected_fd || !state->connected)
        return CURL_SOCKOPT_ERROR;
    return CURL_SOCKOPT_ALREADY_CONNECTED;
}

/* Test-only call-site hook. The real curl multi API still adds the actual easy handle. */
CURLMcode fixture_curl_multi_add_handle(CURLM *multi, CURL *easy) {
    ++socket_shim.installs;
    if (curl_easy_setopt(easy, CURLOPT_OPENSOCKETFUNCTION, fixture_open_socket) != CURLE_OK ||
        curl_easy_setopt(easy, CURLOPT_OPENSOCKETDATA, &socket_shim) != CURLE_OK ||
        curl_easy_setopt(easy, CURLOPT_SOCKOPTFUNCTION, fixture_sockopt) != CURLE_OK ||
        curl_easy_setopt(easy, CURLOPT_SOCKOPTDATA, &socket_shim) != CURLE_OK)
        return CURLM_INTERNAL_ERROR;
    return curl_multi_add_handle(multi, easy);
}

static int all_zero(const void *memory, size_t size) {
    const uint8_t *bytes = memory;
    for (size_t i = 0; i < size; ++i) if (bytes[i] != 0) return 0;
    return 1;
}

static int run_shim_controls(void) {
    struct curl_sockaddr address = {0};
    address.family = AF_INET;
    address.addrlen = sizeof(struct sockaddr_in);
    address.socktype = SOCK_STREAM;
    address.protocol = IPPROTO_TCP;
    struct sockaddr_in fixture_address = {0};
    fixture_address.sin_family = AF_INET;
    fixture_address.sin_addr.s_addr = htonl(0x7f000001u);
    fixture_address.sin_port = htons(443);
    memcpy(&address.addr, &fixture_address, sizeof(fixture_address));
    const int accepts_loopback_443 = destination_allowed(CURLSOCKTYPE_IPCXN, &address);
    ((struct sockaddr_in *)&address.addr)->sin_port = htons(444);
    const int rejects_other_port = !destination_allowed(CURLSOCKTYPE_IPCXN, &address);
    ((struct sockaddr_in *)&address.addr)->sin_port = htons(443);
    ((struct sockaddr_in *)&address.addr)->sin_addr.s_addr = htonl(0x08080808u);
    const int rejects_non_loopback = !destination_allowed(CURLSOCKTYPE_IPCXN, &address);
    ((struct sockaddr_in *)&address.addr)->sin_addr.s_addr = htonl(0x7f000001u);
    const int rejects_wrong_purpose = !destination_allowed(CURLSOCKTYPE_ACCEPT, &address);
    printf("{\"case\":\"socket-shim-controls\",\"accepts_loopback_443\":%s,"
           "\"rejects_other_port\":%s,\"rejects_non_loopback\":%s,"
           "\"rejects_wrong_purpose\":%s}\n",
           accepts_loopback_443 ? "true" : "false", rejects_other_port ? "true" : "false",
           rejects_non_loopback ? "true" : "false", rejects_wrong_purpose ? "true" : "false");
    return accepts_loopback_443 && rejects_other_port && rejects_non_loopback && rejects_wrong_purpose;
}

static int read_ca(const char *path, uint8_t **bytes, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0) { if (file) fclose(file); return -1; }
    long length = ftell(file);
    if (length <= 0 || (size_t)length > MB_NEXUS_DOWNLOAD_CA_LIMIT_BYTES ||
        fseek(file, 0, SEEK_SET) != 0) { fclose(file); return -1; }
    uint8_t *buffer = malloc((size_t)length + 1u);
    if (!buffer) { fclose(file); return -1; }
    const int read_failed = fread(buffer, 1, (size_t)length, file) != (size_t)length;
    const int close_failed = fclose(file) != 0;
    if (read_failed || close_failed) {
        memset(buffer, 0, (size_t)length + 1u);
        free(buffer);
        return -1;
    }
    buffer[length] = 0;
    *bytes = buffer;
    *size = (size_t)length;
    return 0;
}

static int read_at(void *opaque, int fd, uint64_t offset, uint8_t *output,
                   uint32_t wanted, uint32_t *actual) {
    (void)opaque;
    size_t done = 0;
    while (done < wanted) {
        ssize_t count = pread(fd, output + done, wanted - done, (off_t)(offset + done));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) { *actual = (uint32_t)done; return -1; }
        if (count == 0) break;
        done += (size_t)count;
    }
    *actual = (uint32_t)done;
    return 0;
}

static int write_at(void *opaque, int fd, uint64_t offset, const uint8_t *input,
                    uint32_t wanted, uint32_t *actual) {
    (void)opaque;
    size_t done = 0;
    while (done < wanted) {
        ssize_t count = pwrite(fd, input + done, wanted - done, (off_t)(offset + done));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { *actual = (uint32_t)done; return -1; }
        done += (size_t)count;
    }
    *actual = (uint32_t)done;
    return 0;
}

static int transfer_archive(const char *uri, const uint8_t *ca, size_t ca_bytes,
                            const char *directory) {
    char path[4096];
    if (snprintf(path, sizeof(path), "%s/archive-transfer.bin", directory) < 0 ||
        strlen(path) >= sizeof(path)) return 0;
    int fd = open(path, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd < 0) return 0;
    struct mb_download_file_sink_params_v1 sink = {0};
    sink.struct_size = sizeof(sink);
    sink.abi_version = MB_DOWNLOAD_FILE_SINK_ABI_V1;
    sink.fd = fd;
    sink.cap_bytes = 1024u * 1024u;
    sink.read_at = read_at;
    sink.write_at = write_at;

    mb_curl_download_params_v1 params = {0};
    params.struct_size = sizeof(params);
    params.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    params.url = uri;
    params.ca_pem = ca;
    params.ca_bytes = ca_bytes;
    params.resolve = "owned-archive.fixture.test:443:127.0.0.1";
    params.timeout_ms = 10000;
    params.connect_timeout_ms = 5000;
    params.sink = sink;
    mb_curl_download_result_v1 result = {0};
    result.struct_size = sizeof(result);
    result.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    int rc = mb_curl_download_v1(&params, &result);
    uint8_t observed[sizeof(archive_body)];
    ssize_t count = pread(fd, observed, sizeof(observed), 0);
    struct stat info;
    int stat_ok = fstat(fd, &info) == 0;
    int close_ok = close(fd) == 0;
    int bytes_match = count == (ssize_t)sizeof(archive_body) &&
                      memcmp(observed, archive_body, sizeof(archive_body)) == 0;
    printf("{\"case\":\"archive-transfer\",\"return\":%d,\"status\":%u,"
           "\"complete\":%u,\"http\":%u,\"received\":%llu,\"file_complete\":%u,"
           "\"readback_verified\":%u,\"bytes_match\":%s,\"file_size\":%lld,"
           "\"stat_ok\":%s,\"close_ok\":%s}\n",
           rc, (unsigned)result.status, result.complete, result.http_status,
           (unsigned long long)result.received_bytes, result.file.complete,
           result.file.readback_verified, bytes_match ? "true" : "false",
           stat_ok ? (long long)info.st_size : -1LL, stat_ok ? "true" : "false",
           close_ok ? "true" : "false");
    return rc == MB_CURL_DOWNLOAD_OK && result.complete == 1 &&
           result.file.complete == 1 && result.file.readback_verified == 1 &&
           result.received_bytes == sizeof(archive_body) && bytes_match &&
           stat_ok && (uint64_t)info.st_size == sizeof(archive_body) && close_ok;
}

static int resolve_case(const char *label, const char *domain, const char *credential,
                        uint64_t user_id, int grant_kind,
                        uint32_t timeout_ms, const uint8_t *ca, size_t ca_bytes,
                        const char *directory, int stream_on_success) {
    uint8_t *raw = malloc(RAW_CAPACITY);
    char *report = malloc(REPORT_CAPACITY);
    char *uri = malloc(URI_CAPACITY);
    if (!raw || !report || !uri) { free(raw); free(report); free(uri); return 0; }
    memset(raw, 0xa5, RAW_CAPACITY);
    memset(report, 0x5a, REPORT_CAPACITY);
    memset(uri, 0xa5, URI_CAPACITY);

    char main_domain[MB_NEXUS_DOWNLOAD_DOMAIN_LIMIT_BYTES + 1u];
    char grant_domain[MB_NEXUS_DOWNLOAD_DOMAIN_LIMIT_BYTES + 1u];
    const char key[] = "fixture+key&/v1";
    if (snprintf(main_domain, sizeof(main_domain), "%s", domain) < 0 ||
        snprintf(grant_domain, sizeof(grant_domain), "%s", domain) < 0) {
        free(raw); free(report); free(uri); return 0;
    }
    mb_nexus_download_grant_v1 grant = {0};
    if (grant_kind) {
        grant.struct_size = sizeof(grant);
        grant.abi_version = MB_NEXUS_DOWNLOAD_ABI_V1;
        grant.domain = grant_domain;
        grant.domain_bytes = strlen(grant_domain);
        grant.mod_id = 17;
        grant.file_id = grant_kind == 3 ? 4 : 3;
        grant.user_id = grant_kind == 2 ? user_id + 1u : user_id;
        const time_t now = time(NULL);
        grant.expires = (uint64_t)(now > 0 ? now : 1);
        if (grant_kind == 4) grant.expires = grant.expires > 1 ? grant.expires - 1u : 1u;
        else grant.expires += 3600u;
        grant.key = key;
        grant.key_bytes = sizeof(key) - 1u;
    }

    mb_nexus_download_params_v1 params = {0};
    params.struct_size = sizeof(params);
    params.abi_version = MB_NEXUS_DOWNLOAD_ABI_V1;
    params.auth_mode = MB_NEXUS_DOWNLOAD_API_KEY;
    params.credential = credential;
    params.credential_bytes = strlen(credential);
    params.domain = main_domain;
    params.domain_bytes = strlen(main_domain);
    params.mod_id = 17;
    params.file_id = 3;
    params.grant = grant_kind ? &grant : NULL;
    params.ca_pem = ca;
    params.ca_bytes = ca_bytes;
    params.api_resolve = "api.nexusmods.com:443:127.0.0.1";
    params.users_resolve = "users.nexusmods.com:443:127.0.0.1";
    params.timeout_ms = timeout_ms;
    params.connect_timeout_ms = timeout_ms < 5000u ? timeout_ms : 5000u;
    params.raw_body = raw;
    params.raw_capacity = RAW_CAPACITY;
    params.metadata_report = report;
    params.report_capacity = REPORT_CAPACITY;
    params.uri = uri;
    params.uri_capacity = URI_CAPACITY;

    mb_nexus_download_result_v1 result = {0};
    result.struct_size = sizeof(result);
    result.abi_version = MB_NEXUS_DOWNLOAD_ABI_V1;
    int rc = mb_nexus_resolve_download_v1(&params, &result);
    int scratch_wiped = all_zero(raw, RAW_CAPACITY) && all_zero(report, REPORT_CAPACITY);
    int uri_wiped = all_zero(uri, URI_CAPACITY);
    int uri_matches = result.complete == 1 && result.uri_bytes == sizeof(archive_uri) - 1u &&
                      memcmp(uri, archive_uri, sizeof(archive_uri)) == 0;
    printf("{\"case\":\"%s\",\"return\":%d,\"status\":%u,\"stage\":%u,"
           "\"complete\":%u,\"is_premium\":%u,\"grant_used\":%u,\"adult_content\":%d,"
           "\"user_id\":%llu,\"mod_id\":%llu,\"file_id\":%llu,\"uri_bytes\":%zu,"
           "\"scratch_wiped\":%s,\"uri_wiped\":%s,\"uri_matches\":%s}\n",
           label, rc, (unsigned)result.status, (unsigned)result.stage, result.complete,
           result.is_premium, result.grant_used, result.adult_content,
           (unsigned long long)result.user_id, (unsigned long long)result.mod_id,
           (unsigned long long)result.file_id, result.uri_bytes,
           scratch_wiped ? "true" : "false", uri_wiped ? "true" : "false",
           uri_matches ? "true" : "false");

    int stream_ok = 1;
    if (stream_on_success && uri_matches)
        stream_ok = transfer_archive(uri, ca, ca_bytes, directory);
    memset(uri, 0, URI_CAPACITY);
    memset(raw, 0, RAW_CAPACITY);
    memset(report, 0, REPORT_CAPACITY);
    free(uri); free(report); free(raw);
    return stream_ok;
}

static int load_port(const char *text, unsigned *port) {
    char *end = NULL;
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    if (errno || !end || *end || value == 0 || value > 65535u) return 0;
    *port = (unsigned)value;
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 4) return 2;
    unsigned fixture_port = 0;
    if (!load_port(argv[1], &fixture_port)) return 2;
    socket_shim.fixture_port = fixture_port;
    uint8_t *ca = NULL;
    size_t ca_bytes = 0;
    if (read_ca(argv[2], &ca, &ca_bytes) != 0) return 2;

    mb_curl_runtime_params_v1 start = {0};
    start.struct_size = sizeof(start);
    start.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    start.entropy_read32 = entropy;
    mb_curl_runtime_result_v1 runtime = {0};
    runtime.struct_size = sizeof(runtime);
    runtime.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    int runtime_start = mb_curl_runtime_start_v1(&start, &runtime);
    printf("{\"case\":\"runtime-start\",\"return\":%d,\"initialized\":%u,"
           "\"terminal\":%u}\n", runtime_start, runtime.initialized, runtime.terminal);
    int ok = runtime_start == MB_CURL_DOWNLOAD_OK && runtime.initialized == 1;
    if (ok) {
        ok &= run_shim_controls();
        ok &= resolve_case("premium", "fixture-premium", "fixture-only-premium-key",
            1101, 0, 15000, ca, ca_bytes, argv[3], 1);
        ok &= resolve_case("website-grant", "fixture-grant", "fixture-only-grant-key",
            2202, 1, 15000, ca, ca_bytes, argv[3], 0);
        ok &= resolve_case("nonpremium-no-grant", "fixture-no-grant", "fixture-only-free-key",
            3303, 0, 15000, ca, ca_bytes, argv[3], 0);
        ok &= resolve_case("wrong-account-grant", "fixture-wrong-account", "fixture-only-account-key",
            4404, 2, 15000, ca, ca_bytes, argv[3], 0);
        ok &= resolve_case("wrong-source-grant", "fixture-wrong-source", "fixture-only-source-key",
            5505, 3, 15000, ca, ca_bytes, argv[3], 0);
        ok &= resolve_case("expired-grant", "fixture-expired", "fixture-only-expired-key",
            6606, 4, 15000, ca, ca_bytes, argv[3], 0);
        ok &= resolve_case("hidden-mod", "fixture-hidden", "fixture-only-hidden-key",
            7707, 0, 15000, ca, ca_bytes, argv[3], 0);
        ok &= resolve_case("unknown-adult", "fixture-adult", "fixture-only-adult-key",
            8808, 0, 15000, ca, ca_bytes, argv[3], 0);
        ok &= resolve_case("malformed-final-mirror", "fixture-mirror", "fixture-only-mirror-key",
            9909, 0, 15000, ca, ca_bytes, argv[3], 0);
        ok &= resolve_case("bad-http", "fixture-http", "fixture-only-http-key",
            1010, 0, 15000, ca, ca_bytes, argv[3], 0);
        ok &= resolve_case("total-deadline", "fixture-timeout", "fixture-only-timeout-key",
            1111, 0, 2500, ca, ca_bytes, argv[3], 0);
        ok &= resolve_case("healthy-after-failures", "fixture-healthy", "fixture-only-healthy-key",
            1212, 0, 15000, ca, ca_bytes, argv[3], 0);
    }
    mb_curl_runtime_result_v1 final = {0};
    final.struct_size = sizeof(final);
    final.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    int status_rc = mb_curl_runtime_status_v1(&final);
    int runtime_close = mb_curl_runtime_close_v1(&final);
    printf("{\"case\":\"runtime-close\",\"status_return\":%d,\"close_return\":%d,"
           "\"initialized\":%u,\"terminal\":%u,\"window_active\":%u,"
           "\"windows\":%llu,\"callbacks\":%llu,\"socket_installs\":%u,"
           "\"socket_opens\":%u,\"socket_connected\":%u,\"socket_refused\":%u}\n",
           status_rc, runtime_close, final.initialized, final.terminal, final.window_active,
           (unsigned long long)final.entropy_windows, (unsigned long long)final.entropy_callbacks,
           socket_shim.installs, socket_shim.opens, socket_shim.connected, socket_shim.refused);
    ok &= runtime_close == MB_CURL_DOWNLOAD_OK && final.initialized == 0 &&
          final.terminal == 0 && final.window_active == 0 && socket_shim.refused == 0;
    memset(ca, 0, ca_bytes + 1u);
    free(ca);
    return ok ? 0 : 1;
}
