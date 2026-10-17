# FIRST-RUN-CHECKLIST — observable evidence for the unchanged Item Spawner first run

worker-itemspawner-bundle, 2026-10-04. Expected evidence for the bundle in
`bundle/` once installed per INSTALL-PACKAGE.md. Derived byte-exactly from the
pinned mod source (intake main.lua sha256 `f586043a…658`, NEVER executed on
host) and worker-itemspawner-dryrun's OBSERVABLES.md; nothing here was
executed on console. The mod Lua runs console-side only.

## Pre-flight (all must hold before launch)

- [ ] Layout pair merged and listed (AWAITING-STAGED-PAIR.md complete) —
      otherwise the run ends in `setup_unreal()` with
      `"Native activation requires both member and vtable layout files"` and
      NO mod evidence is possible.
- [ ] `enabled.txt` present (0 B) in `ue4ss/Mods/TownfallItemSpawner/` — the
      enabled marker (NativeModSource.cpp:134-135).
- [ ] `ConsoleEnabled=1` in `ue4ss/UE4SS-settings.ini` (it is, in this bundle)
      so the console-side channel stays enabled.
- [ ] Freshness oracle per launch ritual: log file mtime is fresh
      (`/data/townfall_runtime_aligned_entry.log` is the CURRENT lineage).
- [ ] `runtime-status.txt` reached `mods-started` (INSTALL-PACKAGE §5.2).
- [ ] NO scene/save requirement: phases 1 and 2 below are scene-free by design.

## Phase 1 — mod load (requires only mod start; no world access)

WHERE: grep the ACTIVE retained Lua log path for the mod tag
`[TownfallItemSpawner]` (path-agnostic per OBSERVABLES.md; current lineage:
`/data/townfall_runtime_aligned_entry.log`; records appear ~1 minute after
launch, at injection completion). WHEN: at mod start, before any input.

EXPECTED — byte-exact, exactly ONE new mod-tag line (IS:746 via `log` IS:6):

```
[TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.
```

ABSENCE list (every other log call site — IS:189, 194, 335, 342, 501, 636,
643 — is unreachable in the load path): no `spawnable pickups found`, no
`Scan failed:`, no `Spawned`, no `Failed at`, no `WidgetBlueprintLibrary not
found.`, no `UI creation failed:`, no `Menu created.`

Runtime-registry side evidence (runtime's own records, distinct from the mod
tag): 6/6 keybinds registered (baseline-trial precedent); enabled-mod list
contains `TownfallItemSpawner`.

PASS: the IS:746 line present exactly ONCE and nothing else tagged
`[TownfallItemSpawner]`.

FAIL discrimination:
- No records at all at the usual post-injection time → load/injection failure:
  check the ploader OK/FAIL popup; retry per the launch ritual; do NOT replay
  without a changed implementation or a specific open question.
- Probe records present but no mod line → the chunk aborted before IS:746.
  Only engine-touching calls exist after IS:692: the six `RegisterKeyBind`
  (694–729) and the pcall-guarded `RegisterLoadMapPreHook` (731; the pcall
  swallows a nil map hook, so it cannot abort). A nil `RegisterKeyBind`/`Key`
  aborts at the first bind — the mod dies SILENTLY; look RUNTIME-side for the
  Lua load error. Never re-rate unchanged evidence.

## Phase 2 — F7 pressed, still NO scene/save (early checkpoint)

Only reachable if phase 1 passed. Press F7 once (real device input).

EXPECTED — byte-exact, exactly ONE new mod line
(`log("Scan failed: " .. tostring(err))` IS:194; err from IS:66):

```
[TownfallItemSpawner] Scan failed: Item database not ready. Load a save first.
```

Discriminator tail if the runtime lacks `FindFirstOf` (B2 unresolved) — the
DIFFERENT tail is itself informative:

```
[TownfallItemSpawner] Scan failed: attempt to call a nil value (global 'FindFirstOf')
```

Then expected SILENCE from the mod: `buildUI` returns false with no log call
when no PlayerController exists (IS:493–497), `menuOpen` stays false.
Byte-exact absence list: `Menu created.` (IS:643), `UI creation failed:`
(IS:636), `WidgetBlueprintLibrary not found.` (IS:501), `Spawned` (IS:335),
`Failed at` (IS:341/342).

Sub-case — a PlayerController exists (e.g. main-menu context) without a save:
after the Scan-failed line, `buildUI` proceeds; next possible lines in order
are `WidgetBlueprintLibrary not found.` (IS:501) or `UI creation failed: <err>`
(IS:636). Either is a phase-2 POSITIVE signal: `ExecuteInGameThread` delivered
(IS:695), the require chain resolved (IS:15), `StaticFindObject` executed
(IS:499) — ladder rungs 1–2 alive, rung 4.5 (UMG) failed exactly as
BLOCKERS.md predicts.

PASS (no-PC sub-case): the Scan-failed line once, then silence.
AMBIGUITY WARNING (honest limits): no new mod line after F7 is ambiguous
between (1) the key never physically fired (A8 real device input unproven on
PS5) and (2) the callback died at IS:695 on nil `ExecuteInGameThread` (B1, an
unprotected call — the mod logs NOTHING). Discriminator: the runtime-side Lua
error trace; `attempt to call a nil value (global 'ExecuteInGameThread')` is
positive evidence of key delivery + registration (phase-2 partial pass); its
absence leaves the input question open for the real-input workstream — NOT a
re-press, NOT a re-rate.

## What a full phase-1 + phase-2-no-save pass validates

Chunk load, 6/6 keybind registration, map-hook pcall, log channel,
`ExecuteInGameThread` delivery, the require/FindFirstOf query path, and
graceful database-absence handling — the whole ladder below rung 3 — with zero
3D scene and zero save loaded, and zero engine mutation (the mod's designed
graceful state, IS:66).

## Evidence discipline

Capture the log lines verbatim (byte-exact, with the one-line count) and the
`runtime-status.txt` tail into this worker's directory or the controller's
evidence area. Never claim >8 quality or completion without this actual
evidence; missing evidence stays unrated (docs/QUALITY-RUBRIC.md).
