# REPORT — worker-integration-checklist (2026-10-04)

Status: DELIVERED. `INTEGRATION-CHECKLIST.md` in this directory is the
definitive ordered execution guide from "layout facts promoted" (STEP 0
entry gate) to "harmless-query milestone passed" (STEP 14 acceptance).

## What was built

15 numbered steps (0–14), each with Do / Needs / Evidence / Failure / Detail:
0 promotion-gate verification, 1 axes + identity resolution
(non_shipping/stats from the live scan or explicit user confirmation —
recorded with provenance), 2 real verdict records in worker-5 vocabulary,
3 staged-pair emission (exact assembler command), 4 pair verification
(parser_verify + negative control + 18+14 regression suites), 5
composition-3 spike-tree update (bundle 01/02 — already consumed per
checkpoint `zcode_supplier_composition_3`; re-apply order documented for
drift), 6 recomposition with the query-harness extra unit + S2 arm wiring +
ack module_start supplier (the one composition work NOT already done;
undefined-delta gate: only the carried `[sceKernelGetModuleInfo]` may be
added), 7 PRX packaging (`--verify-only` then `--link`, no GAPS.json),
8 staging (ploader.ini mode 2, ftpsrv decryption oracle), 9 launch ritual,
10 in-process axes→stage→publish chain (`SetResolvedNativeLayoutAxes` →
`load_unreal_offsets_from_file` → `NativeProvidedVirtualVC::set_virtual_offsets`
→ `PublishStagedNativeLayoutMaps`), 11 harness arm (one-liner placement
guarantees + disarm-on-throw), 12 tick delivery (bindings + identity +
rung-1 witness), 13 GetNumElements query (`TrySnapshot` /
`SnapshotDispatchCounters` / `FormatRecordJsonLine` off-thread), 14
acceptance (the full six-condition rung-1→2 bar + ACCEPTED.md ledger).

Plus a failure-escalation map, an "already-done — do NOT redo" section
(composition-3 object `e9679aa5…86350`, vendored emission kit, harness
zero-delta probe, frozen igf4 probe), and an explicit out-of-scope section.

## Sources consumed (read-only)

Checkpoint entries (from
`worker-composition-3/evidence/transport-ledger.composition3-mirror.json`,
the only on-disk ledger snapshot): `zcode_v3_MAPPING_BREAKTHROUGH`,
`zcode_supplier_composition_3`, `zcode_worker_query_harness_integration`,
`zcode_worker_ack_integration`, `zcode_worker_staged_pair_emission`.
`zcode_anchor_validation_RESULTS` is not present in that (older) mirror;
its content was taken from the primary source,
`worker-anchor-validation/REPORT.md` + `STAGE-PLAN-ANCHOR.md` (igf4 probe
design, per-record expectations, launch ritual), plus
`worker-staged-pair-emission/input/anchor-assumptions.json` which encodes
the probe's acceptance contract. Worker outputs read in full:
`worker-staged-pair-emission/EMISSION-RUNBOOK.md`,
`worker-query-harness-integration/S2-ARM.md` (+ its REPORT.md §4 acceptance
bar), `worker-ack-integration-deep/controller-patch/02-composition-wiring/WIRING-SPEC.md`
(+ bundle layout), `worker-layout-minimum/validation/RUNBOOK.md`
(activation + acceptance-ledger steps), `worker-prx-packaging/STAGING-CHECKLIST.md`,
`worker-e2e-console-staging/STEP-BY-STEP.md` (referenced, marked out of
scope).

## Honesty notes

- Everything here is a synthesis of proposal-only worker specs and ledger
  facts; nothing was executed on a console or composed object by this
  worker. The chain assumes the igf4 anchor run lands as designed; STEP 0
  exists precisely so the checklist cannot be started from an un-promoted
  state.
- WIRING-SPEC §S5.3 states gate expectation `added == []` for the ack
  recomposition; the MEASURED composition-3 result is
  `[sceKernelGetModuleInfo]` (identical to composition-2). STEP 6 states
  the measured reality as the expectation and treats anything beyond it as
  stop-and-reconcile.
- The storage field in the arm facts must come from the validated profile
  (worker-2); `ChunkedArray` in S2-ARM is that profile's 5.6 example — the
  checklist says "substitute the profile's actual claim".
- The synthetic staged pair is repeatedly marked never-to-stage; only
  `emission-live/` from real verdicts + real identity is stageable.

## Scope compliance

Writes confined to
`outputs/2026-10-04-zcode-team/worker-integration-checklist/`
(CLAIM.json first, this report last). No nested subagents, no network, no
console dispatch, no existing file elsewhere modified. ~28 tool calls.
