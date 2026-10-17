#include "provider_transfer.h"
#include "modbridge/nexus_requirements_query.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Admission controls use the real pinned transport without starting its owner
 * or any socket. The CA and credential below are explicit fixture placeholders. */
static unsigned checks, failures, consumed;
static const char canonical[] = MB_NEXUS_REQUIREMENTS_BODY_PREFIX "silenthilltownfall"
    MB_NEXUS_REQUIREMENTS_BODY_MIDDLE "62" MB_NEXUS_REQUIREMENTS_BODY_SUFFIX;
static const char credential[] = "owned-requirements-control";
static const uint8_t ca[] = "owned-placeholder-no-network";
static uint8_t raw[512];
static mb_curl_provider_transfer_result_v1 result;

static int consume(void *context, const uint8_t *body, size_t bytes)
{
    (void)context; (void)body; (void)bytes; ++consumed; return 0;
}
static int filled(const void *data, size_t bytes, unsigned value)
{
    const uint8_t *p = data;
    for(size_t i = 0; i < bytes; ++i) if(p[i] != value) return 0;
    return 1;
}
static void check(int condition, const char *name)
{
    ++checks;
    if(!condition) { ++failures; fprintf(stderr, "failed: %s\n", name); }
}
static mb_curl_provider_post_transfer_params_v1 make_params(void)
{
    mb_curl_provider_post_transfer_params_v1 p;
    memset(&p, 0, sizeof(p));
    p.struct_size = sizeof(p); p.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    p.provider.struct_size = sizeof(p.provider); p.provider.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    p.provider.url = MB_NEXUS_REQUIREMENTS_GRAPHQL_URL; p.provider.auth_mode = 2u;
    p.provider.credential = credential; p.provider.credential_bytes = sizeof(credential) - 1u;
    p.provider.ca_pem = ca; p.provider.ca_bytes = sizeof(ca) - 1u;
    p.provider.timeout_ms = 1000u; p.provider.connect_timeout_ms = 100u;
    p.provider.body = raw; p.provider.body_capacity = sizeof(raw);
    p.provider.consume_complete_body = consume;
    p.request_body = canonical; p.request_body_bytes = sizeof(canonical) - 1u;
    return p;
}
static void expect(mb_curl_provider_post_transfer_params_v1 *p, int status,
                   int untouched, const char *name)
{
    memset(raw, 0xa5, sizeof(raw)); memset(&result, 0xa5, sizeof(result));
    int rc = mb_curl_provider_post_transfer_v1(p, &result);
    check(rc == status && !consumed && filled(raw, sizeof(raw), untouched ? 0xa5u : 0u) &&
          (untouched ? filled(&result, sizeof(result), 0xa5u) :
           ((int)result.status == rc && !result.complete && !result.http_status &&
            !result.received_bytes && result.struct_size == sizeof(result) &&
            result.abi_version == MB_CURL_DOWNLOAD_ABI_V1)), name);
}

int main(void)
{
    mb_curl_provider_post_transfer_params_v1 p = make_params();
    expect(&p, MB_CURL_DOWNLOAD_BAD_STATE, 0, "canonical bearer admitted before owner");
    p.provider.auth_mode = 1u;
    expect(&p, MB_CURL_DOWNLOAD_BAD_STATE, 0, "canonical API key admitted before owner");
    p = make_params(); p.provider.cancel_context = &checks; /* Callback remains optional. */
    expect(&p, MB_CURL_DOWNLOAD_BAD_STATE, 0, "unused independent cancel context preserved");
    p = make_params(); p.request_body = NULL; p.request_body_bytes = 0;
    expect(&p, MB_CURL_DOWNLOAD_BAD_ARGUMENT, 0, "empty request refused and raw wiped");
    p = make_params(); --p.request_body_bytes;
    expect(&p, MB_CURL_DOWNLOAD_BAD_ARGUMENT, 0, "short request span refused");
    static const char *const rejected[] = {
        MB_NEXUS_REQUIREMENTS_BODY_PREFIX "Townfall" MB_NEXUS_REQUIREMENTS_BODY_MIDDLE "62" MB_NEXUS_REQUIREMENTS_BODY_SUFFIX,
        MB_NEXUS_REQUIREMENTS_BODY_PREFIX "-townfall" MB_NEXUS_REQUIREMENTS_BODY_MIDDLE "62" MB_NEXUS_REQUIREMENTS_BODY_SUFFIX,
        MB_NEXUS_REQUIREMENTS_BODY_PREFIX "townfall" MB_NEXUS_REQUIREMENTS_BODY_MIDDLE "0" MB_NEXUS_REQUIREMENTS_BODY_SUFFIX,
        MB_NEXUS_REQUIREMENTS_BODY_PREFIX "townfall" MB_NEXUS_REQUIREMENTS_BODY_MIDDLE "062" MB_NEXUS_REQUIREMENTS_BODY_SUFFIX,
        MB_NEXUS_REQUIREMENTS_BODY_PREFIX "townfall" MB_NEXUS_REQUIREMENTS_BODY_MIDDLE "2147483648" MB_NEXUS_REQUIREMENTS_BODY_SUFFIX,
        MB_NEXUS_REQUIREMENTS_BODY_PREFIX "townfall" MB_NEXUS_REQUIREMENTS_BODY_MIDDLE "62" MB_NEXUS_REQUIREMENTS_BODY_SUFFIX " ",
        "{\"query\":\"mutation { anything }\",\"variables\":{}}"
    };
    for(size_t i = 0; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        p = make_params(); p.request_body = rejected[i]; p.request_body_bytes = strlen(rejected[i]);
        expect(&p, MB_CURL_DOWNLOAD_BAD_ARGUMENT, 0, "noncanonical request refused");
    }
    p = make_params(); p.provider.url = "https://api.nexusmods.com/v1/games";
    expect(&p, MB_CURL_DOWNLOAD_BAD_ARGUMENT, 0, "POST does not admit GET endpoint");
    p = make_params(); p.provider.credential = "bad\nheader"; p.provider.credential_bytes = 10u;
    expect(&p, MB_CURL_DOWNLOAD_BAD_ARGUMENT, 0, "invalid credential refused");
    p = make_params(); p.provider.consume_complete_body = NULL;
    expect(&p, MB_CURL_DOWNLOAD_BAD_ARGUMENT, 0, "missing consumer refused");
    p = make_params(); p.provider.cancel_context = raw + 4u;
    expect(&p, MB_CURL_DOWNLOAD_BAD_ARGUMENT, 1, "cancel context raw alias untouched");
    p = make_params(); p.provider.consume_context = &result;
    expect(&p, MB_CURL_DOWNLOAD_BAD_ARGUMENT, 1, "consumer context result alias untouched");
    p = make_params(); p.request_body = (const char *)raw; p.request_body_bytes = 10u;
    expect(&p, MB_CURL_DOWNLOAD_BAD_ARGUMENT, 1, "request raw alias untouched");
    p = make_params(); p.provider.credential = (const char *)&p; p.provider.credential_bytes = 8u;
    expect(&p, MB_CURL_DOWNLOAD_BAD_ARGUMENT, 1, "whole outer params alias untouched");
    p = make_params(); p.provider.url = (const char *)raw;
    expect(&p, MB_CURL_DOWNLOAD_BAD_ARGUMENT, 1, "URL first byte raw alias refused before measuring");
    p = make_params(); p.provider.resolve = (const char *)&result;
    expect(&p, MB_CURL_DOWNLOAD_BAD_ARGUMENT, 1, "resolve first byte result alias refused before measuring");
    p = make_params(); p.request_body = (const char *)(UINTPTR_MAX - 2u); p.request_body_bytes = 8u;
    expect(&p, MB_CURL_DOWNLOAD_BAD_ARGUMENT, 1, "request address overflow refused before access");
    p = make_params(); p.request_body_bytes = MB_NEXUS_REQUIREMENTS_REQUEST_LIMIT + 1u;
    expect(&p, MB_CURL_DOWNLOAD_BAD_ARGUMENT, 1, "oversize request storage untouched");
    p = make_params(); --p.provider.struct_size;
    expect(&p, MB_CURL_DOWNLOAD_BAD_ARGUMENT, 1, "nested layout rejected before mutation");
    p = make_params(); --p.struct_size;
    expect(&p, MB_CURL_DOWNLOAD_BAD_ARGUMENT, 1, "outer layout rejected before mutation");
    p = make_params(); memset(raw, 0xa5, sizeof(raw));
    check(mb_curl_provider_post_transfer_v1(&p, NULL) == MB_CURL_DOWNLOAD_BAD_ARGUMENT &&
          filled(raw, sizeof(raw), 0u), "NULL result erases independent raw storage");
    p = make_params(); memset(raw, 0xa5, sizeof(raw));
    check(mb_curl_provider_post_transfer_v1(&p, (void *)raw) == MB_CURL_DOWNLOAD_BAD_ARGUMENT &&
          filled(raw, sizeof(raw), 0xa5u), "result raw alias untouched");
    p = make_params(); memset(raw, 0xa5, sizeof(raw));
    check(mb_curl_provider_transfer_v1(&p.provider, &result) == MB_CURL_DOWNLOAD_BAD_ARGUMENT &&
          result.status == MB_CURL_DOWNLOAD_BAD_ARGUMENT && !result.complete && filled(raw, sizeof(raw), 0u),
          "old GET refuses GraphQL endpoint");
    p.provider.url = "https://api.nexusmods.com/v1/games"; memset(raw, 0xa5, sizeof(raw));
    check(mb_curl_provider_transfer_v1(&p.provider, &result) == MB_CURL_DOWNLOAD_BAD_STATE &&
          result.status == MB_CURL_DOWNLOAD_BAD_STATE && !result.complete && filled(raw, sizeof(raw), 0u),
          "old GET endpoint still admitted before owner");
    printf("REQUIREMENTS_POST_ADMISSION {\"checks\":%u,\"failures\":%u,\"consumed\":%u,\"runtime_started\":false}\n",
           checks, failures, consumed);
    return failures ? 1 : 0;
}
