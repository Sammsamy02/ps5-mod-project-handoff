#include "http.h"
#include <stdio.h>
#include <string.h>

static void request(const char *target, unsigned char *bytes, size_t *size) {
    int n = snprintf((char *)bytes, HTTP_HEADER_LIMIT,
                     "GET %s HTTP/1.1\r\nHost: 127.0.0.1:18080\r\n\r\n", target);
    *size = n > 0 ? (size_t)n : 0;
}

int main(void) {
    struct host_info info = {0};
    struct http_response response;
    unsigned char bytes[HTTP_HEADER_LIMIT];
    size_t size = 0;
    strcpy(info.authority, "127.0.0.1:18080");
    strcpy(info.url_origin, "http://127.0.0.1:18080");
    request("/app.js", bytes, &size);
    http_handle(bytes, size, &info, &response);
    if (response.status != 200 || !response.static_body || !response.static_size ||
        response.static_body == (const unsigned char *)response.body || response.size != 0)
        return 1;

    http_error(&response, 404);
    if (response.status != 404 || response.static_body || response.static_size || response.size == 0)
        return 2;

    request("/app", bytes, &size);
    http_handle(bytes, size, &info, &response);
    if (response.status != 200 || !response.static_body || !response.static_size)
        return 3;
    request("/app?query=1", bytes, &size);
    http_handle(bytes, size, &info, &response);
    if (response.status != 404 || response.static_body || response.static_size)
        return 4;
    request("/api/mod-service", bytes, &size);
    http_handle(bytes, size, &info, &response);
    if (response.status != 503) return 5;
    request("/api/mod-service/records", bytes, &size);
    http_handle(bytes, size, &info, &response);
    if (response.status != 503) return 6;
    size = (size_t)snprintf((char *)bytes, sizeof(bytes),
        "POST /api/mod-service/records HTTP/1.1\r\nHost: 127.0.0.1:18080\r\nContent-Length: 0\r\n\r\n");
    http_handle(bytes, size, &info, &response);
    if (response.status != 405) return 7;
    return 0;
}
