# RUN-N READING PROCEDURE — durable per-epoch evidence read (one page)

Worker: worker-durable-events · 2026-10-05 · host-side, LOCAL analysis only.
Implements the user directive: **runtime-status.txt is LATEST-STATE-ONLY (last-writer-wins by
design). The authoritative activation/terminal evidence is the durable per-build/run/PID
events — the sdiag jsonl (accumulates across epochs) + the UE4SS mirror lines. Correlate by
build/run/pid; NEVER race the status file.** Supersedes the old "fetch status before the
terminal write" caveat.

Paths below assume repo root `/WORKSPACE/.codex/worktrees/translation-core/what`.
`EV=outputs/2026-10-04-zcode-team/worker-durable-events/tools/epoch_view.py`

## Step 0 — before launch: pin the baseline cut
The accumulated sdiag of the LAST completed run state is the append-only baseline for this
run. For the next (RUN-5) launch it already exists:
`outputs/2026-10-04-zcode-controller/launch10-prep/sdiag-baseline-run5.jsonl`
(318910 B · sha256 40504e28… · 4 epochs: 317/341/355/370). Record `N5=$(wc -c < that file)`.
Also verify the other launch10-prep/run5 baselines are unchanged before staging (ue4ss
8823 B 324145c9…, status 117 B session 370, entry 4b33b985…, xwp 20cff601…).

## Step 1 — launch, wait ~60 s, then fetch once each (controller; fresh curl per file)
Exactly LAUNCH-DAY-10 §3 order, into a new run dir `outputs/2026-10-04-zcode-controller/runN/`:
```
UE4SS.log -> runtime-status.txt -> modbridge-supplier-diag.jsonl (absence is normal)
-> modbridge-exec-write-probe.jsonl (drift guard) -> townfall_runtime_aligned_entry.log (history guard)
```
Do NOT re-fetch to "catch the status before it truncates" — there is nothing to race;
the status is latest-state-only and the durable evidence is in the other two files.

## Step 2 — read the DURABLE per-epoch evidence FIRST (this is the authoritative read)
```
python3 $EV runN/modbridge-supplier-diag.jsonl \
    --ue4ss runN/UE4SS.log --status runN/runtime-status.txt \
    --baseline-bytes $N5 [--build-id <edition from the staging receipt>] [--json > runN/epochs.json]
```
Reading rules:
- ONE row per epoch = one game pid. New epochs are the rows marked `[NEW]` (records at or
  after the baseline cut). History stays visible — the accumulated file IS the evidence.
- Correlate the UE4SS refusal line to the sdiag epoch by pid (and t_ms; agreement is exact,
  ≤1 ms skew observed). Refusal text + code come from the pid-prefixed mirror line; codes
  alone come from the sdiag `runtime_start_failed` / `mfa_activate_failed` values (int32).
- The status file is applied ONLY to the epoch whose pid equals its `session` row. If its
  session pid has no epoch yet, the tool says so — never apply it to older epochs.
- Verdict vocabulary (oracle): E2 START / pad phase-6 / B7 MFA / engine-ready / mods-started,
  plus the named classes D6/D7/C12/C13/E1 and STARTED. "MINIMAL-ACCEPT-grade conversion"
  = the D1-D5 quartet (4 extent events + anchor_count>0 + intervals_found>0 + cfg events)
  with no D6 — the rung declaration itself stays the controller's (ORACLE §2).
- New console boot? t_ms goes backwards between epochs; the tool flags the BOOT-GAP per
  epoch and keeps file (append) order. Never subtract t_ms across epochs.

## Step 3 — the one-pager + drift guard (evidence.sh, unmodified; owner: evidence workers)
```
sh outputs/2026-10-04-zcode-team/worker-launch10-evidence-upgrade/evidence.sh \
   --from-dir runN --baseline-dir outputs/2026-10-04-zcode-controller/launch10-prep \
   --sdiag runN/modbridge-supplier-diag.jsonl --out runN/summary.txt --json-tail
```
Any CONFIG-DRIFT warning (entry/xwp growth, DIAG lines) -> STOP; verify ini 1216aab6…
before attributing anything. Use the stage chain / timing table as the cross-check for
step 2 — the two tools read the same sinks with different scopes (window vs per-epoch).

## Step 4 — verdict + close-out
- Declare per ORACLE §2 using the per-epoch rows (2-4 verbatim lines). The predicted pair
  to check on every conversion-passing epoch: anchor_count=7, intervals_found=1 next to
  lease_spans=6 (reg=6), extent [0x400000,0xc0602c8), cfg 1/1; intervals=0 is the launch-9
  death; intervals=6 is a page-gap investigation trigger.
- Save `runN/epochs.json` (the `--json` document) next to the raw fetches; it is the
  per-build/run/PID durable record.
- Pin the NEXT baseline cut: `runN/modbridge-supplier-diag.jsonl` byte size + sha becomes
  RUN-(N+1)'s `--baseline-bytes` (save as `sdiag-baseline-run(N+1).jsonl` alongside the
  ue4ss/status baselines). Never replay a failed request; a new run gets a new pid/epoch.
