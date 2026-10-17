# SETTINGS-V3-DECISION.md — worker-settings-v3-package, 2026-10-04

## Question

A-PI (worker-all-signatures-inventory FLAG-3): the ProcessInternal /
ProcessLocalScriptFunction built-in derivation (staged
`UnrealInitializer.cpp:1497-1558`, copy at
`worker-query-s2-arm/staging/UnrealInitializer.cpp`) reads game code bytes at
init whenever its gate is true. The v2 settings package shipped WITHOUT keys
covering it. Which exact ini keys close it, do they kill the Lua event path,
and do they go into v3 now or only under a writability outcome?

## 1. The exact keys

The derivation block is gated once, at `SINI:1497`:

```cpp
if (UnrealConfig.bHookProcessInternal || UnrealConfig.bHookProcessLocalScriptFunction)
```

Both flags come from the settings file via `REGISTER_BOOL_SETTING` in
`port/ue4ss/src/SettingsManager.cpp:279-280` (section `Hooks`, key names are
the member names, parser `get_bool`, exceptions swallowed = missing/invalid key
silently keeps the `true` default from `SettingsManager.hpp:116-130`), and are
copied into the config at `UE4SSProgram.cpp:1075-1076`. Therefore:

- ini key 1: `[Hooks] HookProcessInternal`
- ini key 2: `[Hooks] HookProcessLocalScriptFunction`
- accepted values: the retained IniParser bools — `0`/`false` (both forms
  proven by the v2 oracle in one file), anything invalid = default `true`.

Two traps identified:

1. **The sub-derivation does not re-check its own flag.** The 164-byte Zydis
   read at `SINI:1527-1555` is gated by
   `!UnrealConfig.ScanOverrides.process_local_script_function &&
   Version::IsAtLeast(4,22)` (`SINI:1516`) — NOT by
   `bHookProcessLocalScriptFunction`. `ScanOverrides` is programmatic-only
   (zero references in SettingsManager.cpp; `UE4SSProgram.cpp:868` installs
   only a `version_finder` lambda), so it is NOT ini-reachable. Setting only
   ONE key to 0 leaves the OR true: the block still runs, still does the
   ExecuteUbergraph `GetFuncPtr()` + `resolve_function_address_from_potential_jmp`
   read at `SINI:1505-1506`, and still runs the 164-byte decode. **Both keys
   must be 0** — that makes `SINI:1497` false and skips the whole block.
2. **Silent default.** A typo'd key or value parses as default `true` with no
   error. The v3 oracle therefore asserts both keys READ BACK as false.

## 2. Does disabling the derivation kill Lua calls?

No. It only skips the built-in derivation + the PI/PLSF detour installs.

- **ProcessEvent (the event path worker-engine-tick-fallback relies on) is a
  separate PostInitialize step**: `SINI:1448-1456`, gated by its own key
  `[Hooks] HookUObjectProcessEvent` (default true, untouched in v3), resolved
  from the **vtable** via `OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL` — a data
  read, no Zydis text read, no dependence on PI/PLSF addresses. ProcessEvent
  closures still drain on the game's next ProcessEvent call.
- **ExecuteInGameThread** drains via `engine_tick_hook`
  (`HookEngineTick=true`, untouched), once per frame, with the cross-fallback
  to ProcessEvent mode already implemented in this composition
  (worker-engine-tick-fallback REPORT: LuaMod.cpp:4690-4769, 4413-4436,
  4463-4478). RegisterKeyBind is input-path, independent.
- **What is actually lost**: `UE4SSProgram.cpp:1148-1165` sets
  `can_create_custom_events = false` and logs a warning; the PI/PLSF detours
  are simply not armed (their enabled-flags in
  `Hooks/Internal/Metadata.hpp:176-188` read these same config flags). That
  disables BP custom-event creation and `RegisterHook` on script functions.
  The packaged Lua set (TownfallItemSpawner `main.lua` = RegisterKeyBind +
  ExecuteInGameThread only, 0 RegisterHook; shared UEHelpers.lua = no hooks)
  uses none of it. Nothing throws; no startup gate consults these flags.

## 3. Decision: ONE package with the keys — not conditional, not key-less

The parent's fork was "include vs leave out; if inclusion is only safe under a
specific writability outcome, ship two packages." The code evidence says
inclusion is safe under **both** outcomes, so the split would imply a hazard
that does not exist:

- **Text unreadable (exec-only, e.g. prot-0x4)** — `worker-exec-writability/`
  is still EMPTY (no verdict landed; tick workers observed an exec-only eboot
  header page). With v2 defaults the derivation enters at `SINI:1497`,
  `GetFuncPtr()` returns a code pointer, and `SINI:1506`
  (`ASMHelper.cpp:49-69`, `get_first_instruction_at_address`) reads
  instruction bytes at it with NO null short-circuit — an unconditional
  init-time fault, crash-shaped, BEFORE the 164-byte loop. Both keys = 0 skip
  the entire block: the fault site disappears, init proceeds, ProcessEvent
  arms from the vtable. **Keys prevent a probable init crash.**
- **Text readable** — with defaults the derivation succeeds and arms the
  PI/PLSF detours, which patch code on demand when a hook arms (FLAG-1-class
  WRITE against text, inventory row E5). Neither is used by this package.
  With keys = 0 the runtime is behavior-identical for our Lua; the only
  differences are the warning-only `can_create_custom_events` path and no
  dormant detours. **Keys are behavior-neutral and remove unused risk.**

Rollback stays trivial: v3 is a strict append to the v2 settings bytes
(prefix-verified by `cmp`), so reverting to the oracle-clean v2 pair
(834 B `9570ead3…` + 446 B `3afd10ce…`) is always available.

### What v3 does NOT fix (honest scope)

The two keys remove only the `SINI:1497-1558` reads. Other text-read
dependencies are separate and remain owned by their workers: EngineTick
sig-scan resolution (`EngineTickResolveMethod` default Scan,
`SINI:1352`, already covered by tick-address-validation/seed-table work), the
mismatch-diagnostic vtable walk (`SINI:1327-1344`, only on address mismatch),
and the wider signature scanner. v3 also does not force
`EngineTickResolveMethod=VTable`; changing that was out of scope and the tick
workers' verdict governs it.

## 4. Shipped artifact

`package-v3/` = package-v2 with exactly two changed files:

- `ue4ss/UE4SS-settings.ini`: 834 -> **1448 B**,
  `d8faa4eca2ed2f22c02693b6841b7939412b4dd60c218a26438f985152d67038` —
  appended `\n[Hooks]` + 6 full-line comments +
  `HookProcessInternal=0` + `HookProcessLocalScriptFunction=0` (+614 bytes;
  v2 bytes verified a strict prefix; no CR, no NUL, ASCII+LF only; full-line
  comments only per the parser trap).
- `loadlist.txt`: settings row 834 -> 1448,
  **447 B**, `4d98ecd7aba1777028cca50552ef9d9616ecb070a0eb8772b8f3e40a590f5d5d`
  — all other bytes identical; every row re-checked against actual file sizes.
- All 5 other payload files byte-identical to v2 (shas in REPORT.md).
