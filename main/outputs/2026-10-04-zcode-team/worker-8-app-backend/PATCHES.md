# PATCHES.md — worker-8-app-backend: controller-applied unified diffs

Origin: worker-8-app-backend, 2026-10-04. Existing files were NOT modified by
this worker. The drop-in modules themselves are NEW files already in place at
`outputs/ps5-mod-bridge/native-host/src/modforge-worker8/` (and mirrored in
this directory) — no patch needed for them. The diffs below connect them to
the existing app; apply with `git apply` or manually. All three patches are
independent of each other; Patch 3 depends on Patches 1-2.

Shared intent: today the app can only GET `service.describe` /
`records.list` from the mod service (`http.c:771-799`), so the proven
prepare/install/uninstall backend is unreachable from the app even when
linked. The patches add exactly one generic POST passthrough,
`POST /api/mod-service/request` with body `{"method":..., "params":...}`,
returning the service envelope as-is. No method is special-cased in the app;
authorization stays with the service backend and the existing session
admission gate is unchanged.

---

## Patch 1 — `outputs/ps5-mod-bridge/native-host/mod_service/read_bridge.h`

```diff
--- a/outputs/ps5-mod-bridge/native-host/mod_service/read_bridge.h
+++ b/outputs/ps5-mod-bridge/native-host/mod_service/read_bridge.h
@@ -16,7 +16,8 @@
 struct mb_mod_service_bridge {
     mfs_service *service;
     mfs_owned (*request)(mfs_service *, mfs_view);
 };
-enum mb_mod_service_read { MB_MOD_SERVICE_DESCRIBE, MB_MOD_SERVICE_RECORDS };
+enum mb_mod_service_read { MB_MOD_SERVICE_DESCRIBE, MB_MOD_SERVICE_RECORDS,
+                           MB_MOD_SERVICE_REQUEST };
 static inline int mb_mod_service_usable(const struct mb_mod_service_bridge *b) {
     return b && b->service && b->request;
 }
@@ -24,5 +25,11 @@
 /* Allocates app-owned bytes; caller frees. 200 preserves service envelope,
  * including ok:false. 502 invalid response, 503 unavailable/allocation failure. */
 int mb_mod_service_read(const struct mb_mod_service_bridge *, enum mb_mod_service_read,
                         char **bytes, size_t *length);
+/* Generic one-request passthrough: forwards the caller's bounded
+ * {"method":...,"params":...} envelope verbatim to the backend and returns
+ * the service envelope. Same ownership and status contract as
+ * mb_mod_service_read. The backend, not this bridge, validates the method. */
+int mb_mod_service_call(const struct mb_mod_service_bridge *,
+                        const char *request_bytes, size_t request_bytes_len,
+                        char **bytes, size_t *length);
 #ifdef __cplusplus
 }
 #endif
 #endif
```

## Patch 2 — `outputs/ps5-mod-bridge/native-host/mod_service/read_bridge.cpp`

```diff
--- a/outputs/ps5-mod-bridge/native-host/mod_service/read_bridge.cpp
+++ b/outputs/ps5-mod-bridge/native-host/mod_service/read_bridge.cpp
@@ -33,3 +33,45 @@ extern "C" int mb_mod_service_read(const mb_mod_service_bridge *b,
         std::free(copy);
     }
     return status;
 }
+
+extern "C" int mb_mod_service_call(const mb_mod_service_bridge *b,
+    const char *request_bytes, size_t request_bytes_len,
+    char **bytes, size_t *length) {
+    if (!bytes || !length) return 502;
+    *bytes = nullptr; *length = 0;
+    if (!mb_mod_service_usable(b)) return 503;
+    if (!request_bytes || !request_bytes_len ||
+        request_bytes_len > MB_MOD_SERVICE_RESPONSE_LIMIT) return 502;
+    mfs_owned response = b->request(b->service,
+        {request_bytes, request_bytes_len});
+    int status = 502;
+    char *copy = nullptr;
+    size_t size = response.length;
+    if (response.bytes && response.release && size && size <= MB_MOD_SERVICE_RESPONSE_LIMIT) {
+        copy = static_cast<char *>(std::malloc(size + 1));
+        if (copy) { std::memcpy(copy, response.bytes, size); copy[size] = 0; }
+        else status = 503;
+    }
+    mfs_release(&response);
+    if (copy) {
+        try {
+            auto value = nlohmann::json::parse(copy, copy + size);
+            if (value.is_object() && value.contains("ok") && value["ok"].is_boolean() &&
+                ((value["ok"].get<bool>() && value.contains("result") && value["result"].is_object()) ||
+                 (!value["ok"].get<bool>() && value.contains("error") && value["error"].is_object()))) {
+                *bytes = copy; *length = size; return 200;
+            }
+        } catch (...) { }
+        std::free(copy);
+    }
+    return status;
+}
```

## Patch 3 — `outputs/ps5-mod-bridge/native-host/http.c`

Two hunks. Hunk A admits the new target to the POST body-framing gate
(current lines 287-293); Hunk B routes it inside `mod_service_response`
(current lines 771-799). Everything else in the function (session admission,
MB_HOST_DOWNLOAD guard, ownership/dispose) stays exactly as it is.

```diff
--- a/outputs/ps5-mod-bridge/native-host/http.c
+++ b/outputs/ps5-mod-bridge/native-host/http.c
@@ -287,9 +287,12 @@
     int auth_post = !strcmp(method, "POST") &&
         (!strcmp(target, "/api/auth/start") || !strcmp(target, "/api/auth/disconnect"));
     int json_post = !strcmp(method, "POST") &&
         (!strcmp(target, "/api/jobs") || !strcmp(target, "/api/generations") ||
          !strcmp(target, "/api/acquisition-plan") ||
          !strcmp(target, "/api/acquisitions") || job_resume_target(target, NULL));
-    size_t body_limit = auth_post ? HTTP_AUTH_BODY_LIMIT : json_post ? HTTP_JOB_BODY_LIMIT : 0;
+    int service_post = !strcmp(method, "POST") &&
+        !strcmp(target, "/api/mod-service/request");
+    size_t body_limit = auth_post ? HTTP_AUTH_BODY_LIMIT :
+        (json_post || service_post) ? HTTP_JOB_BODY_LIMIT : 0;
```

```diff
--- a/outputs/ps5-mod-bridge/native-host/http.c
+++ b/outputs/ps5-mod-bridge/native-host/http.c
@@ -771,14 +771,22 @@
 static int mod_service_response(struct parsed_request *request, const struct host_info *info,
                                 struct http_response *out) {
     int records = !strcmp(request->target, "/api/mod-service/records");
-    if (!records && strcmp(request->target, "/api/mod-service")) return 0;
-    if (strcmp(request->method, "GET")) { http_error(out, 405); return 1; }
+    int forward = !strcmp(request->target, "/api/mod-service/request");
+    if (!records && !forward && strcmp(request->target, "/api/mod-service")) return 0;
+    if (!forward && strcmp(request->method, "GET")) { http_error(out, 405); return 1; }
+    if (forward && strcmp(request->method, "POST")) { http_error(out, 405); return 1; }
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
-    int status = mb_mod_service_read(info->mod_service,
-        records ? MB_MOD_SERVICE_RECORDS : MB_MOD_SERVICE_DESCRIBE, &copy, &size);
+    int status;
+    if (forward) {
+        if (!request->body || !request->body_bytes ||
+            request->body_bytes > MB_MOD_SERVICE_RESPONSE_LIMIT) { http_error(out, 400); return 1; }
+        status = mb_mod_service_call(info->mod_service,
+            (const char *)request->body, request->body_bytes, &copy, &size);
+    } else {
+        status = mb_mod_service_read(info->mod_service,
+            records ? MB_MOD_SERVICE_RECORDS : MB_MOD_SERVICE_DESCRIBE, &copy, &size);
+    }
     if (status != 200) { http_error(out, status); return 1; }
     out->owned_body = copy; out->owned_capacity = size + 1;
     out->size = size; out->status = 200;
 #else
     http_error(out, 503);
 #endif
     return 1;
 }
```

Note on the early dispatch: `mod_service_response` is already the first
handler consulted by `http_handle_request` (http.c:806), so no additional
dispatch change is needed; the frame-gate hunk above is what makes the POST
body admissible at all.

---

## Linking the backend (no source patch; build step, controller-owned)

`host.c:442-455` + `mod_service/STARTUP.md` already define the integration
path: the builder force-loads a self-contained static archive exposing
`mfs_open` / `mfs_request` / `mfs_close` (exact vendor ABI signatures) plus
`--mod-service-config` JSON. The worker-8 modules satisfy that contract via
the aliases in `mfw8_service.h`:

```sh
cd outputs/ps5-mod-bridge/native-host/src/modforge-worker8
cc -std=c11 -O2 -pthread -I. -I../../mod_service/vendor -c \
   mfw8_records.c mfw8_actions.c mfw8_caps.c mfw8_service.c
libtool -static -o libmfw8_service.a mfw8_records.o mfw8_actions.o mfw8_caps.o mfw8_service.o
shasum -a 256 libmfw8_service.a   # feed path+sha to build.py --mod-service-profile
```

Example service config (state_dir/mod_root_base are the required keys;
STARTUP.md's extra keys are accepted and ignored):

```json
{"state_dir": "/data/modforge", "mod_root_base": "/data/modbridge",
 "installer_path": "/data/modbridge/bin/modbridge-installer.payload.elf",
 "intake_dir": "/data/modbridge/intake"}
```

`installer_path` must point at the composed installer payload (console:
`worker-installer-composition`/`worker-e2e-final` payload ELF, or the
worker-ps5io-rdwr download ELF when the download stage is linked). On the
console the fork/exec host driver is NOT the shipping transport — the
controller-owned integration step is to bind `mfw8_actions` to the payload
launch path; the action mapping and records semantics are already proven and
unchanged (see REPORT.md honesty section).
