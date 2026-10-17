# REPORT.md — worker-app-backend-integration

Origin: worker-app-backend-integration, 2026-10-04 (ZCode team, PS5
mod-manager). Everything here is HOST-side: no network, no console, no
credentials, and the ORIGINAL app tree (`outputs/ps5-mod-bridge/native-host/`)
was never written to — all work lives in this worker dir.

## 1. Result

**The missing app mod-service surface is integrated and host-proven in a
vendored copy of the app tree.** Worker-8's recorded backend + http.c patches
are APPLIED (with honest notes), worker-app-modmanager's proven core loop
(18/18) is vendored verbatim behind new HTTP endpoints, and the integrated
API passes 42/42 offline host checks with every mutation verified through the
installer binary's own reader.

```
python3 test/run_api_tests.py
== PASS=42 FAIL=0 ==                                (receipts/api-tests.log)
```

| Deliverable | Path | Status |
|---|---|---|
| Claim (first) | `CLAIM.json` | written before any work |
| Vendored app tree | `vendored/repo/outputs/ps5-mod-bridge/{native-host,web,tools,third_party,manifest.json,include,src}` + repo-level sources referenced by build.py | 449 files (build artifacts excluded) |
| worker-8 patches applied | `patches/*.applied.patch` + `patches/APPLIED.md` | applied; `*.applied.patch` files are exact diff-vs-upstream proof |
| Proven core loop, verbatim | `vendored/.../native-host/app_core/{modmgr.py,console_link.py}` | sha `2590e0e4...` = worker-app-modmanager's bytes |
| API driver (new, mine) | `vendored/.../native-host/app_core/modmgr_api.py` | the ONLY code the C server executes for mod state |
| C wiring (mine) | `vendored/.../native-host/{http.h,http.c,host.c}` (+ `tools/bootstrap_deps.py` local-url fix, documented) | compiles clean under `-Wall -Wextra -Werror` in BOTH configs |
| Host tests | `test/{modmgr_http_tests.c,run_api_tests.py}` → `receipts/api-tests.{json,log}` | 42/42 |
| Full app build | `receipts/host-build.json` | desktop_fixture builds clean, build_id `85fe7f02...`, mfw8 service linked |
| Proven installer binary | `vendored/bin/modinst_composed` | sha `f2215f2f...` = CONSOLE-CONTRACT §7 pin |

## 2. Architecture of the integration

```
web UI (app.js)  --HTTP-->  vendored http.c (C server)
   GET  /api/installed          -> fork/exec python3 app_core/modmgr_api.py list
   POST /api/install            -> {"title","id","path"} -> modmgr.install
                                   + console-queue staging (console_link emitter)
   POST /api/mods/<id>/enable   -> modmgr.enable  + staged recompose command
   POST /api/mods/<id>/disable  -> modmgr.disable + staged recompose command
   DELETE /api/mods/<id>        -> modmgr.uninstall + staged remove command
   (worker-8) GET/POST /api/mod-service[/records|/request] -> mfw8 service
```

- Enabling configuration is RUNTIME env (`MB_MODMGR_BIN/_STATE/_ROOTS/_SCRIPT/
  _PYTHON`), read by `host.c` into `info->modmgr`. With no env the routes do
  not exist (fall-through, capabilities stay `installed_records:false`) — the
  vendored app behaves exactly like upstream when not configured.
- `GET /api/installed` speaks the UI's REAL contract (found at
  `web/app.js:935`): `{"items":[...]}` where the tab filters on
  `transaction_confirmed === true` and a 64-hex `sha256` (the mod's bundle
  file sha). Items additionally carry modmgr's honest projection
  (`state`, `enabled`, `files`, `bytes`, `loadlist_diverged`, overlay ids).
- Mutations require session cookie + `Origin` + `X-CSRF-Token` and request
  `MB_NEXUS_AUTH_ADMISSION_MUTATION` (the app's existing auth ABI); reads use
  the existing READ admission. POST /api/install bodies are bounded by the
  existing 4096-byte JSON gate; the archive itself must ALREADY sit in the
  intake dir next to the state dir (the download pipeline's deposit point),
  enforced by realpath containment in C — raw-byte upload was NOT added
  because the C request parser bounds bodies at 4096 bytes by design.
- Console drive protocol behind mutations: every successful host mutation
  also stages the console-reproducing command file(s) with the PROVEN
  `console_link.build_command` emitter under
  `<state>/modmgr/console-queue/` + a sha-bound `manifest.jsonl` (verified by
  readback in tests). Disable stages `install --mods` WITHOUT the mod;
  enable stages the full desired set; uninstall stages `install --remove`;
  zip install stages register-mod + prepare-archive + install — byte grammar
  of the pinned staged-command schema.
- `modmgr_api.py` serializes ops with an flock on `<state>/modmgr/api.lock`;
  driver output is a single JSON envelope; C maps driver refusals to
  400/404/409/500 with the envelope body passed through.

## 3. Honest endpoint table (host-proven vs needs-console)

| Endpoint | Host-proven (this work) | Console / controller dependency |
|---|---|---|
| `GET /api/installed` | YES — real records.txt + live loadlist parse; UI-contract items; tested incl. empty/ambiguous/disabled states | Nothing (read-only). `running:true` stays ack-derived only |
| `POST /api/install` | YES — real zip->mfb bridge + journaled root swap via the proven binary; records `installed` + generation++ + sha committed and read back through the binary's own `records` verb | Console EFFECT needs the payload session: staged commands must be transferred/consumed per drive protocol (controller-only); payload has no config-file reader yet (MODE A argv only) |
| `POST /api/mods/<id>/disable` | YES — guarded row strip, bundle-index attribution, backup written, records sha resync VERIFIED via binary reader | Console EFFECT needs the staged `install --mods` recompose to run on console |
| `POST /api/mods/<id>/enable` | YES — exact inverse from overlay; byte-equal restoration lineage | same as disable |
| `DELETE /api/mods/<id>` | YES — F-R2 removal path; idempotent second remove returns 200 honestly | same (staged `install --remove`) |
| `GET/POST /api/mod-service[/records|/request]` | Linked + built (worker-8's 61/61 lineage + my build); routes wired by applied patches | Session admission in tests is a STUB (the tree's own offline discipline); a real session needs the download-runtime build — the retained `--download-profile` inputs are NOT recorded in this workspace (worker-dll-mod-acquire noted the same), so live-cookie HTTP through the FULL binary is NOT claimed |

Not proven anywhere here: UE4SS in-game execution, `running:true` from a real
ack, console command consumption/transcript pairing against a live payload —
`console_executed:false` in every receipt.

## 4. Safety gates (CONSOLE-CONTRACT.md §5) — where each lives

1. Payload identity — pin (`e3489297...`, 681528 B) written into every staged
   manifest row; dispatch-time enforcement is the controller's.
2. Stale-command — staging refuses to write an unconsumed slot (tested).
3. Transcript pairing — `modmgr_api.py transcript` validates e2e-status.jsonl
   via the proven `E2eTranscript` (tested present-less; live pairing is
   console-side, pending).
4. Ownership marker — enforced inside proven modmgr (refuses unmanaged roots).
5. Transactional root swaps — all installs/uninstalls through the binary's
   journaled swap; recover on every open.
6. Backup/restoration — `<state>/modmgr/backups/<TITLE>/` per edit + disable
   overlay as exact inverse (proven lineage, exercised here).
7. Records-sha resync — verified through the binary's own `records` verb on
   every edit (tested; mismatch is io_error, never silent).
8. Capability honesty — `installed_records` reflects the runtime truth;
   `console_execution:"unverified"` untouched; no `running` anywhere.
9. No auto-retry — refusals are terminal (409/4xx with envelope); new slots
   for new decisions; the tests never replay a refused command.

## 5. Honest limits

- All HTTP-level proof runs the tree's own offline pattern: http.c compiled
  with a stubbed auth (exactly like `tests/test_mod_service_read.py`) and raw
  frames through `http_handle()` — plus a real `fork/exec` of the real driver
  and real installer binary. It is NOT a live-cookie run of the full binary.
- The built desktop_fixture binary (receipt above) serves `/version`,
  `/api/capabilities`, and 404-falls-through `/api/installed` when the modmgr
  env is absent — the routes are compile-time bound to the session runtime
  (`MB_HOST_DOWNLOAD`), matching upstream structure.
- worker-8's recorded patches needed count corrections to apply (their
  headers were miscounted); content applied is verbatim. See APPLIED.md.
- `tools/bootstrap_deps.py` vendored-copy fix for pre-existing `local://`
  manifest verification failure (fails in the ORIGINAL tree too) — bytes stay
  sha-pinned; no fetch path added.
- The disable staged-command semantic bug (recompose including the disabled
  mod) was found BY these tests and fixed in `modmgr_api.py` before final green.

## 6. Bounds

~85 tool calls (target 50-90). CLAIM.json first, REPORT.md last. Writes only
inside this worker dir (plus process-local /tmp fixtures); the original app
tree, the studio tree, and all global settings untouched.
