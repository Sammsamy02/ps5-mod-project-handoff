# REPORT — worker-launch10-staging-packet (2026-10-04)

## What was delivered

The exact launch-10 console staging packet, built on the JUST-FIXED batch tool
(`worker-console-batch-runner/console_batch.py`, staging-order revision
`6940971e…7432040c`, patched in place, now `b982f794…f639303`).

### 1. NEW flow `stage-launch10` in console_batch.py

Controller staging facts implemented literally: ONE file changes on the console
(`/data/plugins/modbridge_supplier_composition.prx` ← the rebuilt launch-10
composition FSELF); `ploader.ini` STAYS the 2-plugin config
(sha `1216aab61236b0fc9734b1647c96e03db48beb3d333efe174ca08e97abededa0`,
composition + path_test enabled) — NO ini republish, and the flow has NO ini
write anywhere. Console: ftp ps5-host-not-configured.invalid:2121, pldmgr http 8084 (staging only).

Step order (the flow's whole law):

| step | what | group |
|------|------|-------|
| `l10.0`  | preflight: `--fself`/`--expect-sha` must be filled + the local FSELF must hash to the pinned sha (BUILD-ID law). Unfilled `<L10_FSELF_LOCAL_PATH>`/`<L10_FSELF_SHA256>` placeholders = loud refusal; no unidentified build is ever staged | preflight |
| `l10.0b` | G0 reachability — pldmgr 8084 `/list_payloads` (existing pattern; never probe alternates) | g0 |
| `l10.1`  | INI FREEZE (read-only): RETR `ploader.ini`, must byte-equal `1216aab6…` BEFORE any console change; drift refuses at zero console writes | ini-freeze |
| `l10.2`  | PRE-IMAGE + `.bak` FIRST: RETR the current composition PRX (sha+size saved to the run dir) + console `.bak` with a distinct TAG — the ride.9c-style rollback artifacts | preimage |
| `l10.3`  | UPLOAD EXACTLY ONCE: STOR the FSELF to its FINAL name (the ONE console write; no alias, no re-STOR). A failed STOR is ambiguous → ONE bounded restore + refusal | upload |
| `l10.4`  | READBACK CONTENT LAW vs the LOCAL FSELF: full byte identity, or ONLY the known ftpsrv SHT-strip tail-artifact class (len == max(p_offset+p_filesz) of the local file, body 64..end byte-identical, header diff inside `SHT_HDR_ALLOWED`, e_shnum zeroed). Any mismatch → OLD PRX restored byte-exact from the pre-image + refusal | verify |
| `l10.5`  | INI STILL BYTE-EQUAL: re-RETR `ploader.ini`, require `1216aab6…` again. Drift → restore the OLD PRX + refusal (the flow never wrote the ini — external drift; reconcile) | ini-post |
| `l10.6`  | FRESH BASELINES re-fetched into the run dir: `baseline-UE4SS.log`, `baseline-runtime-status.txt`, `baseline-entry-log.txt`, `baseline-supplier-diag.jsonl` (absence recorded honestly, never deleted). These supersede the older `launch10-prep` captures as the comparison point | baseline |
| `l10.7`  | GO/NO-GO summary — rescue step, so the verdict lands in `l10-go-nogo.json` even after a halt; GO requires every gate green. The next launch is USER-driven: the tool NEVER launches | summary |

Guard law (ride.9c, launch-10 form): on ANY failed verification the console is
returned to the OLD PRX — `_l10_restore_old_prx_once` does exactly ONE bounded
byte-exact STOR of the saved pre-image, proven by a fresh RETR, records
`l10-restore-verdict.json`, and never retries (the console `.bak` remains as the
operator's second rollback artifact). ploader.ini is only ever READ.

### 2. `run_stage_launch10.sh` (the commands the controller pastes)

- `plan`  — `python3 console_batch.py plan --only stage-launch10` (dry, zero contact).
- `stage` — refuses to run while the placeholders are unfilled, then
  `python3 console_batch.py stage-launch10 --fself <path> --expect-sha <sha> --run-root .../runs/launch10`.
  FILL `L10_FSELF_LOCAL_PATH="<L10_FSELF_LOCAL_PATH>"` and
  `L10_FSELF_SHA256="<L10_FSELF_SHA256>"` after the build lands.
- `post`  — ONLY after the user-driven launch: fetches UE4SS.log, runtime-status,
  entry log, supplier-diag into a timestamped dir + SHASUMS, and re-proves
  `ploader.ini` is STILL `1216aab6…` post-launch (sha-check with loud warning on drift).

### 3. Tests — full suites green, no regression

- `python3 console_batch.py selftest` → **462 ok, 0 failed** (exit 0); was 430/0
  at the staging-order revision (+32 launch-10 checks: order assertions, the
  no-ini-STOR assertion, exactly-one-upload-STOR, `.bak`-before-upload, rescue
  GO/NO-GO, plan anchors, 7 content-law fixtures, placeholder/BUILD-ID refusals).
- `python3 tests/fake_console_drive.py` → **143 checks ok, 0 failed** (exit 0);
  was 117/0 (+26). New scenarios against the REAL execution engine:
  **(a)** happy path — console sequence is exactly `.bak` → ONE FSELF STOR, the
  fake serves the SHT-strip view of the staged ELF, ini still byte-equal, 2 ini
  RETRs, baselines + manifest written, verdict GO; **(b)** upload-verify mismatch
  — l10.4 FAILs, OLD PRX restored byte-exact, the FSELF was stored exactly once,
  no ini STOR ever, NO-GO recorded; **(c)** ini drift between freeze and re-check
  — refusal names the pinned sha, OLD PRX restored, the drifted bytes remain (the
  flow does not reconcile the ini itself), NO-GO; **(d)** missing local FSELF and
  unfilled placeholders — refusal at ZERO console writes, NO-GO.
- Full output: `SELFTEST-RECEIPT.txt` (both suites + plan preview, exit codes 0/0/0).

## Limits (honest)

- **No real-console contact.** Everything ran against the in-process fake console
  or plan mode. The new steps' real behavior against the actual ftpsrv/pldmgr
  services is unproven until the controller runs them.
- **The FSELF identity is intentionally a placeholder.** The build has not landed;
  `<L10_FSELF_LOCAL_PATH>`/`<L10_FSELF_SHA256>` are filled via `--fself`/`--expect-sha`
  (or in the wrapper). The tool refuses to stage without them.
- **Content-law shape assumption.** The readback law accepts full identity or the
  modeled SHT-strip artifact class derived from the established §3 semantics
  (`sht_prefix_end`, `SHT_HDR_ALLOWED`, e_shnum=0) but verified against the LOCAL
  FSELF (no separate bare-sdk reference is pinned for launch-10). If the real
  channel serves a different shape, l10.4 REFUSES and restores — safe by
  construction, but staging would stop for reconciliation. The law must never be
  widened on a live refusal; re-verify the artifact first.
- **The pinned 2-plugin ini sha is taken from the controller's staging facts**
  (`1216aab6…`); its exact bytes were not available locally, so the fake exercises
  the ini law via a same-shape fixture (its own sha) — the drift refusal is proven
  structurally, not against the real bytes.
- **Failed-STOR ambiguity handling is fake-tested only in the readback-mismatch
  variant**, not the curl-failure variant: on `rc != 0` the tool cannot know
  whether bytes landed, so it performs ONE bounded restore of the pre-image and
  reports honestly.
- **`--continue-on-failure` interaction:** with distinct dependency groups per
  step, continue-mode would run read-only steps after a verification failure;
  the GO/NO-GO rescue step then records an honest NO-GO. The default operating
  mode is stop-on-first-failure, where the halt skips everything (except the rescue).
- The staged PRX replaces a historical console file IN PLACE (that is launch-10's
  point); the restore path returns the exact previous bytes, but any ploader-side
  caching of the old file across a mid-session swap is out of scope (staging runs
  with the game CLOSED).

## Controller runbook (exact)

```
# 0) dry-run (no console contact)
outputs/2026-10-04-zcode-team/worker-launch10-staging-packet/run_stage_launch10.sh plan
# 1) after the build: fill the two placeholders in run_stage_launch10.sh, then
outputs/2026-10-04-zcode-team/worker-launch10-staging-packet/run_stage_launch10.sh stage
#    -> GO: newest runs/launch10/<stamp>-stage-launch10 holds l10-go-nogo.json,
#       l10-readback-verdict.json, l10-baselines.json + baseline-* files
#    -> NO-GO: read l10-go-nogo.json + l10-restore-verdict.json; the console .bak
#       (modbridge_supplier_composition.prx.$TAG.bak) is the rollback artifact
# 2) USER launches the game (the tool never launches)
# 3) after the launch:
outputs/2026-10-04-zcode-team/worker-launch10-staging-packet/run_stage_launch10.sh post
```
