/* modmgr_http_tests.c — offline route-wiring + real-driver integration.
 *
 * Worker-app-backend-integration, 2026-10-04. Same discipline as the tree's
 * tests/mod_service_http_tests.c: http.c compiled with stubbed auth, raw
 * HTTP frames driven through http_handle(), statuses asserted. Difference:
 * info->modmgr points at the REAL vendored core loop (app_core/modmgr_api.py)
 * and the REAL composed installer binary over a fixture /data/modbridge-like
 * tree, so every mutating step runs the proven host pipeline for real and
 * emits a STEP receipt line for the python driver to verify afterwards.
 * No network, no console, no account. Console execution NOT claimed.
 */
#include "http.h"
#include "auth.h"
#include "download_store/store.h"
#include "mod_service/read_bridge.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned admissions, mutation_admissions, session_calls, releases;

static const char valid_cookie[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

int mb_nexus_auth_admit_v1(mb_nexus_auth_context_v1 *ctx,
 const mb_nexus_auth_admit_params_v1 *p, mb_nexus_auth_result_v1 *r) {
 (void)ctx;
 assert(ctx && (p->admission == MB_NEXUS_AUTH_ADMISSION_READ ||
                p->admission == MB_NEXUS_AUTH_ADMISSION_MUTATION) &&
        !p->require_account);
 ++admissions;
 if (p->admission == MB_NEXUS_AUTH_ADMISSION_MUTATION) {
  /* mutation requires origin+csrf and the valid session cookie */
  ++mutation_admissions;
  assert(p->origin && p->origin_bytes && p->csrf && p->csrf_bytes);
 }
 r->credentials_present = 0;
 r->status = p->cookie && p->cookie_bytes == 43 &&
                     !memcmp(p->cookie, valid_cookie, 43)
   ? MB_NEXUS_AUTH_OK : MB_NEXUS_AUTH_SESSION_REQUIRED;
 r->complete = r->status == MB_NEXUS_AUTH_OK;
 return r->status;
}
int mb_nexus_auth_session_v1(mb_nexus_auth_context_v1 *c,
 const mb_nexus_auth_session_params_v1 *p, mb_nexus_auth_result_v1 *r) {
 (void)c;
 ++session_calls;
 assert(p->cookie_output && p->cookie_capacity == 44 &&
        p->csrf_output && p->csrf_capacity == 44);
 memset(p->cookie_output, 'a', 43); p->cookie_output[43] = 0;
 memset(p->csrf_output, 'b', 43); p->csrf_output[43] = 0;
 r->status = MB_NEXUS_AUTH_OK; r->complete = 1;
 r->new_session = p->cookie_bytes == 0;
 return MB_NEXUS_AUTH_OK;
}
#define STUB(name) int mb_nexus_auth_##name##_v1(mb_nexus_auth_context_v1 *c, const mb_nexus_auth_##name##_params_v1 *p, mb_nexus_auth_result_v1 *r) { (void)c;(void)p;(void)r; assert(0); return 14; }
STUB(start) STUB(callback) STUB(account) STUB(disconnect) STUB(catalog) STUB(acquisition_plan)
int mb_download_store_list(mb_download_store *store,char *out,size_t cap){(void)store;(void)cap;strcpy(out,"{\"items\":[]}");return MB_DOWNLOAD_STORE_OK;}

static mfs_owned bridge_request(mfs_service *s, mfs_view v) {
 (void)s; (void)v;
 static const char body[] = "{\"ok\":true,\"result\":{\"mods\":[],\"games\":[]}}";
 static char copy[sizeof(body)];
 memcpy(copy, body, sizeof(body));
 mfs_owned out = {copy, sizeof(body) - 1, 0, 0};
 return out;
}

static char state_dir[256], roots_dir[256], bin_path[256], script_path[256];

static void env_or_fail(const char *name, char *out, size_t cap) {
 const char *v = getenv(name);
 if (!v || !*v || strlen(v) >= cap) { fprintf(stderr, "missing %s\n", name); exit(3); }
 strcpy(out, v);
}

static struct mb_modmgr_runtime modmgr;

static void request(const char *name, const char *method, const char *target,
                    const char *cookie, const char *csrf, const char *origin,
                    const char *body) {
 struct host_info info = {0};
 struct mb_mod_service_bridge bridge = {(mfs_service *)1, bridge_request};
 strcpy(info.authority, "127.0.0.1:18080");
 strcpy(info.url_origin, "http://127.0.0.1:18080");
 info.auth = (mb_nexus_auth_context_v1 *)1;
 info.mod_service = &bridge;
 info.modmgr = &modmgr;
 char frame[8192];
 int n = snprintf(frame, sizeof(frame),
  "%s %s HTTP/1.1\r\nHost: 127.0.0.1:18080\r\n", method, target);
 if (cookie) n += snprintf(frame + n, sizeof(frame) - (size_t)n,
  "Cookie: mb_session=%s\r\n", cookie);
 if (csrf) n += snprintf(frame + n, sizeof(frame) - (size_t)n,
  "X-CSRF-Token: %s\r\n", csrf);
 if (origin) n += snprintf(frame + n, sizeof(frame) - (size_t)n,
  "Origin: %s\r\n", origin);
 if (body) n += snprintf(frame + n, sizeof(frame) - (size_t)n,
  "Content-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",
  strlen(body), body);
 else n += snprintf(frame + n, sizeof(frame) - (size_t)n, "\r\n");
 assert(n > 0 && (size_t)n < sizeof(frame));
 struct http_response out;
 http_handle((unsigned char *)frame, (size_t)n, &info, &out);
 char body_copy[16384];
 if (out.owned_body) {
  size_t take = out.size < sizeof(body_copy) - 1 ? out.size : sizeof(body_copy) - 1;
  memcpy(body_copy, out.owned_body, take);
  body_copy[take] = 0;
 } else if (out.size && out.static_body) {
  size_t take = out.size < sizeof(body_copy) - 1 ? out.size : sizeof(body_copy) - 1;
  memcpy(body_copy, out.static_body, take);
  body_copy[take] = 0;
 } else {
  body_copy[0] = 0;
 }
 printf("STEP %s status=%d body=%.16000s\n", name, out.status,
        (out.owned_body || out.static_body) ? body_copy : out.body);
 fflush(stdout);
 http_response_dispose(&out);
}

static void disabled_request(const char *name, const char *method,
                             const char *target, const char *cookie) {
 struct host_info info = {0};
 strcpy(info.authority, "127.0.0.1:18080");
 info.auth = (mb_nexus_auth_context_v1 *)1;
 info.modmgr = NULL;
 char frame[2048];
 int n = snprintf(frame, sizeof(frame),
  "%s %s HTTP/1.1\r\nHost: 127.0.0.1:18080\r\n", method, target);
 if (cookie) n += snprintf(frame + n, sizeof(frame) - (size_t)n,
  "Cookie: mb_session=%s\r\n", cookie);
 n += snprintf(frame + n, sizeof(frame) - (size_t)n, "\r\n");
 struct http_response out;
 http_handle((unsigned char *)frame, (size_t)n, &info, &out);
 printf("STEP %s status=%d body=\n", name, out.status);
 fflush(stdout);
 http_response_dispose(&out);
}

static void capabilities_check(const char *name, int expect_true) {
 struct host_info info = {0};
 strcpy(info.authority, "127.0.0.1:18080");
 info.auth = (mb_nexus_auth_context_v1 *)1;
 info.modmgr = expect_true ? &modmgr : NULL;
 char frame[512];
 int n = snprintf(frame, sizeof(frame),
  "GET /api/capabilities HTTP/1.1\r\nHost: 127.0.0.1:18080\r\n\r\n");
 struct http_response out;
 http_handle((unsigned char *)frame, (size_t)n, &info, &out);
 printf("STEP %s status=%d body=%.16000s\n", name, out.status, out.body);
 fflush(stdout);
 http_response_dispose(&out);
}

int main(void) {
 env_or_fail("MB_TEST_STATE", state_dir, sizeof(state_dir));
 env_or_fail("MB_TEST_ROOTS", roots_dir, sizeof(roots_dir));
 env_or_fail("MB_TEST_BIN", bin_path, sizeof(bin_path));
 env_or_fail("MB_TEST_SCRIPT", script_path, sizeof(script_path));
 strcpy(modmgr.binary, bin_path);
 strcpy(modmgr.state, state_dir);
 strcpy(modmgr.roots, roots_dir);
 strcpy(modmgr.api_script, script_path);
 strcpy(modmgr.python, "python3");
 const char *origin = "http://127.0.0.1:18080";
 const char *install_body = getenv("MB_TEST_INSTALL_BODY");
 const char *second_id = getenv("MB_TEST_SECOND_ID");
 const char *outside_body = getenv("MB_TEST_OUTSIDE_BODY");

 /* S1 routes dead when modmgr runtime absent */
 disabled_request("S1-installed-disabled", "GET", "/api/installed", valid_cookie);
 /* S2 admission: no cookie -> 401 */
 request("S2-installed-no-cookie", "GET", "/api/installed", NULL, NULL, NULL, NULL);
 /* S3 empty state -> 200 items:[] */
 request("S3-installed-empty", "GET", "/api/installed", valid_cookie, NULL, NULL, NULL);
 /* S4 mutation without csrf/origin -> 403 */
 request("S4-install-no-csrf", "POST", "/api/install", valid_cookie, NULL, NULL,
         install_body);
 /* S5 real install of the fixture archive (intake containment satisfied) */
 request("S5-install", "POST", "/api/install", valid_cookie, "csrf-token-43-bytes-aaaaaaaaaaaaa", origin, install_body);
 /* S6 list shows both mods */
 request("S6-installed-two", "GET", "/api/installed", valid_cookie, NULL, NULL, NULL);
 /* S7 disable the second mod (no body: not in the JSON body gate) */
 char disable_target[128];
 snprintf(disable_target, sizeof(disable_target), "/api/mods/%s/disable", second_id);
 request("S7-disable", "POST", disable_target, valid_cookie,
         "csrf-token-43-bytes-aaaaaaaaaaaaa", origin, NULL);
 /* S8 re-enable */
 char enable_target[128];
 snprintf(enable_target, sizeof(enable_target), "/api/mods/%s/enable", second_id);
 request("S8-enable", "POST", enable_target, valid_cookie,
         "csrf-token-43-bytes-aaaaaaaaaaaaa", origin, NULL);
 /* S9 DELETE uninstalls the second mod */
 char delete_target[128];
 snprintf(delete_target, sizeof(delete_target), "/api/mods/%s", second_id);
 request("S9-uninstall", "DELETE", delete_target, valid_cookie,
         "csrf-token-43-bytes-aaaaaaaaaaaaa", origin, NULL);
 /* S10 DELETE again -> 404 envelope */
 request("S10-uninstall-again", "DELETE", delete_target, valid_cookie,
         "csrf-token-43-bytes-aaaaaaaaaaaaa", origin, NULL);
 /* S11 install path outside intake -> 400, nothing executed */
 request("S11-install-outside-intake", "POST", "/api/install", valid_cookie,
         "csrf-token-43-bytes-aaaaaaaaaaaaa", origin, outside_body);
 /* S12 method guards (no bodies: those targets are not in the JSON gate) */
 request("S12-installed-post", "POST", "/api/installed", valid_cookie,
         "csrf-token-43-bytes-aaaaaaaaaaaaa", origin, NULL);
 request("S12b-install-get", "GET", "/api/install", valid_cookie, NULL, NULL, NULL);
 /* S13 capabilities report the modmgr surface honestly */
 capabilities_check("S13-capabilities-enabled", 1);
 capabilities_check("S13b-capabilities-disabled", 0);
 (void)admissions; (void)mutation_admissions; (void)session_calls; (void)releases;
 return 0;
}
