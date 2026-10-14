# REPORT.md — worker-8-app-backend: audit + drop-in install vertical (records + actions + caps)

Origin: worker-8-app-backend (re-dispatch of the orphaned fork worker; the
previous instance died with an empty output dir), 2026-10-04, ZCode team,
PS5 mod-manager. Everything here is HOST-side work: no network, no console,
no credentials touched, no existing file modified (PATCHES.md carries the
controller-applied diffs).

## 1. Result

**Delivered and host-proven: the missing records+actions vertical connecting
the existing app to the proven installer stack.** A drop-in
`modforge-service/1` backend (`mfw8_service`) now maps the app's records
surface onto the PROVEN `records.txt` store and executes
prepare/install/uninstall/cancel by delegating to the composed installer
binary. Full vertical proven against the REAL
`worker-zip-to-mfb/composition/build/host/modinst_composed` binary and a
REAL corpus zip:

```
sh outputs/ps5-mod-bridge/native-host/src/modforge-worker8/test/run_tests.sh
==== mfw8 host tests: PASS=61 FAIL=0 ====          (test/mfw8-test.log)
```

| Deliverable | Path | Status |
|---|---|---|
| Audit | `AUDIT.md` (also here) | file:line evidence, actual route/capability state |
| Drop-in modules | `outputs/ps5-mod-bridge/native-host/src/modforge-worker8/` (mirrored in `src/` here) | new files only |
| Records store | `mfw8_records.{h,c}` | proven records.txt reader/writer + identity sidecar |
| Job actions | `mfw8_actions.{h,c}` | register/prepare(+archive)/install/uninstall/recover/cancel delegating to the composed CLI |
| Capability report | `mfw8_caps.{h,c}` | honest JSON; running only from ack |
| Service adapter | `mfw8_service.{h,c}` | exact vendor mfs ABI (mfs_open/request/close) |
| Host tests | `test/mfw8_test.c` + `test/run_tests.sh` | 61/61 vs the real binary + real corpus zip |
| Controller patches | `PATCHES.md` | 3 unified diffs (read_bridge.h/.cpp, http.c) + archive link recipe |
| Receipts | `CLAIM.json`, `test/mfw8-test.log` | test log includes binary+corpus sha256 |

## 2. What the vertical proves (evidence)

Records semantics (mapped 1:1 from the proven lineage):
- `modforge-records\t1` TSV with `mod/variant/choice/game` rows exactly as
  `worker-7-installer/src/modinst.c:340-362` writes it; unknown rows/kinds
  are refused as corrupt, never guessed (check: unknown header/state refused).
- Only `downloaded -> prepared` forward; re-prepare idempotent; wrong state
  refused (e2e-final B1-B4b rules re-proven through my store, then END-TO-END
  through the real binary's own commit).
- Empty sdk in game rows accepted (the real binary writes `game <t> unreal <empty> 2 <sha>`
  — found by running the real binary; my first loader was stricter than the
  proven writer and was fixed to match reality).
- Identity sidecar `identity.tsv` (worker-8's own file) carries
  `download_id -> provider/domain/mod_id/file_id` so `records.list` can emit
  the exact `source{...}` shape `mod_service/register_bridge.cpp:214-221`
  validates; CLI-registered rows without identity are reported in a separate
  honest `installer_records` array instead of being hidden or faked.

Actions (all through the REAL composed binary):
- register -> `downloaded`; prepare -> `prepared`; install -> `installed`
  with generation + 64-hex loadlist sha committed to the game row; staged
  root carries the `.modforge-root` ownership marker.
- `--cancel-at pre-stage` -> exit 5/cancelled and the previously installed
  set is byte-identically unchanged (proven safe-point cancel semantics).
- uninstall -> `prepared`, file removed from the managed root.
- Exit contract 0/1/2/3/4/5 mapped to succeeded/io_error/bad_request/
  not_ready/needs_choices/cancelled (modinst.c:54-60).

Capability report honesty:
- `running:true` appears ONLY after a real runtime ack (`runtime-status.txt`
  last state `mods-started`, written in tests via the binary's own
  `simulate-runtime` proven hook); `installation_supported:false` without
  explicit console evidence; download-link and installer presence reported
  per flavor; caps.tsv passthrough (available/integrating/planned).

Service adapter (drop-in mfs ABI):
- `mfs_open` recovers via the installer at open, accepts the app-shaped
  config (extra STARTUP.md keys ignored), refuses ABI mismatch.
- `mod.register_download` (dl- + PPSA/CUSA + source identity) ->
  `records.list` `mods[]` passes the app's register_bridge shape; archive
  prepare via the proven auto-bridge (`prepare-archive --mod`) composed a
  kind-1 bundle from the REAL corpus zip; `install.apply` and
  `mod.uninstall` round-tripped through the service; `job.status/job.cancel/
  job.list` honest (synchronous job model documented in describe;
  cancel of unknown job -> `unknown_job`, terminal job -> state no-op).

## 3. Audit highlights (full detail with file:line in AUDIT.md)

- The app's 13 route groups are real but the mod-service surface is
  read-only passthrough (`http.c:771-799`); nothing in the app can drive
  prepare/install/uninstall today. `jobs.h:153-154` hard-codes
  runtime/installation false for its layer.
- `web/app.js:935` calls `GET /api/installed`, which does not exist in
  `http.c` (grep count 0) — the Installed tab always degrades to empty.
- `app_service_startup` checkpoint: the previously linked backend was a
  reference archive, `console_executed:false`; STARTUP.md leaves ModForge
  disabled in ordinary builds.

## 4. Controller-applied patches (PATCHES.md)

1-2. `mod_service/read_bridge.{h,cpp}`: add `mb_mod_service_call` generic
     passthrough (envelope validated, bytes bounded).
3. `http.c`: admit `POST /api/mod-service/request` to the body-framing gate
   (http.c:287-293) and route it in `mod_service_response` (http.c:771-799),
   keeping the existing session admission and `#if MB_HOST_DOWNLOAD` guard
   untouched. Plus the static-archive build recipe that links these modules
   through the EXISTING `build.py --mod-service-profile` path — no build.py
   change needed.

## 5. Honest limits (read before claiming more)

- Everything is HOST-proven. No console run, no network, no real Nexus
  download. The checkpoint line `installation_supported:false` remains true
  for the product until a console E2E proves otherwise; my caps report
  hard-defaults it to false.
- The action driver uses POSIX fork/exec — a HOST transport. The console
  payload process model cannot fork; binding `mfw8_actions` to the payload
  launch path is the named controller integration step. The delegation
  mapping, exit contract and records semantics are exactly the proven ones.
- The service executes jobs synchronously inside the serialized request;
  `service.describe` says so (`job_model:"synchronous"`). `job.cancel` is
  therefore a documented no-op for terminal jobs and refuses unknown ones;
  real cooperative cancellation is the installer's own proven safe points
  (`--cancel-at`), which the actions layer passes through.
- Multi-mod archive prepare is refused specifically (the proven intake is
  one archive per download id); FOMOD variant bundles, `install.plan`,
  `game.register`, FOMO choices and prerequisites remain unimplemented
  (`unknown_method`) — the method subset is advertised honestly.
- One upstream test-tool note: my first loader rejected the real binary's
  empty-sdk game row; fixed to the observed proven format. The fixture
  corpus used is `worker-zip-to-mfb/test/run/zb-lua/intake/zb-lua.zip`
  (sha in the test log).

## 6. Bounds

≤55 tool calls (used ~54). CLAIM.json first, REPORT.md last. Existing files
untouched; new files only in this output dir and the in-tree drop-in area.
