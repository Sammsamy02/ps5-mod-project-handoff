#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1
#include "http.h"
#ifndef HOST_NATIVE
#define HOST_NATIVE 0
#endif
#ifndef MB_HOST_DOWNLOAD
#define MB_HOST_DOWNLOAD 0
#endif
#ifndef MB_HOST_JOBS
#define MB_HOST_JOBS 0
#endif
#ifndef MB_HOST_PAK_PREPARATION
#define MB_HOST_PAK_PREPARATION 0
#endif
#ifndef MB_HOST_PAK_PROFILE
#define MB_HOST_PAK_PROFILE MB_HOST_PAK_PREPARATION
#endif
#ifndef MB_HOST_IOSTORE_PROFILE
#define MB_HOST_IOSTORE_PROFILE 0
#endif
#if MB_HOST_DOWNLOAD
#include "download/curl_download.h"
#include "auth.h"
#if MB_HOST_JOBS
#include "jobs.h"
#include <sys/stat.h>
#endif
#if HOST_NATIVE
#include "download/ps5_entropy.h"
#else
#include <sys/random.h>
#endif
#endif
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#ifndef MB_HOST_OAUTH_CONFIGURED
#define MB_HOST_OAUTH_CONFIGURED 0
#endif
#ifndef HOST_PROFILE
#define HOST_PROFILE "desktop_fixture"
#endif
#ifndef HOST_ADDRESS
#define HOST_ADDRESS "127.0.0.1"
#endif
#ifndef HOST_LIFETIME
#define HOST_LIFETIME 0
#endif
#ifndef BUILD_ID
#error Build through build.py or provide an explicit source identity
#endif

#if MB_HOST_DOWNLOAD
static mb_nexus_auth_context_v1 *host_auth;
static int download_runtime_start_attempted;
static int download_runtime_close_attempted;
#if MB_HOST_JOBS
static mb_nexus_jobs_context_v1 *host_jobs;
static int host_scratch_directory = -1;
#if MB_HOST_PAK_PROFILE
static int host_pak_base = -1;
#endif
#if MB_HOST_IOSTORE_PROFILE
static int host_iostore_base_toc = -1;
static int host_iostore_base_header = -1;
#endif
#endif

static void clear_entropy32(uint8_t output[32]) {
    volatile uint8_t *owned = output;
    for (size_t i = 0; i < 32; ++i) owned[i] = 0;
}

static int host_entropy_read32(void *context, uint8_t output[32]) {
    if (!output) return -1;
    if (context) { clear_entropy32(output); return -1; }
#if HOST_NATIVE
    return mb_curl_ps5_entropy_read32_v1(NULL, output);
#else
    errno = 0;
    int rc = getentropy(output, 32);
    int api_errno = errno;
    if (rc == 0 && api_errno == 0) return 0;
    clear_entropy32(output);
    return -1;
#endif
}
#endif

static int finish(int status) {
#if MB_HOST_DOWNLOAD
#if MB_HOST_JOBS
    if (mb_nexus_jobs_free_v1(&host_jobs) != MB_NEXUS_JOBS_OK) status = 5;
    if (!host_jobs && host_scratch_directory >= 0) {
        if (close(host_scratch_directory) && status == 0) status = 5;
        host_scratch_directory = -1;
    }
#if MB_HOST_PAK_PROFILE
    if (!host_jobs && host_pak_base >= 0) {
        if (close(host_pak_base) && status == 0) status = 5;
        host_pak_base = -1;
    }
#endif
#if MB_HOST_IOSTORE_PROFILE
    if (!host_jobs && host_iostore_base_toc >= 0) {
        if (close(host_iostore_base_toc) && status == 0) status = 5;
        host_iostore_base_toc = -1;
    }
    if (!host_jobs && host_iostore_base_header >= 0) {
        if (close(host_iostore_base_header) && status == 0) status = 5;
        host_iostore_base_header = -1;
    }
#endif
    /* A refused cleanup retains a job owner and its borrowed dependencies.
     * Exit with failure instead of destroying those dependencies underneath it. */
    if (!host_jobs) {
#endif
    if (mb_nexus_auth_free_v1(&host_auth) != MB_NEXUS_AUTH_OK && status == 0) status = 5;
    if (download_runtime_start_attempted && !download_runtime_close_attempted) {
        download_runtime_close_attempted = 1;
        mb_curl_runtime_result_v1 result = {
            .struct_size = sizeof(result), .abi_version = MB_CURL_DOWNLOAD_ABI_V1
        };
        if (mb_curl_runtime_close_v1(&result) != MB_CURL_DOWNLOAD_OK && status == 0)
            status = 5;
    }
#if MB_HOST_JOBS
    }
#endif
#endif
#if HOST_NATIVE
    _exit(status);
    __builtin_trap();
#else
    return status;
#endif
}
static long long milliseconds(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value)) return -1;
    if (value.tv_sec < 0 || value.tv_sec > 9000000000LL || value.tv_nsec < 0 || value.tv_nsec >= 1000000000L) return -1;
    return (long long)value.tv_sec * 1000 + value.tv_nsec / 1000000;
}
static int nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return flags < 0 ? -1 : fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
static int wait_ready(int fd, short events, long long deadline) {
    for (;;) {
        long long now = milliseconds();
        if (now < 0) return -1;
        long long remaining = deadline - now;
        if (remaining <= 0) return 0;
        struct pollfd item = {fd, events, 0};
        int rc = poll(&item, 1, (int)remaining);
        if (rc < 0 && errno == EINTR) continue;
        if (rc <= 0) return rc;
        if (item.revents & events) return 1;
        if (item.revents & (POLLERR | POLLHUP | POLLNVAL)) return -1;
    }
}
static int send_bytes(int fd, const char *buffer, size_t size, long long deadline) {
    size_t position = 0;
    while (position < size) {
        if (wait_ready(fd, POLLOUT, deadline) != 1) return -1;
#ifdef MSG_NOSIGNAL
        ssize_t n = send(fd, buffer + position, size - position, MSG_NOSIGNAL);
#else
        ssize_t n = send(fd, buffer + position, size - position, 0);
#endif
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (n <= 0 || (size_t)n > size - position) return -1;
        position += (size_t)n;
    }
    return 0;
}
static void wipe_host(void *value, size_t size) {
    volatile unsigned char *p = value;
    while (size--) *p++ = 0;
}
static int serve_peer(int peer, const struct host_info *info, long long deadline, long long expiry) {
    unsigned char input[HTTP_HEADER_LIMIT + HTTP_JOB_BODY_LIMIT];
    size_t used = 0;
    struct http_response out;
    http_error(&out, 400);
    if (nonblocking(peer)) return 0;
#ifdef SO_NOSIGPIPE
    int no_signal = 1;
    if (setsockopt(peer, SOL_SOCKET, SO_NOSIGPIPE, &no_signal, sizeof(no_signal))) return 0;
#elif !defined(MSG_NOSIGNAL)
#error A checked socket signal-suppression interface is required
#endif
    for (;;) {
        int ready = wait_ready(peer, POLLIN, deadline - 100);
        if (!ready) { http_error(&out, 408); break; }
        if (ready < 0) goto abandoned;
        ssize_t got = recv(peer, input + used, sizeof(input) - used, 0);
        if (got < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (got <= 0) goto abandoned;
        used += (size_t)got;
        int status = 400;
        int frame = http_request_extent(input, used, info, &status);
        if (frame < 0) { http_error(&out, status); break; }
        if (frame == 1) { http_handle(input, used, info, &out); break; }
        if (used == sizeof(input)) { http_error(&out, 431); break; }
    }
    wipe_host(input, sizeof(input));
    /* Provider work uses its own bounded budget; give its result a fresh send window. */
    long long send_start = milliseconds();
    if (send_start < 0) goto abandoned;
    deadline = send_start + (out.owned_body ? 10000 : 2000);
    if (expiry && deadline > expiry) deadline = expiry;
    char cookie_line[160] = "", location_line[48] = "";
    if (*out.cookie) {
        int size = snprintf(cookie_line, sizeof(cookie_line), "Set-Cookie: %s\r\n", out.cookie);
        if (size < 0 || (size_t)size >= sizeof(cookie_line)) goto abandoned;
    }
    if (*out.location) {
        int size = snprintf(location_line, sizeof(location_line), "Location: %s\r\n", out.location);
        if (size < 0 || (size_t)size >= sizeof(location_line)) goto abandoned;
    }
    char header[1024];
    const unsigned char *response_body = out.owned_body ? (const unsigned char *)out.owned_body :
        out.static_body ? out.static_body : (const unsigned char *)out.body;
    size_t response_size = out.static_body && !out.owned_body ? out.static_size : out.size;
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n"
        "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: %s\r\n%s%s\r\n",
        out.status, http_reason(out.status), out.content_type, response_size, out.csp ? out.csp : http_csp(), cookie_line, location_line);
    if (n > 0 && (size_t)n < sizeof(header) && !send_bytes(peer, header, (size_t)n, deadline))
        (void)send_bytes(peer, (const char *)response_body, response_size, deadline);
    /* Complete our write half before bounded draining of already queued excess input.
       Closing with unread TCP bytes otherwise resets oversized-request responses. */
    (void)shutdown(peer, SHUT_WR);
    unsigned char discarded[512];
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        long long now = milliseconds();
        if (now < 0 || now >= deadline) break;
        ssize_t got = recv(peer, discarded, sizeof(discarded), 0);
        if (got <= 0) break;
    }
    wipe_host(discarded, sizeof(discarded));
    int stop = out.shutdown;
    http_response_dispose(&out);
    wipe_host(header, sizeof(header));
    wipe_host(cookie_line, sizeof(cookie_line));
    return stop;
abandoned:
    wipe_host(input, sizeof(input));
    http_response_dispose(&out);
    return 0;
}
#if !HOST_NATIVE
static int number(const char *s, unsigned max, unsigned *value) {
    char *end;
    errno = 0;
    long parsed = strtol(s, &end, 10);
    if (errno || !*s || *end || parsed <= 0 || (unsigned long)parsed > max) return -1;
    *value = (unsigned)parsed;
    return 0;
}
#endif
int main(int argc, char **argv) {
    unsigned port = 8085, lifetime = HOST_LIFETIME;
#if HOST_NATIVE
    (void)argc; (void)argv;
#else
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) return finish(2);
        if (!strcmp(argv[i], "--port")) {
            if (number(argv[i+1], 65535, &port)) return finish(2);
        } else if (!strcmp(argv[i], "--lifetime")) {
            if (number(argv[i+1], 3600, &lifetime)) return finish(2);
        } else return finish(2);
    }
#endif
    struct host_info info = {0};
    info.version = "0.1.0"; info.build_id = BUILD_ID; info.profile = HOST_PROFILE;
    info.origin = HOST_NATIVE ? "ps5_sdk_entry" : "desktop_fixture";
    info.pid = getpid(); info.uid = getuid(); info.euid = geteuid();
    errno = 0; info.page_size = sysconf(_SC_PAGESIZE); info.page_errno = errno;
    info.lifetime = lifetime;
    size_t core_length = 0;
    if (mb_core_selftest(info.core_report, sizeof(info.core_report), &core_length) != MB_CORE_OK)
        strcpy(info.core_report, "{\"schema_version\":1,\"core_version\":\"0.1.0\",\"status\":\"unavailable\",\"scope\":\"owned_builtin_fixtures\",\"installation_supported\":false}");
#if MB_HOST_DOWNLOAD
    info.entropy.status = "unavailable";
    info.entropy.reason = "not_selected";
    info.entropy.elapsed_ms = -1;
    info.drbg.status = "unavailable";
    info.drbg.reason = "not_selected";
    info.drbg.elapsed_ms = -1;
    mb_curl_runtime_params_v1 runtime_params = {
        .struct_size = sizeof(runtime_params),
        .abi_version = MB_CURL_DOWNLOAD_ABI_V1,
        .entropy_context = NULL,
        .entropy_read32 = host_entropy_read32
    };
    mb_curl_runtime_result_v1 runtime_result = {
        .struct_size = sizeof(runtime_result),
        .abi_version = MB_CURL_DOWNLOAD_ABI_V1
    };
    download_runtime_start_attempted = 1;
    int runtime_status = mb_curl_runtime_start_v1(&runtime_params, &runtime_result);
    info.download_runtime.selected = 1;
    info.download_runtime.initialized = runtime_result.initialized != 0;
    info.download_runtime.terminal = runtime_result.terminal != 0;
    info.download_runtime.window_active = runtime_result.window_active != 0;
    info.download_runtime.window_expired = runtime_result.window_expired != 0;
    info.download_runtime.entropy_windows = runtime_result.entropy_windows;
    info.download_runtime.entropy_callbacks = runtime_result.entropy_callbacks;
    info.download_runtime.entropy_bytes = runtime_result.entropy_bytes;
    info.download_runtime.clock_samples = runtime_result.clock_samples;
    if (runtime_status != MB_CURL_DOWNLOAD_OK || !runtime_result.initialized)
        return finish(5);
#else
    mb_random_checkpoint(&info.entropy, &info.drbg);
#endif
    snprintf(info.authority, sizeof(info.authority), "%s:%u", HOST_ADDRESS, port);
    snprintf(info.url_origin, sizeof(info.url_origin), "http://%s", info.authority);
#if MB_HOST_DOWNLOAD && MB_HOST_OAUTH_CONFIGURED
    char expected_callback[128];
    int expected_length = snprintf(expected_callback, sizeof(expected_callback), "%s/auth/callback", info.url_origin);
    if (expected_length < 0 || (size_t)expected_length >= sizeof(expected_callback) ||
        strcmp(expected_callback, MB_HOST_OAUTH_REDIRECT_URI)) return finish(2);
    mb_nexus_auth_create_params_v1 auth_params = {
        .struct_size = sizeof(auth_params), .abi_version = MB_NEXUS_AUTH_ABI_V1,
        .origin = info.url_origin, .origin_bytes = strlen(info.url_origin),
        .client_id = MB_HOST_OAUTH_CLIENT_ID, .client_id_bytes = sizeof(MB_HOST_OAUTH_CLIENT_ID) - 1,
        .redirect_uri = MB_HOST_OAUTH_REDIRECT_URI, .redirect_uri_bytes = sizeof(MB_HOST_OAUTH_REDIRECT_URI) - 1,
        .scope = MB_HOST_OAUTH_SCOPE, .scope_bytes = sizeof(MB_HOST_OAUTH_SCOPE) - 1,
        .ca_pem = mb_host_oauth_ca, .ca_bytes = MB_HOST_OAUTH_CA_BYTES,
        .api_resolve = MB_HOST_OAUTH_API_RESOLVE, .api_resolve_bytes = sizeof(MB_HOST_OAUTH_API_RESOLVE) - 1,
        .users_resolve = MB_HOST_OAUTH_USERS_RESOLVE, .users_resolve_bytes = sizeof(MB_HOST_OAUTH_USERS_RESOLVE) - 1,
        .entropy_context = NULL, .entropy_read32 = host_entropy_read32
    };
    mb_nexus_auth_result_v1 auth_result = {
        .struct_size = sizeof(auth_result), .abi_version = MB_NEXUS_AUTH_ABI_V1
    };
    if (mb_nexus_auth_create_v1(&auth_params, &host_auth, &auth_result) != MB_NEXUS_AUTH_OK ||
        auth_result.status != MB_NEXUS_AUTH_OK || !auth_result.complete || !auth_result.configured || !host_auth)
        return finish(5);
    info.auth = host_auth;
#endif
#if MB_HOST_JOBS
    if (!host_auth) return finish(5);
    host_scratch_directory = open(MB_HOST_JOB_DIRECTORY, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    struct stat scratch_identity;
    if (host_scratch_directory < 0 || fstat(host_scratch_directory, &scratch_identity) ||
        !S_ISDIR(scratch_identity.st_mode) || scratch_identity.st_uid != geteuid() ||
        (scratch_identity.st_mode & 07777) != 0700) return finish(5);
    mb_nexus_jobs_create_params_v1 jobs_params = {
        .struct_size = sizeof(jobs_params), .abi_version = MB_NEXUS_JOBS_ABI_V1,
        .auth = host_auth, .scratch_directory_fd = host_scratch_directory,
        .execution_context = HOST_NATIVE ? MB_ARCHIVE_ANALYSIS_PS5_NATIVE : MB_ARCHIVE_ANALYSIS_DESKTOP_PREPARATION,
        .ca_pem = mb_host_oauth_ca, .ca_bytes = MB_HOST_OAUTH_CA_BYTES,
        .archive_resolve = sizeof(MB_HOST_OAUTH_API_RESOLVE) > 1 ? MB_HOST_OAUTH_API_RESOLVE : NULL,
        .archive_resolve_bytes = sizeof(MB_HOST_OAUTH_API_RESOLVE) - 1,
        .resolver_timeout_ms = MB_HOST_OAUTH_TIMEOUT_MS, .download_timeout_ms = 300000,
        .analysis_timeout_ms = 300000, .connect_timeout_ms = MB_HOST_OAUTH_CONNECT_TIMEOUT_MS,
        .archive_cap_bytes = MB_NEXUS_JOBS_ARCHIVE_LIMIT,
        .entropy_context = NULL, .entropy_read32 = host_entropy_read32
    };
    mb_nexus_jobs_result_v1 jobs_result = { .struct_size = sizeof(jobs_result), .abi_version = MB_NEXUS_JOBS_ABI_V1 };
    if (mb_nexus_jobs_create_v1(&jobs_params, &host_jobs, &jobs_result) != MB_NEXUS_JOBS_OK ||
        jobs_result.status != MB_NEXUS_JOBS_OK || !jobs_result.complete || !host_jobs) return finish(5);
#if MB_HOST_PAK_PROFILE
    host_pak_base = open(MB_HOST_PAK_BASE_PATH, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    mb_nexus_jobs_pak_profile_v1 pak_profile = {
        .struct_size = sizeof(pak_profile), .abi_version = MB_NEXUS_JOBS_ABI_V1,
        .base_fd = host_pak_base, .immutable_captured_base = 1,
        .expected_bytes = MB_HOST_PAK_BASE_BYTES,
        .expected_sha256 = MB_HOST_PAK_BASE_SHA256,
        .nexus_game = MB_HOST_PAK_NEXUS_GAME, .nexus_game_bytes = sizeof(MB_HOST_PAK_NEXUS_GAME) - 1,
        .timeout_ms = MB_HOST_PAK_TIMEOUT_MS
    };
    if (host_pak_base < 0 || mb_nexus_jobs_configure_pak_v1(host_jobs, &pak_profile, &jobs_result) != MB_NEXUS_JOBS_OK ||
        jobs_result.status != MB_NEXUS_JOBS_OK || !jobs_result.complete) return finish(5);
#endif
#if MB_HOST_IOSTORE_PROFILE
    host_iostore_base_toc = open(MB_HOST_IOSTORE_BASE_TOC_PATH, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    host_iostore_base_header = open(MB_HOST_IOSTORE_BASE_HEADER_PATH, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    mb_nexus_jobs_iostore_profile_v1 iostore_profile = {
        .struct_size = sizeof(iostore_profile), .abi_version = MB_NEXUS_JOBS_ABI_V1,
        .base_toc_fd = host_iostore_base_toc, .base_header_fd = host_iostore_base_header,
        .immutable_captured_base = 1,
        .expected_toc_bytes = MB_HOST_IOSTORE_BASE_TOC_BYTES,
        .expected_header_bytes = MB_HOST_IOSTORE_BASE_HEADER_BYTES,
        .expected_toc_sha256 = MB_HOST_IOSTORE_BASE_TOC_SHA256,
        .expected_header_sha256 = MB_HOST_IOSTORE_BASE_HEADER_SHA256,
        .observed_base_ucas_bytes = MB_HOST_IOSTORE_BASE_UCAS_BYTES,
        .nexus_game = MB_HOST_IOSTORE_NEXUS_GAME,
        .nexus_game_bytes = sizeof(MB_HOST_IOSTORE_NEXUS_GAME) - 1,
        .timeout_ms = MB_HOST_IOSTORE_TIMEOUT_MS, .reserved = 0
    };
    if (host_iostore_base_toc < 0 || host_iostore_base_header < 0 ||
        mb_nexus_jobs_configure_iostore_v1(host_jobs, &iostore_profile, &jobs_result) != MB_NEXUS_JOBS_OK ||
        jobs_result.status != MB_NEXUS_JOBS_OK || !jobs_result.complete) return finish(5);
#endif
    info.jobs = host_jobs;
#endif
    long long start = milliseconds();
    if (start < 0) return finish(4);
    long long expiry = lifetime ? start + (long long)lifetime * 1000 : 0;
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) return finish(3);
    int reuse = 1;
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
#if defined(__FreeBSD__) || defined(__APPLE__)
    address.sin_len = sizeof(address);
#endif
    address.sin_family = AF_INET; address.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, HOST_ADDRESS, &address.sin_addr) != 1 ||
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) ||
        nonblocking(listener) || bind(listener, (struct sockaddr *)&address, sizeof(address)) ||
        listen(listener, 4)) {
        close(listener); return finish(3);
    }
    int result = 0;
    for (;;) {
        long long now = milliseconds();
        if (now < 0) { result = 4; break; }
        if (expiry && now >= expiry) break;
        long long check = now + 100;
        if (expiry && check > expiry) check = expiry;
        int ready = wait_ready(listener, POLLIN, check);
        if (!ready) continue;
        if (ready < 0) { result = 4; break; }
        int peer = accept(listener, NULL, NULL);
        if (peer < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (peer < 0) { result = 4; break; }
        now = milliseconds();
        if (now < 0) { close(peer); result = 4; break; }
        long long deadline = now + 2000;
        if (expiry && deadline > expiry) deadline = expiry;
        int stop = serve_peer(peer, &info, deadline, expiry);
        close(peer);
        if (stop) break;
#if MB_HOST_JOBS
        /* The queued response precedes synchronous work. This preserves the
         * single curl/PSA owner; polling waits until this call returns. */
        mb_nexus_jobs_result_v1 job_result = { .struct_size = sizeof(job_result), .abi_version = MB_NEXUS_JOBS_ABI_V1 };
        if (mb_nexus_jobs_run_one_v1(host_jobs, &job_result) != MB_NEXUS_JOBS_OK ||
            job_result.status != MB_NEXUS_JOBS_OK || !job_result.complete) { result = 5; break; }
#endif
    }
    close(listener);
    return finish(result);
}
