# READINESS.md — pre-console readiness report, full staged E2E rehearsal (host)

Origin: worker-e2e-rehearsal, 2026-10-04 (ZCode team, PS5 mod-manager).
Everything below ran HOST-SIDE ONLY on a fresh rebuild of the newest wiring
(worker-e2e-final patch chain over worker-installer-composition). No network,
no console, no original file modified. Binary identity: the rebuilt host
binary is **bit-identical** to worker-e2e-final's pinned receipt
(`c1a594c33ce74005a572c5ad7b9cf37731b6261e6252b3ee5c77a823e1db048e`), and the
regenerated `composition_e2e_inner.py` matches its pinned sha
(`606a8a98…`) — the patch chain reproduces exactly.

Rehearsal result: **PASS=61 FAIL=0 INFO=3** (`rehearsal/summary.log`,
`rehearsal/console.log`, transcript `rehearsal/e2e-status.jsonl`).
Regression on the same rebuilt binary: `test/smoke_host.sh` 26/26,
`test/smoke_e2e.sh` 31/31 (logs in `composition/receipts/`).

## 1. Per-stage pass/fail (host rehearsal, title PPSA33286)

| Stage | Verdict | Evidence (from rehearsal/console.log + summary.log) |
|---|---|---|
| S0 baseline | **PASS** | fresh tree: records empty, exit 0, no mod root, no e2e-status.jsonl before first command |
| S1 download (stub `file://` transport, REAL sink + records) | **PASS** | exit 0; `downloaded mod=rehearsal-minimap … sha256=cf3c353b…`; intake zip bytes == pinned corpus sha (worker-6 zip-roundtrip value); sidecar `downloads/rehearsal-minimap.json` content-addressed, no URL; records row `downloaded`; e2e-status `stage-1-download` begin + status `outcome:ok` (detail = sha) + end |
| S2a prepare (REAL libarchive intake on the corpus zip) | **PASS** | exit 0; durable `downloaded->prepared` transition (`records prepared mod=rehearsal-minimap`); quarantine members + `manifest-v1.bin`; idempotent re-run (`records unchanged`); nothing under mod root; unknown-mod refusal proven in the 31/31 suite (B4/B4b) |
| S2a probe: install of the raw archive mod | **REFUSED (expected)** | exit 2, `error archive_unreadable bundle … not a modforge bundle` — finding F-R1, see §4 |
| S2b bundle flow | **PASS** | two bundles built (pak + lua_mod roles), registered `downloaded`, `prepare` -> ready; both `prepared`; nothing under mod root yet |
| S3 install (transactional) | **PASS** | exit 0; `installed title=PPSA33286 generation=1 mods=mm-skin,mm-lua`; pak + lua files staged; `loadlist.txt`; `.modforge-root` marker; journal gone after commit; e2e-status stage-3 begin + end exit 0 |
| S4/5 activation-records | **PASS (simulated ack only)** | `activated` -> records `running=0` (honest), `mods-started` -> `running=1`; both derive ONLY from the runtime ack file. Host used `simulate-runtime` = ack-file format proof; a REAL `running=1` exists only from the injected runtime's `module_start` ack on console |
| S6 uninstall | **PASS** | `install --remove mm-lua` -> file gone, mod `prepared`, mm-skin still `installed`; remove-all -> exit 0, mm-skin `prepared` |
| S7 clean state | **PASS** | 0 installed rows in records.txt; root is an empty managed root (marker present, header-only loadlist, no mod files, no runtime-status.txt); no `.next`/`.prev`, no journal, no `.tmp`; download sidecar still durable. Transcript: 44 lines, 21 begins, 20 ends, **0 failed ends**, 1 unmatched begin = exactly the S2a refusal probe (F-R2) |

Final durable state (`rehearsal/records.txt`): 3 mods all `prepared`
(`rehearsal-minimap`, `mm-skin`, `mm-lua`), game row generation 3 (install +
two uninstall transactions), load-list sha recorded.

## 2. F1 (ps5_io RDWR gap) — exact impact on the console flavor

The REAL file sink (`archive/private/file_sink.c:31`) admits only an O_RDWR
fd (fcntl F_GETFL check) because the transport streams through it with
readback verification. The exported `ps5_io` flag set has **no RDWR
member**. Consequences, exactly:

- Host flavor: opens the sink tmp with POSIX `open(O_RDWR|O_CREAT|O_TRUNC)`
  directly — works (proven above and by the 31/31 suite).
- Console flavor: cannot express the sink's O_RDWR admission through
  `ps5_io`; therefore the download TU (`owned/modinst_download.c`) is **not
  linked into the console payload at all** (`MI_HAS_DOWNLOAD` is host-only).
  The console binary's `download` dispatch answers
  `error bad_request download transport not linked into this flavor`.
- Net effect: **stage 1 through the binary's own download command is
  impossible on console today.** All other stages (0, 2a/2b, 3, 4/5, 6) are
  unaffected by F1 — gap-1 records wiring IS compiled into the console
  payload (my rebuild: 664400 bytes, 116 imports, 0 missing).
- Until F1 closes, the console run's stage 1 must use the owner-placed
  archive alternative (CONSOLE-E2E-PLAN stage 1: the download owner puts
  `/data/modbridge/intake/<id>.zip` and the sidecar/registration happens via
  `register-mod`/job placement).

## 3. Precise remaining console-only steps

1. **ps5_io RDWR extension** (upstream ask to worker-installer-ps5io): add an
   RDWR member to the exported flag set so the file sink's O_RDWR admission
   is expressible on console. Required before step 2.
2. **§1A native relink of the download TU** (RUNBOOK §1A): relink the console
   flavor adding, from the pinned closure
   (`outputs/ps5-mod-bridge/native-host/download` + its
   `download/dependency_build.py`): `owner.c`, `curl_download.c`,
   `ps5_entropy.c` + the pinned native curl/PSA closure, keeping
   `-Wl,--no-undefined,-z,now` (missing symbol aborts loudly). Retained API
   symbols exactly: `mb_curl_runtime_start_v1`, `mb_curl_download_v1`,
   `mb_curl_runtime_close_v1`, `mb_curl_ps5_entropy_read32_v1`. The host stub
   `owned/mb_curl_stub_host.c` must NEVER be linked on console.
3. **Signed-in provider session for real downloads** (RUNBOOK §1B): a
   mod-provider user session owned by the DOWNLOAD OWNER process (not this
   installer); URI resolution via `mb_nexus_resolve_download_v1` under that
   session; the installer receives only the resolved URI in the TSV job file
   (`id`,`title`,`url`,`intake`, optional `sha256`,`timeout_ms`,
   `connect_timeout_ms`,`max_bytes`; unknown keys refused). No credentials in
   code, job files, records or logs, ever.
4. **UE4SS module presence** (deferred component, capability table
   `state/caps.tsv` gates `planned`/`integrating` frameworks): without UE4SS
   actually present and enumerable, `lua_mod` mods install files but stage 5
   must honestly end `running=0` (mods-started unreachable). Never fabricated.
5. **Runtime ack emissions in the injected runtime** (worker-runtime-ack
   `runtime-ack.diff`, RuntimeAckSink at `module_start`): without it stage 5
   honestly fails; `running` derives only from the runtime's own status file.
6. **Console environment (C4)**: jailbroken PS5, `/data/modbridge` writable,
   payload deployed via the pinned SDK loader path, title under test known,
   game NOT running during stages 1-4.

## 4. Controller decision point (not a blocker, must be chosen before the run)

**F-R1 (confirmed in this rehearsal):** a real downloaded zip can be
downloaded and quarantined (2a) but NOT installed — `install` refuses with
`not a modforge bundle` (exit 2). There is no archive->bundle bridge yet.
The user-fixed end goal "download -> prepare -> install -> activate ->
uninstall" on console therefore needs either (a) the supplier/owner delivers
`.mfb` bundles (2b path — proven end-to-end here), or (b) a new
quarantine->mfb adapter (new work, not scoped today). The console acceptance
run should state which path it exercises.

## 5. Go / no-go recommendation

- **GO (conditional)** for a console acceptance run of stages 0, 2a
  (owner-placed corpus zip), 2b, 3, 4/5 and 6 with prerequisites: C4
  environment, gap-1 console payload (this rebuild: 664400 B / 116 imports /
  0 missing), runtime-ack-carrying injected runtime, and an explicit UE4SS
  decision (accept honest `running=0`, or ship UE4SS first). Every one of
  those stages is host-proven end-to-end here with the exact newest wiring.
- **NO-GO** for stage 1 (native on-console download) until F1's ps5_io RDWR
  extension + the §1A relink + the owner's signed-in session exist. Until
  then the console run must enter at stage 1's owner-placed-archive
  alternative.
- **NO-GO** for claiming `running=1` on console without both the runtime-ack
  payload and UE4SS present (honest-fail rule).
- Minor, non-blocking: F-R2 (dangling transcript begin on install's refusal
  path) and the console-ELF sha drift vs the e2e-final receipt (same size,
  116 imports resolved; see REPORT §3) should be recorded in the console
  run's plan but block nothing.
