# FIXES.md — worker-app-ui-flows, 2026-10-04

Every change below lives ONLY in this worker's vendored copy
(`vendored/repo/outputs/ps5-mod-bridge/...`). The original app tree
(`outputs/ps5-mod-bridge/`) and the source worker dirs were never written.
Diffs are against worker-app-backend-integration's vendored tree (the
pristine 42/42 baseline).

## FIX 1 (server, genuinely broken): install driver received an empty `--file`

- **File:** `native-host/http.c`, `modmgr_response()` install branch.
- **Symptom:** every real `POST /api/install` through the REAL built binary
  returned `404 {"kind":"not_found","message":"source archive missing: "}` —
  the fork/exec'd `app_core/modmgr_api.py install` got `--file ''`.
- **Root cause (proven by instrumentation, receipts/server-debug*.log):**
  `char resolved[MODMGR_PATH_LIMIT]` was declared INSIDE the
  `else if (install) { ... }` block, but its address escapes that block via
  `file_arg` into the `modmgr_exec(...)` call below it. With the release
  build (`-Os`, handler inlined into `http_handle`), clang's stack coloring
  reuses the out-of-scope slot, so the buffer was already zeroed when
  `modmgr_exec` read it: same pointer (`0x16b490cf0`), content empty.
  worker-app-backend-integration's 42/42 passed because their harness
  compiles http.c WITHOUT `-Os` (default -O0), which masked the reuse.
- **Fix:** hoist `resolved` to function scope next to `title/mod_id/source`
  (5-line change + comment). Containment logic untouched.
- **Proof:** rebuilt download-runtime binary (build_id `9b46b161…`, sha256
  `a677e7b2…`): `POST /api/install` 200 ok envelope, staged commands written,
  new row confirmed via the binary's own reader (receipts/ui-flow-tests.json
  "install from intake 200 ok envelope"); worker's backend 42/42 offline
  suite re-run against the fixed http.c: **PASS=42 FAIL=0**
  (receipts/api-regression-42.log).

## FIX 2 (UI, genuinely broken): Installed tab had no way to exercise the API

`web/app.js` (92 changed lines) + `web/style.css` (12 lines):

- `renderInstalled()`: with `capabilities.installed_records === true` the tab
  now renders the install panel + real rows (the app's own
  `transaction_confirmed` + 64-hex `sha256` filter is unchanged) with, per
  row: an Enable/Disable toggle (`aria-pressed`), a two-step Uninstall
  (first click arms → "Confirm uninstall" on that row only; second click
  issues `DELETE /api/mods/<id>`), and the honest modmgr projection
  (enabled state, file count, bytes). The read-only `mod_service_records`
  branch is untouched.
- `installPanel()` + `installArchive()`: install-from-archive flow —
  Title ID + Mod ID + archive path in intake → `POST /api/install`
  (JSON `{title,id,path}` ≤4096-byte gate) with a busy "Installing…" progress
  state, then `loadInstalled()` refresh; server verdicts (intake containment
  400, token 400) surface verbatim. The archive must already sit in the host
  intake dir (server enforces by realpath containment; the 4096-byte JSON
  gate rules out uploads by design).
- `modToggle()`: `POST /api/mods/<id>/enable|disable` with **no body** — the
  C parser refuses bodies outside the JSON gate (`if (!body_limit && length)
  return -1` → 400), so an `{}` body would have broken every toggle.
- `tokenValue()` helper mirrors the server's token charset
  (`[A-Za-z0-9][A-Za-z0-9_-]*`, `modmgr_token_char` / driver `TOKEN`).
- `state`: `installedBusy`, `uninstallArm`, `installStatus` (busy/confirm
  guard state); `loadInstalled()` clears a stale armed uninstall when its row
  disappears.
- `style.css`: `#install-form*` + `.mod-row-actions` rules (confirm button
  gets a distinct danger tint).

## ENABLER 3 (build, required for any UI flow at all): fixture-OAuth build

The delivered download-runtime binary has `info.auth == NULL` (built without
`--oauth-config`), so EVERY modmgr route hard-503s
(`if (!info->auth) { 503 }` in http.c) and `/api/capabilities` reports the
fallback body. Rebuilt the same tree with the RETAINED fixture config
(`.superpowers/.../native-nexus-acquisition-plans-1/fixture-config-1/oauth-config.json`,
loopback-resolved, no credentials) so the real session admission
(Set-Cookie `mb_session` + CSRF + Origin) runs exactly as the browser
experiences it. Server run with `--port 62283` to match the fixture's
registered redirect URI. No real credentials, no provider contact
(`resolve` maps Nexus hosts to 127.0.0.1).

## New test assets (mine)

- `test/run_ui_flow_tests.py` — Level-1 real-server suite (34 checks).
- `test/drive_app_js.mjs` — Level-2 harness executing the REAL served app.js
  bytes in a Node DOM stub (18 checks).
- `test/regression/run_api_tests.py` — backend 42/42 retargeted to my tree.
