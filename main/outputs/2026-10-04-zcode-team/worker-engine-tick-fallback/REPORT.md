# REPORT.md — worker-engine-tick-fallback

2026-10-04. Decision: **KEEP `DefaultExecuteInGameThreadMethod=EngineTick` in the staged
UE4SS-settings.ini — no restage needed.** Full reasoning + exact settings line (only if the
controller overrides): TICK-FALLBACK-DECISION.md. Evidence summary in CLAIM.json.

## What was asked and found

1. **Values + call-time behavior.** Exactly two ini values parse: `EngineTick` and
   `ProcessEvent` (`SettingsManager.cpp:150-160`); anything else keeps the code default, which
   is already EngineTick (`SettingsManager.hpp:45`) — the staged line is an explicit pin, not a
   behavior change. Plain `ExecuteInGameThread` (`LuaMod.cpp:4690-4769`) checks availability at
   call time and throws with no fallback (`:4715-4720` for EngineTick, `:4723-4729` for
   ProcessEvent); the WithDelay/Loop siblings DO cross-fall back on `__PROSPERO__`
   (`:4796-4841`, `:4954-4996`, `:5126/5141+`). EngineTick closures drain once per frame
   (`engine_tick_hook`, 4463→4478); ProcessEvent closures drain only when the game calls
   ProcessEvent (`process_event_hook`, 4413→4436). ProcessEvent mode is *implemented* in our
   composition (resolve via `bHookUObjectProcessEvent=true` default, `SettingsManager.hpp:128`,
   assigned `UnrealInitializer.cpp:1375-1382` — independent of the Tick scan) but has **zero
   captured console evidence**, while EngineTick success is a structural precondition of ever
   reaching engine-ready/mods-started.
2. **Availability timing.** Not lazy. `UEngine::TickInternal` is assigned once inside
   `UnrealInitializer::Initialize` (`UnrealInitializer.cpp:1252-1318`), called from
   `setup_unreal` (`UE4SSProgram.cpp:1273`), i.e. before the engine-ready ack
   (`UE4SSProgram.cpp:654-661`) and before the event loop installs/starts Lua mods
   (`:673 → :1514 → :1476 → :1483-1487`). If the resolve fails, the PROSPERO core
   `HookedEngineTick` registration gets `ERROR_ID` (`DetourInstance.hpp:158-163, 287-296`) and
   throws `Native game-thread initializer callback registration failed`
   (`UnrealInitializer.cpp:1320-1327`) → init catches, shuts down (`UE4SSProgram.cpp:691-707`)
   → runtime-status `failed`. So "EngineTick unavailable" and "mods-started observed" are
   mutually exclusive: the smoke worker's silent-dead-menu world is unreachable.
3. **Recommendation.** Keep EngineTick. Switching would (a) gate F7 on the unevidenced
   ProcessEvent resolve, (b) still not remove the core's hard EngineTick dependence
   (`UnrealInitializer.cpp:1320-1327`, `UE4SSProgram.cpp:1276-1291`), (c) make closure timing
   game-dependent, and (d) add a detour on the engine's hottest function. If overridden, the
   exact restage line is `DefaultExecuteInGameThreadMethod=ProcessEvent` (line 12 of
   `/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini`, +2 bytes); all config-review
   constraints re-verified unaffected ([Overrides] still absent, hot-reload keys still absent —
   `EnableHotReloadSystem` struct default false `SettingsManager.hpp:33`, console keys still
   zeroed per `validate_native_services` `UE4SSProgram.cpp:719-746`) — but the ini sha256 in
   SETTINGS-CONTENT.md (9f3fa12…) and all byte-identical staged mirrors would need restaging
   (worker-ue4ss-settings/staged, worker-settings-bundle-merge/staged-package,
   worker-loadlist-final/final-package; verified identical).
4. **Failure log line for launch-5.** In `<root>/ue4ss/UE4SS.log` (Error level,
   `UE4SSProgram.cpp:255-258`; caught at `LuaMod.cpp:1944-1947`):
   `ExecuteInGameThread: EngineTick method requested but EngineTick hook is not available
   (AOB scan failed)` + Lua traceback (`LuaMadeSimple.cpp:1048-1067`; thrown `LuaMod.cpp:4719`).
   The prefix is unique to this throw site. Distinguish from: rank-2/3 mod texts
   (`UI creation failed:` / `Scan failed:` / `Failed at ground trace.`), the ProcessEvent
   variant (`…ProcessEvent method requested…`, `:4727` — would prove an ini restage), and the
   pre-mods fatal `Native game-thread initializer callback registration failed`
   (`UnrealInitializer.cpp:1323-1326`, runtime-status `failed`, no mods-started line).

## Bottom line for composition-5 r6

- r6 reports **mods-started** → the EngineTick gate is already open by construction; F7's
  ExecuteInGameThread cannot hit it. Remaining F7 risk mass shifts to smoke-worker ranks 2-3
  (engine-backed lookups, LineTraceSingle marshalling) — unchanged by this decision.
- r6 reports **failed without mods-started** → look for `Native game-thread initializer
  callback registration failed` in UE4SS.log; that is the true EngineTick failure shape, and no
  ini value change fixes it.
- Optional direct confirmation after mods-started: grep UE4SS.log for
  `GameEngine::Tick address` / `Using scan address for GameEngine::Tick`
  (`UnrealInitializer.cpp:1294-1318`).

## Method + limits

- Static source trace of the composition port tree (paths in CLAIM.json); 33 tool calls; no
  console contact, no nested subagents, no source or staged file modified; all writes in
  `outputs/2026-10-04-zcode-team/worker-engine-tick-fallback/`.
- `/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini` does not exist on this Mac; the
  three local staged mirrors were used as bytes-of-record (mutual identity verified by diff).
- No live engine evidence gathered by this worker; the "gate open after mods-started"
  conclusion is an ordering proof from cited code, corroborated by
  worker-pad-chord-audit/PAD-DISPATCH-PATH.md:95-135 (independent trace, same order) and the
  worker-lua-mod-smoke rank-1 description.

## Deliverables

- `CLAIM.json` — first
- `TICK-FALLBACK-DECISION.md` — recommendation + exact settings line + log line
- `REPORT.md` — this file (last)
