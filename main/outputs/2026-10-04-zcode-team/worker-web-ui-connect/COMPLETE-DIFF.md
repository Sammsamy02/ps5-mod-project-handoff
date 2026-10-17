# COMPLETE-DIFF.md — worker-web-ui-connect: the one-pass change set

The complete set of changes the controller applies to connect the web UI surface
to the delivered app backend modules. Verified: 5/5 patches pass strict
`git apply --check` and apply sequentially to a byte-identical intended tree
(see PATCH-SET.md §1-3). Apply in the order below from the workspace root
(`/WORKSPACE/.codex/worktrees/translation-core/what`); patch files are at
`outputs/2026-10-04-zcode-team/worker-web-ui-connect/verify/canon/`.

What each patch does (then the verbatim diffs):

| # | File | Effect |
|---|---|---|
| 1 | `outputs/ps5-mod-bridge/native-host/mod_service/read_bridge.h` | Adds `MB_MOD_SERVICE_REQUEST` to the read enum and declares `mb_mod_service_call()` — the generic bounded passthrough with the same 200/502/503 ownership contract as `mb_mod_service_read`. |
| 2 | `outputs/ps5-mod-bridge/native-host/mod_service/read_bridge.cpp` | Implements `mb_mod_service_call()`: forwards the caller's `{"method":...,"params":...}` envelope verbatim to the backend, accepts only a well-formed modforge-service/1 envelope (`ok:true`+object `result`, or `ok:false`+object `error`), ≤256 KiB, caller frees. The backend validates the method; nothing is special-cased here. |
| 3 | `outputs/ps5-mod-bridge/native-host/http.c` | (a) Frame gate admits `POST /api/mod-service/request` with `HTTP_JOB_BODY_LIMIT` bodies (Content-Length + `application/json` enforced by the existing parser). (b) `mod_service_response` accepts the new target: POST-only (else 405), same session admission gate, body required and ≤ `MB_MOD_SERVICE_RESPONSE_LIMIT` (else 400), forwards via `mb_mod_service_call` and returns the service envelope as-is with 200 (including `ok:false`); bridge statuses surface as 400/405/502/503. GET describe/records behavior unchanged. |
| 4 | `outputs/ps5-mod-bridge/web/app.js` | Display-only: honest state pill with unknown-state fallback; runtime shown strictly from ack states (`runtimeAckLabel`); note stating states come from `records.list` and running only from runtime acknowledgments; removes the dead `GET /api/installed` call (route does not exist) in favor of the honest empty state. |
| 5 | `outputs/ps5-mod-bridge/web/index.html` | Display-only: Installed tab subheading now says states come from service records, running is ack-derived, and this UI installs nothing. |

```sh
cd /WORKSPACE/.codex/worktrees/translation-core/what
P=outputs/2026-10-04-zcode-team/worker-web-ui-connect/verify/canon
git apply $P/p1-read_bridge.h.patch
git apply $P/p2-read_bridge.cpp.patch
git apply $P/p3-http-mod-service-request.patch
git apply $P/f1-app-js-display-states.patch
git apply $P/f2-index-html-subheading.patch
```

Controller-owned follow-ups (unchanged from worker-8 PATCHES.md, not part of
this diff): build/link `libmfw8_service.a` per `native-host/src/modforge-worker8/`
+ `mod_service/STARTUP.md`, feed path+sha to `build.py --mod-service-profile`,
and rebuild the embedded web assets so the host serves the patched `app.js`/
`index.html` (`http_asset_*` are compiled in; see `tests/test_web_assets.py`).

---

## Patch 1 — p1-read_bridge.h.patch

```diff
--- a/outputs/ps5-mod-bridge/native-host/mod_service/read_bridge.h
+++ b/outputs/ps5-mod-bridge/native-host/mod_service/read_bridge.h
@@ -11,7 +11,8 @@
     mfs_service *service;
     mfs_owned (*request)(mfs_service *, mfs_view);
 };
-enum mb_mod_service_read { MB_MOD_SERVICE_DESCRIBE, MB_MOD_SERVICE_RECORDS };
+enum mb_mod_service_read { MB_MOD_SERVICE_DESCRIBE, MB_MOD_SERVICE_RECORDS,
+                           MB_MOD_SERVICE_REQUEST };
 static inline int mb_mod_service_usable(const struct mb_mod_service_bridge *b) {
     return b && b->service && b->request;
 }
@@ -19,6 +20,13 @@
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
```

## Patch 2 — p2-read_bridge.cpp.patch

```diff
--- a/outputs/ps5-mod-bridge/native-host/mod_service/read_bridge.cpp
+++ b/outputs/ps5-mod-bridge/native-host/mod_service/read_bridge.cpp
@@ -34,3 +34,36 @@
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

## Patch 3 — p3-http-mod-service-request.patch

```diff
--- a/outputs/ps5-mod-bridge/native-host/http.c
+++ b/outputs/ps5-mod-bridge/native-host/http.c
@@ -290,7 +290,10 @@
         (!strcmp(target, "/api/jobs") || !strcmp(target, "/api/generations") ||
          !strcmp(target, "/api/acquisition-plan") ||
          !strcmp(target, "/api/acquisitions") || job_resume_target(target, NULL));
-    size_t body_limit = auth_post ? HTTP_AUTH_BODY_LIMIT : json_post ? HTTP_JOB_BODY_LIMIT : 0;
+    int service_post = !strcmp(method, "POST") &&
+        !strcmp(target, "/api/mod-service/request");
+    size_t body_limit = auth_post ? HTTP_AUTH_BODY_LIMIT :
+        (json_post || service_post) ? HTTP_JOB_BODY_LIMIT : 0;
     int host_seen = 0, origin_seen = 0, cookie_seen = 0, csrf_seen = 0, type_seen = 0;
     size_t length = 0;
     char *cursor = end + 2;
@@ -771,8 +774,10 @@
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
@@ -787,8 +792,16 @@
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
```

## Patch 4 — f1-app-js-display-states.patch (fallback, see PATCH-SET.md §5)

```diff
--- a/outputs/ps5-mod-bridge/web/app.js
+++ b/outputs/ps5-mod-bridge/web/app.js
@@ -747,13 +747,22 @@
       if (replacement) $$('[data-focus-key]', replacement).find(element => element.dataset.focusKey === focusedKey)?.focus();
     }
   };
+  // Running is derived only from runtime acks (runtime-status.txt), never fabricated.
+  const runtimeAckLabel = ackState => ({
+    'not-started': 'No runtime ack',
+    'activated': 'Running (ack: activated)',
+    'engine-ready': 'Running (ack: engine-ready)',
+    'mods-started': 'Running (ack: mods-started)',
+    'failed': 'Failed (ack: failed)',
+    'stopped': 'Stopped (ack: stopped)',
+  })[ackState] || 'Runtime ack unavailable';
   const renderInstalled = () => {
     const root = $('#installed-content');
     if (state.capabilities.mod_service_records === true) {
       root.className = 'stack';
       const records = state.serviceRecords;
       if (!records) { root.innerHTML = `<div class="empty-state"><h2>Service records unavailable</h2><p>${esc(state.serviceRecordsError || 'Reading service records…')}</p><p>Installation and live gameplay status are unverified.</p></div>`; return; }
-      root.innerHTML = `<p class="content-note">Read-only service records. Live game process unverified; installation controls unavailable.</p>${records.mods.length ? records.mods.map(item => `<article class="card"><div><p class="card-title">${esc(safeText(item.source.name, item.download_id))}</p><p class="card-meta">${esc(item.title_id)} · <span class="pill">${esc({downloaded: 'Downloaded', prepared: 'Prepared', installed: 'Installed'}[item.state])}</span></p>${item.state === 'prepared' ? `<p class="card-meta">${item.blockers?.length ? 'Preparation blocked' : 'Preparation recorded; install readiness unverified'}</p>${(item.blockers || []).map(blocker => `<p>${esc(blocker.code)}: ${esc(blocker.message)}</p>`).join('')}` : ''}</div></article>`).join('') : '<div class="empty-state"><h2>No service mod records</h2></div>'}${records.games.map(game => `<article class="card"><div><p class="card-title">${esc(game.title_id)}</p><p class="card-meta">Installed load list: ${esc(game.installed.mods.length)} mod(s)</p><p>Runtime report: ${esc(game.runtime.state)} · Live game process unverified</p>${game.runtime.detail ? `<p>${esc(game.runtime.detail)}</p>` : ''}</div></article>`).join('')}`;
+      root.innerHTML = `<p class="content-note">Read-only service records. Mod states come from records.list; running is derived only from runtime acknowledgments. Live game process unverified; installation controls unavailable.</p>${records.mods.length ? records.mods.map(item => `<article class="card"><div><p class="card-title">${esc(safeText(item.source.name, item.download_id))}</p><p class="card-meta">${esc(item.title_id)} · <span class="pill">${esc({downloaded: 'Downloaded', prepared: 'Prepared', installed: 'Installed'}[item.state] || 'Unknown state')}</span></p>${item.state === 'prepared' ? `<p class="card-meta">${item.blockers?.length ? 'Preparation blocked' : 'Preparation recorded; install readiness unverified'}</p>${(item.blockers || []).map(blocker => `<p>${esc(blocker.code)}: ${esc(blocker.message)}</p>`).join('')}` : ''}</div></article>`).join('') : '<div class="empty-state"><h2>No service mod records</h2></div>'}${records.games.map(game => `<article class="card"><div><p class="card-title">${esc(game.title_id)}</p><p class="card-meta">Installed load list: ${esc(game.installed.mods.length)} mod(s)</p><p>Runtime ${esc(runtimeAckLabel(game.runtime.state))} · Live game process unverified</p>${game.runtime.detail ? `<p>${esc(game.runtime.detail)}</p>` : ''}</div></article>`).join('')}`;
       return;
     }
     if (!state.installed.length) { root.className = 'empty-state'; root.innerHTML = '<span class="empty-icon" aria-hidden="true">✓</span><h2>No confirmed installations</h2><p>Gameplay status is unverified. ModBridge has not installed anything.</p>'; return; }
@@ -931,9 +940,9 @@
       } finally { state.serviceRecordsLoading = false; }
       return;
     }
-    if (state.capabilities.installed_records !== true) { state.installed = []; renderInstalled(); return; }
-    try { const data = await endpoint('/api/installed'); if (request !== state.requests.installed) return; state.installed = apiItems(data).filter(item => item?.transaction_confirmed === true && typeof item.sha256 === 'string' && /^[a-f0-9]{64}$/i.test(item.sha256)); renderInstalled(); }
-    catch (error) { if (request === state.requests.installed) { state.installed = []; renderInstalled(); showError(error.message); } }
+    // The host exposes no /api/installed route; without service records the
+    // honest state is an empty list, never a request to a missing endpoint.
+    if (request === state.requests.installed) { state.installed = []; renderInstalled(); }
   };
   const cancelLookup = () => { ++state.requests.lookup; state.lookupLoading = false; $('#lookup-status').textContent = ''; };
   const resetSelection = () => { cancelLookup(); state.pendingIntent = null; state.game = null; state.mod = null; state.file = null; state.mods = []; state.files = []; ++state.requests.mods; ++state.requests.files; saveNavigation(); renderGames(); };
```

## Patch 5 — f2-index-html-subheading.patch (fallback, see PATCH-SET.md §5)

```diff
--- a/outputs/ps5-mod-bridge/web/index.html
+++ b/outputs/ps5-mod-bridge/web/index.html
@@ -64,7 +64,7 @@
         <div id="activity-content" class="stack" aria-live="polite"></div>
       </section>
       <section id="installed-view" class="view" role="tabpanel" aria-labelledby="tab-installed" hidden>
-        <div class="view-heading"><div><p class="eyebrow">LOCAL RECORDS</p><h1>Installed</h1><p class="subheading">Only confirmed transactions with verified file hashes appear here.</p></div></div>
+        <div class="view-heading"><div><p class="eyebrow">LOCAL RECORDS</p><h1>Installed</h1><p class="subheading">Mod states come from service records · running is ack-derived · this UI installs nothing.</p></div></div>
         <div id="installed-content" class="empty-state"><span class="empty-icon" aria-hidden="true">✓</span><h2>No confirmed installations</h2><p>Gameplay status is unverified. ModBridge has not installed anything.</p></div>
       </section>
     </main>
```

---

## What the user sees after the set is applied (and the host is rebuilt/linked)

1. **Installed tab is real when the mod service is linked.** With
   `capabilities.mod_service_records === true` (service linked at startup),
   the tab lists per-mod cards from `records.list`: mod name (or download id),
   title id, and a state pill that is exactly one of **Downloaded / Prepared /
   Installed** — or **Unknown state** if the backend ever reports something
   else (nothing is guessed). Prepared mods show their blockers, if any.
   Per-game cards show the installed load-list size and the runtime line
   (below). If the service is absent, the tab shows the honest empty state —
   no network call to a route that does not exist.

2. **The running indicator is ack-based only.** The per-game runtime line now
   reads `Runtime No runtime ack`, `Runtime Running (ack: activated)`,
   `Running (ack: engine-ready)`, `Running (ack: mods-started)`,
   `Failed (ack: failed)`, `Stopped (ack: stopped)` — derived solely from the
   backend's runtime-status acknowledgments — and always keeps
   `Live game process unverified`. No "running" is ever inferred from install
   state or wishful thinking.

3. **A new backend surface exists: `POST /api/mod-service/request`.** Body
   `{"method":..., "params":...}` (JSON, `HTTP_JOB_BODY_LIMIT` max, standard
   Content-Length/Content-Type framing, same session admission as the GETs).
   Responses: 200 with the modforge-service/1 envelope verbatim (including
   `ok:false` + `error` objects — surface errors, don't hide them); 405 wrong
   method; 400 missing/oversized body; 502 malformed bridge reply; 503 service
   unavailable/unlinked. This is what makes prepare/install/uninstall/cancel
   reachable from the app once the controller links `libmfw8_service.a` and
   binds the installer payload — no method is special-cased in the host.
   The display-only frontend patches do not call it yet; any future UI control
   goes through `endpoint('/api/mod-service/request', {method:'POST', body:
   JSON.stringify({method, params})})`.

4. **Honest copy everywhere.** The Installed tab subheading and records note
   now state where states come from (service records), how running is derived
   (runtime acknowledgments), and that this UI installs nothing.

Honesty bounds: nothing here claims installation support on console
(`installation_supported:false`, `console_executed:false` checkpoints stand);
the patches are wire-up and display corrections only, verified by patch
application on copies — not by running the host or the UI.
