# agent IF — UEHelpers/GetWorld audit (compact report)

2026-10-11. Full detail: `UEHELPERS-GETWORLD-AUDIT.md` (same directory).
Analysis only; read-only except these two files; no builds/console.

## GetWorld verdict

- Staged/console/pristine UEHelpers.lua are hash-identical (md5 d6eb453d…,
  9,962 B, loadlist-confirmed resident) — UEHelpers v3.
- `UEHelpers.GetWorld()` (UEHelpers.lua:96-107) does **NOT** ride
  `FindFirstOf("World")`. Chain: FindAllOf("PlayerController"/"Controller") →
  `PC:GetWorld()`; else `FindFirstOf("GameInstance")` → `GI:GetWorld()`.
- The `:GetWorld()` hop is a hardcoded LuaUObject member (port hpp:648 =
  upstream :640) → native `Unreal::UObject::GetWorld()` → vtable-wrapper that
  looks up `VTableLayoutMap["GetWorld"]` and **throws** if absent. The only
  PS5 layout source (compiled pair "compiled-audited-townfall-l25", proven
  running by round-4 log line 21) stages only InitGameState/FProperty×3/
  UEngine LoadMap+Tick/UObject ProcessEvent/FMalloc×16 — **no GetWorld** →
  `UEHelpers.GetWorld()` THROWS in-world today; it returns nil/invalid only in
  the menu-phase shape. IA's FindFirstOf core fix does NOT heal this arm; it
  only heals the shared FindXOf sub-arms (FindAllOf PC, FindFirstOf
  GameInstance). Two surfaces, two fixes (audited vtable entry, or an
  outer-chain fallback — AActor.cpp:38-52 walk already in-tree).
- The observed round-4 "World-nil" symptom is actually zzdiag's surface:
  FFO2 World exact=2 first=0x18005194C0 (FindXOf accepted an instance) then
  `[UF] nofn` GetName-on-World (unreflected, 4 UFunctions on chain) → "?" →
  0 [MAP] lines. Different layer from UEHelpers.GetWorld entirely.
- All known direct GetWorld consumers are pcall-guarded (MiniMap :526;
  ModMenu :2086/:2101/:2319; stamina :58 short-circuits) → feature-dead, not
  script-dead. Resident proven spawner never calls GetWorld. MiniMap uses NO
  UEHelpers (packet premise corrected: self-contained, 0 requires) but dies on
  the same root cause via the direct `pc:GetWorld()` member.

## Surface census counts (24 exported functions)

- 12 GREEN (6 with one unfired-but-proven-family hop: Engine/GameInstance
  queries, GameViewport read, IsPlayerController reflection, .Pawn read).
- 8 BLOCKED on the single missing GetWorld vtable entry: GetWorld,
  GetWorldContextObject, GetPersistentLevel, GetGameModeBase, GetGameStateBase,
  GetWorldSettings, GetAllPlayerStates, GetAllPlayers (none called by the
  currently deployed mods).
- 4 CONDITIONAL: GetActorFromHitResult (P5 struct-OUT), FindFName (P3),
  AddFName (GATE-1 FName-pool admission = mission oracle), FindOrAddFName
  (P3+GATE-1).
- UEHelpers consumers: 7 of the mod pool (LineTrace, ConsoleEnabler,
  SplitScreen, ModMenu, BPModLoader, stamina, spawner). Round-4 runtime:
  UEHelpers load inferred-clean (module silent by design; spawner path ran,
  no error markers) — not directly receipted; zero GetWorld calls fired.

## Launch-2 recommendation (no edits made)

IA's [FL-MAP] + ONE pcall'd `UEHelpers.GetWorld()` probe line (in-world,
zzdiag/ueselftest chassis, IC-style followup) settles both round-8 item-4
surfaces in one launch: ok=false+"unavailable" confirms the vtable blocker;
ok=true+valid → print `w:GetFullName()` (NOT GetName — proven unreflected on
World) for the map-name receipt.
