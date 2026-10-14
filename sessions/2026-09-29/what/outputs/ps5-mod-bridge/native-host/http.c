#include "http.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

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

void http_handle(const unsigned char *bytes, size_t size, const struct host_info *info,
                 struct http_response *out) {
    http_error(out, 400);
    if (size > HTTP_HEADER_LIMIT) { http_error(out, 431); return; }
    if (size < 4) return;
    for (size_t i = 0; i < size; ++i)
        if (!bytes[i] || bytes[i] > 126 || (bytes[i] < 32 && bytes[i] != '\r' && bytes[i] != '\n' && bytes[i] != '\t')) return;
    char input[HTTP_HEADER_LIMIT + 1];
    memcpy(input, bytes, size); input[size] = 0;
    char *end = line_end(input);
    if (!end) return;
    *end = 0;
    char *method = input, *target = strchr(method, ' ');
    if (!target) return;
    *target++ = 0;
    char *version = strchr(target, ' ');
    if (!version) return;
    *version++ = 0;
    if (!*method || !*target || strcmp(version, "HTTP/1.1") || strchr(target, ' ') || strchr(version, ' ')) return;
    for (char *p = method; *p; ++p) if (!token((unsigned char)*p)) return;
    for (char *p = target; *p; ++p) if ((unsigned char)*p <= 32) return;
    int host_seen = 0, length_seen = 0, origin_seen = 0, shutdown_seen = 0;
    int shutdown_value = 0, origin_match = 1;
    char *cursor = end + 2;
    for (;;) {
        end = line_end(cursor);
        if (!end) return;
        if (end == cursor) {
            if (end + 2 != input + size) return;
            break;
        }
        *end = 0;
        char *colon = strchr(cursor, ':');
        if (!colon || colon == cursor) return;
        for (char *p = cursor; p < colon; ++p) if (!token((unsigned char)*p)) return;
        *colon = 0;
        char *value = colon + 1;
        while (*value == ' ' || *value == '\t') ++value;
        char *tail = end;
        while (tail > value && (tail[-1] == ' ' || tail[-1] == '\t')) *--tail = 0;
        for (char *p = value; *p; ++p) if ((unsigned char)*p < 32 && *p != '\t') return;
        if (!strcasecmp(cursor, "Host")) {
            if (host_seen++ || strcmp(value, info->authority)) return;
        } else if (!strcasecmp(cursor, "Content-Length")) {
            if (length_seen++ || strcmp(value, "0")) return;
        } else if (!strcasecmp(cursor, "Transfer-Encoding")) return;
        else if (!strcasecmp(cursor, "Origin")) {
            if (origin_seen++) return;
            origin_match = !strcmp(value, info->url_origin);
        } else if (!strcasecmp(cursor, "X-Modbridge-Shutdown")) {
            if (shutdown_seen++) return;
            shutdown_value = !strcmp(value, "1");
        }
        cursor = end + 2;
    }
    if (!host_seen) return;
    if (!origin_match) { http_error(out, 403); return; }
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
                "\"jobs\":false,\"installed_records\":false,"
                "\"download_analyze\":false,\"prepared_translation\":false,\"deploy\":false,"
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
            if (entropy_report(entropy_result, sizeof(entropy_result), &info->entropy) ||
                drbg_report(drbg_result, sizeof(drbg_result), &info->drbg)) {
                http_error(out, 500); return;
            }
            if (info->page_size > 0) snprintf(page_result, sizeof(page_result),
                "{\"status\":\"observed\",\"value\":%ld,\"api\":\"sysconf(_SC_PAGESIZE)\"}", info->page_size);
            else snprintf(page_result, sizeof(page_result),
                "{\"status\":\"unavailable\",\"value\":null,\"api\":\"sysconf(_SC_PAGESIZE)\",\"errno\":%d}", info->page_errno);
            int n = snprintf(out->body, sizeof(out->body),
                "{\"schema_version\":1,\"version\":\"%s\",\"build_id\":\"%s\",\"profile\":\"%s\",\"origin\":\"%s\","
                "\"phase\":\"main_http_ready\",\"installation_supported\":false,\"authority\":\"%s\",\"lifetime_seconds\":%u,"
                "\"ownership_contract\":\"owned_socket_spawn_only\",\"observations\":{"
                "\"getpid\":{\"status\":\"observed\",\"value\":%ld,\"api\":\"getpid\"},"
                "\"getuid\":{\"status\":\"observed\",\"value\":%lu,\"api\":\"getuid\"},"
                "\"geteuid\":{\"status\":\"observed\",\"value\":%lu,\"api\":\"geteuid\"},\"page_size\":%s,\"entropy\":%s,\"drbg\":%s},"
                "\"firmware\":{\"status\":\"unavailable\",\"value\":null,\"reason\":\"not_queried\"},"
                "\"game\":{\"status\":\"unavailable\",\"value\":null,\"reason\":\"not_queried\"},\"core\":%s}\n",
                info->version, info->build_id, info->profile, info->origin, info->authority, info->lifetime,
                (long)info->pid, (unsigned long)info->uid, (unsigned long)info->euid, page_result, entropy_result, drbg_result, info->core_report);
            if (n < 0 || (size_t)n >= sizeof(out->body)) { http_error(out, 500); return; }
        } else { http_error(out, 404); return; }
    }
    out->status = 200;
    if (!out->static_body) out->size = strlen(out->body);
}
