# CANDIDATE-INVENTORY — locally staged PC Lua mods for the multimod milestone

Agent-EF, 2026-10-08. Task 1 of the multimod-prep packet. READ-ONLY survey —
no downloads, no network, no console writes. Everything below already exists
in the worktree. All paths relative to
`/WORKSPACE/.codex/worktrees/translation-core/what/`.

Two independent local sources hold unmodified PC Lua mods (provenance is
recorded in `source-receipt.json` next to the first tree — archives were
fetched in an earlier session; nothing new was fetched now):

- **TREE-A** (audit staging, CRLF line endings):
  `.superpowers/native-dependency-spike/townfall-ue4ss-real-script-audit-1/`
  - `ue4ss-v1/ue4ss/Mods/…` — the stock UE4SS distribution mod set
    (from `ue4ss-v1.zip`, archive sha256 58a6…, see receipt tail)
  - `minimap-v3.1/ue4ss/Mods/TownfallMiniMap/` — real Nexus mod
    (author DavidWallacee, nexusmods.com), from `minimap-v3.1.zip`,
    archive sha256 cf3c353b…
  - `mod-menu-v6/TownfallModMenu/` — real game-community mod, from
    `mod-menu-v6.zip`, archive sha256 a9257d8e…
- **TREE-B** (UE4SS source-deps assets, LF line endings):
  `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/assets/Mods/`
  — same stock set plus ActorDumperMod / jsbLuaProfilerMod / shared/Types.lua.

Line-ending note: TREE-A copies are CRLF, TREE-B copies are LF, so sizes differ
(88–110 B per file). Both are byte-identical modulo EOL. For console staging
prefer the **TREE-B LF copies** (smaller, and the resident UEHelpers.lua on
console is the LF 9,962 B copy per the verified loadlist row). Whichever copy
is uploaded, `wc -c` THE UPLOADED FILE and pin that number in the loadlist row
(admission is size-exact — MFA law, agent-I-mfa-audit).

## Inventory (per mod: TREE-B LF size / TREE-A CRLF size, sha256-16 of TREE-B)

| Mod | Size (LF/CRLF) | sha256-16 (LF) | Kind | First look |
|---|---|---|---|---|
| LineTraceMod | 2,642 / 2,721 | 05c181a5cc3cb24e | stock UE4SS demo | pure proven-set APIs |
| CheatManagerEnablerMod | 1,407 / 1,439 | 15c7722168eb8966 | stock UE4SS | RegisterHook + SCO |
| ConsoleEnablerMod | 3,173 / 3,261 | 2517ea80a074bc5e | stock UE4SS | console key remap |
| SplitScreenMod | 4,270 / 4,380 | 05094ab61f016ebf | stock UE4SS demo | FindAllOf + player mgmt |
| ActorDumperMod | 593 / (A-tree n/a) | (TREE-B only) | stock UE4SS | RegisterCustomProperty |
| jsbLuaProfilerMod | 565 + shared/jsbProfiler/jsbProfi.lua 14,113 + shared/Types.lua 55,672 | (TREE-B only) | stock UE4SS | Lua profiler |
| Keybinds | 3,980 / 4,190 | 297dd6f7f560e1ce | stock UE4SS | C++ tool keybinds |
| BPML_GenericFunctions | 2,883 / 2,950 | df87c361cdfc231a | stock UE4SS | BP custom events |
| ConsoleCommandsMod | main 286 + set.lua 2,007 + dump_object.lua 12,777 + summon_unloaded_assets.lua 479 (4 files, LF) | 5bf33cd4… (main) | stock UE4SS | console commands |
| shared/UEHelpers | 9,962 / 10,237 | 7476f02eef0c87bf | shared lib | **ALREADY RESIDENT** (console loadlist row 9962) |
| TownfallMiniMap v3.1 | 196,743 (CRLF tree only) | d8f8d64f84d7e093 | Nexus mod (DavidWallacee) | heavyweight capstone |
| TownfallModMenu v0.66 | 128,009 (CRLF tree only) | 2ff1fa5395f668ee | community mod | RegisterHook-heavy |

Excluded from candidacy:
- `TownfallItemSpawner` (every 2026-10-04 package copy) — the proving-ladder
  baseline itself, not a "different mod".
- `claude-takeover…/examples/example-lua-mod/files/ItemSpawner/scripts/main.lua`
  (589 B) — a local deliberately-minimal packaging stub ("Left as a stub on
  purpose"), not a real upstream mod; zero API surface.
- `zzdiag-v3/` (2,314 B) and `zzdiag-v7/` (2,487 B) under
  `outputs/2026-10-08-l25-prep/` — diagnostics slated for retirement
  (see ZZDIAG-RETIREMENT.md), never candidates.
- BPModLoaderMod (10,953/11,254 B) — loads Blueprint mods from pak; depends
  on pak mounting machinery outside this milestone's scope.

## API-surface summary (verified by grep against the actual sources)

- **LineTraceMod**: `require("UEHelpers")`, `IsValid`, `GetFullName`,
  `UnrealVersion:IsBelow`, property reads (`Pawn`, `PlayerCameraManager`),
  `GetCameraLocation`, `GetCameraRotation`, `GetForwardVector`,
  `Multiply_VectorInt`, `Add_VectorVector`, `LineTraceSingle` (12 params with
  HitResult OUT — same signature as the spawner's), nested HitResult member
  read + weak-object `.Get()`, `RegisterKeyBind(Key.F3)`.
- **CheatManagerEnablerMod**: `RegisterHook("/Script/Engine.PlayerController:ClientRestart")`
  with `self:get()` context, `CheatManager`/`CheatClass` property Get/Set,
  `StaticFindObject("/Script/Engine.CheatManager")`,
  `StaticConstructObject(class, outer)` (2-arg), `GetAddress`. No keybind.
- **ConsoleEnablerMod**: `UEHelpers.FindFName` (FName ctor FNAME_Find),
  `StaticFindObject("/Script/Engine.Default__InputSettings")`,
  `ConsoleKeys` ArrayProperty read + element struct field Set
  (`ConsoleKeys[i].KeyName = FName`), `KeyName:ToString()`,
  `GetEngine`, `Engine.GameViewport` read, `StaticConstructObject` 2-arg,
  `ExecuteInGameThread` at load, `RegisterHook` + `UnregisterHook`
  (ClientRestart). No keybind.
- **SplitScreenMod**: UEHelpers getters, `FindAllOf`, `HasAnyInternalFlags`,
  `Player.ControllerId` read, bool property Sets (`bUseSplitscreen`),
  `CreatePlayer`/`RemovePlayer`, `K2_GetComponentLocation/Rotation`,
  `K2_SetActorLocationAndRotation` (HitResult OUT),
  `RegisterKeyBind(Key.Y/U/I, {ModifierKey.CONTROL})`.
- **ActorDumperMod**: `RegisterCustomProperty` (hardcoded offset 0x98),
  `FindFirstOf("Level")`, `Level.Actors` array wrapper `:ForEach`,
  `RegisterKeyBind(Key.NUM_THREE, {ModifierKey.CONTROL})`.
- **Keybinds**: `RegisterKeyBindAsync` + C++ dumper globals
  (DumpAllObjects/GenerateSDK/…) bound to CTRL-combos.
- **BPML_GenericFunctions**: `RegisterCustomEvent`, `ParamContext:get()`,
  `OutParam:set`, `StaticConstructObject` 10-arg, `IsA`.
- **ConsoleCommandsMod**: `RegisterConsoleCommandHandler` (inside requires),
  FOutputDevice userdata `:Log`, `type()` checks.
- **TownfallMiniMap**: `LoopInGameThreadWithDelay` 50 ms tick (guarded),
  `FindAllOf` of game classes, 20+ `StaticConstructObject` sites, `GetClass`
  (8×), `RegisterHook` (pcall'd), `NotifyOnNewObject` (guarded),
  SetColorAndOpacity/SetText/SetBrushResourceObject/SetGamePaused etc.
  (see per-call counts in the sheet). No RegisterKeyBind at all.
- **TownfallModMenu**: `FindFirstOf` of game classes, `StaticConstructObject`
  with `FName(name)`, raw `RegisterHook` sites (some unguarded),
  `NotifyOnNewObject` (pcall'd), `ExecuteInGameThreadWithDelay` (guarded with
  `ExecuteAsync` fallback), `RegisterKeyBind(BACKSPACE/UP_ARROW/DOWN_ARROW/RETURN)`.

## Binding availability (console-composed runtime, verified this pass)

The composed console LuaMod
(`outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp`)
registers the full upstream global surface: FindFirstOf/FindAllOf/
StaticFindObject/StaticConstructObject/LoadAsset, RegisterKeyBind (+ModifierKey
overload)/RegisterKeyBindAsync/IsKeyBindRegistered, RegisterHook/UnregisterHook/
NotifyOnNewObject, ExecuteInGameThread/WithDelay/AfterFrames, LoopAsync/
LoopInGameThreadWithDelay/LoopInGameThreadAfterFrames, UnrealVersion, EFindName,
EInternalObjectFlags, RegisterCustomProperty/RegisterCustomEvent, ForEachUObject,
ModifierKey, Key. So every inventory mod is BINDING-compatible; what varies is
how far beyond the spawner-proven set each one reaches (CANDIDATE-SHEETS.md).

## Verdict on "are local candidates thin?"

No. Twelve distinct unmodified PC Lua codebases exist locally with full
provenance receipts, spanning the whole risk range from a 2.6 KB
pure-proven-set probe (LineTraceMod) to a 196 KB real Nexus mod (MiniMap).
The fetch plan (FETCH-PLAN.md) is therefore written as a documented-for-later
complement — for widening the pool with popular small Nexus mods once the
three-run local ladder passes — not as a substitute for missing stock.
