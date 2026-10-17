# REPORT.md — worker-app-ui-flows

Origin: worker-app-ui-flows, 2026-10-04 (ZCode team, PS5 mod-manager).
Host-side only: no console contact, no nested subagents, no network/provider
contact (`console_executed:false`, `provider_contact:false` in every
receipt). All upstream trees were READ-only: worker-app-backend-integration's
vendored 42/42 API tree and worker-app-download-runtime's vendored-app +
build; the original app tree (`outputs/ps5-mod-bridge/`) was never written.
All work + fixes live in this worker dir.

## 1. Result

**The app's UI flows are now host-proven end-to-end against the integrated
API — and the host testing found and fixed one genuinely broken server path
that the 42/42 offline suite could never catch.**

The Installed tab used to be empty for TWO stacked reasons: the missing
`GET /api/installed` (fixed by worker-app-backend-integration) and — found
this session — the delivered binary hard-503s every modmgr route because a
build without `--oauth-config` has no auth runtime. I rebuilt the tree with
the retained fixture OAuth config (fully offline, loopback-resolved), which
turned the real session admission on, and drove the flows at two levels.

```
Level 1  real server + real session (cookie/CSRF/Origin):  PASS=34 FAIL=0
Level 2  REAL served app.js bytes in Node DOM stub:        PASS=18 FAIL=0
Backend 42/42 offline regression on my fixed http.c:       PASS=42 FAIL=0
```

## 2. What was fixed (all in MY vendored copy — see FIXES.md)

| Fix | Where | Why it matters |
|---|---|---|
| `resolved[]` scope bug (FIX 1) | `native-host/http.c` | Every real `POST /api/install` exec'd the driver with an EMPTY `--file` under the release `-Os` build: the buffer's address escapes its block and stack coloring zeroed it (proven by pointer+content instrumentation). The 42/42 harness compiles at -O0, which masked it. Fixed by hoisting to function scope; install now works through the REAL binary. |
| Installed-tab controls (FIX 2) | `web/app.js`, `web/style.css` | The UI had zero calls to the new endpoints. Added: install-from-archive panel (title/id/path → `POST /api/install` → busy state → refresh), per-row Enable/Disable toggle, two-step uninstall confirm, honest projection meta (enabled/files/bytes). No-body POSTs (the C parser refuses bodies outside the JSON gate). Token validation mirrors the server charset. Read-only service-records branch untouched. |
| Fixture-OAuth rebuild (ENABLER 3) | `build/final/` | Without `info->auth` EVERY modmgr route 503s and the UI stays empty no matter what. Retained fixture config + `--port 62283` gives the true browser-equivalent admission offline. build_id `9b46b161…`, sha256 `a677e7b2…`, 1,682,216 B. |

## 3. Honest proof table (full detail in UI-FLOW-RESULTS.md)

| Flow | Host-proven | Needs console session |
|---|---|---|
| Installed list renders real rows (`{items:[{transaction_confirmed, sha256,...}]}`) | YES — both levels, incl. UI filter semantics | Nothing (read-only) |
| Install from archive → progress → refresh | YES — real binary, real driver + pinned installer, staged commands verified | Console EFFECT (staged command consumption) |
| Disable/enable toggle round-trip | YES — incl. UI flip + server projection | Console recompose consumption |
| Uninstall confirm (two-step) | YES — row honestly leaves the tab (`prepared`, filter), idempotent DELETE | Console `install --remove` consumption |
| Negatives (no session 401, no CSRF/Origin 403, outside-intake 400) | YES | — |

Not proven anywhere here: visual/layout rendering (no browser driver exists
in the tree — Level 2 is the real app.js code on a DOM stub, stated
plainly), live Nexus/OAuth, UE4SS execution, `running:true` from a real ack.

## 4. Deliverables

- `CLAIM.json` (first) — this dir
- `vendored/` — fixed tree; `FIXES.md` — every fix recorded with proof
- `build/final/desktop_fixture-download/modbridge-native-host-desktop-download`
  — the serving binary used for all flows
- `test/run_ui_flow_tests.py` (Level 1), `test/drive_app_js.mjs` (Level 2),
  `test/regression/` (42/42 retarget)
- `receipts/` — ui-flow-tests.{json,log}, ui-flow-harness.json,
  api-regression-42.log, server*.log
- `UI-FLOW-RESULTS.md` + this `REPORT.md` (last)

## 5. Bounds

~70 tool calls (target 40-80). CLAIM.json first, REPORT.md last. Writes
confined to this worker dir plus process-local `/tmp/mb-ui-flows-fix`.
Fixture processes were stopped after the runs (identified tasks only).
Upstream worker dirs and the original app tree: read-only, zero writes.
