# REPORT — worker-post-launch-tools

Worker: worker-post-launch-tools · 2026-10-04 · host-only tooling. No console
contact was made by this worker (the FTP path is exercised only via local
fixtures; live mode is for the controller after the user launches). No nested
subagents. All writes stayed inside this worker directory.

## Delivered

| file | purpose |
|---|---|
| `CLAIM.json` | claims + evidence + honest limits (written first) |
| `post_launch_check.py` | the single stdlib-only post-launch classifier |
| `host-test/run_host_tests.py` | fixture-driven host test suite (20 cases, 73 checks) |
| `host-test/fixtures/` | per-case fixture files (UE4SS.log / runtime-status.txt / townfall_runtime_aligned_entry.log) |
| `receipts/host-test-pass.txt` | 20/20 cases, 0 failed, with environment + sha provenance |

## How the controller uses it (after each launch, ~1 min post-launch)

    python3 outputs/2026-10-04-zcode-team/worker-post-launch-tools/post_launch_check.py

That pulls the three evidence files with one fresh `curl` connection each
(bounded: `--connect-timeout 10 --max-time 60 --max-filesize 32M`), prints a
human-readable evidence summary and ends with exactly one `VERDICT:` line plus
`NEXT:` guidance. `--json` gives the same result machine-readable;
`--dry-run` prints the three exact curl commands without fetching;
`--baseline-file FILE` overrides the embedded 3-line baseline;
`--from-dir DIR` reads local fixture files instead of FTP (host-test mode).

Exit codes: 0 = verdict produced, 4 = FETCH_FAILED (all three fetches
unreachable — RETRY guidance printed, never a crash).

## What it classifies (precedence order)

1. `FETCH_FAILED` — all fetches failed → wait ~60 s, re-run; if still down,
   check FTP service/network before touching staging.
2. `FAILED_<stage>` — `modbridge module_start failed: <stage> code=<n>
   detail=<...>` lines in UE4SS.log. Guidance per mfa-audit signatures:
   code=-2 → /data timing window (verify loadlist staged, relaunch once before
   re-staging); -6/-8 → staged-vs-final-package divergence, detail path names
   the file (first path-sorted candidate: ue4ss/MemberVariableLayout.ini);
   -3/-4/-5/-12 → loadlist content/title/limit/duplicate → regenerate from
   final-package; `runtime_start_failed` code=1/2/3/4 → bridge
   CONVERSION/CONTEXT/START/INTERNAL; `diag_channel_open_failed` code=-22 →
   channel (postmortem note: this line only appears with the 3-line patch
   resident — the delivered entry is SILENT on -22). Multiple failure lines:
   verdict from the LAST line, count reported (append-only log, playbook O5).
3. `FAILED_STATUS` — fresh status session says `failed` with no matching
   UE4SS.log line; shape check separates F6/F7 silent traps (activated then
   failed) from a single terminal row whose log append also failed; code
   guidance reused (mfa codes via stage hint, bridge 1-4).
4. `MODS_STARTED` / `ENGINE_READY` / `ACTIVATED` — fresh status ladder
   (session-ms > 8866785 = postdates the PATH_TEST launch, same
   CLOCK_MONOTONIC base). MODS_STARTED checks new UE4SS.log lines for
   `[TownfallItemSpawner]` prints (rung-4 gate). ENGINE_READY/ACTIVATED →
   re-run in ~30-60 s; frozen = F7/F8 bounded-wait hang check.
5. `STOPPED` — fresh or last state stopped → F10, check observer
   `runtime_stop_result`.
6. `UNKNOWN_NEW_LINES` — non-baseline, non-probe, non-failure lines with no
   status evidence → printed raw; banner-shaped lines suggest mid-launch
   (re-run), else unexpected writer.
7. `NOT_STARTED` — nothing beyond baseline + stale probe repeats → wait ~60 s
   (records appear ~1 min after launch); on repeat emptiness treat as F0
   (PRX never ran): verify staging + ploader; observer
   `module_start_entered` count is the launch counter.

Always reported: per-fetch OK/FAIL, embedded-baseline sha self-check, missing
baseline lines (log replaced/truncated warning), stale probe repeat count
(`PATH_TEST:`/`ACTIVATION_TEST:`/`DIAG: module_start reached` prefixes never
count as new), status session pid/ms/freshness, status ordering anomalies,
parsed failure lines with per-line signature guidance, up to 20 raw new lines,
and aligned-entry observer tag counts (module_start_entered,
mfa_activated_low/high, runtime_start_ok/skipped/failed, runtime_stop_*).

## Embedded baseline (verbatim, sha256)

    L1 02c1950e…b27a73  PATH_TEST: /data/plugins/UE4SS.log writable at 8866785633337 monotonic-ns
    L2 a2a609ca…14d54   ACTIVATION_TEST: loadlist.txt readable, first 16 bytes: 6d6f64666f7267652d6c6f61646c6973
    L3 ee8c12fa…56e71   DIAG: module_start reached at 1791155755.879 pid=298 build=sdg2-6f404cee6b94 variant=imp
    blob 8cf92183…fb58f (LF-terminated)

Honesty note: L1/L3 are verbatim ledger records
(transport-ledger.composition5-mirror.json `zcode_path_test_target_run` /
`zcode_sdg2_target_run`). L2 is reconstructed from the path-test module's
exact emit format (path_test_entry.c:115-146, lowercase hex) plus the ledger's
recorded 16 bytes ("modforge-loadlis"); it is not a verbatim console readback.
Classification never depends on L2's exact bytes — prefix rules absorb drift.

## Host-test results (receipts/host-test-pass.txt)

20/20 cases, 73 checks, 0 failed (Python 3.9.6, Darwin arm64):
exact baseline, stale probe repeats, mfa -2/-6/-8 single + multiple failure
lines, -22 channel, bridge CONTEXT, status-only failed (mfa + bridge codes),
ACTIVATED, ENGINE_READY, MODS_STARTED with 4 new lines + observer extraction,
UNKNOWN_NEW_LINES, total fetch failure (exit 4, human-mode render regression
checked), partial fetch (status only), stale session, fresh-progress vs old
failure-line conflict, fresh stopped, `--baseline-file` override, empty
fetches, JSON/dry-run + sha-vs-CLAIM verification.

The suite caught two real bugs during development, both fixed and now
regression-checked: a FETCH_FAILED early-return that skipped initializing the
baseline report block (KeyError in human mode), and a fresh-`stopped` session
that would have been mis-ordered against historical failure lines.

## Limits (do not over-claim)

- No console contact, no live FTP fetch, no launch performed by this worker;
  live curl behavior follows the ledger-proven plain `curl ftp://…` pattern.
- Stateless between runs: UE4SS.log and the aligned-entry log are append-only,
  so failure-line and observer counts are cumulative across launches (O5);
  per-launch attribution relies on runtime-status.txt O_TRUNC sessions and is
  labeled fresh/stale on the PATH_TEST monotonic base.
- Guidance is evidence interpretation for the controller, not acceptance:
  rung advancement stays governed by FIRST-RUN-PLAYBOOK §5; this tool never
  advances a rung.
