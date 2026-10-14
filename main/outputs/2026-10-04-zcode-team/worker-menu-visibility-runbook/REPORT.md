# REPORT — worker-menu-visibility-runbook

Worker: worker-menu-visibility-runbook · 2026-10-04 · Integration doc only.
No console contact, no nested subagents, no audit duplication. All reads were of
existing worker deliverables inside the worktree; all writes stayed in this directory.

## Deliverables (in write order)

1. `CLAIM.json` — claims, merged sources, bounds, honesty limits (first).
2. `MASTER-VERIFICATION-RUNBOOK.md` — the single merged procedure (main output).
3. `REPORT.md` — this file (last).

## What was merged and how

The task listed 10 upstream deliverables. All were read in full; the runbook folds
them into one ordered procedure (CP0→CP7) from "user launches Townfall" to "item
spawned", citing each source instead of restating it:

- **Ordering spine** from worker-first-run-playbook (26-row trace condensed to CP0-CP4;
  F0-F10 signatures indexed in §6; freshness oracle O1-O5 condensed in §1).
- **Startup depth** from worker-lua-bootstrap-audit (main.lua runs in module_start;
  mods-started gated on engine-ready; F9 script-layer non-fatality) → CP1/CP4.
- **Tick stage** from worker-tick-address-validation (prediction 0x37ac8c0; the 9-outcome
  LAUNCH2 table merged into CP2's failure list) and worker-engine-tick-fallback (KEEP
  EngineTick; the fatal pre-mods-started signature vs the unreachable F7 gate) → CP2/§4.3a.
- **OPTIONS protocol** (task 2) built from worker-pad-chord-audit (edge semantics, chord
  match, provider proof chain) + worker-lua-mod-smoke (exact mod log shapes, phases 2/2b/3)
  + worker-real-input-pad PS5Pad.ini mappings + actual main.lua strings
  (`<N> spawnable pickups found. Pages=<P>`, `Menu created.`, `UI creation failed:`,
  `WidgetBlueprintLibrary not found.`, `Item database not ready. Load a save first.`).
- **Spawn path** (task 3) from the smoke trace (LineTraceSingle → BeginDeferredActor
  SpawnFromClass → property writes → `Spawned … x1 ID=…`) cross-checked against main.lua's
  four `stage` strings; real-spawn proof = log line with post-finish readback ID + the
  user-visible pickup actor (§5.3).
- **Console tooling** (task 1's command requirement) from worker-post-launch-tools
  (step A: classifier invocation, verdicts, exit codes, embedded 3-line baseline) and
  worker-rung-evidence-reader (step B: rung invocations with current RUNG1/RUNG2 strict
  baselines, five pinned paths, `--discover` fallback).
- **Rung-3** from worker-rung3-reflection-prep → CP7 pointer + its F-signatures in the
  triage index.

## Key integration findings

1. **worker-tick-callback-audit is empty.** Same finding two prior workers already
   recorded. Its assigned content (detour mechanism + rung-1 evidence) was reconstructed
   from worker-tick-address-validation (DetourInstance spans) and worker-query-harness
   REPORT §5, and is declared as gap G1 rather than silently papered over.
2. **Pad-provider proof is structural.** Reaching `activated` + the mod load banner jointly
   prove the provider opened and PS5Pad.ini parsed: any provider/ini failure throws during
   construction (before `activated`), and a missing input source makes `RegisterKeyBind`
   throw (`Input registration unavailable`), which would suppress the banner. This became
   precondition P2 in the OPTIONS protocol.
3. **The EngineTick F7 gate is documented as unreachable post-mods-started**; the runbook
   lists its exact throw text as an anomaly indicator (with the ProcessEvent variant
   proving an ini restage), while the REAL EngineTick failure is CP2's fatal
   `Native game-thread initializer callback registration failed` + `failed` status.
4. **Rung-1/2 will honestly NOT PASS this launch** unless a driver add-on PRX writes the
   `native_query_harness` JSONL (no target-side caller exists). The runbook states this as
   gap G2 and sets the pending-test T7 decision instead of burying it.

## Honest limits

- No console run has ever produced a line beyond the 3 baseline UE4SS.log lines; every
  pass criterion in the runbook is a prediction (static trace + host test).
- 0x37ac8c0 is a structural prediction only (gap G3); the OPTIONS→menu hop (G4) and the
  spawn path (G5) have zero engine-executed evidence.
- "Visible menu" and "spawned actor in the world" are user-eye facts; logs cannot fully
  certify them (C4 video capture out of scope).
- Pending tests T1-T8 in the runbook are explicit and none may be closed from host evidence.

## Tool budget

~14 tool calls (well under the 30-60 bound): directory listing, 9 document reads,
3 targeted greps/seds (tool CLI options, main.lua log strings, PS5Pad.ini), CLAIM +
runbook + report writes.
