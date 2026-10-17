# UEHELPERS GETWORLD AUDIT — agent IF (round-8 item 4, UEHelpers surface)

2026-10-11. ANALYSIS ONLY: Mac on battery, read-only except this file + the
agent-IF report; no builds, no console contact. Companion to IA's
FindFirstOf("World") core diagnosis (sibling packet) and to Claude round-8
item 4 (CLAUDE-ROUND8-RECEIPT.md row 4).

## 0. Inputs read

- Staged/console UEHelpers: `outputs/2026-10-08-menu-complete-package/console-test-20261008T140903Z-a0626b/UEHelpers.lua`
  (console fetch) = pristine vendored copy
  `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/assets/Mods/shared/UEHelpers/UEHelpers.lua`
  = staged `outputs/2026-10-04-zcode-team/worker-loadlist-final/final-package/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua`
  — **md5 `d6eb453d019b8dcd1a156b1efbe6ab5b`, 9,962 B, all three IDENTICAL**
  (UEHelpers v3, `UEHelpers.lua:6`). Console loadlist row confirms the
  resident copy: `evidence/tmp-preserved-20261010/tmp/lua-fast-loadlist.txt:12`
  `file lua_mod ue4ss/Mods/shared/UEHelpers/UEHelpers.lua 9962`. No drift.
- FS: `outputs/2026-10-08-l25-prep/API-PARITY-TABLE.md` (port bindings).
- FT: `outputs/2026-10-08-l25-prep/mod-ladder/STATIC-SCAN.md` + `agent-FT-static-scan.md`.
- FU: `outputs/2026-10-08-l25-prep/agent-FU-host-harness.md` + `host-lua-harness/stub-ue4ss.lua` + `RESULTS.md`.
- Round-4 session: `outputs/2026-10-08-l25-prep/evidence/round4-spawn-session-20261011.decoded.log`
  (grep UEHelpers/GetWorld/World/FFO2/UF) + `CLAUDE-ROUND4-SPAWN-PROVEN.md` item 5.
- Port source: `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/` (the
  composed console runtime per FS §sources) + coherent port tree
  `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/`.
- Spawner: `evidence/townfall-main.lua.console-copy`; MiniMap/ModMenu/stamina
  sources under `.superpowers/native-dependency-spike/townfall-ue4ss-real-script-audit-1/`
  and `real-mod-lua-runner-1/inputs/`.

---

## 1. THE GetWorld IMPLEMENTATION and its verdict

### 1.1 What UEHelpers.GetWorld actually calls (staged copy, file:line)

`UEHelpers.lua:96-107` — it does **NOT** call `FindFirstOf("World")` and does
NOT touch `GetEngine()->GetWorldContext`. Exact chain:

1. `UEHelpers.GetPlayerController()` (`:97` → `:69-82`):
   `FindAllOf("PlayerController") or FindAllOf("Controller")` (`:72`), loop
   `Controller:IsValid()` + `Controller.IsPlayerController and
   Controller:IsPlayerController() or Controller:IsLocalPlayerController()`
   (`:75`, reflected calls); returns `CreateInvalidObject()` if none.
2. If PC valid → **`PlayerController:GetWorld()`** (`:99`) — a hardcoded Lua
   member on the UObject userdata, not a reflected UFunction.
3. Else `UEHelpers.GetGameInstance()` (`:101` → `:48-53`):
   `FindFirstOf("GameInstance")` (`:51`); if valid → **`GameInstance:GetWorld()`** (`:103`).
4. Else `CreateInvalidObject()` (`:106`).
5. `GetWorldContextObject` (`:155-157`) is a pure alias of `GetWorld`.

### 1.2 The port's `:GetWorld()` hop and the blocker

- Lua member: port candidate `include/LuaType/LuaUObject.hpp:648-653` (upstream
  `UE4SS/include/LuaType/LuaUObject.hpp:640-645` — body identical):
  `auto_construct_object(lua, object ? object->GetWorld() : nullptr)`.
- Native: port `unreal/src/UObject.cpp:316-319` (= candidate `src/UObject.cpp:316`):
  `IMPLEMENT_UNREAL_VIRTUAL_WRAPPER_NO_PARAMS(UObject, GetWorld, UWorld*)`.
- The macro (`unreal/include/Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtualBaseVC.hpp:26-41`)
  looks up **`Unreal::UObject::VTableLayoutMap.find(L"GetWorld")`** and on miss
  **throws** `std::runtime_error{"Virtual UObject::GetWorld is unavailable,
  possibly unsupported in engine version"}`.
- The map on PS5 is populated ONLY from the audited compiled layout pair
  (`UE4SSProgram.cpp:989-1007`, `#ifndef __PROSPERO__` at :1119 compiles out the
  classic full VTableLayout.ini loader). That pair —
  `outputs/2026-10-08-l26-runtime/profile/units/CompiledNativeLayoutDeclarations.hpp:5`,
  `selection_source="compiled-audited-townfall-l25"` — stages exactly these
  vtable entries and NO others: `[AGameModeBase] InitGameState=0x7b8`;
  `[FProperty] CopyValuesInternal/DestroyValueInternal/GetMinAlignment`;
  `[UEngine] LoadMap=0x4f8, Tick=0x310`; `[UObject] ProcessEvent=0x268`;
  `[FMalloc]` ×16. **`GetWorld` is absent.** The on-console
  `ue4ss/VTableLayout.ini` fallback files are 10-11-line `[NativeLayout]`
  header stubs with no sections (verified: final-package copy +
  worker-emit-final-pair emission), so the fallback arm is equally empty.
  Round-4 log line 21 (`declaration_provider=compiled
  source=compiled-audited-townfall-l25`) proves this is the pair that ran.
- Upstream contrast: the vendored upstream ships full templates —
  `assets/VTableLayoutTemplates/VTableLayout_5_06_Template.ini` has `GetWorld`
  in `[UObject]` (template math: entry index 40 → 0x190 with 4+6 base slots).
  BUT template ≠ this build: the same math gives `ProcessEvent=0x270` while the
  audited pair says **0x268** (one-slot live divergence). A GetWorld offset must
  come from the same live vtable audit that produced 0x268 — never from the
  template.

### 1.3 Failure mode vs the observed World bug — and the IA dependency

**Verdict: UEHelpers.GetWorld does not ride FindFirstOf("World"); its failure
mode is DIFFERENT from the observed World-nil bug, and IA's core fix does NOT
heal the primary arm.**

- The observed round-4 symptom (`CLAUDE-ROUND4-SPAWN-PROVEN.md` item 5: "[MAP]
  absent — World lookup returned nil/'?'") lives on zzdiag's surface:
  `FindFirstOf("World")` + `w:GetName()`. The raw log refines it — decoded.log
  `:849` `[FFO2] q='World' … exact=2 … first=0x18005194C0 arm=wa` (the FindXOf
  walk DID accept a World instance) and `:851-852` `[UF] miss/nofn
  w=[n:29D61(GetName)] c=[n:1E6C3(World)] v=4` — the returned object's
  **GetName reflected call missed** (only 4 UFunctions on the World class
  chain, none named GetName) → pcall-fail → mn() "?" → 0 `[MAP]` lines. That is
  IA's FindXOf surface plus aGetName-reflection miss, NOT the UEHelpers path.
- On the port TODAY, `UEHelpers.GetWorld()` **throws** the §1.2 runtime_error
  the moment its first arm succeeds (a valid PlayerController exists — the
  normal in-world state per round-4 `pc=v`). The throw surfaces at the nearest
  C++ boundary: `start_mod`'s catch (`LuaMod.cpp:8026-8029`, boot-time
  execution) or the TRY wrapper of whichever tick/hook/ExecuteInGameThread
  callback fired it (`LuaMod.cpp:8587+` family) — logged as an exception, script
  aborted at that line. It returns nil/invalid-object only in the menu-phase
  shape where BOTH FindAllOf arms and `FindFirstOf("GameInstance")` miss.
- **IA dependency statement**: IA's FindFirstOf/FindXOf core fix heals UEHelpers
  only through the SHARED machinery — `GetPlayerController`'s
  `FindAllOf("PlayerController"/"Controller")` and `GetGameInstance`'s
  `FindFirstOf("GameInstance")`. The `:GetWorld()` vtable hop is an independent
  port-specific layout-staging blocker: with only IA's fix landed,
  `UEHelpers.GetWorld()` still throws in-world. The two round-8 item-4 surfaces
  need two fixes (or one vtable entry + IA's core).
- Blast-radius today — every known direct consumer is pcall-guarded, so this is
  feature-dead, not script-dead:
  - MiniMap `main.lua:526` `pcall(pc:GetWorld())` → `drawInto` returns false →
    minimap canvas rendering dead, mod alive.
  - ModMenu `main.lua:2086/:2101/:2319` (fog world-identity filtering) → all
    pcall'd; fog filtering degrades to unfiltered, mod alive.
  - Stamina `stamina_main.lua:58` `GetPlayerCharacter() or
    UEHelpers.GetWorldContextObject()` — short-circuits past the throw whenever
    a character exists; otherwise caught at the ExecuteInGameThread boundary.
  - The resident console-proven spawner never calls GetWorld (console copy
    `:14-24` uses `UEHelpers.GetPlayerController` only, pcall'd) — round-4's
    proven spawn is not at risk.
- **Packet-premise correction (honesty)**: "MiniMap uses UEHelpers" is
  imprecise — MiniMap is self-contained, zero `require` lines (FT §6; grep
  verified). It uses the SAME underlying `pc:GetWorld()` LuaUObject member that
  UEHelpers wraps, so the one §1.2 root cause blocks both; but UEHelpers-the-
  library is not MiniMap's dependency.

---

## 2. FULL UEHELPERS SURFACE CENSUS on the port (24 exported functions)

Legend — verdict vs the port: GREEN (all deps bound + console-proven family;
`*` = one unfired-but-proven-family hop), **BLOCKED** (transitively dead on the
§1.2 vtable throw), COND (conditioned on a named probe/gate). "Exercised by"
= which of the 13 mods (+ resident spawner) call it, per FT scan + my greps.

| # | UEHelpers function (line) | Port APIs it calls (FS cites, verified) | Bound | Exercised by | Verdict |
|---|---|---|---|---|---|
| 1 | GetUEHelpersVersion :31 | pure Lua | — | none directly | GREEN |
| 2 | GetEngine :38-43 | FindFirstOf("Engine") (LuaMod.cpp:1870, PROVEN family; "Engine" query unfired) | BOUND | ConsoleEnabler :51 (Engine.GameViewport) | GREEN* |
| 3 | GetGameInstance :48-53 | FindFirstOf("GameInstance") (:1870) | BOUND | BPML_GenericFunctions :58 (LOAD-ONLY) | GREEN* |
| 4 | GetGameViewportClient :57-63 | GetEngine + `Engine.GameViewport` property read (read family PROVEN) | BOUND | ConsoleEnabler :51 | GREEN* |
| 5 | GetPlayerController :69-82 | FindAllOf ×2 (:1921 PROVEN), IsValid (PROVEN), `IsPlayerController`/`IsLocalPlayerController` reflected calls (dispatch PROVEN, these two unfired) | BOUND | spawner :17 (pcall), LineTrace :7, stamina :14 | GREEN* |
| 6 | GetPlayer :86-92 | GetPlayerController + `.Pawn` property read (PROVEN family) | BOUND | stamina :14 | GREEN* |
| 7 | **GetWorld :96-107** | GetPlayerController → `:GetWorld()` member (LuaUObject.hpp:648) → vtable wrapper THROW (§1.2) | BOUND-BUT-THROWS | MiniMap :526 + ModMenu ×3 (direct member), stamina :58 (via GetWorldContextObject) | **BLOCKED (vtable entry absent)** |
| 8 | GetPersistentLevel :111-117 | GetWorld + `World.PersistentLevel` read | BOUND | none found | **BLOCKED** (rides #7) |
| 9 | GetGameModeBase :122-128 | GetWorld + `World.AuthorityGameMode` | BOUND | none found | **BLOCKED** |
| 10 | GetGameStateBase :133-139 | GetWorld + `World.GameState` | BOUND | none found | **BLOCKED** |
| 11 | GetWorldSettings :143-149 | GetPersistentLevel + `PersistentLevel.WorldSettings` | BOUND | none found | **BLOCKED** |
| 12 | GetWorldContextObject :155-157 | GetWorld (alias) | BOUND | stamina :58 | **BLOCKED** |
| 13 | GetAllPlayerStates :161-170 | GetGameStateBase + `GameState.PlayerArray` read, `#array`, element reads (FT gap #4: TArray Length/Index BOUND-UNTESTED) | BOUND | none found | **BLOCKED** (+array risk behind it) |
| 14 | GetAllPlayers :175-187 | GetAllPlayerStates + `PawnPrivate` + IsValid | BOUND | none found | **BLOCKED** |
| 15 | GetActorFromHitResult :193-204 | UnrealVersion:IsBelow (:7689, pure table), `HitResult.Actor`/`HitObjectHandle.Actor`/`.ReferenceObject` reads + weak `:Get()` (LuaFWeakObjectPtr.cpp:52, FT gap #11) | BOUND | LineTrace :72 | COND — rides P5 struct-OUT; World-independent |
| 16 | GetGameplayStatics :208-211 | CacheDefaultObject :14-26 → CreateInvalidObject (:1754), ModRef:Get/SetSharedVariable (:7848, member set identical), StaticFindObject (:1759 PROVEN), IsValid | BOUND | SplitScreen :38/:61 | GREEN |
| 17 | GetKismetSystemLibrary :215-218 | same | BOUND | LineTrace :6, stamina :57 | GREEN |
| 18 | GetKismetMathLibrary :222-225 | same | BOUND | LineTrace :7 | GREEN |
| 19 | GetKismetStringLibrary :229-232 | same | BOUND | none found | GREEN |
| 20 | GetKismetTextLibrary :236-239 | same | BOUND | none found | GREEN |
| 21 | GetGameMapsSettings :243-246 | same + `/Script/EngineSettings` path | BOUND | none found | GREEN |
| 22 | FindFName :251-253 | `FName(Name, EFindName.FNAME_Find)` (ctor :7696 PROVEN plain form; Find variant = P3) + NAME_None (:7698) | BOUND | ConsoleEnabler :12-13 | COND (P3; read-only pool find, no admission) |
| 23 | AddFName :258-260 | `FName(Name, EFindName.FNAME_Add)` — **FName pool ADMISSION** = the GATE-1 copyin oracle surface (mission STATE) | BOUND | none of the 13 directly | COND — blocked by the GATE-1 core (mission-level), NOT by IA's FindXOf item |
| 24 | FindOrAddFName :265-271 | Find arm (P3) + Add arm (GATE-1) + FName `__eq` (LuaFName.cpp:153) | BOUND | BPModLoader :21/:135/:199/:203 (SKIP mod) | COND (P3 + GATE-1) |

Module-internal (not exported): `CacheDefaultObject :14-26` — GREEN (deps all
proven).

**Counts**: 24 exported — 12 GREEN (6 carry one unfired-but-proven-family hop,
`*`), 8 BLOCKED on the single missing `GetWorld` vtable entry (#7,#8,#9,#10,
#11,#12,#13,#14), 4 COND (#15 P5; #22 P3; #23 GATE-1; #24 P3+GATE-1).

**Ecosystem dependency**: 7 of the resident/13-mod pool require UEHelpers —
LineTraceMod, ConsoleEnablerMod, SplitScreenMod, TownfallModMenu (pcall'd
:36), BPModLoaderMod (FindOrAddFName), stamina (13th), + the spawner. None of
the currently-deployed three (spawner/ueselftest/zzdiag) calls a blocked
function. "UEHelpers green" for the ladder = fix #7's vtable hop; the other
12 green functions are already shippable. MiniMap needs no UEHelpers but dies
on the same #7 root cause at `main.lua:526`.

---

## 3. RUNTIME EVIDENCE — round-4 session (`round4-spawn-session-20261011.decoded.log`)

- Mods started: TownfallItemSpawner `:816`, ueselftest `:827`, zzdiag `:835`.
- **UEHelpers load line: there is NONE — by design.** `grep -i uehelpers` over
  the whole decoded log = 0 hits; UEHelpers.lua prints nothing on load. The
  spawner requires it lazily inside `getPlayerController` (console copy :15,
  pcall-wrapped) and the menu-create + 5 spawn receipts prove that path ran
  with no error marker and no `[UF]` miss for IsPlayerController/
  IsLocalPlayerController. Verdict: **loaded clean is INFERRED, not receipted**
  — a pcall'd require failure is silent and falls back to
  `FindFirstOf("PlayerController")` indistinguishably. (FS §4.1's "proven by
  the spawner boot" is the same inference.)
- **GetWorld calls that fired: ZERO.** No vtable-throw signature
  ("GetWorld is unavailable"), no [UF]/[FFO2] lines attributable to
  UEHelpers.GetWorld's arms. The `PlayerController` LUA-FN lines `:844/:861-862`
  are zzdiag T-cycles (`FindFirstOf`) and possibly the spawner's
  `FindAllOf` — indistinguishable, and none proceeds to a `:GetWorld()` hop.
- World-related lines (IA's surface, for contrast, §1.3): `:849` FFO2 World
  exact=2 first=0x18005194C0; `:851-852` [UF] GetName-on-World miss (v=4).
  0 `[MAP]` lines across all 25 T cycles (matches ROUND4-SPAWN-PROVEN item 5).
- FindAllOf("PlayerController") at boot `:656`: exact=1 (the class object),
  **first=0x0, found=0** — no PC instances in menu phase (expected;
  GetPlayerController correctly returns invalid then). Post-load `:861+`:
  instances exist; T2+ `pc=v` (PlayerController resolvable in-world).
- `:21` `[NativeLayout] declaration_provider=compiled
  source=compiled-audited-townfall-l25` — ties the running binary to the §1.2
  layout pair that lacks GetWorld.

## 4. TEST PLAN — what launch-2 settles (design only; I edited nothing)

One launch covers BOTH round-8 item-4 surfaces:

1. IA's `[FL-MAP]` probe settles the FindFirstOf("World") core (IA's packet).
2. **One-line UEHelpers probe** (zzdiag/ueselftest chassis, IC-style followup —
   do NOT edit files for launch-2 without a task order):
   `local ok,w = pcall(function() return UEHelpers.GetWorld() end)` fired
   **in-world** (post-save-load, PC exists — so the PlayerController arm runs;
   in menu phase the GameInstance arm runs instead). Outcomes:
   - `ok=false` with message containing "Virtual UObject::GetWorld is
     unavailable" → confirms §1.2 (vtable entry absent) — the predicted result
     on the frozen round-4 build.
   - `ok=true` + `w:IsValid()` → layout gained the entry or a fallback landed;
     then print `w:GetFullName()` (bound Lua member, LuaUObject.hpp:533 — NOT
     `GetName`, which the round-4 log proves is unreflected on the World class
     `:851`) for the map-name receipt runbook §10.4 Q4 wants.
   - `ok=true` + invalid → both FindAllOf arms AND FindFirstOf("GameInstance")
     missed → feeds IA's shared-core diagnosis.
3. Cheap discriminator beside it: `pcall(function() return
   UEHelpers.GetPlayerController():IsValid() end)` — receipts the PC arm alone.
4. Fix directions for the owner (NOT executed here, task-order required):
   (a) stage an audited `[UObject] GetWorld=0x???` entry via the same live
   vtable-audit path that produced ProcessEvent=0x268 (template 5_06 math says
   0x190 but the audited ProcessEvent is -8 vs template — template values are
   NOT authoritative for this build); or (b) port-side fallback in the
   LuaUObject GetWorld member to the outer-chain walk already in-tree
   (`unreal/src/AActor.cpp:38-52`: walk OuterPrivate until class name ==
   "World" via TypeChecker::get_world_name) when the vtable entry is absent —
   mirrors the shipped AActor::GetWorld semantics without new layout facts.

## 5. Honest limits

- No console contact, no disassembly: the VTableLayoutMap claim rests on the
  compiled-pair source + the session's `[NativeLayout]` line + the digest chain
  (profile_digest d648e85e… matches CompiledNativeLayoutDeclarations.hpp), not
  on runtime memory inspection.
- The throw-surface claim assumes the C++ exception unwinds through LuaRaw C
  frames to the enclosing TRY/catch — the port already relies on this pattern,
  but I did not execute it. If unwinding aborts instead, the failure is worse
  (process abort); either way no usable World is returned.
- Template offset 0x190 is arithmetic on the vendored template, explicitly NOT
  a live fact (ProcessEvent divergence proves one slot of drift).
- "UEHelpers loaded clean" in round-4 is inferred, not logged (§3).
- Exercised-by columns are static (FT scan + greps); no dynamic call graph.
- [FFO2]/[UF] field semantics per agent-FE-ffo2.md and the candidate UObject.cpp
  diagnostic source (:680-730); not re-derived beyond that.

— agent IF (ZCode), 2026-10-11. Files created: this audit +
`agent-IF-uehelpers.md`. No other writes.
