# REPORT — worker-launch10-evidence-upgrade

2026-10-04 (host clock 2026-10-05 UTC) · host-side tooling upgrade only · NO
console contact · no nested subagents · nothing executed against the console;
the fixture suite invokes `evidence.sh` in `--from-dir` mode only. The
worker-launch9-evidence original is untouched; everything below lives in
`outputs/2026-10-04-zcode-team/worker-launch10-evidence-upgrade/`.

## What was delivered

1. **CLAIM.json** (written before the final suite run) — scope, per-claim
   provenance, launch-config truth, limits.
2. **evidence.sh** — the upgraded launch-10 post-launch evidence collector
   (POSIX sh + awk; sha256 `5a034d460e642a51a7689e7cd498f9367609c9cddf64fcb31cdaed101601ba52`
   at receipt time). Modes: live FTP (controller only) · `--from-dir` ·
   `--dry-run` · `--plan` · `--json-tail` · `--out`. Exit codes 0/2/4 unchanged.
3. **STAGE-TABLE.md** — the r13/r14 marker → stage table with per-stage
   success/failure markers, channels, expected terminal state strings, the
   launch-9 terminal anchor case, timing-marker rules, and the full source
   list. Nothing in it is invented; every marker was read back from source.
4. **host-test/run_fixture_tests.sh + fixtures/** — 16 cases, 84 checks,
   built from the REAL launch10-prep baseline bytes (read-only) + synthetic
   appends, so the drift guards meet the exact bytes they will see on launch day.
5. **receipts/fixture-test-pass.txt** — captured passing run:
   **84 passed, 0 failed**; harness rc=0.
6. **REPORT.md** (this file, last).

## Task coverage

### 1. Baseline rebase (launch-9's stale-baseline gap)
- Defaults are now the launch10-prep controller-captured files: UE4SS.log
  **7591 B** `edf404a5…`, entry log **2343756 B** `4b33b985…`, runtime-status
  **169 B** `57c40d4c…` (session 273), xwp jsonl **4362 B** `20cff601…`.
  All four flags overridable (`--baseline-ue4ss-bytes`, `--baseline-entry-bytes`,
  `--baseline-status-bytes`, `--baseline-xwp-bytes`; `--baseline-aligned-bytes`
  kept as a deprecated alias). I hashed the local launch10-prep files and they
  match the briefing prefixes exactly.
- Old launch-9 baselines retained as labeled history in the header comment:
  the 2892 B "UE4SS baseline" was a 411 B error page (never valid); the
  2210306 B entry boundary was the clean attempt boundary (diagnostics-mine).
- Freshness oracle replaced: launch-9's `FRESH_MS=8866785` constant would have
  mis-fired post-launch-9. The tool now pins the baseline SESSION
  (273/44372083, flags `--baseline-session-pid/--baseline-session-ms`); a
  fetched status with a different session = fresh launch-10 window.

### 2. Marker → stage classification (attribution gap)
- In-script stage table + STAGE-TABLE.md: S0 diag-channel → S1 entry/ctors →
  S2 activation → S3 VMMAP walk → S4 lease registration → S5 identity anchors
  → S6 conversion (incl. `derive_readable_ranges`) → S7 configured-roots
  attach → S8 context build/validation → S8B initializer/seeds/GMalloc →
  S9 start ack → S10 engine ladder → S11 stop. Output: per-channel stage
  chains in file-sequence order + attributed terminal stage hits.
- The launch-9 terminal (`code=1 'no observed mapping interval intersects the
  module image extent with valid geometry'`) gets a dedicated verdict branch
  (`FAILED_S6_CONVERSION`) whose NEXT text states the launch-10 attribution
  rule: with `aligned_entry` disabled, the line belongs to supplier_composition.
- The lease throw (`Owned read range includes unregistered bytes`, code=2) is
  treated as DORMANT in r13 (launch-8's r10 threw it, pid 234); its verdict
  (`FAILED_S8_CONTEXT_LEASE`) says the rebuild did not carry the r14 gate and
  to verify the readback sha first. The r14 loud pre-lease gate text
  (`lease coverage gap: requested [… +0x…) outside registered readable
  spans`) gets `FAILED_S6_LEASEGATE` (MRS_ERR_CONVERSION by design).
- Config-drift class added: growth of the disabled entry log / xwp jsonl, or
  DIAG-line plugin output, warn loudly and name the ini pin `1216aab6…`,
  resolving the launch-9 two-plugins-one-log ambiguity at the config level.

### 3. Timing extraction (rides along; no separate diagnostic launch)
- r4 `[t=<ms>ms pid=<pid>]` failure-line prefixes → per-(writer,pid) sequences.
- PATH_TEST `… at <ns> monotonic-ns` stamps (success + OPEN_FAILED variants,
  verified against path_test_entry.c) → same-writer deltas in ns.
- supplier-diag jsonl `t_ms` events (if the diagnostic-sink landed) → the only
  true per-stage elapsed table; deltas within the single writer.
- runtime-status session stamp → single point; state lines explicitly noted as
  sequence-only.
- Every summary carries the hard guard: per-writer sequences only — NEVER
  subtract across writers. The two-writer fixture proves no cross-writer or
  cross-pid arithmetic exists in the output.

### 4. Launch-10 staging runbook (`--plan`)
Preconditions (stray-root disposition, gate v2) → single-file PRX upload with
fresh-RETR readback byte-verify (PT_LOAD content law, filenames unchanged) →
ini NOT republished (pre-image gate `1216aab6…`, STOP on drift) → baselines
re-fetch BEFORE launch with a pre-launch `NOT_STARTED`/zero-warning
classification as the drift-free proof → post-launch fetch order UE4SS.log →
runtime-status → supplier-diag jsonl (optional, sink-dependent) → xwp jsonl
(drift) → entry log (history) → classify with `--baseline-dir`.

### 5. Fixture suite — 84 checks, 16 cases, 0 failures
not_started · l9_terminal · l10_success · timing_two_writers · baseline_drift
· content_drift · shape_a_regress · lease_gate_r14 · lease_throw_dormant ·
seeds_refuted · gmalloc_gate · crash_shaped · truncation · boundary ·
entry_growth · diag_drift. SHAPE_A is re-labeled for launch-10: the armed
table should ship, so that verdict now reads "the armed SeedTable did NOT ship
in this fself" — a staging regression, never an expected terminal.

## Bugs found and fixed during the fixture pass (honesty)

- `command shasum >/dev/null 2>&1` inside `sha256_stdin` consumed the piped
  stdin (shasum with no args hashes stdin), so every head-window compare saw
  the empty-input hash `e3b0c442…` on BOTH sides and the guard could never
  fire. Replaced the probe with `command -v`. Caught by tracing; this bug
  class is exactly the launch-9 "411 B error page looked like data" failure
  mode, so the guard now has its own test.
- `SDIAGCHAIN` printed with one tab (chain in `$2`) while the reader expected
  it in `$3` — supplier-diag chain always displayed empty. Fixed + covered.
- `status_summary` passed the baseline session to awk as FILE arguments
  instead of `-v` variables.
- Harness: `run_case` referenced unset `$2` under `set -u` for no-extra-arg
  cases (three cases ran with `rc=1` and no output); `fresh_status` shifted
  off `$1` before using it as the output directory, so fresh-status fixtures
  silently stayed stale.
- content_drift was first designed as "append a foreign line" — appending can
  never trigger a head-window guard; redesigned as same-size foreign content.

## Limits

- See CLAIM.json `limits`. Key ones: baselines must be RE-FETCHED and re-pinned
  on launch day (this worker verified local launch10-prep bytes, not console
  bytes); the stage table covers the r13/r14 marker vocabulary and will need a
  bounded revision if the launch-10 rebuild adds new tags (unknowns surface as
  raw lines, never dropped); the armed-path `[Seeds]` success wording remains
  not verbatim-pinned; config-drift detection is negative evidence and the ini
  pre-image gate remains the positive check; classification is evidence
  interpretation, not acceptance.
