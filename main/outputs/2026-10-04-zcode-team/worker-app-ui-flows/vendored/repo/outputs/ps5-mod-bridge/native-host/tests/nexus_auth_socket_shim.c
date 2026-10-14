#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct fixture_route_state {
    uint16_t port;
    unsigned configured;
    unsigned installs;
    unsigned open_calls;
    unsigned connected;
    unsigned sockopt_calls;
    unsigned refused;
    curl_socket_t last_fd;
};

static struct fixture_route_state route_state;
static unsigned receipt_registered;

static int fixture_port(uint16_t *out) {
    const char *text = getenv("MB_NEXUS_AUTH_FIXTURE_PORT");
    if (!text || !*text) return -1;
    unsigned value = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p < '0' || *p > '9') return -1;
        value = value * 10u + (unsigned)(*p - '0');
        if (value > 65535u) return -1;
    }
    if (value < 1024u || value == 443u) return -1;
    *out = (uint16_t)value;
    return 0;
}

static void emit_receipt(void) {
    /* Test-only scalar counters. No URL, request target, credential or body is
     * observable through this hook. */
    (void)printf("NEXUS_AUTH_SOCKET_SHIM {\"configured\":%s,\"installs\":%u,"
                 "\"open_calls\":%u,\"connected\":%u,\"sockopt_calls\":%u,"
                 "\"refused\":%u}\n",
                 route_state.configured ? "true" : "false", route_state.installs,
                 route_state.open_calls, route_state.connected,
                 route_state.sockopt_calls, route_state.refused);
    (void)fflush(stdout);
}

static int destination_is_expected(curlsocktype purpose,
                                   const struct curl_sockaddr *address) {
    if (!address || purpose != CURLSOCKTYPE_IPCXN ||
        address->family != AF_INET ||
        address->addrlen < (int)sizeof(struct sockaddr_in)) return 0;
    const struct sockaddr_in *remote = (const struct sockaddr_in *)&address->addr;
    return remote->sin_family == AF_INET &&
           remote->sin_addr.s_addr == htonl(0x7f000001u) &&
           remote->sin_port == htons(443);
}

static curl_socket_t fixture_open_socket(void *opaque, curlsocktype purpose,
                                         struct curl_sockaddr *address) {
    struct fixture_route_state *state = opaque;
    ++state->open_calls;
    if (!state->configured || !destination_is_expected(purpose, address)) {
        ++state->refused;
        return CURL_SOCKET_BAD;
    }

    struct sockaddr_in destination;
    memcpy(&destination, &address->addr, sizeof(destination));
    destination.sin_port = htons(state->port);
    curl_socket_t fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == CURL_SOCKET_BAD ||
        connect(fd, (const struct sockaddr *)&destination, sizeof(destination)) != 0) {
        if (fd != CURL_SOCKET_BAD) close(fd);
        ++state->refused;
        return CURL_SOCKET_BAD;
    }
    state->last_fd = fd;
    ++state->connected;
    return fd;
}

static int fixture_sockopt(void *opaque, curl_socket_t fd, curlsocktype purpose) {
    struct fixture_route_state *state = opaque;
    ++state->sockopt_calls;
    if (purpose != CURLSOCKTYPE_IPCXN || fd != state->last_fd || !state->connected) {
        ++state->refused;
        return CURL_SOCKOPT_ERROR;
    }
    return CURL_SOCKOPT_ALREADY_CONNECTED;
}

/* curl_download.c is compiled for this fixture with only its
 * curl_multi_add_handle call renamed to this test-only symbol. All production
 * URL construction, resolve admission, TLS SNI/CA/name validation and the real
 * curl_multi_add_handle implementation remain in use. */
CURLMcode mb_nexus_auth_fixture_curl_multi_add_handle(CURLM *multi, CURL *easy) {
    if (!receipt_registered) {
        receipt_registered = 1;
        (void)atexit(emit_receipt);
        route_state.configured = fixture_port(&route_state.port) == 0;
    }
    ++route_state.installs;
    if (!route_state.configured ||
        curl_easy_setopt(easy, CURLOPT_OPENSOCKETFUNCTION, fixture_open_socket) != CURLE_OK ||
        curl_easy_setopt(easy, CURLOPT_OPENSOCKETDATA, &route_state) != CURLE_OK ||
        curl_easy_setopt(easy, CURLOPT_SOCKOPTFUNCTION, fixture_sockopt) != CURLE_OK ||
        curl_easy_setopt(easy, CURLOPT_SOCKOPTDATA, &route_state) != CURLE_OK) {
        ++route_state.refused;
        return CURLM_INTERNAL_ERROR;
    }
    return curl_multi_add_handle(multi, easy);
}
