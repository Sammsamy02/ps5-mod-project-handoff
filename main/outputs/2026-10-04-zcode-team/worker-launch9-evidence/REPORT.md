# REPORT — worker-launch9-evidence

2026-10-04 (host clock 2026-10-05 UTC during delivery) · host-side evidence
preparation only · NO console contact · no nested subagents · nothing executed
against the console; the fixture suite invokes `evidence.sh` in `--from-dir`
mode only.

## What was delivered

Under `outputs/2026-10-04-zcode-team/worker-launch9-evidence/`:

1. **CLAIM.json** (written first) — scope, per-claim provenance, limits.
2. **evidence.sh** — the post-launch evidence-collection script the controller
   runs after the user confirms "in game". POSIX sh + awk, stdlib-grade
   dependencies only (`sh`, `awk`, `curl`, `wc`, `grep`, `sed`, `shasum`).
   - Fetches the three evidence files with ONE fresh curl connection each,
     bounded `--connect-timeout 10` / `--max-time` / `--max-filesize`:
     `ftp://ps5-host-not-configured.invalid:2121/data/plugins/UE4SS.log`,
     `.../data/modbridge/roots/PPSA33286/runtime-status.txt`,
     `.../data/townfall_runtime_aligned_entry.log`.
   - Extracts new lines of the append-only UE4SS.log and aligned-entry log
     against the controller-captured pre-launch baseline sizes (UE4SS.log
     **2892 B**, aligned-entry log **2210306 B**, from the dispatch briefing;
     overridable via `--baseline-ue4ss-bytes` / `--baseline-aligned-bytes`).
     A line is NEW iff its start offset >= the baseline size (conservative
     round-up: no baseline content ever leaks into the new-line set; tested).
     A fetched file SHORTER than its baseline is flagged, never classified.
   - Classifies every new line against the launch-9 signatures table
     ((a) pass markers `bridge_vmentry_entries` / `anchor_*` / `lmf_*` /
     `bridge_cfg_gamedir=` / `bridge_cfg_workdir=` / `bridge_exepath_source=`;
     (b) the Shape-A terminal `[Seeds] Required-slot seed table incomplete ...
     missing [gmalloc]` marked EXPECTED, not a failure; (c) `[Seeds] REFUTED`
     variants + any other `[Seeds]` line reported verbatim; (d) engine-ready /
     mods-started / `[TownfallItemSpawner]` progress; (e) runtime-status
     grammar and ladder; (f) entry-r4 `[t=<ms>ms pid=<pid>]` freshness prefix;
     (g) bridge-r13 failure grammar, MRS_* tokens, `-22` diag-channel silence
     note, observer event lines) plus IGW exec-write markers
     {W_WORKS, W_FAILS_R_WORKS, BOTH_FAIL}, the GMalloc PostInitialize throw,
     and the LoadMap/GameEngine crash-tail vicinity.
   - Emits a ONE-PAGE summary: fetch sizes, signature counts, status session
     (pid / monotonic-ms / freshness / ladder / last state), notable verbatim
     lines, raw other-new-lines (capped), then exactly one VERDICT + NEXT.
   - Verdict precedence: FETCH_FAILED (exit 4) > SHAPE_A_SEEDS_GATE
     (EXPECTED-PREDICTED-STOP, exit 0, explicitly forbids bridge stage-1
     CONVERSION triage) > SEEDS_REFUTED > GMALLOC_GATE > FAILED_<stage>
     (stage verbatim; -22 silence guidance) > FAILED_STATUS (fresh failed
     session without a log line) > MODS_STARTED / ENGINE_READY / ACTIVATED
     (fresh sessions only; MODS_STARTED carries beyond-prediction skepticism
     per LAUNCH9-PREDICTION §5) > CRASH_SHAPED_SUSPECTED (fresh activated-only
     + LoadMap-vicinity tail + no failure line = igf6/FLAG-2 candidate) >
     STOPPED > UNKNOWN_NEW_LINES > NOT_STARTED.
   - Modes: live FTP (controller only) · `--from-dir DIR` (host fixtures) ·
     `--dry-run` (prints the exact curl commands) · `--json-tail` (machine-
     readable counts) · `--out FILE`.
3. **host-test/run_fixture_tests.sh + fixtures/** — generates synthetic
   fixtures at the exact baseline sizes (fixture bytes are marked synthetic;
   only sizes and line grammar matter) and asserts 28 checks across 7 cases.
4. **receipts/fixture-test-pass.txt** — captured passing run: **28 passed,
   0 failed** (evidence.sh sha256
   `0c585c1c28f7e622dae20c1a5b0d603d888b6c8a9da533580afa806033fe9735` at
   receipt time; fixture shas in the receipt).
5. **REPORT.md** (this file, last).

## Test cases covered (28 checks)

- shape_a: pass markers counted (4), Shape-A line counted (1) and labeled
  EXPECTED, observer `runtime_start_ok`, entry-r4 prefix counted, status
  failed session shown, no bridge-triage guidance, exit 0.
- mods_started: full ladder verdict with explicit beyond-prediction
  skepticism; ItemSpawner print counted.
- crash_shaped: CRASH_SHAPED_SUSPECTED on fresh activated-only + LoadMap tail,
  naming FLAG-2 / LAUNCH9-PREDICTION §4 #3.
- failed_diag: `FAILED_diag_channel_open_failed` (stage verbatim) with the
  channel-postmortem -22 SILENCE guidance (absence never excludes -22).
- not_started: files exactly at baseline sizes -> NOT_STARTED, no warnings.
- truncation: shorter-than-baseline UE4SS.log -> per-file WARN, still
  classifies, exit 0.
- boundary: an old line STRADDLING the 2892-byte cut never leaks past it; the
  first line starting at/after the cut does appear.

## Bugs found and fixed during the fixture pass (honesty)

- The harness's first aligned generator emitted 1,909,600 B and a mid-line
  cut glued appended lines into the baseline region (fixture bug; also masked
  the classifier's new-line set). Fixed to 17826 whole 124-B lines
  (2,210,424 B).
- That exposed a real classifier display gap: OTHER new lines were counted but
  never printed while the UNKNOWN/NOT_STARTED guidance told the controller to
  read them. Fixed: raw other-new-lines now appear (capped 10).
- Warnings concatenated on one line; now one WARN/NOTE line each.
- One test expectation was wrong (uppercased stage name); the classifier's
  verbatim-stage behavior is kept and the CLAIM updated.

## Limits (unchanged from CLAIM.json)

- Baseline sizes are controller-captured dispatch facts, not re-fetched bytes:
  offset-based diffing with a truncation guard, no content-pin claim.
- The `[Seeds]` armed-path SUCCESS wording is not verbatim-pinned anywhere I
  could read; unmatched `[Seeds]` lines are shown verbatim as seeds-other.
- Freshness threshold (session-ms > 8866785) proves post-path-test only, not
  launch-9 attribution by itself; per-launch truth remains the status O_TRUNC
  session plus the baseline offsets, valid for THIS launch window only —
  re-baseline via flags before any later launch.
- Classification is evidence interpretation, not acceptance: SHAPE_A_SEEDS_GATE
  is the PREDICTED honest outcome of composition-6 (LAUNCH9-PREDICTION §4 #2 /
  §5), not a pass of the composition; engine-ready/mods-started remain
  unproven-by-prediction; rung advancement stays governed by
  FIRST-RUN-PLAYBOOK §5.
- Workspace note: the dispatch named root
  `/WORKSPACE/.codex/worktrees/translation-core/ah`, which does not
  exist; the referenced sibling workers all live under
  `.../translation-core/what/outputs/2026-10-04-zcode-team/`, so delivery was
  made there as a sibling. Nothing outside this worker directory was touched.
