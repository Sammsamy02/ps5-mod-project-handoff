# REPORT.md — worker-e2e-final: E2E gap closure (prepare->records, download hookup)

Origin: worker-e2e-final, 2026-10-04 (ZCode team, PS5 mod-manager).
Everything here was built and verified HOST-SIDE ONLY (PS5IO_HOST=1 plus the
pinned console cross-compile); nothing ran on a console, no network was
touched, no original file was modified. All extensions are vendored copies +
recorded exact-anchor patch chains inside this directory.

## 1. Delivered

| Artifact | Path (here) | Status |
|---|---|---|
| Driver (vendors + splices the composition) | `compose_e2e.py` | reproduces everything: `python3 compose_e2e.py --flavor all` |
| Adapted composition script | `composition_e2e_inner.py` (generated; sha in `e2e-receipts/script-patches.json`) | host + console both build |
| Host binary (download wired, stub transport) | `composition/build/host/modinst_composed` | 26/26 original suite + 31/31 new E2E suite |
| Console payload ELF (gap-1 wiring) | `composition/build/console/modbridge-installer.payload.elf` | 664400 bytes (was 647928), 116 imports, **0 missing** (provider check re-ran clean) |
| Stage-1 download command | `owned/modinst_download.{c,h}` | job->owner ABI v1->REAL file sink->intake->records; host-proven |
| Host test transport stub | `owned/mb_curl_stub_host.c` | contract-faithful local stand-in; NEVER console |
| Records wiring | patch tuples `e2e-*` in `composition/receipts/patches.json` | gap-1 closed on BOTH flavors |
| New suite | `test/smoke_e2e.sh` | **PASS=31 FAIL=0** (log: `composition/receipts/smoke-e2e.log`) |
| Original regression (vendored) | `test/smoke_host.sh` | **PASS=26 FAIL=0** (log: `composition/receipts/smoke-host.log`) |
| Staged procedure | `E2E-RUNBOOK-V2.md` | incl. UE4SS placeholder status + first-real-download needs |
| Provenance | `e2e-receipts/{script-patches.json,owned-manifest.json,final-provenance.json}` + `composition/receipts/*` | sha256 per artifact, anchor + occurrence count per patch |

## 2. Gap 1 — prepare-archive -> records wiring (worker-7 model)

`prepare-archive` success now performs the durable `downloaded -> prepared`
transition in the records line store: new core static
`records_mark_archive_prepared` (invoked from the wired dispatch after
`mi_cmd_prepare_archive` returns 0; `--mod <id>` selects the mod; without it
the run is reporting-only). Rules mirror worker-7 exactly: only
`downloaded -> prepared` is defined; already-`prepared` is idempotent
(`records unchanged`); unknown mod or wrong state => exit 2 with the
quarantine retained (intake rule: a surviving journal is never erased). The
load-list sha256 remains the only install/recovery commit truth — untouched.
The owned prepare command additionally surfaces `mod=<id>` in its success
line (recorded patches `owned-prepare-*`). This wiring compiles and links
into the CONSOLE payload (gap-1 is closed for the target binary too).

Also fixed en route (recorded patch `e2e-dispatch-prepare-archive-wired`):
the original composition's `prepare-archive` dispatch `return`ed before the
trailing `e2e_status_end`, so stage-2a begin lines had no matching end line.
The wired dispatch emits the end line itself (same pattern as `download`).

Evidence: suite checks B1-B4b (transition, durable `records.txt` row,
idempotency, unknown-mod refusal + retained quarantine).

## 3. Gap 2 — download stage bound to the real curl-owner interface

Call path (vendored headers, sha-recorded): `mb_curl_runtime_start_v1` ->
`mb_curl_download_v1` -> `mb_curl_runtime_close_v1` (exclusive serialized
owner, start once / one job / close once, no retry ABI), origin
`outputs/ps5-mod-bridge/native-host/download` per receipt
`callable_archive_download_transport` @ ba0c4f1. The REAL
`archive/private/file_sink.c` is linked and driven by the transport: real
sha256, write-through, sync and readback verification all execute.

Config surface (explicit-config-only, receipt `native_nexus_download_jobs`
"catalog_and_scan_jobs_explicit_config_only"): one TSV job file per run
(`id`,`title`,`url`,`intake` required; `sha256`,`timeout_ms`,
`connect_timeout_ms`,`max_bytes` optional with pinned defaults); unknown
keys are refused (check C5). No credentials in code: the URL must already be
a provider-authorized resource resolved by the owner under the signed-in
user session (`mb_nexus_resolve_download_v1`); scheme policy belongs to the
transport (real owner: HTTPS only; stub: file:// only — check C7 proves the
refusal). URLs never reach records/logs (check A3c).

Flow proven (suite A/C): job -> sink tmp (fresh unpublished, sink-ABI
admission satisfied) -> transport carries 64 KiB stream through the real
sink -> fsync -> rename to `intake/<id>.zip` -> intake-dir fsync ->
content-addressed sidecar `<state>/downloads/<id>.json` -> registration row
`downloaded` written by the core hook `records_register_download_job` (the
record is the command's LAST durable write — the commit point). Failure
paths (sha mismatch, response-refused, mid-stream cut, timeout): exit 1,
tmp unlinked, no sidecar, no record (C1-C4, C8 no `.tmp` leftovers, C9
records intact, C10 fail events).

## 4. Findings

- **F1 (upstream ask, worker-installer-ps5io):** the real file sink admits
  only an O_RDWR fd (fcntl F_GETFL check, `archive/private/file_sink.c:31`),
  but the exported `ps5_io` flag set has no RDWR member. Host flavor opens
  the sink tmp with POSIX `open(O_RDWR|O_CREAT|O_TRUNC)` directly; console
  needs a `ps5_io` RDWR extension before the download TU links into the
  payload. Documented in the code and RUNBOOK §1A.
- **F2 (fixed, recorded):** original composition bug — `prepare-archive`
  returned from main before `e2e_status_end`, so stage-2a never wrote its
  end line (a reader would misread it as a dead command). Fixed in the wired
  dispatch.
- **F3 (found by my own suite, fixed in my TU):** the job parser's error
  paths formatted from `k`/`line` AFTER `free(buf)` (use-after-free;
  malloc tcache metadata replaced the first bytes). All four paths now
  format before freeing. Recorded here because it is exactly the class of
  bug the receipt discipline exists to expose.
- **F4 (honesty):** the host stub is NOT curl/TLS — it proves the wiring,
  admission, sink, records and failure semantics over local files. The
  first real console download additionally needs the §1A link step and the
  owner's signed-in user session (RUNBOOK §1B). Nothing console-side has
  run.

## 5. Patch chains (sha provenance)

- `composition/receipts/patches.json` — the inner script's full chain over
  worker-7 `modinst.c`, now ending with my six `e2e-*` tuples
  (inc-download, flags-mod-job, records-hooks, dispatch-prepare-archive-
  wired, dispatch-download, usage), each with old/new sha256 and verified
  occurrence count 1; plus the original bridge correction.
- `e2e-receipts/script-patches.json` — the five recorded splices into the
  vendored `compose_installer.py` (sha256 e9074052b92c…) producing
  `composition_e2e_inner.py`: DL root, vendor dirs, ABI headers, host
  `-DMI_HAS_DOWNLOAD=1`, host targets (+file_sink, +download, +stub).
- `e2e-receipts/owned-manifest.json` — vendored owned adapters with the
  `owned-prepare-*` (4 patches, `--mod` surfacing) and
  `owned-e2estatus-download-stage` chains.
- `e2e-receipts/final-provenance.json` — sha256 of every delivered
  artifact, console ELF facts (664400 B, 116 imports, missing []), both
  suite results.

## 6. How to rerun

```
cd outputs/2026-10-04-zcode-team/worker-e2e-final
python3 compose_e2e.py --flavor all   # host + console, receipts regenerate
sh test/smoke_host.sh                 # expect PASS=26 FAIL=0
sh test/smoke_e2e.sh                  # expect PASS=31 FAIL=0
```
