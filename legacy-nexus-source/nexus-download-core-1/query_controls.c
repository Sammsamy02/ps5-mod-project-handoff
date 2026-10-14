#include "provider_transfer.h"
#include <stdio.h>
#include <string.h>
static unsigned consumed;
static int consume(void *ctx, const uint8_t *body, size_t n) {
    (void)ctx; (void)body; (void)n; ++consumed; return 0;
}
static int zeroed(const uint8_t *p, size_t n) {
    size_t i; for(i=0;i<n;++i) if(p[i]) return 0; return 1;
}
#define LINK "https://api.nexusmods.com/v1/games/sample-game/mods/42/files/7/download_link"
int main(void) {
    static const struct { const char *url; unsigned mode; int expected; } cases[] = {
        {LINK "?key=owned-grant&expires=9223372036854775807",1,MB_CURL_DOWNLOAD_BAD_STATE},
        {LINK "?key=a-Z_~.%21%40%26%3F%3D%2F%25%2B%5C&expires=9",2,MB_CURL_DOWNLOAD_BAD_STATE},
        {LINK,1,MB_CURL_DOWNLOAD_BAD_STATE},
        {"https://api.nexusmods.com/v1/games",1,MB_CURL_DOWNLOAD_BAD_STATE},
        {"https://users.nexusmods.com/oauth/userinfo",2,MB_CURL_DOWNLOAD_BAD_STATE},
        {LINK "?expires=9&key=grant",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=grant&expires=9&extra=1",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=grant&key=other&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=grant&expires=9&expires=10",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=grant&expires=0",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=grant&expires=09",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=grant&expires=9223372036854775808",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=grant&expires=+9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=grant&expires=9.0",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=%41&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=%7E&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=%2f&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=%00&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=%20&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=%7F&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=%80&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=%&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=raw!&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=raw+&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=raw?&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=grant&expires=9#fragment",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=grant&expires=9?extra",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {LINK "?key=grant&expires=9/",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {"https://api.nexusmods.com/v1/games?key=grant&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {"https://users.nexusmods.com/oauth/userinfo?key=grant&expires=9",2,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {"https://api.nexusmods.com/v1/games/sample-game/mods/042/files/7/download_link?key=grant&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {"https://api.nexusmods.com/v1/games/sample-game/mods/42/files/0/download_link?key=grant&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {"https://api.nexusmods.com/v1/games/sample-game/mods/42/files/9223372036854775808/download_link?key=grant&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {"https://api.nexusmods.com/v1/games/Sample-game/mods/42/files/7/download_link?key=grant&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {"https://api.nexusmods.com/v1/games/sample-game/mods/42/files/7/../7/download_link?key=grant&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {"https://api.nexusmods.com/v1/games/sample-game/mods/42/files/7/download_link/?key=grant&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {"https://api.nexusmods.com:443/v1/games/sample-game/mods/42/files/7/download_link?key=grant&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {"https://foreign.test/v1/games/sample-game/mods/42/files/7/download_link?key=grant&expires=9",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT},
        {"https://api.nexusmods.com/v1/games?",1,MB_CURL_DOWNLOAD_BAD_ARGUMENT}
    };
    static const char secret[]="owned-fixture-secret";
    static const uint8_t ca[]="owned-placeholder-never-used";
    uint8_t body[128];
    mb_curl_provider_transfer_params_v1 p;
    mb_curl_provider_transfer_result_v1 out;
    size_t i;
    memset(&p,0,sizeof(p)); p.struct_size=sizeof(p); p.abi_version=1;
    p.credential=secret; p.credential_bytes=sizeof(secret)-1;
    p.ca_pem=ca; p.ca_bytes=sizeof(ca)-1; p.timeout_ms=1000;
    p.connect_timeout_ms=100; p.body=body; p.body_capacity=sizeof(body);
    p.consume_complete_body=consume;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        int rc;
        p.url=cases[i].url; p.auth_mode=cases[i].mode;
        memset(body,0xa5,sizeof(body)); memset(&out,0xa5,sizeof(out));
        rc=mb_curl_provider_transfer_v1(&p,&out);
        if(rc!=cases[i].expected || (int)out.status!=rc || out.complete ||
           out.http_status || out.received_bytes || consumed || !zeroed(body,sizeof(body))) {
            fprintf(stderr,"case %zu: status %d expected %d\n",i,rc,cases[i].expected);
            return 1;
        }
    }
    printf("%zu query/origin admissions passed; runtime never started\n",sizeof(cases)/sizeof(cases[0]));
    return 0;
}
