#define _DARWIN_C_SOURCE
#include "provider_transfer.h"
#include "owner.h"
#include "modbridge/nexus_requirements_query.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <time.h>
#include <unistd.h>

/* Only owned fixture credentials/response data are used. The socket shim maps
 * the approved loopback:443 destination to the verifier's ephemeral TLS port;
 * production Host/SNI/CA/name verification and pinned curl remain unchanged. */
static const char request_body[] = MB_NEXUS_REQUIREMENTS_BODY_PREFIX "silenthilltownfall"
    MB_NEXUS_REQUIREMENTS_BODY_MIDDLE "62" MB_NEXUS_REQUIREMENTS_BODY_SUFFIX;
static const char fake_bearer[] = "owned-requirements-bearer";
static const char fake_key[] = "owned-requirements-apikey";
static unsigned checks, failures;
struct control { unsigned mode, calls, wrong_body; const char *expected; };
static int entropy(void *context, uint8_t out[32])
{
    (void)context;
    errno = 0;
    return getentropy(out, 32u) || errno ? -1 : 0;
}
static int zeroed(const uint8_t *data, size_t bytes)
{
    for(size_t i = 0; i < bytes; ++i) if(data[i]) return 0;
    return 1;
}
static void check(int okay, const char *name)
{
    ++checks;
    if(!okay) { ++failures; fprintf(stderr, "failed: %s\n", name); }
}
static int consume(void *opaque, const uint8_t *data, size_t bytes)
{
    struct control *c = opaque;
    ++c->calls;
    if(bytes != strlen(c->expected) || memcmp(data, c->expected, bytes)) c->wrong_body = 1u;
    if(c->mode == 3u) {
        struct timespec delay = { 0, 700000000L };
        while(nanosleep(&delay, &delay) && errno == EINTR) {}
    }
    if(c->mode == 5u) mb_curl_owner_fail_v1(); /* Explicit terminal-fault boundary. */
    return c->wrong_body || c->mode == 2u || c->mode == 3u || c->mode == 5u;
}
static int cancelled(void *opaque, uint64_t received)
{
    const struct control *c = opaque;
    (void)received;
    return c->mode == 6u || (c->mode == 4u && c->calls != 0u);
}
static void attempt(const uint8_t *ca, size_t ca_bytes, unsigned mode, unsigned key,
                    unsigned old_get, int status, unsigned calls, const char *name)
{
    uint8_t raw[1024];
    struct control c = { mode, 0, 0, old_get ? "{\"owned_get_body\":true}" : "{\"owned_requirements_body\":true}" };
    mb_curl_provider_post_transfer_params_v1 p;
    mb_curl_provider_transfer_result_v1 r;
    memset(&p, 0, sizeof(p)); memset(&r, 0xa5, sizeof(r)); memset(raw, 0xa5, sizeof(raw));
    p.struct_size = sizeof(p); p.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    p.provider.struct_size = sizeof(p.provider); p.provider.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    p.provider.url = old_get ? "https://api.nexusmods.com/v1/games" : MB_NEXUS_REQUIREMENTS_GRAPHQL_URL;
    p.provider.auth_mode = key ? 1u : 2u;
    p.provider.credential = key ? fake_key : fake_bearer;
    p.provider.credential_bytes = strlen(p.provider.credential);
    p.provider.ca_pem = ca; p.provider.ca_bytes = ca_bytes;
    p.provider.resolve = "api.nexusmods.com:443:127.0.0.1";
    p.provider.timeout_ms = mode == 3u ? 500u : 5000u; p.provider.connect_timeout_ms = 300u;
    p.provider.body = raw; p.provider.body_capacity = sizeof(raw);
    p.provider.consume_context = &c; p.provider.consume_complete_body = consume;
    p.provider.cancel_context = &c; p.provider.cancelled = cancelled;
    p.request_body = request_body; p.request_body_bytes = sizeof(request_body) - 1u;
    int rc = old_get ? mb_curl_provider_transfer_v1(&p.provider, &r) :
                      mb_curl_provider_post_transfer_v1(&p, &r);
    check(rc == status && (int)r.status == status && r.complete == (status == 0) &&
          c.calls == calls && !c.wrong_body && zeroed(raw, sizeof(raw)), name);
    printf("REQUIREMENTS_POST_ATTEMPT {\"case\":\"%s\",\"return\":%d,\"status\":%d,"
           "\"complete\":%u,\"consumer_calls\":%u,\"raw_wiped\":%s}\n",
           name, rc, (int)r.status, r.complete, c.calls, zeroed(raw, sizeof(raw)) ? "true" : "false");
}

int main(int argc, char **argv)
{
    if(argc != 2) return 2;
    FILE *file = fopen(argv[1], "rb");
    if(!file) return 2;
    uint8_t ca[65536]; size_t ca_bytes = fread(ca, 1u, sizeof(ca), file);
    int read_bad = ferror(file) || !feof(file); fclose(file);
    if(read_bad || !ca_bytes) return 2;
    mb_curl_runtime_params_v1 owner = { sizeof(owner), MB_CURL_DOWNLOAD_ABI_V1, NULL, entropy };
    mb_curl_runtime_result_v1 snapshot;
    int started = mb_curl_runtime_start_v1(&owner, &snapshot);
    check(started == 0 && snapshot.initialized && !snapshot.terminal, "runtime_start");
    if(started) return 1;
    attempt(ca, ca_bytes, 0u, 0u, 0u, MB_CURL_DOWNLOAD_OK, 1u, "canonical_bearer");
    attempt(ca, ca_bytes, 0u, 1u, 0u, MB_CURL_DOWNLOAD_OK, 1u, "canonical_apikey");
    attempt(ca, ca_bytes, 0u, 0u, 1u, MB_CURL_DOWNLOAD_OK, 1u, "old_GET");
    attempt(ca, ca_bytes, 6u, 0u, 0u, MB_CURL_DOWNLOAD_CANCELLED, 0u, "cancel_before_socket");
    attempt(ca, ca_bytes, 2u, 0u, 0u, MB_CURL_DOWNLOAD_CONSUMER_FAILED, 1u, "consumer_refusal");
    attempt(ca, ca_bytes, 4u, 0u, 0u, MB_CURL_DOWNLOAD_CANCELLED, 1u, "cancel_after_consumer");
    attempt(ca, ca_bytes, 3u, 0u, 0u, MB_CURL_DOWNLOAD_TIMEOUT, 1u, "late_timeout_over_consumer");
    check(mb_curl_runtime_status_v1(&snapshot) == 0 && snapshot.initialized &&
          !snapshot.terminal && !snapshot.window_active, "ordinary_timeout_owner_reusable");
    attempt(ca, ca_bytes, 0u, 0u, 0u, MB_CURL_DOWNLOAD_OK, 1u, "after_timeout");
    attempt(ca, ca_bytes, 0u, 0u, 0u, MB_CURL_DOWNLOAD_RESPONSE_REFUSED, 0u, "redirect_not_followed");
    attempt(ca, ca_bytes, 5u, 0u, 0u, MB_CURL_DOWNLOAD_OWNER_FAILED, 1u, "late_terminal_owner_fault");
    check(mb_curl_runtime_close_v1(&snapshot) == 0 && !snapshot.initialized &&
          snapshot.terminal && !snapshot.window_active && snapshot.entropy_callbacks &&
          snapshot.entropy_bytes == snapshot.entropy_callbacks * 32u, "terminal_physical_cleanup");
    printf("REQUIREMENTS_POST_TLS {\"checks\":%u,\"failures\":%u,\"entropy_calls\":%llu,"
           "\"entropy_bytes\":%llu,\"terminal\":%u}\n", checks, failures,
           (unsigned long long)snapshot.entropy_callbacks, (unsigned long long)snapshot.entropy_bytes, snapshot.terminal);
    return failures ? 1 : 0;
}
