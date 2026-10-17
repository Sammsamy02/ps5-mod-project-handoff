# UI-FLOW-RESULTS.md — worker-app-ui-flows, 2026-10-04

App: ModBridge vendored copy (`vendored/repo/outputs/ps5-mod-bridge/web/`:
`index.html` + `app.js` + `style.css`, embedded into the host binary at build
time). Server: my rebuilt download-runtime desktop_fixture binary with the
retained fixture OAuth config, run at `--port 62283` with the `MB_MODMGR_*`
env pointing at the pinned installer binary + proven `app_core/modmgr_api.py`
driver over a fixture state tree (`/tmp/mb-ui-flows-fix`, sibling `intake/`).

## 1. How the UI consumes the API contract (located, upstream)

- `web/app.js` `loadInstalled()` (was line 918): chooses the branch by
  capability — `mod_service_records` → `/api/mod-service[/records]`
  (read-only), else `installed_records` → `GET /api/installed`, filtering
  `items` to `transaction_confirmed === true && sha256` is 64-hex
  (`app.js:935` upstream) — exactly the contract worker-app-backend-integration
  implemented (`{"items":[{transaction_confirmed, sha256, id, state, enabled,
  files, bytes, name, note, bundle, ...}]}`, driver `modmgr_api.py
  items_for()`).
- `renderInstalled()` (was line 750) rendered those items as read-only cards
  (name + "Confirmed transaction · sha256") — **no controls existed for the
  new mutating endpoints**; install/enable/disable/uninstall were UI-missing
  (FIX 2).
- Session/CSRF: `endpoint()` sends `credentials:'same-origin'` and
  `X-CSRF-Token` on non-GET; the server issues both via `GET /api/capabilities`
  (`Set-Cookie: mb_session=…; HttpOnly; SameSite=Lax`) and admits READ vs
  MUTATION (`Origin` + CSRF) through the real auth runtime
  (`MB_NEXUS_AUTH_ADMISSION_MUTATION`, `require_account=0`).
- The app page is served at `/app` (`/` is a landing page); static assets are
  compiled INTO the binary, so the rebuilt binary serves my fixed UI bytes.

## 2. Flow-by-flow results

Legend — **Host-proven**: verified on this Mac against the real rebuilt
server with real session admission (Level 1 = scripted HTTP with the exact
headers a browser sends; Level 2 = the REAL served app.js bytes executed in a
Node DOM stub driving the same live server). **Console-session**: the real
effect requires the drive protocol to be consumed on the console
(controller-only; every staged row carries `console_executed:false`).

| # | UI flow | Level 1 (real server, real session) | Level 2 (real app.js) | Real effect |
|---|---|---|---|---|
| 1 | Installed tab renders real rows from `{items:[...]}` | 34/34 suite incl. row contract (transaction_confirmed, 64-hex sha, enabled/files/bytes projection) | 18/18 incl. row markup, meta line, controls rendered | Host-proven. Tab truth = records.txt + live loadlist; `running` never claimed |
| 2 | Install from archive (path select → `POST /api/install` → progress → list refresh) | 200 ok envelope; staged register/prepare/install commands; queue manifest grew; list refresh shows new confirmed row w/ sha | Form submit → "Installing…" busy state → new row `data-mod-toggle="ui-flow-harness"` appears; server sha256 present | Host-local install host-proven. Console EFFECT needs the staged commands consumed per drive protocol (console-session) |
| 3 | Disable/enable toggle round-trip | `POST /api/mods/zb-lua/disable` → verification `enabled:false`; list shows disabled; enable restores; recompose commands staged | Click Disable → row flips to "Enable"; server `enabled=false`; click Enable → "Disable"; server `enabled=true` | Host-side strip/restore host-proven (bundle-index attribution, backups, records-sha resync). Console recompose = console-session |
| 4 | Uninstall confirm | `DELETE /api/mods/<id>` 200; item leaves the tab (state→`prepared`, `transaction_confirmed:false` — the UI's own filter drops it honestly); idempotent second DELETE 200; remove command staged | First click arms "Confirm uninstall" on that row only; second click removes; server no longer transaction_confirmed | Same split: host removal proven, console `install --remove` consumption = console-session |
| 5 | Admission negatives (what the browser never sends) | installed without session → 401; install without CSRF → 403; without Origin → 403; archive outside intake → 400; POST/GET method guards | n/a (browser always sends them) | Host-proven |
| 6 | Capabilities honesty | `installed_records:true`, `mod_service_records:false` (records route chosen, not service), `jobs:false` in this minimal build | Harness renders the install panel exactly under `installed_records` | Host-proven |

Receipts: `receipts/ui-flow-tests.json` + `.log` (Level 1),
`receipts/ui-flow-harness.json` (Level 2), `receipts/api-regression-42.log`
(backend 42/42 re-run on the fixed http.c), `receipts/server*.log`.

## 3. Honest limits

- **No visual/browser rendering.** No browser driver exists in this tree and
  no browser was automated. Level 2 executes the real served app.js code
  (event handlers, state machine, real fetches) against a minimal DOM stub —
  innerHTML/DOM-structure assertions, not pixels. The page has never been
  looked at by a human in this work; layout/visual quality is unrated.
- **The mutating calls' real console effects are NOT proven** — by design.
  Host mutations stage gated console command files
  (`<state>/modmgr/console-queue/`, sha-bound manifest, payload-identity
  rows, stale-command refusal). Consuming them against a live payload is the
  controller's console session; `console_executed:false` everywhere here.
- **Fixture OAuth config only.** The session runtime runs fully offline
  (Nexus hosts resolve to 127.0.0.1). Real OAuth client_id/scope and a real
  Nexus account remain out of scope (user-only).
- **Single-title fixture.** The `?title=` query path and the
  ambiguous-title refusal were proven by the backend worker's 42/42 suite
  (re-run green on my tree); the flows above used one title.
- The harness's DOM stub binds some lookups through a selector registry;
  element identity quirks (e.g., re-bound submit listeners) are harness
  artifacts, not app findings.
