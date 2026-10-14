# FIRST-RUN-CONFIG.md — minimal settings + bundle for a harmless first run

worker-itemspawner-dryrun, 2026-10-04. Static derivation only; nothing here was executed.
Goal: mod ENABLED, logging visible, and ZERO engine effect until a keybind is pressed.
Bundle shape and byte sources are exactly BUNDLE-SPEC §2; this file fixes the minimal
instance of it and argues harmlessness from the pinned source.

## The bundle (six keys, BUNDLE-SPEC §2 — nothing more)

| # | Key | Bytes |
|---|---|---|
| 1 | `ue4ss/UE4SS-settings.ini` | generated, content below |
| 2 | `ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` | intake member-00000000.bin, 21415 B, sha256 `f586043a…658` (unchanged, re-verified) |
| 3 | `ue4ss/Mods/TownfallItemSpawner/enabled.txt` | intake member-00000001.bin, 0 B, sha256 `e3b0c442…855` (presence-only) |
| 4 | `ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` | retained distro asset, sha256 `5a8f6609…15a` (BUNDLE-SPEC 1.4) |
| 5 | `ue4ss/MemberVariableLayout.ini` | **BLOCKED-ON-LAYOUT-DATA (worker-2) — see gate below** |
| 6 | `ue4ss/VTableLayout.ini` | **BLOCKED-ON-LAYOUT-DATA (worker-2) — see gate below** |

No pak, no dlls/, no mods.txt row, no keybind config — the mod has none (BUNDLE-SPEC §2).

## `ue4ss/UE4SS-settings.ini` — minimal content (every option is parsed by SettingsManager.cpp)

```ini
[Overrides]
; MUST stay empty: native activation throws on any override row
; (SettingsManager.cpp:112-113; UE4SSProgram.cpp:2708-2714).
ModsFolderPath=
ControllingModsTxt=

[General]
bUseUObjectArrayCache=false                 ; retained distro value (census 8c)
SecondsToScanBeforeGivingUp=120             ; modern default
DefaultExecuteInGameThreadMethod=EngineTick ; A9: every keybind callback is ExecuteInGameThread (main.lua:695,700,705,710,716,725)
DefaultFNameToStringMethod=Scan             ; B8: backs ToString() (main.lua:59)

[Debug]
ConsoleEnabled=1                            ; retained distro value; keeps mod output visible console-side
```

Deliberately absent (BUNDLE-SPEC 1.1 rationale carries over): `HotReloadKey`/`ToggleGuiKey`
(key-parse risk, no benefit), `EngineVersionOverride.*` (would fake layout axes),
`Hooks.HookLoadMap=false` (the mod's `RegisterLoadMapPreHook` at main.lua:731 is
pcall-guarded; the runtime default stands). No keybind settings inputs: the mod registers
its six binds itself (main.lua:694–729); `EnableDebugKeyBindings` stays default because the
retained trial registered 6/6 binds without it (RUNTIME-REQUIREMENTS A8).

## Hard gate stated honestly (blocks the run itself, today)

Keys 5+6 have NO validated Townfall bytes anywhere in retained evidence
(BUNDLE-SPEC 1.5, LAYOUT-GATE). `load_unreal_offsets_from_file()` throws
`"Native activation requires both member and vtable layout files"` when either key is
absent (UE4SSProgram.cpp:899-904) — the run then ends inside `setup_unreal()`, BEFORE any
Lua. The placeholder mode that can exercise the file-load path is explicitly not layout
facts and must not be used to claim a mod run. **A harmless first run therefore waits on
worker-2 layout data; there is no honest bundle today that reaches Lua startup.**

## Why this config is harmless until a keybind (from the pinned source)

1. Load-time engine contact is limited to: 6 keybind REGISTRATIONS (main.lua:694–729,
   registry only), one pcall-guarded `RegisterLoadMapPreHook` (731), and one `print`
   (746 via 6). No `FindFirstOf`/`StaticFindObject`/`StaticConstructObject`/`LoadAsset`/
   `FText`/UMG call is reachable at load (see TWO-PHASE-CHECKLIST.md, lazy list).
2. All five non-F7 callbacks early-return while `menuOpen=false` (699, 704, 709, 714, 723)
   — they return BEFORE their `ExecuteInGameThread`, so pressing them first does nothing.
3. Even the first F7 press, without a save and without a PlayerController, performs only
   nil-returning lookups and sets Lua-local status: `scanItems` fails gracefully with one
   log line (194; error text from 66), `buildUI` returns false silently at 493–497. No UI
   is built, no world state changes, `menuOpen` stays false.
4. Worst-case residual risk is honestly stated: keybind REGISTRATION is a runtime-registry
   write, and if a real keyboard event later arrives it will dispatch `ExecuteInGameThread`
   — on a runtime without A9 this dies at 695 exactly as observed in the baseline trial
   (nil global, no engine effect).

## Phase-0 variant (even more harmless; NOT the requested config — record only)

Shipping the same bundle with `enabled.txt` REMOVED leaves the mod present but disabled
(NativeModSource.cpp:134-135): zero Lua executes, zero keybinds register. Useful only to
separate "runtime startup works" from "mod load works". The requested first run keeps
`enabled.txt` present per the task ("mod enabled, logs on, no effect until keybind").

## Logging "on" in this config

The mod has no verbosity switch; its only channel is `print` (main.lua:6). "Logs on" is
satisfied by `ConsoleEnabled=1` plus the runtime's retained log routing (see
OBSERVABLES.md for the console-side paths and the byte-exact expected lines).
