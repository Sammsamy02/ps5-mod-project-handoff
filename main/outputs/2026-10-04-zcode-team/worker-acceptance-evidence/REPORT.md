# REPORT — worker-acceptance-evidence (2026-10-04)

Deliverable: `EVIDENCE-SPEC.md` in this directory — the exact evidence-artifact
contract for all five acceptance-ladder rungs. `CLAIM.json` was written first;
nothing outside `outputs/2026-10-04-zcode-team/worker-acceptance-evidence/` was
written; no nested subagents; no network/console; nothing executed. Tool-call
budget: ~18 of ≤50.

## What the spec fixes, per rung

- **RUNG 1 (genuine game-thread callback):** PASS requires accepted-tick
  counter movement across snapshots (`ticks_accepted` ≥ 2), a non-zero
  `game_thread_id_key` published by the first genuine `UGameEngine::Tick`,
  `IsInGameThreadRaw()==true` in the record, ≥1 wrong-thread/identity-unknown
  refusal (guard evidence), and clean `RuntimeOwner::stop()` to a terminal C3
  line. FAIL: no tick movement, identity key stays 0, C3 stuck pre-terminal,
  or any worker-thread impostor identity (automatic FAIL). Explicitly
  downgraded to non-evidence: `engine-ready` alone (an engine-INIT callback),
  the `native_program_construction_console` constructor success, and all host
  fixtures.
- **RUNG 2 (GetNumElements):** the terminal JSONL record fields are pinned
  from worker-query-harness REPORT §5: `status:"completed"`,
  `count_source:"engine_uobject_array"`, `counts.recorded:2`, both counts
  strictly `> 10000`, `Equal`/`NonDecreasing`, `identity_checks:2`, zero
  gate-closed refusals on the accepted path. Three discriminated failures:
  `inconclusive_below_threshold` (honest, ≤10000), counts `0` (layout facts
  refuted), `RefusedLayoutGateClosed` (currently the CORRECT outcome for every
  dispatch — SEAM S2 GAP means `PublishStagedNativeLayoutMaps` has no
  production caller yet).
- **RUNG 3 (harmless reflected access):** PASS is the mod-side discriminator
  lines — `Scan failed: Item database not ready. Load a save first.` (query
  ran, DB absent) or the StaticFindObject-context phase-2-POSITIVE lines — and
  explicitly NOT the `attempt to call a nil value (global 'FindFirstOf')`
  tail. FC-1, if claimed, must come from in-game `ig_run_fc1_probe` C2 records
  (FC1-A exact 64-byte prologue at 0x63d590, FC1-B all 9 windows, FC1-C
  fail-closed uniqueness, FC1-D gated strings, mprotect-first); the offline
  materialization and synthetic-byte host tests are named as NON-evidence.
- **RUNG 4 (unchanged Item Spawner activation):** the byte-exact load line
  `[TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the
  menu.` exactly once, grepped in the ACTIVE retained Lua log (path-agnostic
  oracle; current lineage `/data/townfall_runtime_aligned_entry.log`; which
  path print() lands in is a runtime-build property, stated as an open
  composition fact). Require-chain resolution proven by the phase-2 no-save
  `Scan failed:` line + designed silence. Silent-death and nil-keybind
  discriminations carried over; sha256 `f586043a…658` unchanged is a
  precondition — any edit voids the rung.
- **RUNG 5 (visible spawn through real input):** the full 9-hop chain from
  `scePadReadState` rising edge through `ExecuteInGameThread` and the
  EngineTick gate to a new actor absent from the prior frame; PASS =
  VISIBILITY-TEST §1 items 1–6 from one continuous capture including negative
  controls (menu-closed no-spawn, pad-disconnect no-phantom, pre-publication
  refusal); log-line-only, menu-only, synthetic/keyboard-injected, and
  single-lucky-frame outcomes are non-evidence. Dependencies (A9, C5, C1/C2
  callables, D1–D5) and the pad provider's `hardware_runtime_validated:false`
  status are stated.

## Key findings while distilling

1. The evidence surface is already well-defined across siblings; the spec's
   value is binding each rung to its exact channel, byte-strings, and
   discriminators — and separating honest-negative outcomes
   (Inconclusive, gate-closed refusal, pre-publication refusal) from FAILs.
2. Two structural gaps block the ladder TODAY and are documented as such, not
   hidden: the S2 GAP (rung 2 cannot complete until the layout publish caller
   is wired) and the pad provider's missing hardware validation (rung 5's
   first console attempt is its first real-pad read).
3. Rung ordering is load-bearing in the artifacts themselves: the phase-2
   no-save checkpoint simultaneously evidences rungs below 3 (delivery, query
   path) scene-free — useful as an early, cheap ladder checkpoint.

## Honesty bounds

No rung is claimed PASSED. No console, host test, or compile was run. Every
quoted string/field was read from the cited read-only artifacts (list in
CLAIM.json `deliverables`). The spec defines what WOULD count; it never
certifies that it happened.
