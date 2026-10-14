#ifndef MODBRIDGE_HTTP_H
#define MODBRIDGE_HTTP_H
#include <stddef.h>
#include "core_bridge.h"
#include "entropy.h"
#include "drbg.h"
#include <sys/types.h>
#define HTTP_HEADER_LIMIT 4096
#define HTTP_BODY_LIMIT 8192
struct host_info {
    const char *version, *build_id, *profile, *origin;
    char authority[64], url_origin[80];
    pid_t pid;
    uid_t uid, euid;
    long page_size;
    int page_errno;
    unsigned lifetime;
    char core_report[MB_CORE_REPORT_CAPACITY];
    struct mb_entropy_result entropy;
    struct mb_drbg_result drbg;
};
struct http_static_asset {
    const unsigned char *bytes;
    size_t size;
};
extern const struct http_static_asset http_asset_index_html;
extern const struct http_static_asset http_asset_app_js;
extern const struct http_static_asset http_asset_style_css;
struct http_response {
    int status, shutdown;
    const char *content_type;
    const char *csp;
    char body[HTTP_BODY_LIMIT];
    size_t size;
    const unsigned char *static_body;
    size_t static_size;
};
void http_error(struct http_response *, int);
void http_handle(const unsigned char *, size_t, const struct host_info *, struct http_response *);
const char *http_reason(int);
const char *http_csp(void);
const char *http_app_csp(void);
#endif
