# REPORT — worker-l10b-window-prep

Date: 2026-10-05 · LOCAL ONLY; read-only analysis + authoring in this dir; no console
contact; the base runbook `outputs/2026-10-04-zcode-controller/LAUNCH-DAY-10.md` was
NOT edited (sha re-verified e4121621591dd3d1 before and after).

## What was done

Authored **`LAUNCH-DAY-10B.md`** (sha256-16 `bfb3b2486c082557`) — a DELTA runbook for
the NEXT user window, read on top of LAUNCH-DAY-10.md REV 2026-10-05b, honoring BOTH
controller re-scopes received mid-task:

1. **Combined candidate, full evidence ladder.** The candidate is the COMBINED one
   (pad fix + merged launch-11 luamod/enqueue + LoadMap r2 + marshal consumer +
   launch-10-final units) via the LAUNCH11-PLAN §1 chain; ploader.ini and
   UE4SS-settings.ini unchanged. The run is judged on the user's 5-stage ladder —
   runtime starts → Item Spawner loads → game-thread work runs → controller input
   ARRIVES → menu opens — with each stage mapped to exact verbatim evidence lines and
   the deciding sink (B0 table). The user instruction is split into TWO ASKS with an
   evidence gate: startup evidence is pulled first (epoch_view → status → evidence.sh);
   ASK 2 (the OPTIONS walkthrough) only when stages 1–3 are demonstrated in that run's
   evidence, plus stage 4 if the build ships an input route; the menu is never promised
   before those stages are shown. Base §2's ge.5 standing rule is subsumed by the
   stage-2 gate.
2. **Two pad outcomes kept distinct.** outcome-A = startup-continues
   (`pad_input_deferred`-class tag, no phase-6 refusal, `runtime_start_ok`) delivers
   stage 1 ONLY — input not proven; outcome-B = usable-input-alongside-game (the
   input-arrival evidence line + which SDK route) delivers stage 4. Deferral alone
   never delivers B; with an A3-recorded waiver (no input route at all) the run closes
   at stage 3 and no menu ask is made.
3. **Timing integration (second re-scope).** B6 now carries the SOURCE-EXPLAINED gap
   baseline from worker-ploader-70s (delivered mid-task): the ~71s inter-plugin gap is
   path_test re-paying the per-plugin readiness wait — `wait_stable`'s 500-tick × 50 ms
   budget with ~264 kernel calls/tick ≈ 64.6 s wall; NOT a failure path/timeout (H5
   refuted at source); C_comp 802 ms confirmed 4/4 refusal boots. The runbook records
   the observed gap as a SINGLE number + C_comp per the existing stamps; the internal
   split (ticks vs kernel-call time) stays inference until a ploader steplog exists and
   is never decomposed inline; PROPOSAL.md §4's steplog acceptance criteria are
   referenced as PROPOSAL-ONLY (user decision; never assumed for this run — no ploader
   rebuild, no ini change). Full-startup C_comp (entered→`module_start_return` on the
   SUCCESS path) remains the run's headline new measurement. OPTIONS-gate rules
   unchanged.

## Key integration decisions

- **epoch_view.py (worker-durable-events) DELIVERED — integrated, not just referenced.**
  Its invocation is in B3 (with `--baseline-bytes` from a NEW pre-launch sdiag
  pre-image pin and `--build-id` from the combined EDITION); its verdict vocabulary and
  guards are in B4. PROCEDURE.md was not on disk, so the correlation rule was
  integrated from the tool's own directive header (per-pid durable events; status =
  latest-state-only corroboration for the session pid alone; no truncation-chasing —
  the base runbook's "fetch status before terminal write" caveat is superseded).
  Honest note recorded: epoch_view v1.0.0 does not map `pad_*` tags into its coarse
  chain — defer/arrival tags are read from `--json event_tag_counts`/raw records.
- **The walk-guard world question is left to the evidence.** Current pins DIVERGE:
  the launch-10-final asmhelper-guard (fold-0: W1/W2 on every walk read) vs
  LAUNCH11-PLAN §5.6 (the r3-armed chain links the shared prebuilt asmhelper, no
  walkguard step). The runbook routes both: W1+`guard_off_no_rx_verdict` ⇒ boot aborts
  before mods/`[R3]` lines (stages 2–5 unreachable; oracle GOOD-class at stage 1;
  recorded as a composition-identity fact); no W1 + walk address lines ⇒ walks
  permitted (the l11-plan expectation) and the ladder proceeds. A mid-walk death with
  no W1 = the unguarded unit shipped = FAIL/REGRESSION (CRASH_SHAPED_SUSPECTED class).
- **Pre-launch adds (B1):** the LAUNCH11-PLAN A0–A3/C/D/E gates (A3 pad-defer admission
  BLOCKING, waiver must be recorded), stage-header BOTH pins carried into the new
  lineage, and the NEW diag-sink instrument check: success-path tag presence pinned
  host-side before launch (`module_start_entered`/`runtime_start_ok`/
  `module_start_return` + the pad defer tag from pad-defer's diff), so C_comp cannot
  turn out unmeasurable post-hoc.
- **B5 IF-THEN:** 19 rows, one per named new line (pad defer folded / not folded /
  arrival; the four `[R3]` classes incl. the UNREACHABLE `marshal not armed` anomaly;
  W1/W2 + the five W3–W7 REGRESSION reasons; the igf6 death; seed-table REGRESSION;
  REFUTED-seeds-as-pool-validator-evidence; the four S2.3/S3 seed-string classes; F30;
  MODS_STARTED). All base §5 rows remain in force.

## In-flight worker state at writing (honest record)

- `worker-pad-defer/`: EMPTY (verified 12:16 and 12:26 local). Behavior change taken
  from dispatch scope only: unavailable input never aborts; `pad_input_deferred` tag
  class. The exact tag string, the input-arrival line, and the SDK-route vocabulary
  are CLASS-level here and must be pinned from that worker's delivery before the run.
- `worker-durable-events/`: tools/epoch_view.py delivered; PROCEDURE.md not delivered.
- `worker-ploader-70s/`: GAP-ANALYSIS.md + PROPOSAL.md + PATCHES/05-wallclock-deadline.diff
  delivered; integrated per the coordinator's second message.

## Non-claims

Prepared runbook only — nothing has run on the console; every expected line is a
source-pinned prediction, not an outcome promise. No verdict is pre-declared for the
run; no >8 quality, completion, or in-game acceptance claimed; no file outside this
worker dir was written; LAUNCH-DAY-10.md untouched; no replays, no staging, no
provider calls.
