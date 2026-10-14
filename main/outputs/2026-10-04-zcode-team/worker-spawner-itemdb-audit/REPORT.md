# REPORT.md — worker-spawner-itemdb-audit

2026-10-04. Proposal/audit only; nothing executed on console; no console contact; no nested subagents.

## Task

Audit the staged TownfallItemSpawner mod's item database end to end (main.lua, 21415 B, 746 lines), determine whether the composed PS5 runtime can actually resolve item entries to spawnable UClass objects at spawn time, identify the highest-probability first-spawn entry, and cross-check the mod's class names against in-game evidence. Deliverable: SPAWN-READINESS.md.

## What was done (10 reads/greps over ~5 evidence trees, 25 tool calls total)

1. Read the staged mod in full: `worker-loadlist-final/final-package/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` (746 lines; byte-identical sha256 f586043a…658 to the bundled copy per LUA-API-MATRIX.md:5).
2. Read LoadAsset in the composed runtime PORT `native-coherent-runtime-1/source/port/ue4ss/src/Mod/LuaMod.cpp:3618-3665` and its binding layer (`port/unreal/include/Unreal/UAssetRegistry.hpp`, `UAssetRegistryHelpers.hpp`, `FAssetData.hpp`, `ReflectedFunction.hpp`, `src/UAssetRegistry.cpp`).
3. Read the prior audit basis (`worker-itemspawner-lua-audit/LUA-API-MATRIX.md`) and smoke evidence (`worker-lua-mod-smoke/SMOKE-RESULTS.md`, `REPORT.md`, `harness/PHASE-LOGS/f7-save-dump.txt`).
4. Searched checkpoint + in-game + GUA records (`worker-checkpoint-reconcile`, `worker-ingame-probe*`, `worker-ingame-log-parser`, `worker-fname-runtime-probe`, `worker-acceptance-evidence`, `worker-real-input`, `worker-modreqs/MOD-CENSUS.md`) for any object/class name matching the mod's references.

## Key findings

1. **Item DB map**: hybrid of an embedded 6-entry AlwaysCook path table (main.lua:72-79), a 24-entry deprecated-enum table (main.lua:83-109) resolved live via `ItemDatabaseSubsystem:GetItemIDFromDeprecatedEnum` (IS:163) → `GetInteractablePickupIDActorPath` (IS:168), and per-entry `{name, id, path}` records. Hard gate: `FindFirstOf("ItemDatabaseSubsystem")` (IS:64) must succeed at scan time or the menu list stays empty entirely (IS:185 assignment is after the throwing DB call IS:147) — the static entries do NOT bypass the DB prerequisite, they only bypass the per-entry lookups.
2. **Resolution chain**: `normalizeClassPath` (IS:122-136 — strips `'...'`, appends `.Asset`, appends `_C`) → `StaticFindObject` (IS:283) → `LoadAsset` fallback (IS:285) → retry (IS:286) → `BeginDeferredActorSpawnFromClass`/`FinishSpawningActor` (IS:295,303) → post-spawn gates requiring a `PickupInteractable` component and non-empty `PickupInventoryItem.ItemId` (IS:306-318; extras skip the id-match IS:321). LoadAsset's three return values are discarded by the mod (IS:285) — it cannot distinguish silent-nil from success.
3. **LoadAsset on our runtime**: registered/implemented at PORT LuaMod.cpp:3618 but entirely reflection-backed — three ProcessEvent invocations (ReflectedFunction.hpp:34). Two distinct failure semantics: asset registry pointer null → LOUD throw "Did not load assets because asset_registry was nullptr" (PORT LuaMod.cpp:3636-3639); asset not in registry / UFunction missing → SILENT `(nil,false,false)` (BIND UAssetRegistry.cpp:38-40), which the mod then converts to its own loud "Pickup class could not be loaded: <path>" (IS:289-291). Additional unverified gates: FAssetData assumed 0x50 layout with the header's own version-warning (BIND FAssetData.hpp:34-39), and whether a SHIPPING 5.6.1 runtime asset registry indexes `/Game/Townfall/AlwaysCook/**` at all.
4. **Fallback inventory**: NO entry spawns without asset-class resolution and no generic engine-class fallback exists — the post-spawn gates would reject one by design (IS:306-318). Highest first-try probability: **Antibiotics** (IS:75), the only entry whose complete normalize→find→spawn→configure→banner path was exercised end to end (host smoke: f7-save-dump.txt:618 StaticFindObject of the normalized path hit first-try, :47 "Spawned Antibiotics x1"). Caveat: that evidence is stub-backed, and PS5 asset residency is unevidenced.
5. **Evidence cross-check**: ZERO of the 30 menu entries are evidence-backed against our runtime. No in-game record contains any of the mod's names (`ItemDatabaseSubsystem`, `/Script/Townfall.PickupInteractable`, `/Game/Townfall/AlwaysCook/*`, pickup property names, enum values). GUA is live-validated (0x97ba110, 71,959 objects — CURRENT-STATE.md:7) but with no name-level dump; FName construction's in-game verdict is pending (CURRENT-STATE.md:10, worker-fname-runtime-probe/REPORT.md:64). Closest provenance: MOD-CENSUS.md:119,126 shows the same class names in the retained PC (Windows proxy) intake of the same game — plausibility, not evidence.
6. **Failure-mode signatures** (all in SPAWN-READINESS.md §6): every spawn failure yields `[TownfallItemSpawner] Failed at <stage>. See UE4SS.log. <err>` (IS:341-342); the DB-absent scan yields an empty menu plus `Scan failed: …` (IS:193-194); the keybind-delivery gate yields NO mod line at all (smoke REPORT.md:26-33). Success signature: `[TownfallItemSpawner] Spawned <name> x<N> ID=<id>` (IS:334-335).

## Honest limits

- Host smoke results prove Lua plumbing against stubs, not engine behavior; they are cited only for ordering/coverage, never as engine evidence.
- Asset residency, registry population, ProcessEvent param widths, and FName resolution on the PS5 SHIPPING build remain unverified; this report predicts what each failure will look like, not that a spawn will succeed.
- The BIND headers cited are the coherent runtime's own `port/unreal` layer, verified by grep to contain the cited routes; the LoadAsset body was verified line-by-line in the PORT file itself.

## Deliverables

- `CLAIM.json` (written first, updated with findings)
- `SPAWN-READINESS.md` (item-DB map, per-entry resolution chains, LoadAsset gate analysis, first-spawn recommendation with prerequisites, failure-mode log table)
- `REPORT.md` (this file, written last)
