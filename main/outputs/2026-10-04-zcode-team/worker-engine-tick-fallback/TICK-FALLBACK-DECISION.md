# TICK-FALLBACK-DECISION.md — EngineTick vs ProcessEvent for `DefaultExecuteInGameThreadMethod`

worker-engine-tick-fallback, 2026-10-04. Adjudicates the worker-lua-mod-smoke rank-1 risk:
staged `UE4SS-settings.ini:12` `DefaultExecuteInGameThreadMethod=EngineTick` + the port's
plain `ExecuteInGameThread` having no ProcessEvent fallback.

All sources: port tree `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/{ue4ss,unreal}`
(unless marked). `LuaMod.cpp` = `.../port/ue4ss/src/Mod/LuaMod.cpp`; this is the file the smoke
worker cited (line numbers verified: registrations at 4690/4773/5126, catch at 1937-1948).

## Recommendation: KEEP EngineTick — no staged-ini change

One line: **the gate the risk describes is unreachable after `mods-started`, and in the only
world where it *is* closed, the runtime never reaches `mods-started` at all** — it fails loudly
at init with a distinct status and log line. Switching to ProcessEvent would not fix that world
(the core still hard-depends on EngineTick) and would move the F7 gate onto a candidate with
zero captured console evidence.

## 1. What the settings value does

- Enum `GameThreadExecutionMethod` has exactly two values: `ProcessEvent`, `EngineTick`
  (`ue4ss/include/SettingsManager.hpp:16-20`). The ini parse recognizes only the literal
  strings `"ProcessEvent"` / `"EngineTick"`, case-insensitive (`SettingsManager.cpp:150-160`);
  any other or missing value silently keeps the struct default, which is **EngineTick**
  (`SettingsManager.hpp:45`). So the staged pin is belt-and-braces over the code default —
  deleting the line would change nothing.
- Applied at `UE4SSProgram.cpp:1475-1476` (`LuaMod::m_default_game_thread_method = ...`)
  inside `on_program_start()`, immediately **before** `install_lua_mods()` / `start_lua_mods()`
  (`UE4SSProgram.cpp:1483-1487`). Lua mod states are created after this, so every mod sees the
  ini value from its first call.

### Call-time behavior per route (LuaMod.cpp:4690-4769)

| Route | Availability check at call | Closure runs inside | Drain site |
|---|---|---|---|
| EngineTick (staged) | `!UEngine::TickInternal.is_ready()` → `lua.throw_error` **4719**, no fallback | EngineTick detour, next engine tick (per-frame, deterministic) | `engine_tick_hook` drains `m_engine_tick_actions`, 4463→4478 |
| ProcessEvent | `!UObject::ProcessEventInternal.is_ready()` → throw **4727** | ProcessEvent detour, at the game's **next ProcessEvent call** (game-dependent timing) | `process_event_hook` drains `m_game_thread_actions`, 4413→4436 |

- `ExecuteInGameThread(fn, EGameThreadMethod.ProcessEvent)` per-call override exists
  (4695, 4702-4706; enum table global `EGameThreadMethod` at 4669-4678) — the mod does not use it
  (all six keybind callbacks are plain `ExecuteInGameThread`, per RUNTIME-REQUIREMENTS A9).
- The **WithDelay siblings do fall back** on `__PROSPERO__`: `ExecuteInGameThreadWithDelay`
  tries the default, and on unavailability switches to the other hook, throwing only if BOTH
  are unavailable (`LuaMod.cpp:4796-4817`, reverse direction 4819-4841; same shape for the
  continuation block at 4954-4996 and `LoopInGameThreadWithDelay` at 5126/5141+). The smoke
  worker's asymmetry claim is correct — plain `ExecuteInGameThread` alone has no fallback.
- Availability predicates (`UE4SSRuntime.cpp:10-24`): `is_ready()` on the respective resolve
  candidate — i.e. "address was assigned during the init scan" (comment at 12-14: detour itself
  is created lazily at first callback registration; the check is on the resolved function).

## 2. When is availability decided — registration order vs the acks

Strictly ordered, single-shot, before any Lua exists:

1. Backend ctor → `start_native_mods()` (`bootstrap/ue4ss_backend.cpp:31-36`) — mods inventory,
   `m_native_mods_started=true` (`UE4SSProgram.cpp:582`).
2. `owner->start()` → `program.init()` (`UE4SSProgram.cpp:600`) → `setup_unreal()` (619) →
   `UnrealInitializer::Initialize(config)` (`UE4SSProgram.cpp:1273`):
   - **EngineTick resolve happens HERE, once**: `if (UnrealConfig.bHookEngineTick)`
     (`UnrealInitializer.cpp:1252`; `Hooks.HookEngineTick` default **true**,
     `SettingsManager.hpp:125`, staged ini has no `[Hooks]` section) → scan-then-vtable assign of
     `UEngine::TickInternal` (`UnrealInitializer.cpp:1292-1318`).
   - Then, `__PROSPERO__`-only: the core registers `HookedEngineTick` ("GameThreadInitializer")
     pre-callback (`UnrealInitializer.cpp:1320-1327`). Registration goes through
     `AddCallback → EnsureHooked → InstallHook`, which fails when `!is_ready()`
     (`unreal/include/Unreal/Hooks/Internal/DetourInstance.hpp:158-163`) and returns `ERROR_ID`
     (287-296) → `throw std::runtime_error("Native game-thread initializer callback registration failed")`
     (1322-1326) → `init()` catches and calls `shutdown_native_program()`
     (`UE4SSProgram.cpp:691-707`) → runtime-status `failed` (states per
     `activation/modforge_activation.h:145`). **EngineTick-unresolved is fatal here — it can
     never coexist with engine-ready, let alone mods-started.**
   - ProcessEvent resolve also happens here, independently: `UObject::ProcessEventInternal`
     assigned via virtual-function resolve when `Hooks.HookUObjectProcessEvent=true`
     (default **true**, `SettingsManager.hpp:128`; wired `UE4SSProgram.cpp:1234`; assign at
     `UnrealInitializer.cpp:1375-1382`). Its detour installs lazily at first
     `ensure_process_event_hooked` (`LuaMod.cpp:4536-4547`) with the same
     is_ready→ERROR_ID mechanics (`DetourInstance.hpp`).
3. engine-ready ack: `on_engine_initialized()` fires at `UE4SSProgram.cpp:654-661`
   (backend wires it at `ue4ss_backend.cpp:23-26`) — both candidates' fates already fixed.
4. Event loop jthread (`UE4SSProgram.cpp:673`) → `update()` → `on_program_start()` (1514) →
   ini value applied (1476) → `install_lua_mods(); start_lua_mods()` (1483-1487) → Lua states
   created; capability globals `EngineTickAvailable`/`ProcessEventAvailable` captured at state
   setup (`LuaMod.cpp:4680-4688`) → **mods-started** status follows.

**Conclusion:** availability is NOT lazily decided at the first F7 press. It is fixed during
step 2, before engine-ready and before any keybind can be registered. A user pressing OPTIONS
on a loaded save implies the game has been ticking with the mod started — i.e. the EngineTick
detour was resolved, installed, and has been firing (it pumps the core initializer and the Lua
action drain). `is_ready()` never transitions after init. The "silent dead menu" world requires
`mods-started` AND `!is_ready()`, which the ordering above makes unreachable.

## 3. Why not switch to ProcessEvent (even as insurance)

1. **No evidence advantage.** A grep across all of `outputs/2026-10-04-zcode-team` finds no
   captured console line `ProcessEvent address ...` (only symbol-probe stdouts). EngineTick
   success, by contrast, is *implied by every engine-ready/mods-started ack ever observed*
   (step 2 above). The switch would gate F7 on the weaker-evidenced candidate.
2. **It doesn't decouple from EngineTick anyway.** The core registers the GameThreadInitializer
   on EngineTick unconditionally under `HookEngineTick=true` (`UnrealInitializer.cpp:1320-1327`),
   and `UE4SSProgram.cpp:1276-1291` prefers the EngineTick detour for the HashTables self-test.
   If EngineTick resolve fails, the runtime dies regardless of the ini value.
3. **Weaker pump.** ProcessEvent closures run at the game's next `ProcessEvent` call
   (`LuaMod.cpp:4413-4436`) — normally next-frame in a BP-heavy scene, but not guaranteed
   per-frame like the EngineTick drain (4463-4478). Menu-toggle latency becomes game-dependent.
4. **Extra detour on the hottest function.** `ensure_process_event_hooked` installs a second
   global detour (on every `UObject::ProcessEvent` call in the engine) purely to pump Lua
   closures — additional PS5 detour-surface for no benefit in the reachable world.
5. **The error text would move, not disappear.** In the fatal world the mod would instead throw
   `ExecuteInGameThread: ProcessEvent method requested but ProcessEvent hook is not available
   (AOB scan failed)` (`LuaMod.cpp:4727`) — or more likely nothing at all, because the runtime
   already died in step 2.

### If the controller overrides and switches anyway

Exact replacement (restage-only, no PRX rebuild) — line 12 of
`/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini`:

```ini
DefaultExecuteInGameThreadMethod=ProcessEvent
```

(case-insensitive at parse, `SettingsManager.cpp:150-160`; `+2` bytes vs current value).

Downstream re-verification (all satisfied by a line-12-only change, verified against the
staged bytes and SETTINGS-CONTENT.md):
- `[Overrides]` section stays absent — unchanged (`SettingsManager.cpp:112-113` refuses
  non-empty ModsFolderPath/ControllingModsTxt).
- Hot-reload keys stay absent — `General.EnableHotReloadSystem` / `HotReloadKey` /
  `Debug.ToggleGuiKey` remain unparsed → struct default false (`SettingsManager.hpp:33`) → the
  HotReload keybind is never registered (`UE4SSProgram.cpp:1440-1448`). The switch does not
  touch them.
- Console keys stay zeroed — `Debug.ConsoleEnabled=0`, `GuiConsoleEnabled=0`,
  `GuiConsoleVisible=0`, `RenderMode=ExternalThread` are MANDATORY on native
  (`validate_native_services`, `UE4SSProgram.cpp:719-746`); untouched.
- `CrashDump.EnableDumping=0` untouched (non-PROSPERO-only path anyway).
- BUT: the ini sha256 recorded in `SETTINGS-CONTENT.md`
  (`9f3fa1277218331432294fcdac88097d5268a2db6007b72b4266f45a17730f7f`, 779 B) and the
  byte-identical mirrors (`worker-ue4ss-settings/staged`, `worker-settings-bundle-merge/staged-package`,
  `worker-loadlist-final/final-package` — verified identical) would all diverge; any size/hash
  pin in the staging gates must be updated on restage. This is the only mechanical cost, and it
  applies only if the controller chooses the (not recommended) switch.

## 4. Exact failure log line (for launch-5 interpretation)

If the gate were ever hit, the F7 press produces, in `<root>/ue4ss/UE4SS.log`
(LogLevel::Error, file device named at `UE4SSProgram.cpp:255-258`, `m_log_directory =
m_root_directory` on PROSPERO; the keybind dispatch lambda catches the C++ runtime_error at
`LuaMod.cpp:1944-1947`):

```
ExecuteInGameThread: EngineTick method requested but EngineTick hook is not available (AOB scan failed)
```

followed by the Lua stack traceback of the keybind callback (or the literal `No traceback`
suffix — `first/src/LuaMadeSimple/LuaMadeSimple.cpp:1048-1067`, error text thrown at
`LuaMod.cpp:4719`).

Distinguishing features:
- The literal prefix `ExecuteInGameThread: EngineTick method requested` is unique to this
  throw site (`LuaMod.cpp:4719`); no other path emits it. Rank-2/3 F7 failures log the mod's
  own texts instead (`UI creation failed:`, `Scan failed:`, `Failed at ground trace.`).
- A ProcessEvent-mode failure would read `ExecuteInGameThread: ProcessEvent method requested
  but ProcessEvent hook is not available (AOB scan failed)` (`LuaMod.cpp:4727`) — its presence
  would prove someone restaged the ini.
- The pre-mods-started EngineTick fatal has a DIFFERENT signature: UE4SS.log line
  `Native game-thread initializer callback registration failed` (`UnrealInitializer.cpp:1323-1326`)
  plus runtime-status.txt `failed` with **no** mods-started line. That — not a dead menu — is
  the real shape of an EngineTick AOB failure for composition-5 r6.

## 5. Honesty limits

- Static trace only; no console execution by this worker (no console contact, per scope).
- `/data/modbridge/roots/PPSA33286/...` is the controller-side restage path and does not exist
  on this Mac; the staged mirrors listed in CLAIM.json were used as the bytes-of-record
  (verified mutually identical at line 12).
- "EngineTick resolve succeeds on this title" is not directly observed by this worker either;
  it is *inferable* from any engine-ready/mods-started ack because the init ordering makes it a
  precondition (section 2). The console check remains: after r6 reports mods-started, the gate
  is closed-open by construction; optionally grep UE4SS.log for `GameEngine::Tick address` /
  `Using scan address for GameEngine::Tick` for direct confirmation.
