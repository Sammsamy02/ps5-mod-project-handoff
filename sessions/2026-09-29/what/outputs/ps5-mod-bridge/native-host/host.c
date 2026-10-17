#define _POSIX_C_SOURCE 200809L
#include "http.h"
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
#ifndef HOST_NATIVE
#define HOST_NATIVE 0
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

static int finish(int status) {
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
    if (value.tv_sec < 0 || value.tv_sec > 9000000000LL) return -1;
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
static int serve_peer(int peer, const struct host_info *info, long long deadline) {
    unsigned char input[HTTP_HEADER_LIMIT];
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
        if (ready < 0) return 0;
        ssize_t got = recv(peer, input + used, sizeof(input) - used, 0);
        if (got < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (got <= 0) return 0;
        used += (size_t)got;
        int complete = 0;
        for (size_t i = 3; i < used; ++i) {
            if (input[i-3] == '\r' && input[i-2] == '\n' && input[i-1] == '\r' && input[i] == '\n') {
                complete = 1; break;
            }
        }
        if (complete) { http_handle(input, used, info, &out); break; }
        if (used == sizeof(input)) { http_error(&out, 431); break; }
    }
    char header[768];
    const unsigned char *response_body = out.static_body ? out.static_body : (const unsigned char *)out.body;
    size_t response_size = out.static_body ? out.static_size : out.size;
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n"
        "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: %s\r\n\r\n",
        out.status, http_reason(out.status), out.content_type, response_size, out.csp ? out.csp : http_csp());
    /* Reserve the final 100 ms of the absolute connection deadline for an error response. */
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
    return out.shutdown;
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
    mb_random_checkpoint(&info.entropy, &info.drbg);
    snprintf(info.authority, sizeof(info.authority), "%s:%u", HOST_ADDRESS, port);
    snprintf(info.url_origin, sizeof(info.url_origin), "http://%s", info.authority);
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
        int stop = serve_peer(peer, &info, deadline);
        close(peer);
        if (stop) break;
    }
    close(listener);
    return finish(result);
}
