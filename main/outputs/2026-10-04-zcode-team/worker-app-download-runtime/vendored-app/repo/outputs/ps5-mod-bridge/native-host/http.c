#include "http.h"
#include "mod_service/read_bridge.h"
#ifndef MB_HOST_DOWNLOAD
#define MB_HOST_DOWNLOAD 0
#endif
#ifndef MB_HOST_JOBS
#define MB_HOST_JOBS 0
#endif
#ifndef MB_HOST_DOWNLOAD_STORE
#define MB_HOST_DOWNLOAD_STORE MB_HOST_JOBS
#endif
#ifndef MB_HOST_OAUTH_TIMEOUT_MS
#define MB_HOST_OAUTH_TIMEOUT_MS 10000u
#endif
#ifndef MB_HOST_OAUTH_CONNECT_TIMEOUT_MS
#define MB_HOST_OAUTH_CONNECT_TIMEOUT_MS 5000u
#endif
#if MB_HOST_DOWNLOAD
#include "auth.h"
#include "download_store/store.h"
#if MB_HOST_JOBS
#include "jobs.h"
#include "download_store/store.h"
#endif
#endif
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/wait.h>

static const char page[] =
"<!doctype html><html lang=\"en\"><meta charset=\"utf-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>Modbridge native status</title><style>body{background:#111827;color:#f9fafb;"
"font:24px system-ui;max-width:1100px;margin:5vh auto;padding:24px}button{font:inherit;"
"padding:16px;margin:16px 0}pre{white-space:pre-wrap;overflow-wrap:anywhere;font-size:20px}"
"button:focus{outline:4px solid #60a5fa}</style><h1>Modbridge native status</h1>"
"<p>Application runtime status. Installation is unavailable in this checkpoint.</p>"
"<button id=\"stop\" type=\"button\">Shut down this application</button>"
"<pre id=\"status\" role=\"status\">Loading native report…</pre><script>fetch('/report').then(r=>r.text()).then(r=>{document.getElementById('status').textContent=r});document.getElementById('stop').addEventListener('click',()=>{fetch('/shutdown',{method:'POST',headers:{'X-Modbridge-Shutdown':'1'}}).then(r=>r.text()).then(r=>{document.getElementById('status').textContent=r})});</script></html>";

const char *http_csp(void) {
    return "default-src 'none'; connect-src 'self'; style-src 'unsafe-inline'; "
           "script-src 'sha256-592HA5tyxWCjT8/6wd/Ih6t8yGFKrOGOaE4XqZLW6ro='; "
           "base-uri 'none'; form-action 'none'; frame-ancestors 'none'";
}
const char *http_app_csp(void) {
    return "default-src 'none'; connect-src 'self'; script-src 'self'; style-src 'self'; "
           "img-src 'none'; object-src 'none'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'";
}
const char *http_reason(int status) {
    switch (status) {
    case 200: return "OK";
    case 202: return "Accepted";
    case 303: return "See Other";
    case 401: return "Unauthorized";
    case 409: return "Conflict";
    case 413: return "Payload Too Large";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Timeout";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 408: return "Request Timeout";
    case 431: return "Request Header Fields Too Large";
    default: return "Internal Server Error";
    }
}
void http_error(struct http_response *out, int status) {
    memset(out, 0, sizeof(*out));
    out->status = status;
    out->content_type = "application/json";
    int n = snprintf(out->body, sizeof(out->body), "{\"error\":\"%s\"}\n", http_reason(status));
    out->size = n > 0 ? (size_t)n : 0;
}
static int static_response(struct http_response *out,
                           const struct http_static_asset *asset,
                           const char *content_type) {
    if (!asset || !asset->bytes || !asset->size || asset->size > 256u * 1024u)
        return -1;
    out->status = 200;
    out->content_type = content_type;
    out->csp = http_app_csp();
    out->size = 0;
    out->static_body = asset->bytes;
    out->static_size = asset->size;
    return 0;
}
static int json_response(struct http_response *out, const char *value) {
    size_t length = strlen(value);
    if (length >= sizeof(out->body)) return -1;
    memcpy(out->body, value, length + 1);
    out->size = length;
    out->content_type = "application/json";
    return 0;
}
static int token(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || strchr("!#$%&'*+-.^_`|~", c) != NULL;
}
static char *line_end(char *p) { return strstr(p, "\r\n"); }

static int job_resume_target(const char *target, const char **id_out) {
    static const char prefix[] = "/api/jobs/";
    static const char suffix[] = "/resume";
    const size_t id_bytes = 32;
    if (strncmp(target, prefix, sizeof(prefix) - 1)) return 0;
    const char *id = target + sizeof(prefix) - 1;
    if (strlen(id) != id_bytes + sizeof(suffix) - 1 ||
        memcmp(id + id_bytes, suffix, sizeof(suffix) - 1)) return 0;
    for (size_t i = 0; i < id_bytes; ++i)
        if (!((id[i] >= 'a' && id[i] <= 'f') || (id[i] >= '0' && id[i] <= '9'))) return 0;
    if (id_out) *id_out = id;
    return 1;
}

static int entropy_report(char *out, size_t capacity, const struct mb_entropy_result *result) {
    char rc[32] = "null", error[32] = "null", length[32] = "null", elapsed[32] = "null";
    if (result->call_count) {
        snprintf(rc, sizeof(rc), "%d", result->return_code);
        snprintf(error, sizeof(error), "%d", result->error_number);
        snprintf(length, sizeof(length), "%zu", result->returned_length);
        if (result->elapsed_ms >= 0) snprintf(elapsed, sizeof(elapsed), "%" PRId64, result->elapsed_ms);
    }
    int n = snprintf(out, capacity,
        "{\"status\":\"%s\",\"reason\":\"%s\",\"api\":\"sysctl(KERN_ARND)\",\"scope\":\"api_result_only\","
        "\"requested_bytes\":32,\"call_count\":%u,\"return_code\":%s,\"errno\":%s,"
        "\"returned_length_bytes\":%s,\"elapsed_ms\":%s,\"hard_deadline_enforced\":false}",
        result->status, result->reason, result->call_count, rc, error, length, elapsed);
    return n < 0 || (size_t)n >= capacity ? -1 : 0;
}

static int drbg_report(char *out, size_t capacity, const struct mb_drbg_result *result) {
    char add[32]="null", seed[32]="null", generate[32]="null", completed[32]="null", elapsed[32]="null";
    char version[48]="null", config[80]="null", failure[320]="null";
    if (result->add_source_attempted) snprintf(add,sizeof(add),"%d",result->add_source_rc);
    if (result->seed_attempted) snprintf(seed,sizeof(seed),"%d",result->seed_rc);
    if (result->generate_attempted) {
        snprintf(generate,sizeof(generate),"%d",result->generate_rc);
        if (!result->generate_rc) snprintf(completed,sizeof(completed),"%zu",result->completed_length);
    }
    if (result->elapsed_ms >= 0) snprintf(elapsed,sizeof(elapsed),"%" PRId64,result->elapsed_ms);
    int n;
    if (result->mbedtls_version) {
        n=snprintf(version,sizeof(version),"\"%s\"",result->mbedtls_version);
        if (n<0 || (size_t)n>=sizeof(version)) return -1;
    }
    if (result->config_id) {
        n=snprintf(config,sizeof(config),"\"%s\"",result->config_id);
        if (n<0 || (size_t)n>=sizeof(config)) return -1;
    }
    if (result->source_failed) {
        char rc[32]="null", error[32]="null", length[32]="null", policy[48]="null";
        if (result->has_source_result) {
            snprintf(rc,sizeof(rc),"%d",result->source_result.return_code);
            snprintf(error,sizeof(error),"%d",result->source_result.error_number);
            snprintf(length,sizeof(length),"%zu",result->source_result.returned_length);
        }
        if (result->policy_failure) {
            n=snprintf(policy,sizeof(policy),"\"%s\"",result->policy_failure);
            if (n<0 || (size_t)n>=sizeof(policy)) return -1;
        }
        n=snprintf(failure,sizeof(failure),
            "{\"reason\":\"%s\",\"policy_failure\":%s,\"return_code\":%s,\"errno\":%s,\"returned_length_bytes\":%s}",
            result->has_source_result ? result->source_result.reason : "not_observed",policy,rc,error,length);
        if (n<0 || (size_t)n>=sizeof(failure)) return -1;
    }
    n=snprintf(out,capacity,
        "{\"status\":\"%s\",\"reason\":\"%s\",\"scope\":\"owned_startup_checkpoint\","
        "\"mbedtls_version\":%s,\"config_id\":%s,\"requested_generation_bytes\":32,"
        "\"add_source_rc\":%s,\"seed_rc\":%s,\"generate_rc\":%s,\"completed_length_bytes\":%s,"
        "\"source_callback_count\":%u,\"source_os_call_count\":%u,\"total_api_call_count\":%u,"
        "\"api_call_limit\":%u,\"elapsed_ms\":%s,\"hard_deadline_enforced\":false,\"source_failure\":%s}",
        result->status ? result->status : "unavailable",result->reason ? result->reason : "desktop_fixture",
        version,config,add,seed,generate,completed,result->source_callback_count,result->source_os_call_count,
        result->total_api_call_count,result->api_call_limit ? result->api_call_limit : MB_ENTROPY_API_LIMIT,elapsed,failure);
    return n<0 || (size_t)n>=capacity ? -1 : 0;
}

static int download_runtime_report(char *out, size_t capacity,
                                   const struct host_download_runtime *result) {
    const char *status = !result->selected ? "not_selected" :
                         result->initialized ? "initialized" : "failed";
    const char *scope = result->selected ? "process_runtime" : "none";
    int n = snprintf(out, capacity,
        "{\"selected\":%s,\"status\":\"%s\",\"scope\":\"%s\","
        "\"initialized\":%s,\"terminal\":%s,\"window_active\":%s,\"window_expired\":%s,"
        "\"entropy_windows\":%" PRIu64 ",\"entropy_callbacks\":%" PRIu64 ","
        "\"entropy_bytes\":%" PRIu64 ",\"clock_samples\":%" PRIu64 "}",
        result->selected ? "true" : "false", status, scope,
        result->initialized ? "true" : "false",
        result->terminal ? "true" : "false",
        result->window_active ? "true" : "false",
        result->window_expired ? "true" : "false",
        result->entropy_windows, result->entropy_callbacks,
        result->entropy_bytes, result->clock_samples);
    return n < 0 || (size_t)n >= capacity ? -1 : 0;
}

static void wipe_http(void *value, size_t size) {
    volatile unsigned char *p = value;
    while (size--) *p++ = 0;
}
void http_response_dispose(struct http_response *out) {
    if (!out) return;
    if (out->owned_body) {
        wipe_http(out->owned_body, out->owned_capacity);
        free(out->owned_body);
    }
    wipe_http(out, sizeof(*out));
}
struct parsed_request {
    char storage[HTTP_HEADER_LIMIT + HTTP_JOB_BODY_LIMIT + 1];
    char cookie[44];
    char *method, *target, *origin, *csrf;
    int length_seen, shutdown_seen, shutdown_value;
    const unsigned char *body;
    size_t body_bytes;
};
static int empty_object(const unsigned char *p, size_t size) {
    size_t i = 0;
    while (i < size && (p[i] != 0 && strchr(" \t\r\n", p[i]))) ++i;
    if (i == size || p[i++] != '{') return 0;
    while (i < size && (p[i] != 0 && strchr(" \t\r\n", p[i]))) ++i;
    if (i == size || p[i++] != '}') return 0;
    while (i < size && (p[i] != 0 && strchr(" \t\r\n", p[i]))) ++i;
    return i == size;
}
static int session_cookie(char *value, char output[44]) {
    int seen = 0;
    while (*value) {
        while (*value == ' ' || *value == '\t') ++value;
        char *end = strchr(value, ';');
        if (!end) end = value + strlen(value);
        char *tail = end;
        while (tail > value && (tail[-1] == ' ' || tail[-1] == '\t')) --tail;
        if (tail == value) return -1;
        char *equal = memchr(value, '=', (size_t)(tail - value));
        if (!equal || equal == value) return -1;
        if ((size_t)(equal - value) == 10 && !memcmp(value, "mb_session", 10)) {
            if (seen++ || tail - equal - 1 != 43) return -1;
            for (char *p = equal + 1; p < tail; ++p)
                if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                      (*p >= '0' && *p <= '9') || *p == '_' || *p == '-')) return -1;
            memcpy(output, equal + 1, 43); output[43] = 0;
        }
        if (!*end) break;
        value = end + 1;
        if (!*value) return -1;
    }
    return 0;
}
static int parse_request(const unsigned char *bytes, size_t size,
                         const struct host_info *info, struct parsed_request *request,
                         int *status) {
    *status = 400;
    size_t header_size = 0;
    for (size_t i = 3; i < size && i < HTTP_HEADER_LIMIT; ++i)
        if (!memcmp(bytes + i - 3, "\r\n\r\n", 4)) { header_size = i + 1; break; }
    if (!header_size) {
        if (size >= HTTP_HEADER_LIMIT) { *status = 431; return -1; }
        return 0;
    }
    if (size > HTTP_HEADER_LIMIT + HTTP_JOB_BODY_LIMIT) return -1;
    for (size_t i = 0; i < header_size; ++i)
        if (!bytes[i] || bytes[i] > 126 ||
            (bytes[i] < 32 && bytes[i] != '\r' && bytes[i] != '\n' && bytes[i] != '\t')) return -1;
    memset(request, 0, sizeof(*request));
    memcpy(request->storage, bytes, size);
    char *input = request->storage;
    char *end = line_end(input);
    if (!end) return -1;
    *end = 0;
    char *method = input, *target = strchr(method, ' ');
    if (!target) return -1;
    *target++ = 0;
    char *version = strchr(target, ' ');
    if (!version) return -1;
    *version++ = 0;
    if (!*method || !*target || strcmp(version, "HTTP/1.1") ||
        strchr(target, ' ') || strchr(version, ' ')) return -1;
    for (char *p = method; *p; ++p) if (!token((unsigned char)*p)) return -1;
    for (char *p = target; *p; ++p) if ((unsigned char)*p <= 32) return -1;
    int auth_post = !strcmp(method, "POST") &&
        (!strcmp(target, "/api/auth/start") || !strcmp(target, "/api/auth/disconnect"));
    int json_post = !strcmp(method, "POST") &&
        (!strcmp(target, "/api/jobs") || !strcmp(target, "/api/generations") ||
         !strcmp(target, "/api/acquisition-plan") ||
         !strcmp(target, "/api/acquisitions") || job_resume_target(target, NULL));
    int service_post = !strcmp(method, "POST") &&
        !strcmp(target, "/api/mod-service/request");
    int modmgr_install_post = !strcmp(method, "POST") &&
        !strcmp(target, "/api/install");
    size_t body_limit = auth_post ? HTTP_AUTH_BODY_LIMIT :
        (json_post || service_post || modmgr_install_post) ? HTTP_JOB_BODY_LIMIT : 0;
    int host_seen = 0, origin_seen = 0, cookie_seen = 0, csrf_seen = 0, type_seen = 0;
    size_t length = 0;
    char *cursor = end + 2;
    for (;;) {
        end = line_end(cursor);
        if (!end) return -1;
        if (end == cursor) {
            if (end + 2 != input + header_size) return -1;
            break;
        }
        *end = 0;
        char *colon = strchr(cursor, ':');
        if (!colon || colon == cursor) return -1;
        for (char *p = cursor; p < colon; ++p) if (!token((unsigned char)*p)) return -1;
        *colon = 0;
        char *value = colon + 1;
        while (*value == ' ' || *value == '\t') ++value;
        char *tail = end;
        while (tail > value && (tail[-1] == ' ' || tail[-1] == '\t')) *--tail = 0;
        for (char *p = value; *p; ++p) if ((unsigned char)*p < 32 && *p != '\t') return -1;
        if (!strcasecmp(cursor, "Host")) {
            if (host_seen++ || strcmp(value, info->authority)) return -1;
        } else if (!strcasecmp(cursor, "Content-Length")) {
            if (request->length_seen++ || !*value ||
                (*value == '0' && value[1]) || strlen(value) > 10) return -1;
            for (char *p = value; *p; ++p) {
                if (*p < '0' || *p > '9') return -1;
                length = length * 10 + (unsigned)(*p - '0');
            }
            if (body_limit && length > body_limit) { *status = 413; return -1; }
            if (!body_limit && length) return -1;
        } else if (!strcasecmp(cursor, "Transfer-Encoding")) return -1;
        else if (!strcasecmp(cursor, "Origin")) {
            if (origin_seen++) return -1;
            request->origin = value;
        } else if (!strcasecmp(cursor, "Cookie")) {
            if (cookie_seen++ || session_cookie(value, request->cookie)) return -1;
        } else if (!strcasecmp(cursor, "X-CSRF-Token")) {
            if (csrf_seen++) return -1;
            request->csrf = value;
        } else if (!strcasecmp(cursor, "Content-Type")) {
            if (type_seen++) return -1;
            if (body_limit && strcmp(value, "application/json")) return -1;
        } else if (!strcasecmp(cursor, "X-Modbridge-Shutdown")) {
            if (request->shutdown_seen++) return -1;
            request->shutdown_value = !strcmp(value, "1");
        }
        cursor = end + 2;
    }
    if (!host_seen) return -1;
    if (request->origin && strcmp(request->origin, info->url_origin)) { *status = 403; return -1; }
    if (body_limit && (!request->length_seen || !type_seen || length < 2)) return -1;
    if (size > header_size + length) return -1;
    if (size < header_size + length) return 0;
    if (auth_post && !empty_object(bytes + header_size, length)) return -1;
    request->body = (const unsigned char *)request->storage + header_size;
    request->body_bytes = length;
    request->method = method; request->target = target;
    return 1;
}
int http_request_extent(const unsigned char *bytes, size_t size,
                        const struct host_info *info, int *status) {
    struct parsed_request request;
    int result = parse_request(bytes, size, info, &request, status);
    wipe_http(&request, sizeof(request));
    return result;
}
static void auth_error(struct http_response *out, int status, const char *code) {
    http_error(out, status);
    int n = snprintf(out->body, sizeof(out->body),
        "{\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}", code, http_reason(status));
    out->size = n > 0 && (size_t)n < sizeof(out->body) ? (size_t)n : 0;
}
#if MB_HOST_DOWNLOAD
static void auth_status_error(struct http_response *out, mb_nexus_auth_status_v1 status) {
    int http = 500;
    const char *code = "auth_failed";
    switch (status) {
    case MB_NEXUS_AUTH_NOT_CONFIGURED: http = 503; code = "auth_unavailable"; break;
    case MB_NEXUS_AUTH_SESSION_REQUIRED: http = 401; code = "session_required"; break;
    case MB_NEXUS_AUTH_RECONNECT_REQUIRED: http = 401; code = "reconnect_required"; break;
    case MB_NEXUS_AUTH_REFUSED: http = 403; code = "auth_refused"; break;
    case MB_NEXUS_AUTH_BAD_ARGUMENT: http = 400; code = "invalid_input"; break;
    case MB_NEXUS_AUTH_BUSY: http = 409; code = "auth_busy"; break;
    case MB_NEXUS_AUTH_TRANSPORT_FAILED: http = 502; code = "provider_unavailable"; break;
    case MB_NEXUS_AUTH_PAYLOAD_FAILED: http = 502; code = "provider_invalid"; break;
    case MB_NEXUS_AUTH_TIMEOUT: http = 504; code = "provider_timeout"; break;
    case MB_NEXUS_AUTH_CANCELLED: http = 503; code = "auth_cancelled"; break;
    case MB_NEXUS_AUTH_SOURCE_FAILED: http = 503; code = "auth_source_failed"; break;
    case MB_NEXUS_AUTH_CONTENT_HIDDEN: http = 403; code = "content_hidden"; break;
    default: break;
    }
    auth_error(out, http, code);
}
static int auth_response(struct parsed_request *request, const struct host_info *info,
                         struct http_response *out) {
    if (!info->auth) return 0;
    const char *cookie = *request->cookie ? request->cookie : NULL;
    size_t cookie_bytes = cookie ? 43u : 0u;
    mb_nexus_auth_result_v1 result = { .struct_size = sizeof(result), .abi_version = MB_NEXUS_AUTH_ABI_V1 };
    int rc;
    if (!strcmp(request->target, "/api/capabilities")) {
        char cookie_out[44], csrf_out[44];
        mb_nexus_auth_session_params_v1 params = {
            .struct_size = sizeof(params), .abi_version = MB_NEXUS_AUTH_ABI_V1,
            .cookie = cookie, .cookie_bytes = cookie_bytes,
            .cookie_output = cookie_out, .cookie_capacity = sizeof(cookie_out),
            .csrf_output = csrf_out, .csrf_capacity = sizeof(csrf_out)
        };
        rc = mb_nexus_auth_session_v1(info->auth, &params, &result);
        if (rc != MB_NEXUS_AUTH_OK || result.status != MB_NEXUS_AUTH_OK || !result.complete) {
            wipe_http(cookie_out, sizeof(cookie_out)); wipe_http(csrf_out, sizeof(csrf_out));
            auth_status_error(out, result.status); return 1;
        }
        int n = snprintf(out->body, sizeof(out->body),
            "{\"mode\":\"console_preview\",\"console_execution\":\"unverified\",\"csrf_token\":\"%s\",\"capabilities\":{"
            "\"nexus_catalog\":true,\"account_status\":true,\"nexus_auth_start\":true,"
            "\"detected_titles\":false,\"title_inventory_paging\":false,\"jobs\":%s,"
            "\"installed_records\":%s,\"mod_service_records\":%s,\"download_analyze\":%s,\"retained_downloads\":%s,\"prepared_translation\":false,"
            "\"deploy\":false,\"adapter_operation\":false}}", csrf_out,
            info->jobs ? "true" : "false", info->modmgr ? "true" : "false",
            mb_mod_service_usable(info->mod_service) ? "true" : "false",
            info->jobs ? "true" : "false", info->download_store ? "true" : "false");
        if (result.new_session) {
            int length = snprintf(out->cookie, sizeof(out->cookie),
                "mb_session=%s; HttpOnly; SameSite=Lax; Path=/", cookie_out);
            if (length < 0 || (size_t)length >= sizeof(out->cookie)) n = -1;
        }
        wipe_http(cookie_out, sizeof(cookie_out)); wipe_http(csrf_out, sizeof(csrf_out));
        if (n < 0 || (size_t)n >= sizeof(out->body)) http_error(out, 500);
        else { out->status = 200; out->size = (size_t)n; }
        return 1;
    }
    int start = !strcmp(request->target, "/api/auth/start");
    int disconnect = !strcmp(request->target, "/api/auth/disconnect");
    if (start || disconnect) {
        if (!request->origin || !request->csrf) { auth_error(out, 403, "auth_refused"); return 1; }
        if (start) {
            char url[4096];
            mb_nexus_auth_start_params_v1 params = {
                .struct_size = sizeof(params), .abi_version = MB_NEXUS_AUTH_ABI_V1,
                .cookie = cookie, .cookie_bytes = cookie_bytes,
                .csrf = request->csrf, .csrf_bytes = strlen(request->csrf),
                .origin = request->origin, .origin_bytes = strlen(request->origin),
                .authorization_url = url, .authorization_capacity = sizeof(url)
            };
            rc = mb_nexus_auth_start_v1(info->auth, &params, &result);
            if (rc == MB_NEXUS_AUTH_OK && result.status == MB_NEXUS_AUTH_OK && result.complete) {
                int n = snprintf(out->body, sizeof(out->body), "{\"authorization_url\":\"%s\"}", url);
                if (strchr(url, '"') || strchr(url, '\\') || n < 0 || (size_t)n >= sizeof(out->body))
                    http_error(out, 500);
                else { out->status = 200; out->size = (size_t)n; }
            } else auth_status_error(out, result.status);
            wipe_http(url, sizeof(url));
        } else {
            mb_nexus_auth_disconnect_params_v1 params = {
                .struct_size = sizeof(params), .abi_version = MB_NEXUS_AUTH_ABI_V1,
                .cookie = cookie, .cookie_bytes = cookie_bytes,
                .csrf = request->csrf, .csrf_bytes = strlen(request->csrf),
                .origin = request->origin, .origin_bytes = strlen(request->origin)
            };
            rc = mb_nexus_auth_disconnect_v1(info->auth, &params, &result);
            if (rc == MB_NEXUS_AUTH_OK && result.status == MB_NEXUS_AUTH_OK && result.complete) {
                out->status = 200; (void)json_response(out, "{\"status\":\"disconnected\"}");
            } else auth_status_error(out, result.status);
        }
        return 1;
    }
    if (!strncmp(request->target, "/auth/callback?", 15)) {
        mb_nexus_auth_callback_params_v1 params = {
            .struct_size = sizeof(params), .abi_version = MB_NEXUS_AUTH_ABI_V1,
            .cookie = cookie, .cookie_bytes = cookie_bytes,
            .target = request->target, .target_bytes = strlen(request->target),
            .timeout_ms = MB_HOST_OAUTH_TIMEOUT_MS,
            .connect_timeout_ms = MB_HOST_OAUTH_CONNECT_TIMEOUT_MS,
            .summary = out->body, .summary_capacity = sizeof(out->body)
        };
        rc = mb_nexus_auth_callback_v1(info->auth, &params, &result);
        if ((rc == MB_NEXUS_AUTH_OK && result.status == MB_NEXUS_AUTH_OK && result.complete) ||
            (rc == MB_NEXUS_AUTH_AUTHORIZATION_CANCELLED && result.status == MB_NEXUS_AUTH_AUTHORIZATION_CANCELLED)) {
            wipe_http(out->body, sizeof(out->body));
            out->status = 303; out->size = 0; strcpy(out->location, "/app");
        } else auth_status_error(out, result.status);
        return 1;
    }
    if (!strcmp(request->target, "/api/account")) {
        mb_nexus_auth_account_params_v1 params = {
            .struct_size = sizeof(params), .abi_version = MB_NEXUS_AUTH_ABI_V1,
            .cookie = cookie, .cookie_bytes = cookie_bytes,
            .timeout_ms = MB_HOST_OAUTH_TIMEOUT_MS,
            .connect_timeout_ms = MB_HOST_OAUTH_CONNECT_TIMEOUT_MS,
            .summary = out->body, .summary_capacity = sizeof(out->body)
        };
        rc = mb_nexus_auth_account_v1(info->auth, &params, &result);
        if (rc == MB_NEXUS_AUTH_OK && result.status == MB_NEXUS_AUTH_OK && result.complete) {
            out->status = 200; out->size = result.output_bytes;
        } else auth_status_error(out, result.status);
        return 1;
    }
    return 0;
}

static int catalog_route(const char *target, mb_nexus_auth_catalog_kind_v1 *kind,
                         char domain[81], uint64_t *mod_id) {
    *mod_id = 0; domain[0] = 0;
    if (!strcmp(target, "/api/games")) { *kind = MB_NEXUS_AUTH_CATALOG_GAMES; return 1; }
    if (strncmp(target, "/api/games/", 11)) return 0;
    const char *start = target + 11, *end = strchr(start, '/');
    if (!end || end == start || (size_t)(end - start) > 80) return -1;
    for (const char *p = start; p < end; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
              (p != start && (*p == '_' || *p == '-')))) return -1;
    memcpy(domain, start, (size_t)(end - start)); domain[end - start] = 0;
    if (!strcmp(end, "/mods")) { *kind = MB_NEXUS_AUTH_CATALOG_MODS; return 1; }
    if (strncmp(end, "/mods/", 6)) return -1;
    const char *id = end + 6;
    if (*id < '1' || *id > '9') return -1;
    while (*id >= '0' && *id <= '9') {
        unsigned digit = (unsigned)(*id++ - '0');
        if (*mod_id > (INT64_MAX - digit) / 10u) return -1;
        *mod_id = *mod_id * 10u + digit;
    }
    if (!*id) { *kind = MB_NEXUS_AUTH_CATALOG_MOD; return 1; }
    if (!strcmp(id, "/files")) { *kind = MB_NEXUS_AUTH_CATALOG_FILES; return 1; }
    if (!strcmp(id, "/requirements")) {
        if (*mod_id > INT32_MAX) return -1;
        *kind = MB_NEXUS_AUTH_CATALOG_REQUIREMENTS; return 1;
    }
    return -1;
}
static int acquisition_plan_response(struct parsed_request *request, const struct host_info *info,
                                      struct http_response *out) {
    if (strcmp(request->target, "/api/acquisition-plan")) return 0;
    if (strcmp(request->method, "POST")) { http_error(out, 405); return 1; }
    if (!info->auth) { auth_error(out, 503, "auth_unavailable"); return 1; }
    if (!request->origin || !request->csrf) { auth_error(out, 403, "auth_refused"); return 1; }
    char *output = malloc(MB_NEXUS_AUTH_ACQUISITION_OUTPUT_LIMIT);
    if (!output) { http_error(out, 503); return 1; }
    const char *cookie = *request->cookie ? request->cookie : NULL;
    mb_nexus_auth_acquisition_plan_params_v1 params = {
        .struct_size = sizeof(params), .abi_version = MB_NEXUS_AUTH_ABI_V1,
        .cookie = cookie, .cookie_bytes = cookie ? 43u : 0u,
        .csrf = request->csrf, .csrf_bytes = strlen(request->csrf),
        .origin = request->origin, .origin_bytes = strlen(request->origin),
        .intent = (const char *)request->body, .intent_bytes = request->body_bytes,
        .timeout_ms = MB_HOST_OAUTH_TIMEOUT_MS,
        .connect_timeout_ms = MB_HOST_OAUTH_CONNECT_TIMEOUT_MS,
        .output = output, .output_capacity = MB_NEXUS_AUTH_ACQUISITION_OUTPUT_LIMIT
    };
    mb_nexus_auth_result_v1 result = { .struct_size = sizeof(result), .abi_version = MB_NEXUS_AUTH_ABI_V1 };
    int rc = mb_nexus_auth_acquisition_plan_v1(info->auth, &params, &result);
    if (rc == MB_NEXUS_AUTH_OK && result.status == MB_NEXUS_AUTH_OK && result.complete &&
        result.output_bytes < MB_NEXUS_AUTH_ACQUISITION_OUTPUT_LIMIT && output[result.output_bytes] == 0) {
        out->owned_body = output; out->owned_capacity = MB_NEXUS_AUTH_ACQUISITION_OUTPUT_LIMIT;
        out->size = result.output_bytes; out->status = 200;
    } else {
        wipe_http(output, MB_NEXUS_AUTH_ACQUISITION_OUTPUT_LIMIT); free(output);
        auth_status_error(out, result.status);
    }
    return 1;
}
static int catalog_response(struct parsed_request *request, const struct host_info *info,
                            struct http_response *out) {
    char domain[81]; uint64_t mod_id;
    mb_nexus_auth_catalog_kind_v1 kind = MB_NEXUS_AUTH_CATALOG_GAMES;
    int route = catalog_route(request->target, &kind, domain, &mod_id);
    if (!route) return 0;
    if (strcmp(request->method, "GET")) { http_error(out, 405); return 1; }
    if (route < 0) { auth_error(out, 400, "invalid_input"); return 1; }
    if (!info->auth) { auth_error(out, 503, "catalog_unavailable"); return 1; }
    const char *cookie = *request->cookie ? request->cookie : NULL;
    mb_nexus_auth_admit_params_v1 admission = {
        .struct_size = sizeof(admission), .abi_version = MB_NEXUS_AUTH_ABI_V1,
        .cookie = cookie, .cookie_bytes = cookie ? 43u : 0u,
        .admission = MB_NEXUS_AUTH_ADMISSION_READ, .require_account = 1
    };
    mb_nexus_auth_result_v1 result = { .struct_size = sizeof(result), .abi_version = MB_NEXUS_AUTH_ABI_V1 };
    if (mb_nexus_auth_admit_v1(info->auth, &admission, &result) != MB_NEXUS_AUTH_OK || !result.complete) {
        auth_status_error(out, result.status); return 1;
    }
    char *output = malloc(HTTP_METADATA_LIMIT);
    if (!output) { http_error(out, 503); return 1; }
    mb_nexus_auth_catalog_params_v1 params = {
        .struct_size = sizeof(params), .abi_version = MB_NEXUS_AUTH_ABI_V1,
        .cookie = cookie, .cookie_bytes = cookie ? 43u : 0u,
        .kind = kind, .domain = *domain ? domain : NULL, .domain_bytes = strlen(domain),
        .mod_id = mod_id, .timeout_ms = MB_HOST_OAUTH_TIMEOUT_MS,
        .connect_timeout_ms = MB_HOST_OAUTH_CONNECT_TIMEOUT_MS,
        .output = output, .output_capacity = HTTP_METADATA_LIMIT
    };
    int rc = mb_nexus_auth_catalog_v1(info->auth, &params, &result);
    if (rc == MB_NEXUS_AUTH_OK && result.status == MB_NEXUS_AUTH_OK && result.complete &&
        result.output_bytes < HTTP_METADATA_LIMIT && output[result.output_bytes] == 0) {
        out->owned_body = output; out->owned_capacity = HTTP_METADATA_LIMIT;
        out->size = result.output_bytes; out->status = 200;
    } else {
        wipe_http(output, HTTP_METADATA_LIMIT); free(output);
        auth_status_error(out, result.status);
    }
    return 1;
}
#if MB_HOST_JOBS
static void jobs_status_error(struct http_response *out, mb_nexus_jobs_status_v1 status) {
    int http = 500; const char *code = "job_failed";
    switch (status) {
    case MB_NEXUS_JOBS_BAD_ARGUMENT: http = 400; code = "invalid_input"; break;
    case MB_NEXUS_JOBS_SESSION_REQUIRED: http = 401; code = "session_required"; break;
    case MB_NEXUS_JOBS_AUTH_REQUIRED: http = 401; code = "reconnect_required"; break;
    case MB_NEXUS_JOBS_REFUSED: http = 403; code = "auth_refused"; break;
    case MB_NEXUS_JOBS_INTENT_CONFLICT: http = 409; code = "intent_conflict"; break;
    case MB_NEXUS_JOBS_LIMIT_REACHED: http = 409; code = "job_limit"; break;
    case MB_NEXUS_JOBS_NOT_FOUND: http = 404; code = "not_found"; break;
    case MB_NEXUS_JOBS_BUSY: http = 409; code = "job_busy"; break;
    case MB_NEXUS_JOBS_SOURCE_FAILED: http = 503; code = "job_source_failed"; break;
    case MB_NEXUS_JOBS_STORAGE_FAILED: http = 503; code = "job_storage_failed"; break;
    case MB_NEXUS_JOBS_OUTPUT_TOO_SMALL: http = 413; code = "job_response_limit"; break;
    case MB_NEXUS_JOBS_ACQUISITION_UNRESOLVED: http = 409; code = "acquisition_unresolved"; break;
    default: break;
    }
    auth_error(out, http, code);
}
static int job_resume_response(struct parsed_request *request, const struct host_info *info,
                               struct http_response *out) {
    const char *id = NULL;
    if (!job_resume_target(request->target, &id)) return 0;
    if (strcmp(request->method, "POST")) { http_error(out, 405); return 1; }
    if (!info->jobs) { auth_error(out, 503, "jobs_unavailable"); return 1; }
    if (!request->origin || !request->csrf) {
        auth_error(out, 403, "auth_refused"); return 1;
    }
    char *output = malloc(MB_NEXUS_JOBS_OUTPUT_LIMIT);
    if (!output) { http_error(out, 503); return 1; }
    const char *cookie = *request->cookie ? request->cookie : NULL;
    mb_nexus_jobs_resume_params_v1 params = {
        .struct_size = sizeof(params), .abi_version = MB_NEXUS_JOBS_ABI_V1,
        .cookie = cookie, .cookie_bytes = cookie ? 43u : 0u,
        .csrf = request->csrf, .csrf_bytes = strlen(request->csrf),
        .origin = request->origin, .origin_bytes = strlen(request->origin),
        .id = id, .id_bytes = 32,
        .intent = request->body, .intent_bytes = request->body_bytes,
        .output = output, .output_capacity = MB_NEXUS_JOBS_OUTPUT_LIMIT
    };
    mb_nexus_jobs_result_v1 result = {
        .struct_size = sizeof(result), .abi_version = MB_NEXUS_JOBS_ABI_V1
    };
    int rc = mb_nexus_jobs_resume_v1(info->jobs, &params, &result);
    if (rc == MB_NEXUS_JOBS_OK && result.status == MB_NEXUS_JOBS_OK && result.complete &&
        result.output_bytes < MB_NEXUS_JOBS_OUTPUT_LIMIT && output[result.output_bytes] == 0) {
        out->owned_body = output; out->owned_capacity = MB_NEXUS_JOBS_OUTPUT_LIMIT;
        out->status = 202; out->size = result.output_bytes;
    } else {
        wipe_http(output, MB_NEXUS_JOBS_OUTPUT_LIMIT); free(output);
        jobs_status_error(out, result.status);
    }
    return 1;
}
static int jobs_response(struct parsed_request *request, const struct host_info *info,
                         struct http_response *out) {
    int generation = !strncmp(request->target, "/api/generations", 16);
    const char *collection_target = generation ? "/api/generations" : "/api/jobs";
    size_t collection_bytes = strlen(collection_target);
    int collection = !strcmp(request->target, collection_target);
    if (!collection && (strncmp(request->target, collection_target, collection_bytes) ||
                        request->target[collection_bytes] != '/')) return 0;
    int submit = collection && !strcmp(request->method, "POST");
    if (!submit && strcmp(request->method, "GET")) { http_error(out, 405); return 1; }
    if (!info->jobs) { auth_error(out, 503, "jobs_unavailable"); return 1; }
    const char *id = collection ? NULL : request->target + collection_bytes + 1;
    if (id) {
        if (strlen(id) != 32) { auth_error(out, 400, "invalid_input"); return 1; }
        for (const char *p = id; *p; ++p)
            if (!((*p >= 'a' && *p <= 'f') || (*p >= '0' && *p <= '9'))) {
                auth_error(out, 400, "invalid_input"); return 1;
            }
    }
    if (submit && (!request->origin || !request->csrf)) {
        auth_error(out, 403, "auth_refused"); return 1;
    }
    char *output = malloc(MB_NEXUS_JOBS_OUTPUT_LIMIT);
    if (!output) { http_error(out, 503); return 1; }
    const char *cookie = *request->cookie ? request->cookie : NULL;
    size_t cookie_bytes = cookie ? 43u : 0u;
    mb_nexus_jobs_result_v1 result = { .struct_size = sizeof(result), .abi_version = MB_NEXUS_JOBS_ABI_V1 };
    int rc;
    if (submit) {
        mb_nexus_jobs_submit_params_v1 params = {
            .struct_size = sizeof(params), .abi_version = MB_NEXUS_JOBS_ABI_V1,
            .cookie = cookie, .cookie_bytes = cookie_bytes,
            .csrf = request->csrf, .csrf_bytes = strlen(request->csrf),
            .origin = request->origin, .origin_bytes = strlen(request->origin),
            .intent = request->body, .intent_bytes = request->body_bytes,
            .output = output, .output_capacity = MB_NEXUS_JOBS_OUTPUT_LIMIT
        };
        rc = generation ? mb_nexus_generations_submit_v1(info->jobs, &params, &result) :
                          mb_nexus_jobs_submit_v1(info->jobs, &params, &result);
    } else if (collection) {
        mb_nexus_jobs_list_params_v1 params = {
            .struct_size = sizeof(params), .abi_version = MB_NEXUS_JOBS_ABI_V1,
            .cookie = cookie, .cookie_bytes = cookie_bytes,
            .output = output, .output_capacity = MB_NEXUS_JOBS_OUTPUT_LIMIT
        };
        rc = generation ? mb_nexus_generations_list_v1(info->jobs, &params, &result) :
                          mb_nexus_jobs_list_v1(info->jobs, &params, &result);
    } else {
        mb_nexus_jobs_get_params_v1 params = {
            .struct_size = sizeof(params), .abi_version = MB_NEXUS_JOBS_ABI_V1,
            .cookie = cookie, .cookie_bytes = cookie_bytes, .id = id, .id_bytes = 32,
            .output = output, .output_capacity = MB_NEXUS_JOBS_OUTPUT_LIMIT
        };
        rc = generation ? mb_nexus_generations_get_v1(info->jobs, &params, &result) :
                          mb_nexus_jobs_get_v1(info->jobs, &params, &result);
    }
    if (rc == MB_NEXUS_JOBS_OK && result.status == MB_NEXUS_JOBS_OK && result.complete &&
        result.output_bytes < MB_NEXUS_JOBS_OUTPUT_LIMIT && output[result.output_bytes] == 0) {
        out->owned_body = output; out->owned_capacity = MB_NEXUS_JOBS_OUTPUT_LIMIT;
        out->status = submit ? 202 : 200; out->size = result.output_bytes;
    } else {
        wipe_http(output, MB_NEXUS_JOBS_OUTPUT_LIMIT); free(output);
        jobs_status_error(out, result.status);
    }
    return 1;
}
static int acquisition_response(struct parsed_request *request, const struct host_info *info,
                                struct http_response *out) {
    if (strcmp(request->target, "/api/acquisitions")) return 0;
    if (strcmp(request->method, "POST")) { http_error(out, 405); return 1; }
    if (!info->jobs) { auth_error(out, 503, "jobs_unavailable"); return 1; }
    if (!request->origin || !request->csrf) {
        auth_error(out, 403, "auth_refused"); return 1;
    }
    char *output = malloc(MB_NEXUS_JOBS_OUTPUT_LIMIT);
    if (!output) { http_error(out, 503); return 1; }
    const char *cookie = *request->cookie ? request->cookie : NULL;
    size_t cookie_bytes = cookie ? 43u : 0u;
    mb_nexus_jobs_submit_params_v1 params = {
        .struct_size = sizeof(params), .abi_version = MB_NEXUS_JOBS_ABI_V1,
        .cookie = cookie, .cookie_bytes = cookie_bytes,
        .csrf = request->csrf, .csrf_bytes = strlen(request->csrf),
        .origin = request->origin, .origin_bytes = strlen(request->origin),
        .intent = request->body, .intent_bytes = request->body_bytes,
        .output = output, .output_capacity = MB_NEXUS_JOBS_OUTPUT_LIMIT
    };
    mb_nexus_jobs_result_v1 result = {
        .struct_size = sizeof(result), .abi_version = MB_NEXUS_JOBS_ABI_V1
    };
    int rc = mb_nexus_jobs_submit_acquisition_v1(info->jobs, &params, &result);
    if (rc == MB_NEXUS_JOBS_OK && result.status == MB_NEXUS_JOBS_OK && result.complete &&
        result.output_bytes < MB_NEXUS_JOBS_OUTPUT_LIMIT && output[result.output_bytes] == 0) {
        out->owned_body = output; out->owned_capacity = MB_NEXUS_JOBS_OUTPUT_LIMIT;
        out->status = 202; out->size = result.output_bytes;
    } else {
        wipe_http(output, MB_NEXUS_JOBS_OUTPUT_LIMIT); free(output);
        jobs_status_error(out, result.status);
    }
    return 1;
}
#endif
#endif
#if MB_HOST_DOWNLOAD
static int downloads_response(struct parsed_request *request,const struct host_info *info,struct http_response *out) {
    if(strcmp(request->target,"/api/downloads"))return 0;
    if(strcmp(request->method,"GET")){http_error(out,405);return 1;}
    if(!info->auth||!info->download_store){http_error(out,503);return 1;}
    mb_nexus_auth_admit_params_v1 admission={.struct_size=sizeof(admission),.abi_version=MB_NEXUS_AUTH_ABI_V1,
        .cookie=*request->cookie?request->cookie:NULL,.cookie_bytes=*request->cookie?43u:0u,
        .admission=MB_NEXUS_AUTH_ADMISSION_READ,.require_account=0};
    mb_nexus_auth_result_v1 result={.struct_size=sizeof(result),.abi_version=MB_NEXUS_AUTH_ABI_V1};
    if(mb_nexus_auth_admit_v1(info->auth,&admission,&result)!=MB_NEXUS_AUTH_OK||result.status!=MB_NEXUS_AUTH_OK||!result.complete){auth_status_error(out,result.status);return 1;}
#if MB_HOST_DOWNLOAD_STORE
    char *output=calloc(1,HTTP_DOWNLOADS_LIMIT);
    if(!output){http_error(out,503);return 1;}
    if(mb_download_store_list(info->download_store,output,HTTP_DOWNLOADS_LIMIT)!=MB_DOWNLOAD_STORE_OK){free(output);http_error(out,503);return 1;}
    out->owned_body=output;out->owned_capacity=HTTP_DOWNLOADS_LIMIT;out->status=200;out->size=strlen(output);return 1;
#else
    http_error(out,503);return 1;
#endif
}
#endif
static int mod_service_response(struct parsed_request *request, const struct host_info *info,
                                struct http_response *out) {
    int records = !strcmp(request->target, "/api/mod-service/records");
    int forward = !strcmp(request->target, "/api/mod-service/request");
    if (!records && !forward && strcmp(request->target, "/api/mod-service")) return 0;
    if (!forward && strcmp(request->method, "GET")) { http_error(out, 405); return 1; }
    if (forward && strcmp(request->method, "POST")) { http_error(out, 405); return 1; }
    if (!mb_mod_service_usable(info->mod_service)) { http_error(out, 503); return 1; }
#if MB_HOST_DOWNLOAD
    if (!info->auth) { http_error(out, 503); return 1; }
    mb_nexus_auth_admit_params_v1 admission = {
        .struct_size = sizeof(admission), .abi_version = MB_NEXUS_AUTH_ABI_V1,
        .cookie = *request->cookie ? request->cookie : NULL,
        .cookie_bytes = *request->cookie ? 43u : 0u,
        .admission = MB_NEXUS_AUTH_ADMISSION_READ, .require_account = 0
    };
    mb_nexus_auth_result_v1 result = { .struct_size = sizeof(result), .abi_version = MB_NEXUS_AUTH_ABI_V1 };
    if (mb_nexus_auth_admit_v1(info->auth, &admission, &result) != MB_NEXUS_AUTH_OK || !result.complete) {
        auth_status_error(out, result.status); return 1;
    }
    char *copy = NULL; size_t size = 0;
    int status;
    if (forward) {
        if (!request->body || !request->body_bytes ||
            request->body_bytes > MB_MOD_SERVICE_RESPONSE_LIMIT) { http_error(out, 400); return 1; }
        status = mb_mod_service_call(info->mod_service,
            (const char *)request->body, request->body_bytes, &copy, &size);
    } else {
        status = mb_mod_service_read(info->mod_service,
            records ? MB_MOD_SERVICE_RECORDS : MB_MOD_SERVICE_DESCRIBE, &copy, &size);
    }
    if (status != 200) { http_error(out, status); return 1; }
    out->owned_body = copy; out->owned_capacity = size + 1;
    out->size = size; out->status = 200;
#else
    http_error(out, 503);
#endif
    return 1;
}

#if MB_HOST_DOWNLOAD /* modmgr routes need the session runtime */
/* ---- modmgr core-loop routes (worker-app-backend-integration) -------------
 * GET    /api/installed[?title=T]   -> modmgr list (records+loadlist truth)
 * POST   /api/install               -> {"title","id","path"} archive already
 *                                      deposited in the intake dir by the
 *                                      download pipeline; runs modmgr install
 * POST   /api/mods/<id>/enable      -> guarded loadlist row restore
 * POST   /api/mods/<id>/disable     -> guarded loadlist row strip
 * DELETE /api/mods/<id>             -> modmgr uninstall (install --remove)
 * All routes run app_core/modmgr_api.py (fork/exec host driver); mutating
 * routes additionally stage the matching console command files with the
 * proven console_link emitter (drive protocol, app-side half). Disabled
 * entirely when info->modmgr is NULL (routes never match; capability stays
 * false). Console execution remains controller-only. */
#define MODMGR_OUTPUT_LIMIT (512u * 1024u)
#define MODMGR_PATH_LIMIT 256

static int modmgr_token_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_';
}

/* Query-param extraction from the raw target: ?key=value, value limited to
 * token/path characters. No percent-decoding is offered (refuse instead). */
static int modmgr_query_param(const char *target, const char *key,
                              char *value, size_t capacity) {
    const char *query = strchr(target, '?');
    if (!query) return 0;
    size_t key_len = strlen(key);
    const char *p = query + 1;
    while (*p) {
        const char *amp = strchr(p, '&');
        size_t chunk = amp ? (size_t)(amp - p) : strlen(p);
        if (chunk > key_len && !strncmp(p, key, key_len) && p[key_len] == '=') {
            size_t value_len = chunk - key_len - 1;
            if (value_len >= capacity) return -1;
            for (size_t i = 0; i < value_len; ++i) {
                char c = p[key_len + 1 + i];
                if (!modmgr_token_char(c) && c != '.' && c != '/') return -1;
                value[i] = c;
            }
            value[value_len] = 0;
            return (int)value_len;
        }
        if (!amp) break;
        p = amp + 1;
    }
    return 0;
}

/* Strict flat-JSON string field reader: "key"<ws>:<ws>"value". Escapes and
 * control characters are refused, not interpreted. 1 found, 0 absent. */
static int modmgr_json_string(const char *body, size_t size, const char *key,
                              char *value, size_t capacity) {
    char pattern[64];
    int n = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (n < 0 || (size_t)n >= sizeof(pattern)) return -1;
    size_t pattern_len = (size_t)n;
    if (size < pattern_len) return 0;
    const char *hit = NULL;
    for (size_t i = 0; i <= size - pattern_len; ++i)
        if (!memcmp(body + i, pattern, pattern_len)) { hit = body + i + pattern_len; break; }
    if (!hit) return 0;
    while (hit < body + size &&
           (*hit == ' ' || *hit == '\t' || *hit == '\n' || *hit == '\r' || *hit == ':')) ++hit;
    if (hit >= body + size || *hit != '"') return -1;
    ++hit;
    size_t out = 0;
    while (hit < body + size && *hit != '"') {
        if (*hit == '\\' || (unsigned char)*hit < 0x20) return -1;
        if (out + 1 >= capacity) return -1;
        value[out++] = *hit++;
    }
    if (hit >= body + size) return -1;
    value[out] = 0;
    return (int)out;
}

/* Fork/exec the python driver, capture bounded stdout. Returns malloc'd
 * NUL-terminated bytes (caller frees) or NULL on pipe/fork failure. */
static char *modmgr_exec(const struct mb_modmgr_runtime *m, const char *op,
                         const char *title, const char *mod_id,
                         const char *source, int *exit_code, size_t *out_len) {
    char *argv[20];
    int i = 0;
    argv[i++] = (char *)(m->python[0] ? m->python : "python3");
    argv[i++] = (char *)m->api_script;
    argv[i++] = (char *)op;
    argv[i++] = (char *)"--binary"; argv[i++] = (char *)m->binary;
    argv[i++] = (char *)"--state"; argv[i++] = (char *)m->state;
    argv[i++] = (char *)"--roots"; argv[i++] = (char *)m->roots;
    if (title) { argv[i++] = (char *)"--title"; argv[i++] = (char *)title; }
    if (mod_id) { argv[i++] = (char *)"--id"; argv[i++] = (char *)mod_id; }
    if (source) { argv[i++] = (char *)"--file"; argv[i++] = (char *)source; }
    argv[i] = NULL;
    int fds[2];
    if (pipe(fds)) return NULL;
    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return NULL; }
    if (!pid) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
        execvp(argv[0], argv);
        _exit(127);
    }
    close(fds[1]);
    size_t size = 0, capacity = 8192;
    char *buffer = (char *)malloc(capacity);
    if (!buffer) { close(fds[0]); waitpid(pid, NULL, 0); return NULL; }
    for (;;) {
        if (size + 4096 > capacity) {
            if (capacity >= MODMGR_OUTPUT_LIMIT) break;
            capacity *= 2;
            char *grown = (char *)realloc(buffer, capacity);
            if (!grown) break;
            buffer = grown;
        }
        ssize_t got = read(fds[0], buffer + size, capacity - size - 1);
        if (got < 0) { if (errno == EINTR) continue; break; }
        if (!got) break;
        size += (size_t)got;
    }
    close(fds[0]);
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) status = -1;
    buffer[size] = 0;
    *out_len = size;
    *exit_code = (status == -1 || !WIFEXITED(status)) ? -1 : WEXITSTATUS(status);
    return buffer;
}

/* Forward a driver envelope as the response body with the mapped status. */
static void modmgr_respond_envelope(struct http_response *out, char *bytes,
                                    size_t size, int exit_code) {
    if (exit_code == 0) {
        out->owned_body = bytes; out->owned_capacity = size + 1;
        out->content_type = "application/json";
        out->size = size; out->status = 200;
        return;
    }
    int status = 500;
    if (exit_code == 2) {
        char kind[32];
        int kind_len = bytes ? modmgr_json_string(bytes, size, "kind",
                                                  kind, sizeof(kind)) : 0;
        if (kind_len > 0) {
            if (!strcmp(kind, "not_found")) status = 404;
            else if (!strcmp(kind, "conflict") || !strcmp(kind, "stale_command")) status = 409;
            else if (!strcmp(kind, "ambiguous_title") || !strcmp(kind, "bad_request")) status = 400;
        }
        if (bytes) {
            out->owned_body = bytes; out->owned_capacity = size + 1;
            out->content_type = "application/json";
            out->size = size; out->status = status;
            return;
        }
    }
    free(bytes);
    static const char driver_failure[] =
        "{\"ok\":false,\"error\":{\"kind\":\"driver_failure\"}}";
    memcpy(out->body, driver_failure, sizeof(driver_failure));
    out->content_type = "application/json";
    out->size = sizeof(driver_failure) - 1;
    out->status = exit_code == 127 ? 502 : 500;
}

static void modmgr_respond_error(struct http_response *out, int status) {
    http_error(out, status);
}

static int modmgr_response(struct parsed_request *request, const struct host_info *info,
                           struct http_response *out) {
    if (!info->modmgr) return 0;
    char path[MODMGR_PATH_LIMIT];
    size_t path_len = strcspn(request->target, "?");
    if (path_len >= sizeof(path)) { modmgr_respond_error(out, 400); return 1; }
    memcpy(path, request->target, path_len); path[path_len] = 0;
    int installed = !strcmp(path, "/api/installed");
    int install = !strcmp(path, "/api/install");
    int mods = !strncmp(path, "/api/mods/", 10);
    if (!installed && !install && !mods) return 0;
#if MB_HOST_DOWNLOAD
    if (!info->auth) { modmgr_respond_error(out, 503); return 1; }
    int mutating = install || mods;
    int method_ok = installed ? !strcmp(request->method, "GET") :
                    install ? !strcmp(request->method, "POST") :
                    !strcmp(request->method, "POST") || !strcmp(request->method, "DELETE");
    if (!method_ok) { modmgr_respond_error(out, 405); return 1; }
    if (mutating && (!request->origin || !request->csrf)) {
        auth_error(out, 403, "auth_refused"); return 1;
    }
    mb_nexus_auth_admit_params_v1 admission = {
        .struct_size = sizeof(admission), .abi_version = MB_NEXUS_AUTH_ABI_V1,
        .cookie = *request->cookie ? request->cookie : NULL,
        .cookie_bytes = *request->cookie ? 43u : 0u,
        .csrf = request->csrf, .csrf_bytes = request->csrf ? strlen(request->csrf) : 0,
        .origin = request->origin, .origin_bytes = request->origin ? strlen(request->origin) : 0,
        .admission = mutating ? MB_NEXUS_AUTH_ADMISSION_MUTATION : MB_NEXUS_AUTH_ADMISSION_READ,
        .require_account = 0
    };
    mb_nexus_auth_result_v1 result = { .struct_size = sizeof(result), .abi_version = MB_NEXUS_AUTH_ABI_V1 };
    if (mb_nexus_auth_admit_v1(info->auth, &admission, &result) != MB_NEXUS_AUTH_OK ||
        result.status != MB_NEXUS_AUTH_OK || !result.complete) {
        auth_status_error(out, result.status); return 1;
    }
    const struct mb_modmgr_runtime *m = info->modmgr;
    char title[64], mod_id[64], source[MODMGR_PATH_LIMIT];
    const char *op = NULL, *title_arg = NULL, *id_arg = NULL, *file_arg = NULL;
    if (installed) {
        int got = modmgr_query_param(request->target, "title", title, sizeof(title));
        if (got < 0) { modmgr_respond_error(out, 400); return 1; }
        if (got > 0) title_arg = title;
        op = "list";
    } else if (install) {
        if (!request->body || !request->body_bytes) { modmgr_respond_error(out, 400); return 1; }
        if (modmgr_json_string((const char *)request->body, request->body_bytes,
                               "title", title, sizeof(title)) <= 0 ||
            modmgr_json_string((const char *)request->body, request->body_bytes,
                               "id", mod_id, sizeof(mod_id)) <= 0 ||
            modmgr_json_string((const char *)request->body, request->body_bytes,
                               "path", source, sizeof(source)) <= 0) {
            modmgr_respond_error(out, 400); return 1;
        }
        for (size_t i = 0; i < strlen(title); ++i)
            if (!modmgr_token_char(title[i])) { modmgr_respond_error(out, 400); return 1; }
        for (size_t i = 0; i < strlen(mod_id); ++i)
            if (!modmgr_token_char(mod_id[i])) { modmgr_respond_error(out, 400); return 1; }
        /* Intake containment: the archive must already sit in the intake dir
         * next to the state dir (download-pipeline deposit point). */
        char intake[MODMGR_PATH_LIMIT], resolved[MODMGR_PATH_LIMIT],
             intake_real[MODMGR_PATH_LIMIT];
        char state_copy[MODMGR_PATH_LIMIT];
        if (snprintf(state_copy, sizeof(state_copy), "%s", m->state) >= (int)sizeof(state_copy) ||
            snprintf(intake, sizeof(intake), "%s/intake",
                     dirname(state_copy)) >= (int)sizeof(intake) ||
            !realpath(source, resolved) ||
            !realpath(intake, intake_real) ||
            strncmp(resolved, intake_real, strlen(intake_real)) ||
            resolved[strlen(intake_real)] != '/') {
            modmgr_respond_error(out, 400); return 1;
        }
        op = "install"; title_arg = title; id_arg = mod_id; file_arg = resolved;
    } else {
        /* /api/mods/<id>[/enable|/disable] */
        const char *rest = path + 10;
        size_t id_len = 0;
        while (rest[id_len] && modmgr_token_char(rest[id_len])) ++id_len;
        if (!id_len || id_len >= sizeof(mod_id)) { modmgr_respond_error(out, 400); return 1; }
        memcpy(mod_id, rest, id_len); mod_id[id_len] = 0;
        const char *suffix = rest + id_len;
        if (!strcmp(request->method, "POST")) {
            if (!strcmp(suffix, "/enable")) op = "enable";
            else if (!strcmp(suffix, "/disable")) op = "disable";
            else { modmgr_respond_error(out, 404); return 1; }
        } else {
            if (*suffix) { modmgr_respond_error(out, 404); return 1; }
            op = "uninstall";
        }
        id_arg = mod_id;
        int got = modmgr_query_param(request->target, "title", title, sizeof(title));
        if (got < 0) { modmgr_respond_error(out, 400); return 1; }
        if (got > 0) title_arg = title;
    }
    int exit_code = 0;
    size_t size = 0;
    char *bytes = modmgr_exec(m, op, title_arg, id_arg, file_arg,
                              &exit_code, &size);
    if (!bytes) { modmgr_respond_error(out, 502); return 1; }
    modmgr_respond_envelope(out, bytes, size, exit_code);
#else
    (void)request; (void)path; (void)installed; (void)install; (void)mods;
    modmgr_respond_error(out, 503);
#endif
    return 1;
}

#endif
static void http_handle_request(struct parsed_request *request, const struct host_info *info,
                                struct http_response *out) {
    char *method = request->method, *target = request->target;
    int length_seen = request->length_seen, shutdown_seen = request->shutdown_seen;
    int shutdown_value = request->shutdown_value;
    if (mod_service_response(request, info, out)) return;
#if MB_HOST_DOWNLOAD
    if (modmgr_response(request, info, out)) return;
#endif
#if MB_HOST_DOWNLOAD
    if (downloads_response(request, info, out)) return;
    if (acquisition_plan_response(request, info, out)) return;
    if (catalog_response(request, info, out)) return;
#if MB_HOST_JOBS
    if (job_resume_response(request, info, out)) return;
    if (acquisition_response(request, info, out)) return;
    if (jobs_response(request, info, out)) return;
#endif
#endif
    if (!strcmp(target, "/api/acquisition-plan")) {
        if (strcmp(method, "POST")) { http_error(out, 405); return; }
        auth_error(out, 503, "auth_unavailable"); return;
    }
    if (!strcmp(target, "/api/acquisitions")) {
        if (strcmp(method, "POST")) { http_error(out, 405); return; }
        auth_error(out, 503, "jobs_unavailable"); return;
    }
    if ((!strcmp(target, "/api/jobs") || !strcmp(target, "/api/generations")) &&
        !strcmp(method, "POST")) {
        auth_error(out, 503, "jobs_unavailable"); return;
    }
    if (job_resume_target(target, NULL)) {
        if (strcmp(method, "POST")) { http_error(out, 405); return; }
        auth_error(out, 503, "jobs_unavailable"); return;
    }
    if (!strcmp(target, "/api/auth/start") || !strcmp(target, "/api/auth/disconnect")) {
        if (strcmp(method, "POST")) { http_error(out, 405); return; }
#if MB_HOST_DOWNLOAD
        if (auth_response(request, info, out)) return;
#endif
        auth_error(out, 503, "auth_unavailable"); return;
    }
#if MB_HOST_DOWNLOAD
    if ((!strcmp(target, "/api/capabilities") || !strcmp(target, "/api/account") ||
         !strncmp(target, "/auth/callback?", 15)) && !strcmp(method, "GET") &&
        auth_response(request, info, out)) return;
#endif
    if (!strcmp(target, "/shutdown")) {
        if (strcmp(method, "POST")) { http_error(out, 405); return; }
        if (!length_seen || !shutdown_seen || !shutdown_value) { http_error(out, 403); return; }
        out->shutdown = 1;
        strcpy(out->body, "{\"status\":\"shutdown_requested\"}\n");
    } else {
        if (strcmp(method, "GET")) { http_error(out, 405); return; }
        if (!strcmp(target, "/")) {
            memcpy(out->body, page, sizeof(page));
            out->content_type = "text/html; charset=utf-8";
        } else if (!strcmp(target, "/app")) {
            if (static_response(out, &http_asset_index_html, "text/html; charset=utf-8")) {
                http_error(out, 500); return;
            }
        } else if (!strcmp(target, "/app.js")) {
            if (static_response(out, &http_asset_app_js, "text/javascript; charset=utf-8")) {
                http_error(out, 500); return;
            }
        } else if (!strcmp(target, "/style.css")) {
            if (static_response(out, &http_asset_style_css, "text/css; charset=utf-8")) {
                http_error(out, 500); return;
            }
        } else if (!strcmp(target, "/api/capabilities")) {
            static const char response[] =
                "{\"mode\":\"console_preview\",\"console_execution\":\"unverified\",\"capabilities\":{"
                "\"nexus_catalog\":false,\"account_status\":false,\"nexus_auth_start\":false,"
                "\"detected_titles\":false,\"title_inventory_paging\":false,"
                "\"jobs\":false,\"installed_records\":false,\"mod_service_records\":false,"
                "\"download_analyze\":false,\"retained_downloads\":false,\"prepared_translation\":false,\"deploy\":false,"
                "\"adapter_operation\":false}}";
            if (json_response(out, response)) { http_error(out, 500); return; }
        } else if (!strcmp(target, "/api/account")) {
            static const char response[] =
                "{\"status\":\"disconnected\",\"live_service\":\"unavailable\","
                "\"console_translation\":\"unverified\"}";
            if (json_response(out, response)) { http_error(out, 500); return; }
        } else if (!strcmp(target, "/api/detected-titles")) {
            static const char response[] =
                "{\"schema_version\":1,\"status\":\"unavailable\",\"complete\":false,"
                "\"snapshot_id\":null,\"source_scope\":\"none\",\"items\":[],"
                "\"reason\":\"platform_provider_unavailable\",\"issues\":[]}";
            if (json_response(out, response)) { http_error(out, 500); return; }
        } else if (!strcmp(target, "/version")) {
            int n = snprintf(out->body, sizeof(out->body),
                "{\"version\":\"%s\",\"build_id\":\"%s\",\"profile\":\"%s\",\"origin\":\"%s\"}\n",
                info->version, info->build_id, info->profile, info->origin);
            if (n < 0 || (size_t)n >= sizeof(out->body)) { http_error(out, 500); return; }
        } else if (!strcmp(target, "/core")) {
            int n = snprintf(out->body, sizeof(out->body), "%s", info->core_report);
            if (n < 0 || (size_t)n >= sizeof(out->body)) { http_error(out, 500); return; }
        } else if (!strcmp(target, "/report")) {
            char page_result[160];
            char entropy_result[512];
            char drbg_result[1536];
            char download_runtime_result[512];
            if (entropy_report(entropy_result, sizeof(entropy_result), &info->entropy) ||
                drbg_report(drbg_result, sizeof(drbg_result), &info->drbg) ||
                download_runtime_report(download_runtime_result, sizeof(download_runtime_result),
                                        &info->download_runtime)) {
                http_error(out, 500); return;
            }
            if (info->page_size > 0) snprintf(page_result, sizeof(page_result),
                "{\"status\":\"observed\",\"value\":%ld,\"api\":\"sysconf(_SC_PAGESIZE)\"}", info->page_size);
            else snprintf(page_result, sizeof(page_result),
                "{\"status\":\"unavailable\",\"value\":null,\"api\":\"sysconf(_SC_PAGESIZE)\",\"errno\":%d}", info->page_errno);
            int n = snprintf(out->body, sizeof(out->body),
                "{\"schema_version\":1,\"version\":\"%s\",\"build_id\":\"%s\",\"profile\":\"%s\",\"origin\":\"%s\","
                "\"phase\":\"main_http_ready\",\"installation_supported\":false,\"authority\":\"%s\",\"lifetime_seconds\":%u,"
                "\"download_runtime\":%s,"
                "\"ownership_contract\":\"owned_socket_spawn_only\",\"observations\":{"
                "\"getpid\":{\"status\":\"observed\",\"value\":%ld,\"api\":\"getpid\"},"
                "\"getuid\":{\"status\":\"observed\",\"value\":%lu,\"api\":\"getuid\"},"
                "\"geteuid\":{\"status\":\"observed\",\"value\":%lu,\"api\":\"geteuid\"},\"page_size\":%s,\"entropy\":%s,\"drbg\":%s},"
                "\"firmware\":{\"status\":\"unavailable\",\"value\":null,\"reason\":\"not_queried\"},"
                "\"game\":{\"status\":\"unavailable\",\"value\":null,\"reason\":\"not_queried\"},\"core\":%s}\n",
                info->version, info->build_id, info->profile, info->origin, info->authority, info->lifetime,
                download_runtime_result,
                (long)info->pid, (unsigned long)info->uid, (unsigned long)info->euid, page_result, entropy_result, drbg_result, info->core_report);
            if (n < 0 || (size_t)n >= sizeof(out->body)) { http_error(out, 500); return; }
        } else { http_error(out, 404); return; }
    }
    out->status = 200;
    if (!out->static_body) out->size = strlen(out->body);
}

void http_handle(const unsigned char *bytes, size_t size, const struct host_info *info,
                 struct http_response *out) {
    struct parsed_request request;
    int status = 400;
    http_error(out, 400);
    int frame = parse_request(bytes, size, info, &request, &status);
    if (frame == 1) http_handle_request(&request, info, out);
    else http_error(out, status);
    wipe_http(&request, sizeof(request));
}
