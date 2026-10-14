# MANIFEST — 12-mod generality ladder: per-mod API risk classification <!-- SUPERSEDED by LADDER-V2.md (Claude round-5) -->

Agent-FO, 2026-10-08. Offline prep only (console in REST MODE, zero contact).
Sources: every `Scripts/main.lua` read in full (small mods) or grep-scanned with
line numbers (MiniMap 4,275 lines / ModMenu 2,700+ lines); binding facts cited
against the composed console runtime
`outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp` (L26-R4).
Proven-tonight evidence: TownfallItemSpawner 21,415 B main.lua (package-v3) —
its exact call sites are the definition of category (a).

## Category key

- **(a) PROVEN TONIGHT** — the spawner exercised the identical call family on
  the console: FindFirstOf / FindAllOf / StaticFindObject (long + short names),
  RegisterKeyBind + ExecuteInGameThread game-thread dispatch, reflected
  `obj:Fn(...)` calls (incl. UMG `WidgetBlueprintLibrary:Create` spawner:510,
  `K2_GetActorLocation` **FVector return** spawner:233,
  `BeginDeferredActorSpawnFromClass` spawner:295 + `FinishSpawningActor`
  **FTransform param** spawner:303, `GetItemIDFromDeprecatedEnum` **FName
  return** spawner:163), StaticConstructObject(Class, Outer, FName) spawner:50/516,
  FName/FText constructors, SetText(FText) spawner:434/469, IsValid, LoopAsync,
  print, GetFullName.
- **(b) BRIDGED, NOT YET CONSOLE-EXERCISED** — global is registered in
  LuaMod.cpp but no console session has fired it: FString param pushes into
  reflected calls, name/text property Set (incl. array-element struct field
  Set), FName-userdata `:ToString()`, FName ctor with `EFindName.FNAME_Find`,
  the delay family (`ExecuteInGameThreadWithDelay` :6017, `ExecuteAsync` :3611,
  `LoopInGameThreadWithDelay` :6488), `GetFName`, `IsA`, `GetClass`.
- **(c) UNKNOWN RISK** — three sub-grades:
  - **(c1) probe-first**: machinery is bound and same-depth as proven paths,
    but the exact shape is new (struct OUT params, hook registration, notify
    callbacks, file IO through the port's Sony-native stdio, ForEachUObject).
  - **(c2) binding-gap**: global/method referenced by the mod is NOT in the
    composed runtime (binding list verified by grep this pass) — mod loads but
    that path can never work unmodified.
  - **(c3) CRASH LAW = auto-SKIP**: `FindFunction` / `bpfn` / class-children
    enumeration (zzdiag v2 crash), input injection, HTTP. VERIFIED CLEAN this
    pass: `grep -rn "FindFunction|bpfn|GetDefaultClass|GetAllClasses"` over all
    12 mod trees = zero hits, so no mod takes the auto-SKIP.
- **(d) HARMLESS** — pure Lua (string/table math, locals).

SpawnActor/BeginDeferred note: per task order these are PROVEN working tonight
(spawner:295/303) — listed under (a), not (c).

## Inventory: 12 distinct mods, hashes of main.lua (sha256, this pass)

Staging trees (both verified present, read-only):
- **TREE-B LF** (preferred for upload; smaller): `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/assets/Mods/<Mod>/Scripts/main.lua`
- **TREE-A CRLF** (audit copies; same content mod EOL):
  `.superpowers/native-dependency-spike/townfall-ue4ss-real-script-audit-1/ue4ss-v1/ue4ss/Mods/...`
  and, MiniMap/ModMenu ONLY (no TREE-B copy exists):
  `.../townfall-ue4ss-real-script-audit-1/minimap-v3.1/ue4ss/Mods/TownfallMiniMap/Scripts/main.lua`
  and `.../townfall-ue4ss-real-script-audit-1/mod-menu-v6/TownfallModMenu/Scripts/main.lua`.

| # | Mod | sha256 (TREE-B LF main.lua) | B | sha256 (TREE-A CRLF) | B | Ladder verdict |
|---|-----|------------------------------|---|----------------------|---|----------------|
| 1 | LineTraceMod | 807d1cb0142f24de1534eb85e3ff98a0db5242c7d5856962738c268b3cbf89a6 | 2642 | 05c181a5cc3cb24e0bd7496d6bbc14a1111d0a1e731a2526203b41b07d972dae | 2721 | RUN-1 |
| 2 | CheatManagerEnablerMod | 0b70465317ef3fced4e342f04592896658e590c3cab472e2993460fba6734c83 | 1407 | 15c7722168eb8966ed454e900e04de4b0ade42657905d8016c0e0660c119cd9e | 1439 | RUN-2 |
| 3 | ConsoleEnablerMod | b31feb8ffb8adac1aa8204d65665ddd2fc4971e61cba210f18054e9bfbe911a8 | 3173 | 2517ea80a074bc5e6f00c55263c1867bf8e74c6b8665a79217adb53fdbe04343 | 3261 | RUN-3 |
| 4 | ConsoleCommandsMod | eb691aba47da830383e85c6208fda8cf6984ad37a12d6fe8eaa5b6daa4092f84 | 286 | 5bf33cd4ef9f6a155fe5f7842218a6486c2c227858ffd99806e75d81d0da9f27 | 297 | RUN-4 (partial core) |
| 5 | SplitScreenMod | 2cbb7df946a157ab51d93ad012f33b998520336fd43238eaf46d894141b19694 | 4270 | 05094ab61f016ebfcb651a0271a4c614884881b847090e6a74e6a51754e088db | 4380 | RUN-5 |
| 6 | TownfallMiniMap v3.1 | — (TREE-A only) | — | d8f8d64f84d7e0933b828211f6610d950e6244ea1a45f1e9f4f3bd8c353c920f | 196743 | RUN-6 capstone |
| 7 | TownfallModMenu v0.66 | — (TREE-A only) | — | 2ff1fa5395f668ee7539b6fb3b993c79293a37350889a3aa287f9c104ff39883 | 128009 | RUN-7 (spawner-free boot) |
| 8 | Keybinds | 6dc767037f55e09f9e04405814ff965b9c9943138509f7d5d2f29e9dfe4c14a8 | 3980 | 297dd6f7f560e1cebc15cd4509ad481495d9179f5758a8034be4c417a369803a | 4190 | LOAD-ONLY |
| 9 | BPML_GenericFunctions | dccb6501355247c83b2afb2179f6f135a32a3dc5202a5261d5afa32a6ae443a3 | 2883 | df87c361cdfc231ac8017ff9b930c323fac09f436ca4c41d48e952ff4b38d38f | 2950 | LOAD-ONLY |
| 10 | BPModLoaderMod | 81a3ff37b5376a84ab1fa7e880da914aa571f55f2beafaf030c817503a962ec9 | 10953 | c4d6602dc57559029e0e709650e43f33354bcb23f1a5df2ebe84d4651ebd08a3 | 11254 | SKIP |
| 11 | ActorDumperMod | e69510e52bf43f1b53854de6fb185013ab87e2dcd55a4007a0dac3297fe80c2b | 593 | — (not in A) | — | SKIP |
| 12 | jsbLuaProfilerMod | d7b9395b68b17000a1103e13d9a9a8271a77115a13f8fef19ffd30c634e2f14f | 565 | — (not in A) | — | SKIP |

ConsoleCommandsMod sidecars (TREE-B LF, all required by `require` at main.lua:10-11):
`set.lua` a2e170d3367c20537c676954fced3ea88e0c82418b1eba657e33002518806993 (2,007 B),
`dump_object.lua` 0d9b9bddcd3de38910c8321073d7d262e086b16b1265c6809cd714d050a28b71 (12,777 B),
`summon_unloaded_assets.lua` a8a0ee6547482212fe89241792f0c5cb2b5c09ab612d020c2d10c7771805b289 (479 B).
Resident shared lib: `shared/UEHelpers/UEHelpers.lua` TREE-B LF
5a8f6609e555eee4a535f51d21fcdef768ac654bf4d4abe283c719b1c12215a7 (9,962 B) — already on
console (loadlist row 9962); never re-upload. MiniMap/ModMenu ship `enabled.txt` + `Scripts/main.lua` only.
Not candidates (excluded, per agent-EF + baseline law): TownfallItemSpawner (the baseline itself),
zzdiag-v3..v10 (diagnostics), shared libs.

FINDING (staging gap): the two real game-community mods exist ONLY as CRLF
TREE-A copies. Before launch they must be staged to the console mod root as
new mod dirs; MFA law: `wc -c` the exact uploaded bytes and pin per-file rows
in the loadlist. Everything else stages from TREE-B LF.

---

## Per-mod risk sheets (line numbers = TREE-B LF copy unless marked TREE-A)

### 1. LineTraceMod — RUN-1 (lowest-novelty first rung)
- (d) L1-9 locals; `require("UEHelpers")` L2 (resident LF copy).
- (a) `IsValid` L12/L16; `GetFullName` L14/L18/L73; property reads `Pawn` L38,
  `PlayerCameraManager` L39; reflected Kismet calls `GetCameraLocation` L40,
  `GetCameraRotation`+`GetForwardVector` L41 (FVector returns — same marshal
  family as proven `K2_GetActorLocation` spawner:233), `Multiply_VectorInt` L41,
  `Add_VectorVector` L42 (FVector params); `RegisterKeyBind(Key.F3)` L79; `print` throughout.
- (b) `UnrealVersion:IsBelow` L26/28 (bound table, pure-Lua method — b/d boundary).
- (c1) `LineTraceSingle` 12-arg with **HitResult struct OUT table** L56-69 and
  bool return L71 — first struct-OUT marshal on console; nested weak-object
  `.Get()` read L27/29/31 (`HitObjectHandle.ReferenceObject:Get()` on UE5.4+).
  PROBE FIRST: template step P5 is exactly this call.
- Keybind F3 needs a PS5Pad.ini chord from the free set
  (TOUCH_PAD/TRIANGLE/CIRCLE/SQUARE/L1/R1/L2/R2; no-chord-contains-chord law).

### 2. CheatManagerEnablerMod — RUN-2 (first RegisterHook rung)
- (c1) `RegisterHook("/Script/Engine.PlayerController:ClientRestart")` L3 —
  bound (LuaMod.cpp:1182) but NEVER console-exercised; fires on next level
  load/pawn restart, not on keypress. `self:get()` param unwrap L4.
- (a) property reads `CheatManager` L6, `CheatClass` L11; `IsValid` L6/12/18/24;
  `StaticFindObject("/Script/Engine.CheatManager")` L15; `StaticConstructObject`
  2-arg (Class, Outer) L23 — spawner proved the 3-arg FName form only, 2-arg is
  trivially the same binding minus optional arg; `GetAddress` L25; `print` L7/13/19/28.
- (b) object-reference property Set `PlayerController.CheatManager = ...` L27 —
  same name/text Set machinery (LuaMod property Set), not yet exercised.
- (d) none beyond locals.

### 3. ConsoleEnablerMod — RUN-3 (array-write + FName roundtrip rung)
- (b) `UEHelpers.FindFName` L12-13 (→ `FName(Name, EFindName.FNAME_Find)`
  UEHelpers.lua:251-252); `ConsoleKeys` ArrayProperty read L25, `#ConsoleKeys`
  L29/41; **array-element struct field Set** `ConsoleKeys[#ConsoleKeys+1].KeyName =
  FName` L36 — the deepest (b) write in the ladder; FName-userdata `:ToString()` L42;
  FName equality compare L30.
- (a) `StaticFindObject("/Script/Engine.Default__InputSettings")` L22 (long name
  with Default__ prefix — spawner used both long and short forms);
  `Engine.GameViewport` property read L51; `ViewportConsole` read L52;
  `StaticConstructObject(ConsoleClass, GameViewport)` 2-arg L57;
  `IsValid` L23/48/52/56/58; `ExecuteInGameThread(CreateConsole)` at load L75;
  `print` L23/42/48/58/64.
- (c1) `RegisterHook` + `UnregisterHook` pair L79-87 — unhook-after-first-fire
  pattern; bound (LuaMod.cpp:2260). PROBE FIRST: template step P6 registers and
  immediately unhooks.
- (d) config tables L6-14.
- Note: constructing `UConsole` may have no visible effect on a PS5 build; core
  function acceptance = the mod's own printed keymap lines L42 + `ViewportConsole`
  assignment L60 (log evidence, not UI).

### 4. ConsoleCommandsMod — RUN-4 (handler-family rung; partial core)
- (a) `print` main.lua:4; `StaticFindObject` class lookups dump_object.lua:30-33.
- (b) `RegisterConsoleCommandHandler("summon"...)` summon:1 — bound
  (LuaMod.cpp:3865), never exercised; `LoadAsset` summon:5 — bound (:3906),
  never exercised.
- (c1) `RegisterConsoleCommandHandler("set")` set.lua:1; `FindObject` set.lua:18
  (bound :3956) and `FindObjects` set.lua:28 (bound :4128); FOutputDevice
  userdata `GlobalAr:Log` main.lua:5-6 and `Ar` param — userdata method,
  binding unverified → PROBE FIRST (P0 env report includes it).
- (c2) binding-gap: `EObjectFlags` set.lua:11, `ObjectOrClass:IsClass()`
  set.lua:25, `:Reflection():GetProperty()` set.lua:26/40, `Property:ImportText`
  / `:ContainerPtrToValuePtr` set.lua:30/42 — Reflection API absent from
  LuaMod.cpp (grep zero hits). The `set` command errors if actually invoked.
- (d) `type()` guards main.lua:5.
- VERDICT SCOPE: unmodified core function = command handlers register + log
  the mod banner; `set` deep-path is documented-binding-gap (S5 partial).

### 5. SplitScreenMod — RUN-5 (player-management rung)
- (a) `FindAllOf("PlayerController"/"Controller")` L23; `IsValid` L25/42/65/91;
  property reads `Player.ControllerId` L26, `Pawn` L86/91, `RootComponent` L87-88;
  reflected `K2_GetComponentLocation`/`K2_GetComponentRotation` L87-88 (FVector/
  FRotator returns — (a) family per K2_GetActorLocation precedent);
  `K2_SetActorLocationAndRotation` with HitResult OUT L92 (c1 — same struct-OUT
  novelty as P5); `ExecuteInGameThread` L39/67/85; `print` L15-16/35/38/44/58/61/64/77/83/98.
- (b) bool property Set `bUseSplitscreen`/`bOffsetPlayerGamepadIds` L13-14.
- (c1) `HasAnyInternalFlags(EInternalObjectFlags.PendingKill)` L25 — bound enum
  + method, never exercised; `CreatePlayer` L40 / `RemovePlayer` L68 — reflected
  GameStatics calls that mutate the local-player array (side-effectful; single
  pad on console — acceptance is the mod's own "Player %s created." L44 marker,
  not a second viewport).
- (b→pad) `RegisterKeyBind(Key.Y/U/I, {ModifierKey.CONTROL})` L106-110 — CTRL
  chord has NO PS5Pad.ini mapping; keys never fire unless a chord row is added
  (pad-chord budget law; each PS5Pad.ini edit re-pins its config row).

### 6. TownfallMiniMap v3.1 (TREE-A CRLF only) — RUN-6 capstone
- (a) `FindAllOf("PlayerController")` :307, `"InteractComponent"` :345, game
  classes :1450/2579/2880/3000; `StaticFindObject` :237; `StaticConstructObject`
  20+ sites :604/612/658/1062/1088/1125/1132/1354/1958/3132/3193/3201/3203/3331/
  3339/3342/3348/3618/3623/3652/3715 (UMG CanvasPanel/Image/TextBlock tree —
  same forms the spawner proved); `GetClass()` :507/1578/2291/2656/2680/2704/2785/
  4138 (bound method, (b) grade); `print` via its own log helpers.
- (b) `LoopInGameThreadWithDelay(1, M.frame)` :4034 and `(TICK_MS, safeTick)`
  :4268 — bound (:6488), never exercised (P7); `SetText` family (a); `str()`
  FName conversions throughout (b).
- (c1) `pcall(RegisterHook, path, ...)` :515 (guarded hook install — P6 first);
  `NotifyOnNewObject` guarded :4248-4249 (bound :1187/1213/1239, never
  exercised); struct math on read Vectors :1387-1391 (d once values are read).
- (c1) file IO: `io.open(settingsPath,"r")` :251 and `"w"` :293 — settings
  persistence through the port's Sony-native stdio; unverified whether the mod
  dir is writable on console. P0 reports `type(io)`; settings write failure is
  non-fatal to the widget path if `assert` :293 is only reached on save.
  RUN CONDITION: verify the mod's own pcall wrappers cover the settings path
  before launch (they do for reads :251→pcall, writes are `assert`-guarded :293 —
  if S5 fails at settings-save, that is the documented owner).
- No RegisterKeyBind at all (menu-less HUD overlay) — S2 = N/A, tick-driven.

### 7. TownfallModMenu v0.66 (TREE-A CRLF only) — RUN-7, spawner-free boot
- (a) `FindFirstOf("PlayerController")` :45; `StaticFindObject` :67/:1505;
  `StaticConstructObject(class(path), outer, FName(name))` :73 (exact spawner
  form); `ExecuteInGameThread` :887/:1005/:2440; `print` throughout;
  `FindAllOf` pcall'd :267/276/854/973/1294-1328/2327-2329.
- (b) `ExecuteInGameThreadWithDelay` with `ExecuteAsync` fallback :884-887/
  1002-1005/2048-2053/2429-2440/2563-2566/2641-2644 (P7); `GetClass` :127/379.
- (c1) `RegisterHook` sites :1163 (pcall'd), :1234, :1250, :1539, :1559, :2038,
  :2120, :2143, :2602 — SOME UNGUARDED (:1234/:1250/:1539+): an unguarded hook
  failure errors the main script at install time. P6 must pass first;
  if S1 fails at a RegisterHook line, that line is the owner (documented, not
  fixed — mods are never edited). `NotifyOnNewObject` :2037 (pcall'd).
  `ForEachUObject` :1515 — bound (:2833), never exercised (P0 env + optional).
- (c1) keybinds BACKSPACE/UP_ARROW/DOWN_ARROW/RETURN co-fire with the
  spawner's nav binds — MUST run in a spawner-free boot (EF finding 5); pad
  mapping from the free chord set.
- (d) big config/logic blocks.

### 8. Keybinds — LOAD-ONLY
- (a) `IsKeyBindRegistered` L25 (bound :1938); `RegisterKeyBindAsync` L26
  (bound :2000); `Key.*`/`ModifierKey.*` tables L14-20 (d).
- (c2) binding-gap: callables reference C++ tool globals `DumpAllObjects` L30,
  `GenerateSDK` L31, `GenerateUHTCompatibleHeaders` L32, `DumpStaticMeshes` L33,
  `DumpAllActors` L34, `DumpUSMAP` L35, `DumpJMAP` L36 — `DumpAllObjects`/
  `GenerateSDK` exist as no-op-ish registrations (:2356/:2367) but the other five
  are absent (grep). Loads clean (globals only dereferenced on press); full
  function impossible on the port. S1-S3 may pass; S5 = SKIP-by-gap.

### 9. BPML_GenericFunctions — LOAD-ONLY
- (a) `StaticFindObject("/Script/CoreUObject.Class")` L50; `print` L10.
- (c1) `RegisterCustomEvent("PrintToModLoader"/"ConstructPersistentObject")`
  L29/L45 — bound (:2922), never exercised; handlers only fire when a BP mod
  calls the custom event — impossible on console (no BP loader). `OutParam:set`
  L66 — UFunction Out-param set, unreachable without a caller.
- (b) `ParamMessage:get()` L31 + `Message:type()` FString check L36 +
  `Message:ToString()` L41 — FString userdata reads; `StaticConstructObject`
  10-arg EObjectFlags form L60 — first >3-arg SCO on console.
- (d) `Explode` string helper L14-27.
- VERDICT: S1 (loads) + P-probe only; S5 SKIP-by-gap (no BP caller can exist).

### 10. BPModLoaderMod — SKIP
- (c2/c1) pak mounting machinery L119-153 (Content/Paks/LogicMods), `io.open`
  rb L29, `io.lines` L44, `Dirs.Game...` UE4SS directory globals L119-125 —
  Dirs binding absent from LuaMod.cpp (grep zero); pak mounting is outside the
  port's supported IO set. Core function unreachable unmodified.
- (a) `RegisterKeyBind(Key.INS)` L269; `FindFirstOf("Actor")` L297.

### 11. ActorDumperMod — SKIP (offset law)
- (c2) `RegisterCustomProperty` with **hardcoded `OffsetInternal = 0x98`** for
  `Level.Actors` L1-9 — offset is ENGINE-BUILD-SPECIFIC and unverified for the
  console Townfall build; a wrong offset walks garbage → crash risk with zero
  generality value. Also `Level.Actors:ForEach` wrapper L15 (c1).
- (a) `FindFirstOf("Level")` L12; `GetAddress`/`GetFullName` L18;
  `RegisterKeyBind(Key.NUM_THREE, CTRL)` L11.
- Rejected identically by agent-EF (finding 6); stock mods.txt ships it
  disabled (`ActorDumperMod : 0`).

### 12. jsbLuaProfilerMod — SKIP
- (d) main.lua is ALL COMMENTS + one require L1 — loads as a no-op.
- (c1) `require "jsbProfi"` pulls `shared/jsbProfiler/jsbProfi.lua` (14,113 B)
  which uses `io.open` :71, `os.clock`/`os.date` :95-122, and `debug.getinfo`
  :146 (profiler machinery); `shared/Types.lua` (55,672 B) is an SDK stub table.
  A Lua VM profiler on the console port proves nothing about UE4SS generality
  and risks the hook budget. Stock mods.txt ships it disabled.

---

## Recommended order (cumulative, one launch each — see VERDICT-SCHEMA.md)

RUN-1 LineTraceMod (P5 first) → RUN-2 CheatManagerEnablerMod (P6 first) →
RUN-3 ConsoleEnablerMod (P3+P6) → RUN-4 ConsoleCommandsMod (P0 env) →
RUN-5 SplitScreenMod (P5; pad chord decision) → RUN-6 TownfallMiniMap (P0+P6+P7)
→ RUN-7 TownfallModMenu (spawner-free boot, P6+P7). LOAD-ONLY: Keybinds,
BPML_GenericFunctions (S1 evidence, no S5). SKIP: BPModLoaderMod,
ActorDumperMod, jsbLuaProfilerMod (reasons above; never run, never replay).
The zzladder probe mod (probe-template.lua, 4,080 B < 4,096 B budget) rides
ALONGSIDE each target mod in the same boot; target mods are never edited,
never re-uploaded mid-ladder. Probe step keys in the final template: S.P0
(env globals), S.P3 (P3+P4 jointly: TextBlock SCO/SetText/GetText read-back,
FName FNAME_Find roundtrip, text property Set), S.P5 (Kismet math +
LineTraceSingle HitResult OUT), S.P6 (hook reg/unreg), S.P7 (EITWD + LIGWD);
P1-class lookup corroboration folded into the 6 s `T pc=` cycle and the
R-run's own FindAllOf; P2 is the `[P2] q`/`[P2] D` dispatch pair.
