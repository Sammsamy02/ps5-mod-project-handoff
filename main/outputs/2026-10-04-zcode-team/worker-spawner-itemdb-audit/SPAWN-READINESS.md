# SPAWN-READINESS.md — TownfallItemSpawner item DB + class-resolution audit

worker-spawner-itemdb-audit, 2026-10-04. No console contact; nothing was executed on target.

Cited files (short forms used below):
- **IS** = staged mod `outputs/2026-10-04-zcode-team/worker-loadlist-final/final-package/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` (21415 B, 746 lines; byte-identical to the bundle sha256 f586043a…658 per `worker-itemspawner-lua-audit/LUA-API-MATRIX.md:5`).
- **PORT** = composed console runtime `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss` (per `LUA-API-MATRIX.md:11`).
- **BIND** = its binding layer `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal`.
- **CS** = `outputs/2026-10-04-zcode-team/worker-checkpoint-reconcile/CURRENT-STATE.md`.

---

## 1. Where the item list comes from

The menu list is built at runtime by `scanItems()` (IS:145-196) from THREE merged sources, all behind one hard prerequisite:

1. **Live game database (hard gate)**: `database()` = `FindFirstOf("ItemDatabaseSubsystem")` (IS:63-69). If absent it throws `"Item database not ready. Load a save first."` (IS:66). Because `items = found` is only assigned at the END of the pcall body (IS:185), a DB failure leaves the menu EMPTY even though the static entries were already collected — the DB must be found before ANY item (including the static ones) appears. Status then reads `Could not build pickup list. Load a save first.` (IS:193).

2. **Embedded static table `extraPickupClasses` (IS:72-79)** — 6 hardcoded cooked actor paths, added unconditionally (IS:152-158), `id=""`:

   | Display name | Path (IS line) |
   |---|---|
   | Axe | `/Game/Townfall/AlwaysCook/Items/Weapon_Items/BP_Weapon_Axe` (IS:73) |
   | Pistol3000 | `/Game/Townfall/AlwaysCook/Items/Weapon_Items/BP_Weapon_Pistol3000` (IS:74) |
   | Antibiotics | `/Game/Townfall/AlwaysCook/Items/Collectable_Items/BP_Item_Antibiotics` (IS:75) |
   | Empty IV Bag | `/Game/Townfall/AlwaysCook/Items/Collectable_Items/BP_Item_Empty_IV_Bag` (IS:76) |
   | Filled IV Bag | `/Game/Townfall/AlwaysCook/Items/Collectable_Items/BP_Item_Filled_IV_Bag` (IS:77) |
   | Enhanced IV Bag | `/Game/Townfall/AlwaysCook/Items/Collectable_Items/BP_Item_Enhanced_IV_Bag` (IS:78) |

   Entry shape: `{name, id, path}` — display name, game item id (empty for extras), actor class path (package path, no `.Asset_C` suffix yet).

3. **Deprecated enum table `legacyItems` (IS:83-109)** — 24 entries `{name, value}` (Pipe=4 … AA Batteries=212). Each is live-resolved at scan time: `db:GetItemIDFromDeprecatedEnum(entry.value)` (IS:163) → if a non-None id comes back, `db:GetInteractablePickupIDActorPath(id)` (IS:168) → kept ONLY if that returns a real path (IS:166-171). Both are UFunction calls on the game subsystem — engine-backed, currently PRESENT-BUT-GATED per `LUA-API-MATRIX.md:46`. Paths for these entries are authored by the game itself at scan time (engine-canonical strings).

## 2. Resolution chain per entry (spawnSelected, IS:215-348)

1. Player/transform: pawn + controller (IS:227-231), position + yaw (IS:233-238); ground `LineTraceSingle` via `/Script/Engine.Default__KismetSystemLibrary` CDO (IS:241-258) with out-param `hit` (IS:264-271) — highest-risk reflected feature (`LUA-API-MATRIX.md:47`).
2. `normalizeClassPath(entry.path)` (IS:280; def IS:122-136): strip `'...'` quoting (IS:124), append `.<AssetName>` if no dot (IS:126-130), append `_C` if missing (IS:132-134). Extras become e.g. `/Game/Townfall/AlwaysCook/Items/Collectable_Items/BP_Item_Antibiotics.BP_Item_Antibiotics_C`.
3. **Class resolution**: `StaticFindObject(path)` (IS:283) → if invalid, `LoadAsset(path)` (IS:285) → retry `StaticFindObject(path)` (IS:286) → if still invalid, error `Pickup class could not be loaded: <path>` (IS:289-291). Note: LoadAsset returns THREE values `(object, was_asset_found, did_asset_load)` (PORT LuaMod.cpp:3662-3664) and the mod DISCARDS all three (IS:285) — it cannot distinguish "asset not in registry" from "found but not loaded".
4. Spawn: `GameplayStatics:BeginDeferredActorSpawnFromClass(pawn, pickupClass, transform, 2, pawn, 0)` (IS:294-297) + `FinishSpawningActor` (IS:303); invalid actor → error (IS:299-301).
5. Post-spawn validation (all must pass): `PickupInteractable` component exists (IS:306-309; lookup IS:198-213 incl. `/Script/Townfall.PickupInteractable` IS:208), `PickupInventoryItem.ItemId` non-empty (IS:311-318), and for DB-resolved entries `actualID == entry.id` (IS:321-323; extras skip the match). Then `PickupAmount`/`bAutoDestroyOnPickUp` writes, `SetEnabled(true)`, visibility + collision (IS:325-330), success banner (IS:334-335).

## 3. LoadAsset on the composed runtime — status and gates

**Registered, engine-backed by reflection, never executed live.** PORT LuaMod.cpp:3618 registers `LoadAsset`; implementation :3618-3665. This matches `LUA-API-MATRIX.md:32` ("IMPLEMENTED — PORT/src/Mod/LuaMod.cpp:3618 (UAssetRegistry route…); Fallback only").

Exact chain and gates, in order:

1. **Game-thread only**: throws `Function 'LoadAsset' can only be called from within the game thread` (PORT LuaMod.cpp:3626). The mod only calls it inside `ExecuteInGameThread` closures — satisfied when keybind delivery works at all.
2. **Asset registry acquisition**: `UAssetRegistryHelpers::GetAssetRegistry()` (PORT LuaMod.cpp:3635) is a `StaticReflectedFunction` bound to `/Script/AssetRegistry.AssetRegistryHelpers:GetAssetRegistry` (BIND include/Unreal/UAssetRegistryHelpers.hpp:21) invoked via `Owner::Self->ProcessEvent(Function, &params)` (BIND include/Unreal/ReflectedFunction.hpp:34) on the class CDO resolved from GObjects. **If the returned interface pointer is null → LOUD throw** `Did not load assets because asset_registry was nullptr` (PORT LuaMod.cpp:3636-3639).
3. **Registry lookup**: `asset_registry->GetAssetByObjectPath(FName)` (PORT LuaMod.cpp:3644) is ALSO a reflected function (`/Script/AssetRegistry.AssetRegistry:GetAssetByObjectPath`, BIND include/Unreal/UAssetRegistry.hpp:18). **If that UFunction cannot be resolved, it silently returns an empty `FAssetData`** (BIND src/UAssetRegistry.cpp:38-40) → `was_asset_found=false` → LoadAsset returns `(nil, false, false)` with NO log line (the only prints are on success/found, PORT LuaMod.cpp:~3650-3659). **This is the silent-nil failure mode.**
4. **Actual load**: on a registry hit, `UAssetRegistryHelpers::GetAsset(asset_data)` (PORT LuaMod.cpp:3648; BIND UAssetRegistryHelpers.hpp:22,48) — the synchronous load; prints `Asset loaded` or `Asset was found but not loaded, could be a package`.

What this means on target (UE 5.6.1 SHIPPING, STATS OFF — CS:6):

- The entire LoadAsset path is **three ProcessEvent invocations on reflected functions**, so it inherits every unproven prerequisite of the reflection layer (GObjects walk — live-validated as GUObjectArray 0x97ba110 / 71,959 objects read+write, CS:7; FName resolution for `/Game/...` paths — FName ctor lead 0x63d590 exists but the FC-1 in-game verdict is pending, CS:10 + `worker-fname-runtime-probe/REPORT.md:64`; param marshalling — PRESENT-BUT-GATED, `LUA-API-MATRIX.md:44-50`).
- **Layout risk specific to LoadAsset**: `GetAsset_Params` assumes `sizeof(FAssetData) == 0x50` (BIND include/Unreal/UAssetRegistryHelpers.hpp:31-36), and the header itself warns the FAssetData struct "needs to be put into a VC [versioned container]" because Epic changed unreflected members across versions (BIND include/Unreal/FAssetData.hpp:34-39). A wrong width would corrupt the call, not throw.
- **Registry population**: whether the game's SHIPPING runtime asset registry indexes `/Game/Townfall/AlwaysCook/**` (cooked AssetRegistry.bin in the mounted pak vs loaded-packages-only) is **unevidenced**. If not indexed, LoadAsset is a no-op returning `(nil,false,false)` and the mod's StaticFindObject retry fails with the loud mod-side line.
- **Failure semantics summary**: registry missing → loud throw (caught by the mod's pcall, IS:226/340-342); asset not indexed → silent nil then the mod's own loud `Pickup class could not be loaded: …` (IS:290). A `nullptr` UFunction in the binding layer can additionally hit BIND UAssetRegistry.cpp:40's silent empty return.

## 4. Fallback inventory — what can spawn WITHOUT the DB / without asset loading

- **No zero-asset-load entry exists.** The menu spawns only `/Game/...` pickup blueprints. There is no generic `/Script/Engine` actor entry, and the post-spawn gates (PickupInteractable component + valid `PickupInventoryItem.ItemId`, IS:306-318) would REJECT any generic engine actor by design. `/Script/...` classes are used only for infrastructure (KismetSystemLibrary IS:241, GameplayStatics IS:294, UMG IS:455-560, `/Script/Townfall.PickupInteractable` IS:208).
- **The 6 extras remove the per-entry DB dependency, not the DB prerequisite**: they resolve without `GetItemIDFromDeprecatedEnum`/`GetInteractablePickupIDActorPath` (IS:152-158) but the menu only fills at all if `ItemDatabaseSubsystem` is found (IS:147 + IS:185 assignment ordering).
- **Single highest first-try probability: `Antibiotics`** (IS:75, normalized `/Game/Townfall/AlwaysCook/Items/Collectable_Items/BP_Item_Antibiotics.BP_Item_Antibiotics_C`).

  Exact resolution requirements: (a) menu scan completed (DB found); (b) FName/GObjects path resolution live; (c) the BP class already resident in GObjects — LoadAsset is not invoked if StaticFindObject hits (IS:283-287); (d) ground trace hit with normal Z ≥ 0.5 (IS:260-271); (e) spawned actor exposes PickupInteractable with a non-empty ItemId (no id-match required for extras, IS:321). Tie-break rationale: all 6 extras share an identical chain, and Antibiotics is the only entry whose full normalize→find→spawn→configure→banner path was exercised end-to-end (`worker-lua-mod-smoke/harness/PHASE-LOGS/f7-save-dump.txt:618` — StaticFindObject of the normalized path succeeded first-try, no LoadAsset needed; `:47` — `Spawned Antibiotics x1 ID=DeprecatedItem`), so its script-level plumbing has the least untested surface. Residency on the PS5 build is, however, **unevidenced** — the first real attempt is itself the cheapest evidence generator. Second choice: any legacy item that survives the scan (a DB-issued path is game-vouched and carries the matching id, eliminating the mismatch gate IS:321-323); third: Axe (IS:73).

## 5. Cross-check against in-game evidence

Evidence bank searched: checkpoint records (`worker-checkpoint-reconcile/*`), in-game probe records (`worker-ingame-probe`, `-v2`, `-v3`, `worker-supplier-ingame-update`, `worker-ingame-log-parser`), GUA validation records (`zcode_anchor_validation_RESULTS` / `zcode_igf6_RESULTS` summaries via CS:7), acceptance docs (`worker-acceptance-evidence`, `worker-real-input`), and the mod census.

- **What in-game evidence EXISTS**: engine identity 5.6.1 SHIPPING (CS:6); GUObjectArray live at 0x97ba110 with 71,959 objects, read+write (CS:7); map/module scan (10,047 records / 8,685 spans, CS:9); FName ctor candidate + materialized call sites, in-game verdict pending (CS:10). **None of these records contains a single object or class NAME.**
- **Mod-referenced names — all UNEVIDENCED against our runtime**: `ItemDatabaseSubsystem` (IS:64), `/Script/Townfall.PickupInteractable` (IS:208), `PickupInventoryItem.ItemId` / `PickupAmount` / `bAutoDestroyOnPickUp` (IS:313,325,326), all six `/Game/Townfall/AlwaysCook/...` paths (IS:73-78), and the 24 deprecated enum values (IS:83-109). A repo-wide search finds these strings only in copies of the mod itself, audit documents about it, and intake fixtures.
- **Closest provenance**: `worker-modreqs/MOD-CENSUS.md:119,126` documents the SAME names (`ItemDatabaseSubsystem`, `BP_Bill_C`, `WBP_SimonOS_C`, `/Script/Townfall.TownfallPlayerCharacter`, …) in the retained Windows-proxy intake of this same mod/game (Nexus mod 95 for Silent Hill Townfall, MOD-CENSUS.md:8-9) — PC-build consistency that makes the names *plausible*, not *evidenced*, on the PS5 build.
- **Verdict**: every one of the 30 menu entries (6 static + 24 legacy) is evidence-backed ZERO on our runtime; entry plausibility rests entirely on PC-build provenance. The acceptance ladder treats "valid object query → harmless reflected access" as rungs 2-3 with rungs still pending (CS:12) — consistent with this gap.

## 6. Prerequisite chain + failure-mode log signatures for the first spawn

Ordered prerequisites (each masked by the one above it): runtime loaded → `ExecuteInGameThread` delivered (game-thread identity published; refusal otherwise — smoke REPORT.md:26-33) → `FindFirstOf("PlayerController")` + Pawn (IS:21,26-41) → `FindFirstOf("ItemDatabaseSubsystem")` (IS:147) → FName/GObjects `/Game` path resolution (IS:283) → [only if not resident] LoadAsset (IS:285) → LineTraceSingle out-params (IS:245-269) → deferred spawn + finish (IS:295,303) → pickup component + id gates (IS:306-323) → property writes (IS:325-330).

Every spawn failure surfaces as two lines: status string + `[TownfallItemSpawner] Failed at <stage>. See UE4SS.log. <err>` (IS:341-342), with the partial actor destroyed (IS:344-346).

| Failure mode | stage | Exact UE4SS.log signature | Source |
|---|---|---|---|
| Asset registry missing | load pickup class | `Failed at load pickup class. See UE4SS.log. Did not load assets because asset_registry was nullptr` | PORT LuaMod.cpp:3638 + IS:341 |
| Asset not indexed / class not found | load pickup class | `Failed at load pickup class. See UE4SS.log. Pickup class could not be loaded: /Game/...._C` (LoadAsset itself prints NOTHING — silent nil) | IS:289-291; PORT LuaMod.cpp:3644-3664 |
| Engine helper class missing | (before stage rename) | `Could not find class: <path>` | IS:45 |
| Spawn refused / invalid actor | spawn actor | `Failed at spawn actor. See UE4SS.log. Actor creation returned invalid.` | IS:299-301 |
| No ground / steep surface | ground trace | `Failed at ground trace. See UE4SS.log. No ground ahead. Move to an open, flat area.` / `… Surface too steep.` | IS:260-262, 269-271 |
| No pickup component | configure pickup | `Failed at configure pickup. See UE4SS.log. Spawned actor has no PickupInteractable.` | IS:306-309 |
| Missing inventory id | configure pickup | `… Spawned pickup has no valid inventory ID.` | IS:316-318 |
| DB/id mismatch (DB entries only) | configure pickup | `… Pickup mismatch: expected <id>, got <id>` | IS:321-323 |
| DB absent at scan | (menu) | `Scan failed: Item database not ready. Load a save first.` + status `Could not build pickup list. Load a save first.` — menu EMPTY (even the 6 extras) | IS:66,193-194,185 |
| Keybind delivery gate | (nothing runs) | callback dies at `attempt to call a nil value (global 'ExecuteInGameThread')` (host-proven) or port refusal log — no mod line at all | smoke REPORT.md:26-33; IS:695 |
| SUCCESS | — | `[TownfallItemSpawner] Spawned <name> x<N> ID=<id>` (+ on-screen status, IS:334) | IS:334-335 |

## 7. Bottom line

The item DB is a hybrid (embedded table + live `ItemDatabaseSubsystem` queries) whose per-entry resolution is `StaticFindObject → LoadAsset → StaticFindObject`, but (a) the MENU requires the live DB to appear at all, (b) LoadAsset is fully reflection/ProcessEvent-backed with a loud registry-null throw and a silent nil when the asset is not indexed, with an unproven 0x50 FAssetData layout on 5.6.1, and (c) no entry — static or dynamic — has any in-game name evidence on our runtime. Recommended first spawn: **Antibiotics**, in a loaded save, open flat ground, expecting one of the exact log signatures above as the next evidence datum.
