# REPORT — worker-l2-adapter (2026-10-05)

LOCAL ONLY. No console contact, no target execution, no canonical edits; the only writes are
the four files in this directory. Everything else was read-only.

## What was delivered

1. **`ADAPTER-CONTRACT.json`** — the machine-readable L2 ENGINE-ADAPTER contract (validated
   parseable JSON): layer model (L1/L2/L3/L4), status vocabulary, provenance legend, the full
   input surface (activation IN-ACT-1..5, seed slots IN-SEED-0/2/3/4/6/10 with validation
   classes and current Townfall-PS5 values, settings keys IN-SET-1..10, pad input IN-PAD-1..3,
   capture/lease/spanpub policy IN-POL-1..5), the full output surface (14-element Lua API
   load-time surface + degraded/gapped items; 17 internal-API elements across tick pump,
   ExecuteInGameThread enqueue/drain, mod loading chain, diagnostics; 7 lifecycle stages with
   the runtime-status state channel), the 11-group named-refusal failure taxonomy, the
   second-game reuse analysis (UE-generic / game-specific / engine-version-specific /
   platform-specific + porting sequence), a completeness dashboard summary, and honesty notes.
2. **`ADAPTER-CONTRACT.md`** — the human-readable dashboard: the contract at a glance (ASCII
   boundary diagram), per-element tables with statuses and provenance, the lifecycle table, the
   refusal taxonomy summary, the completeness dashboard, and the reuse analysis with the
   enumerated engine-version conditionals.
3. **`CLAIM.json`** (written first) and this **`REPORT.md`** (written last).

## Method

The contract was extracted from the composition's actual seams, not invented: every element cites
either an existing worker receipt (worker-mod-load-readiness, worker-executedrain-trace,
worker-seed-table-derivation, worker-required-slots-feasibility, worker-lease-coverage-fix,
worker-oracle-final-sync, launch9-evidence, worker-engineversion-ini, worker-diagnostic-sink,
worker-installer-ps5io RUNTIME-ACK, worker-engine-ready-walkthrough, pad workers, l11 workers,
worker-post-popup-evidence FAILURE-TAXONOMY) or a pinned file:line in the consumed sources
(activated_mod_source.cpp, NativeModSource.cpp, SettingsManager.cpp, comp6 UE4SSProgram.cpp,
ue4ss_backend.cpp, NativePadInput.cpp, runtime_owner.cpp, runtime_ack_supplier.cpp, the
readpath-guard UnrealInitializer.cpp staging, notify-cancel LuaMod.cpp, the port/unreal tree,
the as-built bridge + SeedValidation.hpp under launch10-build).

Status classification used one rule: **DELIVERED-VERIFIED requires real in-game evidence for that
specific element**. What qualifies today: activation mechanics + configured roots + vmmap walk +
lease registration (launch-9 supplier plugin ran to lease registration, spans=6/reg=6); the
runtime-status.txt channel (sessions 234/273 written and read back); the UE4SS.log mirror and t=
stamps; the slot-0 address (zcode_anchor_validation_RESULTS + igf6 promotion, 71959 objects);
the launch-8/9 named refusals (code=1 geometry, code=2 unregistered-bytes, aligned-entry
geometry refusal); lifecycle failed paths; and the launch-9 entry harness's in-game synthetic
Lua VM. **Launch-9 loaded NO mods**, so every Lua-API, mod-chain, pump, drain, and seed-arming
element is DELIVERED-HOST-ONLY at best — the dashboard says so explicitly.

## Key findings

- The contract already exists de facto and is remarkably complete on paper: input surface 5
  groups / 27 elements, output surface 31 elements, 7 lifecycle stages, 11 refusal groups —
  but only a narrow band of it is in-game verified, exactly the band upstream of the
  initializer (activation, roots, capture, lease, ack channel, fail-closed naming).
- The engine-version boundary is large but enumerable: ~317 `IsBelow`/`IsAtLeast` sites across
  ~40 predicate pairs (4.8–5.8) in the consumed port tree, plus the versioned container
  publication (`PublishStagedNativeLayoutMaps` + `ArmLayoutGate` S2 gate) and the layout-ini
  identity-agreement refusal. The platform boundary is separately enumerable: 249 `__PROSPERO__`
  sites + the PS5 substrate (vmmap walk, lease registry, scePad, PRX packaging/imports, mfa
  activation). This split is the adapter system's porting checklist.
- The reuse analysis' sharpest edge: **seed values, geometry numbers, title identity, and layout
  ini contents must never transfer between games** — that transfer is precisely the
  silent-wrong-address failure class the loud-refusal design exists to prevent. Everything else
  on the input side is mechanism, not data, and is reusable as-is.
- One genuinely reusable finding for a second game: the ExecuteInGameThread **drain requires
  only the EngineTick closure — no ProcessEvent** (DRAIN-TRACE Q2). That means the tick-pump
  contract element is portable across engine versions without the marshaling gap, which remains
  a layer above the drain (menu/spawn execution).

## Honest limits

- Static extraction only; no new verification was performed. Where evidence was ambiguous
  (e.g. the as-built bridge fc06fe52 failing packaging compile vs the repaired 663e7ce9), both
  facts are recorded and the element stays DELIVERED-HOST-ONLY/prediction-pinned.
- Line pins are valid for the cited stagings; re-staging shifts them (the +69-line initializer
  shift in launch10-final-b is the standing example). The JSON carries the lineage note.
- The census numbers are greps of the consumed trees at 2026-10-05; a re-staging that swaps
  units (e.g. a future luamod variant-B composition) changes the luamod-side counts.
- No claim is made about PC-Townfall feasibility: `inputs/pc-townfall` is an empty placeholder;
  the second-game analysis is a contract-level porting checklist, not a schedule.
- This worker did not reconcile or touch the launch-10/L11 gate machinery; all IN-FLIGHT items
  remain owned by their named workers (worker-fname-validator-v4, worker-l11-marshal-integration,
  worker-luamod-rebase, worker-luamod-l11-dryrun, worker-launch10-walkguard-prep,
  worker-pad-hardware-probe).

## Controller usage

- `ADAPTER-CONTRACT.json` is the machine artifact: completeness dashboard per element, refusal
  vocabulary for acceptance tooling, and the reuse classification for any second-game brief.
- `ADAPTER-CONTRACT.md` §6 is the one-table answer to "what is actually verified vs built";
  §7 is the porting checklist for the adapter system.
