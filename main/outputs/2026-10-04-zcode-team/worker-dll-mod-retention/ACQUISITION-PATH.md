# ACQUISITION-PATH — how the controller acquires one representative DLL mod (2026-10-04)

Authority for every claim below is cited from retained receipts. Workers never
perform network calls (this worker ran with NO NETWORK).

## The two existing, receipt-backed job owners (from `outputs/ZCODE-HANDOFF-2026-10-04/TRANSPORT_CHECKPOINT.json`)

### 1. `native_nexus_download_jobs` — the Nexus-side job pipeline
- Checkpoint entry: `status: complete_committed`, commit `fcc16d5caec15a4976562a45658238da1ba8cfcc`
- Plan: `docs/superpowers/plans/2026-10-02-native-nexus-download-jobs.md`
- Ledger: `.superpowers/sdd/2026-10-02-native-nexus-download-jobs/progress.md`
- Verified evidence: 26 actual HTTP/TLS checks, 21 actual TLS transfers, 65 job
  controls, 43 archive controls, 18 dependency controls
  (`.superpowers/native-dependency-spike/native-nexus-download-jobs-1/actual-http-tls-1/run-3/receipt.json`;
  root final verification at `.../native-nexus-download-jobs-1/root-final-verification.json`)
- Standing constraints carried in the receipt: `credentials_jobs_storage:
  "process_lifetime"`, `catalog_and_scan_jobs_explicit_config_only: true`,
  `console_contact: false`, `live_account_contact: false`,
  `game_or_system_files_changed: false`.
- Job manifests already exist as templates:
  `.../native-nexus-download-jobs-1/app-host-5/desktop_fixture-download/manifest.json`
  (host side) and `.../app-native-3/loopback-download/manifest.json` (native side).

### 2. `callable_archive_download_transport` — the exclusive download producer
- Source: `outputs/ps5-mod-bridge/native-host/download/` (`owner.c`, `owner.h`,
  `curl_download.c/h`, `README.md`, `verify.py`, `requirements_post_verify.py`)
- API (checkpoint + `curl_download.h`):
  `mb_curl_runtime_start_v1` / `mb_curl_download_v1(
  const mb_curl_download_params_v1*, mb_curl_download_result_v1*)` /
  `mb_curl_runtime_close_v1`
- README constraints: "The caller must exclusively own curl/PSA, serialize every
  call"; sink ceiling 1 GiB; URIs resolved "using the existing Nexus client
  rules; a native job dispatcher must bind that result before calling this
  producer"; HTTP/TLS rejection vs ordinary job timeout have distinct owner
  semantics (header comment in `curl_download.h`).
- Verified red/green evidence cited in the checkpoint:
  `.../curl-stream-download-1/{red-repeat,owner-clock-red-3,smoke-3,full-1}/...`
  (19 jobs; real 195,035,136-byte and 105,647,244-byte archive streams).

## Exact controller procedure (one representative mod)

1. **Pick candidate** per RETENTION-CRITERIA.md (first acquisition: proxy-shape
   UE4 title fix mod, e.g. FF7RFix/P3RFix class).
2. **File an explicit-config Nexus job** through the `native_nexus_download_jobs`
   pipeline (controller only — `catalog_and_scan_jobs_explicit_config_only`):
   record game, mod id, file id, version, page URL, and the Nexus account flow
   already established by the project. Credentials stay process-lifetime; never
   persisted by workers.
3. **Execute transfer via the exclusive curl/PSA owner** using
   `mb_curl_runtime_start_v1 -> mb_curl_download_v1 -> mb_curl_runtime_close_v1`,
   serialized, sink ceiling 1 GiB (candidates #1/#2/#5 are far below it). The
   job dispatcher binds the Nexus resolution result before invoking the producer.
4. **Retain bytes** under a new dated observation dir mirroring existing intake
   convention (cf. `item-spawner-real-mod-intake-1/`):
   `.superpowers/native-observations/dll-mod-retention-2026-10-XX-1/<slug>/original.zip`
5. **Write `manifest.json`** next to the archive, modeled on the verified
   `app-host-5` manifest format, adding mod provenance:
   - `attributions[]`: project (mod name), source (repo + Nexus page URL),
     version, license (from mod page/repo; note if ambiguous)
   - `inputs{}`: per-file `bytes` + `sha256` (existing format)
   - provenance block: game, engine, mod_id/file_id/version, downloaded_at_utc,
     account flow used, job receipt path, original request URL
6. **Write `receipt.json`**: job id, transfer stats (bytes, duration, resume
   events), sha256 verified-on-disk (readback), and the
   `verify.py`/`requirements_post_verify.py` outcome from the download owner.
7. **Run the census immediately** (host-side, zero-risk, no execution of mod
   bytes):
   `python3 outputs/2026-10-04-zcode-team/worker-dll-mod-retention/inspect_dll_census.py <retained>.zip -o <retained-dir>/census.json`
8. **Compare against baseline** (`baseline-dll-census.json`) and update the
   services backlog with the third-party deltas (new families, new uncategorized
   APIs, static-TLS/CRT-startup findings).

## Rules that bind this path

- No replay of failed jobs without reconciliation; owner timeout vs TLS refusal
  semantics differ (curl_download.h note).
- Never bypass the exclusive owner, never parallel curl/PSA use.
- Archive is retained inert; only the census tool parses it (pure-stdlib,
  in-memory zip read; nothing extracted or executed).
- Hash (sha256) must verify before the artifact is counted as retained.
