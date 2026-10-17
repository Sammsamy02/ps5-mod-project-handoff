# OBSERVABLES.md — console-side evidence of a first run WITHOUT a loaded 3D scene

worker-itemspawner-dryrun, 2026-10-04. Static analysis; nothing executed on host or console.
Purpose: make ladder phase-1 (mod load) checkable from the retained console log paths alone,
before any scene/save exists.

## Retained console log paths (evidence, not assumption)

- `/data/townfall_runtime_aligned_entry.log` — the CURRENT in-game runtime lineage
  (mode-2 `modbridge_runtime_aligned_entry.prx`; freshness oracle per ZCODE-FORK-HANDOFF
  launch ritual; probe v3 keeps "same log path/module name (replaces console file, no ini
  change)"). Records appear ~1 minute after launch, at injection completion
  ("injecting 2 plugins" → OK/FAIL).
- `/data/townfall_runtime_buffered_lua.log` — the full-runtime buffered-pure-Lua probe's
  log (checkpoint `completed_buffered_lua_console_probe`,
  `modbridge_runtime_buffered_lua.prx`, title PPSA33286, mode 2); Lua-side marker output
  was read back from this path via plain FTP in the retained evidence.
- `/data/townfall_mod.log` — older native module-probe log (91-byte marker read by
  ordinary FTP; profile `townfall.json` `log_path`). Historical.

**Open composition fact, stated honestly:** which of these the supplier-composed UE4SS
runtime routes the Lua mod's `print()` (main.lua:6) into is a property of the runtime
build, not of the mod. The phase-1 oracle below is therefore path-agnostic: grep the
ACTIVE retained Lua log path for the mod tag. `ConsoleEnabled=1` (FIRST-RUN-CONFIG) keeps
the console-side channel enabled; freshness = file mtime per the launch ritual.

## Phase-1 (load) expected evidence — no scene, no save, no key needed

Byte-exact, exactly ONE new mod-tag line, from IS:746 via `log` IS:6
(`print(MOD_TAG .. " " .. tostring(s) .. "\n")`):

```
[TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.
```

Absence list at load (all other log call sites — IS:189, 194, 335, 342, 501, 636, 643 —
are unreachable in the load path): no `spawnable pickups found`, no `Scan failed:`,
no `Spawned`, no `Failed at`, no `WidgetBlueprintLibrary not found.`, no
`UI creation failed:`, no `Menu created.`

Runtime-registry side evidence (runtime's own records, distinct from the mod tag): 6/6
keybinds registered (baseline trial precedent); enabled-mod list contains
`TownfallItemSpawner` (enabled.txt marker, NativeModSource.cpp:134-135;
BUNDLE-SPEC §3.6 observables are host/process-side, listed for correlation only).

**Phase-1 pass criterion:** the IS:746 line present in the active log, exactly once, and
nothing else tagged `[TownfallItemSpawner]`.
**Phase-1 fail discrimination** (per STAGE-PLAN-V2 §4 logic):
- No records at all at the usual post-injection time → load/injection failure
  (check ploader OK/FAIL popup; retry per ritual; do not replay without changed
  implementation or a specific open question).
- Probe records present but no mod line → the mod chunk aborted before IS:746. Only
  engine-touching calls exist after IS:692: the six `RegisterKeyBind` (694–729), the
  pcall-guarded `RegisterLoadMapPreHook` (731), and the final `log` (746). A nil
  `RegisterKeyBind`/`Key` aborts at the first bind; the pcall swallows a nil map hook,
  so it cannot abort. The chunk dies SILENTLY from the mod's perspective — the mod tag
  never appears; look runtime-side for the Lua load error.

## Phase-2 evidence (F7 pressed, still NO scene/save) — the early checkpoint

Only reachable if phase-1 passed. Expected outcome, byte-exact, exactly ONE new mod line
(composition: `log("Scan failed: " .. tostring(err))` IS:194; err from IS:66):

```
[TownfallItemSpawner] Scan failed: Item database not ready. Load a save first.
```

If instead the runtime lacks `FindFirstOf` (B2 unresolved), the SAME line appears with a
different tail — this tail is the discriminator:

```
[TownfallItemSpawner] Scan failed: attempt to call a nil value (global 'FindFirstOf')
```

Then expected SILENCE from the mod: `buildUI` returns false with no log call when no
PlayerController exists (IS:493–497), `menuOpen` stays false. Byte-exact absence list for
this scenario: `Menu created.` (IS:643), `UI creation failed:` (IS:636),
`WidgetBlueprintLibrary not found.` (IS:501), `Spawned` (IS:335), `Failed at` (IS:341/342).

Sub-case — a PlayerController exists (e.g., main-menu context) without a save: after the
Scan-failed line, `buildUI` proceeds and the next possible lines, in order, are
`WidgetBlueprintLibrary not found.` (IS:501) or `UI creation failed: <err>` (IS:636).
Either is a phase-2-POSITIVE signal: it proves `ExecuteInGameThread` delivered (IS:695),
the require chain resolved far enough (IS:15), and `StaticFindObject` executed (IS:499) —
i.e., ladder rungs 1–2 alive — while rung 4.5 (UMG, missing-new) failed, exactly as
BLOCKERS.md predicts.

## Critical ambiguity warning (honest limits)

No new mod line after an F7 press is ambiguous between:
1. the key never physically fired (A8 real device input unproven on PS5), and
2. the callback died at IS:695 on nil `ExecuteInGameThread` (B1) — unprotected call, so
   the mod itself logs NOTHING.

Discriminator: the runtime-side Lua error trace. The retained baseline trial captured
exactly this error mode at IS:695 with `attempt to call a nil value (global
'ExecuteInGameThread')` — its presence in runtime error logging is positive evidence of
key delivery + registration (a phase-2 partial pass), while its absence leaves the input
question open (needs the real-input workstream, not a re-press).

## Scene-free checkability summary

- Phase-1 (IS:746 line) requires only mod start — the load path never touches a world.
- Phase-2-no-save is the mod's DESIGNED graceful state (IS:66 error text exists for
  exactly this): one log line, zero engine mutation.
- A full phase-1 + phase-2-no-save pass therefore validates: chunk load, 6/6 registration,
  map-hook pcall, log channel, `ExecuteInGameThread` delivery, the require/FindFirstOf
  query path, and graceful database-absence handling — the whole ladder below rung 3 —
  all before any 3D scene or save is loaded.
