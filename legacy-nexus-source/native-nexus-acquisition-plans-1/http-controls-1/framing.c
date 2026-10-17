#include "http.h"
#include <stdio.h>
#include <string.h>

static unsigned checks, failures;
static void check(int condition, const char *name) {
    ++checks;
    if (!condition) { ++failures; printf("FAIL %s\n", name); }
}
static size_t frame(unsigned char *bytes, const char *method, const char *target,
                    const char *headers, const unsigned char *body, size_t body_bytes) {
    int n = snprintf((char *)bytes, HTTP_HEADER_LIMIT + HTTP_JOB_BODY_LIMIT + 1,
        "%s %s HTTP/1.1\r\nHost: 127.0.0.1:18080\r\n%s\r\n", method, target, headers);
    if (n < 0 || (size_t)n + body_bytes > HTTP_HEADER_LIMIT + HTTP_JOB_BODY_LIMIT) return 0;
    if (body_bytes) memcpy(bytes + n, body, body_bytes);
    return (size_t)n + body_bytes;
}
int main(void) {
    struct host_info info = {0};
    strcpy(info.authority, "127.0.0.1:18080");
    strcpy(info.url_origin, "http://127.0.0.1:18080");
    static const unsigned char intent[] = "{\"game\":\"silenthilltownfall\",\"mod_ids\":[62],\"file_choices\":[],\"file_policy\":\"unique_current_primary\"}";
    unsigned char bytes[HTTP_HEADER_LIMIT + HTTP_JOB_BODY_LIMIT + 1];
    char headers[512];
    snprintf(headers, sizeof(headers), "Content-Length: %zu\r\nContent-Type: application/json\r\nOrigin: http://127.0.0.1:18080\r\n", sizeof(intent)-1);
    size_t size = frame(bytes, "POST", "/api/acquisition-plan", headers, intent, sizeof(intent)-1);
    int status = 0;
    check(http_request_extent(bytes, size, &info, &status) == 1, "acquisition body admitted");
    check(http_request_extent(bytes, size - (sizeof(intent)-1), &info, &status) == 0, "split body waits after header");
    check(http_request_extent(bytes, size-1, &info, &status) == 0, "split body waits through final byte");
    struct http_response response;
    http_handle(bytes, size, &info, &response);
    check(response.status == 503 && strstr(response.body, "auth_unavailable"), "unselected acquisition service unavailable");
    http_response_dispose(&response);
    size = frame(bytes, "POST", "/api/jobs", headers, intent, sizeof(intent)-1);
    check(http_request_extent(bytes, size, &info, &status) == 1, "existing job body remains admitted");
    http_handle(bytes, size, &info, &response);
    check(response.status == 503 && strstr(response.body, "jobs_unavailable"), "existing unselected jobs behavior preserved");
    http_response_dispose(&response);
    unsigned char capped[HTTP_JOB_BODY_LIMIT]; memset(capped, ' ', sizeof(capped)); capped[0]='{';capped[sizeof(capped)-1]='}';
    size = frame(bytes,"POST","/api/acquisition-plan","Content-Length: 4096\r\nContent-Type: application/json\r\n",capped,sizeof(capped));
    check(http_request_extent(bytes,size,&info,&status)==1,"4096 byte cap admitted");
    size = frame(bytes,"POST","/api/acquisition-plan","Content-Length: 4097\r\nContent-Type: application/json\r\n",NULL,0);
    check(http_request_extent(bytes,size,&info,&status)<0&&status==413,"4097 byte declaration refused");
    const char *bad_headers[] = {
        "Content-Length: 2\r\nContent-Length: 2\r\nContent-Type: application/json\r\n",
        "Content-Length: 2\r\nTransfer-Encoding: chunked\r\nContent-Type: application/json\r\n",
        "Content-Length: 02\r\nContent-Type: application/json\r\n",
        "Content-Length: 2\r\nContent-Type: text/plain\r\n",
        "Content-Length: 2\r\n",
        "Content-Type: application/json\r\n"
    };
    for(size_t i=0;i<sizeof(bad_headers)/sizeof(*bad_headers);++i){
        size=frame(bytes,"POST","/api/acquisition-plan",bad_headers[i],(const unsigned char*)"{}",2);
        check(http_request_extent(bytes,size,&info,&status)<0&&status==400,"malformed acquisition framing refused");
    }
    size=frame(bytes,"POST","/api/acquisition-plan","Content-Length: 2\r\nContent-Type: application/json\r\nOrigin: http://wrong.example\r\n",(const unsigned char*)"{}",2);
    check(http_request_extent(bytes,size,&info,&status)<0&&status==403,"foreign origin refused");
    size=frame(bytes,"POST","/api/acquisition-plan?extra=1","Content-Length: 2\r\nContent-Type: application/json\r\n",(const unsigned char*)"{}",2);
    check(http_request_extent(bytes,size,&info,&status)<0,"noncanonical collection post refused");
    size=frame(bytes,"POST","/api/acquisition-plan","Content-Length: 2\r\nContent-Type: application/json\r\n",(const unsigned char*)"{}x",3);
    check(http_request_extent(bytes,size,&info,&status)<0,"trailing body bytes refused");
    printf("{\"checks\":%u,\"failures\":%u}\n",checks,failures);
    return failures?1:0;
}
