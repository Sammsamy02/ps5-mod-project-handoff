# VERDICT-SCHEMA — per-mod ladder stages, log markers, and launch discipline <!-- SUPERSEDED by LADDER-V2.md (Claude round-5) -->

Agent-FO, 2026-10-08. Companion to `MANIFEST.md` (risk sheets + run order)
and `probe-template.lua` (zzladder probe). Offline prep; console in REST
MODE. Evidence law (ACCEPTANCE-LADDER.md spirit): every stage verdict needs
the actual log line in the harvested UE4SS.log (harvest.py shows line
numbers) or, where marked USER-VISIBLE, the player's on-screen observation.
No stage is scored from inference; klog must be clean for the run to count.

## Ladder stages (S1-S5, in order; each includes the previous)

| Stage | Name | Pass evidence |
|---|---|---|
| S1 | loads-without-uninstall | admission + mod-start markers, NO uninstall/crash |
| S2 | keybinds fire (or documented alternate trigger) | dispatch markers + mod's own response line |
| S3 | engine lookups resolve | mod's own log lines naming resolved objects |
| S4 | mod-specific capability probe | zzladder selected steps PASS, same boot |
| S5 | full mod function | UNMODIFIED mod achieves its core function |

**Overall mod verdict = the lowest stage reached.** Acceptance for the
generality claim = S5 PASS. Stage outcomes are PASS / FAIL / SKIP:

- **PASS** — the exact marker(s) listed below hit in the harvested log.
- **FAIL** — the stage ran and its failure marker hit (or its pass marker is
  absent after the trigger was exercised). Any FAIL gets the full
  harvest.py treatment (below) and the mod's failure OWNER is the MANIFEST
  category of the failing call site — never edit the mod, never replay the
  same launch hoping for different output.
- **SKIP** — the stage cannot apply: S2 for keybind-less mods (MiniMap;
  ConsoleCommandsMod's console-command trigger), S5 for binding-gap mods
  (Keybinds C++ globals; BPML has no possible BP caller; ActorDumper offset
  unverified; BPModLoader pak machinery; jsbProfiler out of scope). A SKIP
  is written with its reason; it never blocks the ladder, it bounds it.
  The CRASH LAW skip (FindFunction/bpfn/class-children) applies to ZERO of
  the 12 (verified by grep, MANIFEST) — no mod is skipped for crash law.

## Universal markers (all mods, from the resident runtime)

- S1: `[l25] event loop started` + `Starting callback garbage collector!`
  (init passed :528), then `[l22] Lua mod started: <ModName> (main script
  returned)`. FAIL shapes: no `[l22]` line (script error'd — harvest shows
  the Lua error chunk), or `Stopping mod` / uninstall line, or crash.
- S2 (keybound mods): `[l22] keybind callback entry key=<n>` for the mod's
  mapped key, paired `[l22] ExecuteInGameThread queued ...` /
  `drained ... (callback entry)` where the mod uses the pump, plus the mod's
  own per-press line (per-mod table below). zzladder's own keybind evidence:
  `[Lua] [zzl] K1 f7`.
- S4 (zzladder probe, rides every boot; drop-in as its own mod dir
  `zzladder/Scripts/main.lua` generated from probe-template.lua with the
  rung's CONFIG): `[Lua] [zzl] Z0 zzl1 <MOD>` (load), `T pc=v|n|i|e`
  (6 s auto-report), `R mod=<MOD>`, `[P2] q` then `[P2] D` (game-thread
  dispatch pair = the dispatch family S2 relies on), then selected steps:
  - `[P0] env <g>=<type>…` — PASS if every global the rung's mod needs
    reports `function`/`table` (RUN-4 needs RegisterConsoleCommandHandler,
    FindObject, FindObjects; RUN-6/7 need io, NotifyOnNewObject,
    ExecuteInGameThreadWithDelay, LoopInGameThreadWithDelay).
  - `[P3] sco|stext|fname|tset PASS|FAIL <tok>` — S.P3 runs P3+P4 jointly:
    TextBlock StaticConstructObject, SetText(FText) + GetText read-back,
    FName ctor FNAME_Find roundtrip, text property Set.
  - `[P5] trace PASS hit=y|n out=got|weak|none` (or `[P5] arg FAIL no-pc`
    if probed before a save loaded — re-run in-game via F7 before judging;
    `[P5] call FAIL e` = marshal error, error text on the `   E ` line).
  - `[P6] hook PASS unreg=y|e` — RegisterHook + UnregisterHook both return.
  - `[P7] eitwd` fired within ~50 ms-schedule and `[P7] ligwd 1` / `2` —
    PASS = both lines present (ligwd stops itself after 2 ticks).
  - `[WD] no-run` = the +18 s run never drained — S4 FAIL (pump).
  S4 GATING LAW: if a rung's selected step FAILs, do NOT exercise the mod's
  trigger that boot; clean-exit, harvest, fix owner, next boot. The probe
  is never edited around a FAIL; F7 re-run is allowed only within the same
  boot to distinguish transient (no-pc) from hard errors.

## Per-mod stage markers (exact strings from the unmodified sources)

**RUN-1 LineTraceMod** (pad row e.g. TOUCH_PAD=F3)
- S2: keybind entry for F3 + `[LineTraceMod] Doing line trace`
- S3: `[LineTraceMod] KismetSystemLibrary: <fullname>` +
  `[LineTraceMod] KismetMathLibrary: <fullname>` at load
- S4 steps: P0, P5 (+P3 optional)
- S5: `[LineTraceMod] HitActor: <fullname>` (or `Nothing hit.` with aim at
  sky = PASS-shape for the call; HitActor preferred as the full proof)

**RUN-2 CheatManagerEnablerMod** (no keybind; trigger = save load /
ClientRestart)
- S2: N/A-keybind — alternate trigger documented: ClientRestart hook
- S3: none at load (hook body) — S3 folds into S5 evidence
- S4 steps: P0, P6
- S5: `[CheatManagerEnabler] Constructed CheatManager [0x…]` +
  `[CheatManagerEnabler] Enabled CheatManager` (skip-shape:
  `already exist, skipping restoration` = hook FIRED = S5 PASS with note)

**RUN-3 ConsoleEnablerMod** (no keybind; trigger = load + ClientRestart)
- S2: N/A-keybind — trigger = ExecuteInGameThread at load
- S3: `StaticFindObject` of Default__InputSettings — failure line
  `[ConsoleEnabler] InputSettings not found…`; pass = key-list lines
- S4 steps: P0, P3 (array/name/text write class), P6
- S5: `[ConsoleEnabler] ConsoleKey[%d]: %s` lines listing Tilde/F10.
  USER-VISIBLE (optional stronger evidence): synthesized F10 opens the
  game console.

**RUN-4 ConsoleCommandsMod** (trigger = console command; pairs with RUN-3)
- S2: N/A-keybind — trigger = `set`/`summon` command typed in the console
  RUN-3 built
- S3: `[ConsoleCommandsMod] ` banner lines via print
- S4 steps: P0 (env: RegisterConsoleCommandHandler/FindObject/FindObjects)
- S5: `summon <asset>` handler path = `LoadAsset` fires without error
  (PASS-shape: no Lua error chunk; absence-of-error acceptance). `set`
  deep path = documented binding-gap SKIP (Reflection/ImportText absent,
  MANIFEST §4): S5 partial-PASS with the gap named.

**RUN-5 SplitScreenMod** (pad rows from free set for CTRL+Y/U/I chords —
chords must be added to PS5Pad.ini BEFORE this rung, re-pinning its row)
- S2: keybind entry + `[SplitScreenMod] Creating player..`
- S3: `[SplitScreenMod] UseSplitScreen: true` at load (property read-back
  of the (b) bool Set — doubling as first (b)-class proof)
- S4 steps: P0, P5 (K2_SetActorLocationAndRotation shares the HitResult
  OUT class)
- S5: `Player 2 created.` (log); destroy path `Players teleport to Player
  1.` optional. USER-VISIBLE second viewport NOT required (single pad).

**RUN-6 TownfallMiniMap v3.1** (no keybind; self-driving 50 ms tick)
- S2: N/A-keybind — trigger = tick liveness
- S3: load-silent by design; S3 folds into S4/S5
- S4 steps: P0 (io + delay families), P6, P7 (LIGWD is this mod's engine)
- S5: USER-VISIBLE-first (agent-EF S9): minimimap panel visible in a
  corner during play + UE4SS.log free of Lua error chunks + absence of the
  too-old-runtime failure line `[TownfallMiniMap] this UE4SS is too old…`.
  Screen capture is the primary artifact; absence-of-error is corroboration
  only, never sole acceptance.

**RUN-7 TownfallModMenu v0.66** (spawner-FREE boot mandatory — nav binds
co-fire; pad rows for BACKSPACE/UP/DOWN/RETURN from the free chord set)
- S2: keybind entry + menu toggle line after BACKSPACE
- S3: load marker `[TownfallModMenu] Loaded v0.66-weight-invisibility.
  BACKSPACE opens/closes the menu.`
- S4 steps: P0, P6, P7
- S5: USER-VISIBLE: menu opens/navigates/closes; log free of unguarded
  RegisterHook error chunks.

**LOAD-ONLY rungs (S1 evidence, then stop):** Keybinds (S5 SKIP: C++ tool
globals absent), BPML_GenericFunctions (S5 SKIP: no BP caller can exist on
console). **Never run:** BPModLoaderMod (pak machinery + Dirs gap),
ActorDumperMod (hardcoded 0x98 offset law), jsbLuaProfilerMod (debug-hook
profiler, zero generality value). These three are SKIP at S1 by decision —
running them buys nothing the binding-gap analysis has not already pinned.

## Launch discipline (PS5 console rules, hard)

1. **One launch per mod rung.** Fresh boot per launch (autoload arms
   ploader; `pldmgr /loadpayload` restarts ploader between launches in one
   boot ONLY per the mission doc). NEVER rapid-fire launches: failed
   launches leave kstuff paused (ShadowMount pauses 15-25 s, resumes only
   on clean game exit) and then ALL games fail auth until clean exit or
   reboot.
2. **Clean exit to dashboard** at the end of every rung (resets launch
   state). A crash (CE-108255-1) = reboot before ANY next launch, and the
   rung verdict is FAIL at the crashed stage regardless of earlier PASSes.
3. **klog tap connected BEFORE the launch** (TCP ps5-host-not-configured.invalid:3232, capture
   to a NEW file per rung). klog clean gates the whole run: no copyin
   error bursts, no nonsleeping violations, no fatal kernel backtrace.
4. **Staging:** exactly ONE new mod per boot (+ the zzladder probe dir and
   its CONFIG for that rung). New loadlist rows per mod (main.lua +
   enabled.txt; ConsoleCommandsMod adds its 3 sidecar files; re-pin
   `wc -c` of the exact uploaded bytes — MFA size-exact law). Never upload
   executables; text-only staging, so no SITE SELF concern; /data/plugins
   and the mod root only.
5. **Rollback** = remove the mod's loadlist rows (+ any PS5Pad.ini chord
   row it added, re-pinning PS5Pad.ini's own row), verify the expected
   file-row count, clean exit. Nothing else changes between rungs.
6. **Harvest:** after every launch run
   `python3 harvest.py --label <MODNAME>-r<n>` (per-launch evidence pull +
   marker analysis; `--local-ue4ss` for analysis-only regression on saved
   logs). The `[zzl]` marker family above is the probe's parse contract —
   a future backward-compatible harvest extension may add a `diag-zzl`
   section; absent lines read ABSENT, never guessed.
7. **Mods are never edited.** A FAIL at any stage routes to the MANIFEST
   category owner (port-side fix or documented gap), never to a mod patch,
   never to an identical re-launch. New evidence = new rung attempt with a
   distinct label, only after the owner change lands.
8. **TownfallItemSpawner** stays resident ONLY for rungs that need it as
   baseline cross-evidence; RUN-7 (ModMenu) runs spawner-free. zzdiag v10
   retires per multimod-prep/ZZDIAG-RETIREMENT.md once zzladder is
   resident (never re-upload zzdiag after removal).
