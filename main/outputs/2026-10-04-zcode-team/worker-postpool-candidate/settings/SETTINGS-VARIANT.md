# SETTINGS-VARIANT — HookLoadMap=false (M-E "relocates the gate" probe)

worker-postpool-candidate · 2026-10-06 · PROPOSAL-ONLY. Config-side staging
only: no console write, no composition, no boot claim. The ini is
DATA on the console root (`/data/modbridge/roots/PPSA33286/ue4ss/`), so this
variant needs NO recompose — it is the one post-pool experiment that costs
zero rebuilds.

## 1. Verified current state

- The current staged settings ini is sha256
  `b980cea75c46a390a2182b642e4991fcc830e7401afc740c086ca20c359d3ce2`
  (1,947 B) — verified byte-identical against the on-disk pre-image
  `/tmp/ppsa33286-preimages-next/UE4SS-settings.ini.pre` (the Launch-5
  restore record; worker-regression-ledger + worker-test-matrix pin).
- ANSWER TO THE DISPATCH QUESTION: **YES — line 63 is `HookLoadMap=true`.**
- The key IS live in the composed runtime (this contradicts one stale
  worker note and is recorded here with sources):
  - `SettingsManager` REGISTERS it:
    `REGISTER_BOOL_SETTING(Hooks.HookLoadMap, section_hooks, HookLoadMap)`
    (`ue4ss-native-settings-defaults-2/src/SettingsManager.cpp:271`; same
    registration in defaults-1 :267 and original :262).
  - The composed program MAPS it on the unconditional path:
    `config.bHookLoadMap = settings_manager.Hooks.HookLoadMap;`
    (composed `UE4SSProgram.cpp:1242`; the whole Hooks family :1240-1254).
  - Consequence: the initializer's `if (UnrealConfig.bHookLoadMap)` (units
    `782dce71…` :2159) is TRUE under b980cea7, so the first exec-text walk
    is GameEngine::LoadMap — the walkguard-gate REPORT's GATE-1 placement.
    (The gate5 receipt's "bHookLoadMap has NO INI mapping and defaults
    false" statement is NOT true of the current composed world + staged
    ini; its own GATE-5 refusal conclusion is unaffected — that arm dies at
    InstallHook's hooking-disabled check regardless.)

## 2. The variant

- `UE4SS-settings.ini.b980cea7-current` — verified copy of the live bytes
  (`b980cea7…`).
- `UE4SS-settings.ini.hookloadmap-false` — sha256
  `a4b524a602b2d2f1075d3d217089e85539be8e1c7f75e8175035991df77cd899`
  (1,947 B). ONE-line delta, verified by diff: line 63
  `HookLoadMap=true` → `HookLoadMap=false`. Nothing else moves (no inline
  comments added — the parser corrupts values on inline `;`, per
  worker-ue4ss-settings measured finding 3; full-line `;` comments and the
  existing shape untouched).

## 3. Expected behavior (M-E, from worker-walkguard-gate REPORT §2 M-E +
current initializer source; PREDICTIONS of evidence shape, not captured
results — the boot still does NOT pass)

With `HookLoadMap=false`, `config.bHookLoadMap` goes false and the whole
LoadMap block (:2159-:2167) is skipped — including its resolve walk. The
gate RELOCATES to the next enabled flag: `HookEngineTick=true` (ini line
70, same parsing path, UE4SSProgram.cpp:1249) at initializer :2168.

Expected log shape for the variant boot (seeds phase identical to the
pool-validation candidate's — the capture-first pool admission, seeds
slots 0/3/pool/2/4 bind lines, then engine init):

    Object CDO: 0x…                      (data-side, unchanged)
    Struct CDO: 0x…                      (data-side, unchanged)
    Finding 'GameEngine'                 (data-side FindFirstOf)
    GameEngine CDO/Instance: 0x… [n: …]
    Finding 'GameModeBase' / GameMode CDO / Actor CDO / GameViewportClient CDO
    (NO "Finding GameEngine::LoadMap" line — the block is skipped)
    Finding GameEngine::Tick
    [WalkRxGuard] REFUSED: NO read attempted; walk text reads require the IGW RX verdict …   (W1, ASM:234)
    state N failed 1 [WalkRxGuard] refused: guard_off_no_rx_verdict (walk text read not permitted; no read attempted; detail=0)   (W2)

then the same fail-closed unwind as today (DisarmLayoutGate, rethrow,
native shutdown; no event loop, no mods, no pump, no input).

## 4. What the variant BUYS (and what it does not)

- BUYS: proves the gate is exactly the flag-gated resolve family, not
  something hidden behind the LoadMap site; names the NEXT gate
  (GameEngine::Tick vtable resolve, :2170-:2171) and confirms the
  CDO/find data-side region between the seeds and the family is clean;
  converts the post-Tick ordering (InitGameState :2294 → ProcessEvent
  :2339, both enabled in b980cea7) from reading into measurement once the
  RX verdict (M-A) lands — the flip then has a measured "what fires next"
  ladder instead of a predicted one.
- DOES NOT: pass GATE-1 (walkguard-gate REPORT M-E: "Does NOT pass GATE-1 —
  relocates it"); produce the RX verdict; unblock mods/pump/input.
- HAZARD (recorded, not relied on; walkguard-gate REPORT §4): this variant
  disables ONLY LoadMap — HookEngineTick stays true, so the Tick resolve
  still refuses and the boot still dies; the availability-predicate silent
  coupling the REPORT warns about (Tick left UNASSIGNED flipping the
  predicate toward the marshal) requires disabling the whole family AND a
  surviving boot — neither happens here. Never disable the whole family as
  a "route": RUNBOOK §5 forbids the silent patch; §5.1 must land as its own
  distinct-ID fix.
- If BOTH keys were false the walkguard would still fire later in the
  enabled family (InitGameState :2294, ProcessEvent :2339) — the variant
  ladder can be extended the same one-line way if the controller ever wants
  the next hop named; each additional flip is the same zero-rebuild shape.

## 5. Console handling law (for the controller/user; this worker ran nothing)

- Files-first: RETR-read the live ini, verify sha == `b980cea7…` BEFORE any
  change; STOR the variant as `UE4SS-settings.ini.next` + RNTO onto the
  live name (atomic); `.bak` the original; byte-exact RETR readback must
  equal `a4b524a6…`; restore law per worker-regression-ledger (the b980cea7
  bytes are the pinned restore point — keep a second copy host-side).
- Any in-game evidence from a variant boot is triage evidence only; it
  closes no acceptance rung by itself.
