#ifndef MODBRIDGE_HTTP_H
#define MODBRIDGE_HTTP_H
#include <stddef.h>
#include <stdint.h>
#include "core_bridge.h"
#include "entropy.h"
#include "drbg.h"
#include <sys/types.h>
#define HTTP_HEADER_LIMIT 4096
#define HTTP_BODY_LIMIT 8192
#define HTTP_AUTH_BODY_LIMIT 16
/* Shared bounded JSON envelope for job and explicit generation submissions. */
#define HTTP_JOB_BODY_LIMIT 4096
#define HTTP_METADATA_LIMIT 16777216u
#define HTTP_DOWNLOADS_LIMIT 262144u
struct mb_mod_service_bridge;
struct mb_nexus_auth_context_v1;
struct mb_nexus_jobs_context_v1;
struct mb_download_store;
struct host_download_runtime {
    unsigned selected, initialized, terminal, window_active, window_expired;
    uint64_t entropy_windows, entropy_callbacks, entropy_bytes, clock_samples;
};
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
    struct host_download_runtime download_runtime;
    struct mb_nexus_auth_context_v1 *auth;
    struct mb_nexus_jobs_context_v1 *jobs;
    struct mb_download_store *download_store; /* borrowed device-local archive store */
    const struct mb_mod_service_bridge *mod_service; /* borrowed; NULL by default */
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
    char cookie[128], location[16];
    char body[HTTP_BODY_LIMIT];
    size_t size;
    const unsigned char *static_body;
    size_t static_size;
    char *owned_body;
    size_t owned_capacity;
};
/* Release one initialized response after transmission or abandonment. */
void http_response_dispose(struct http_response *);
void http_error(struct http_response *, int);
/* 0: incomplete; 1: one complete frame; negative: status describes refusal. */
int http_request_extent(const unsigned char *, size_t, const struct host_info *, int *status);
void http_handle(const unsigned char *, size_t, const struct host_info *, struct http_response *);
const char *http_reason(int);
const char *http_csp(void);
const char *http_app_csp(void);
#endif
