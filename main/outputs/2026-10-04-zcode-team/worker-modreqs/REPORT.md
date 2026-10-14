# REPORT.md — worker-modreqs (slot 9), representative-mod requirements census

Date: 2026-10-04. Output area: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-modreqs/`
(CLAIM.json, MOD-CENSUS.md, RUNTIME-REQUIREMENTS.md, SERVICES-BACKLOG.md).

## Method

1. Read the bounded handoff set: `outputs/ZCODE-HANDOFF-2026-10-04/START_HERE.txt`, `RUNTIME_MAP.txt`,
   `TEAM_PLAN.txt` (needed to name worker-1/2/3 dependency owners), and the census corpus
   `outputs/2026-10-04-zcode-team/worker-6-containers/evidence/zip-roundtrip.json` (8/8 original ZIPs present,
   all roundtrip_pass).
2. Read ONLY the eight named entries of `.superpowers/native-observations/transport-experiments-progress.json`
   (python json keys: item_spawner_trial, ue4ss_payload_direction, portable_lua_callback_progress,
   native_lua_mod_path_lifetime, portable_pak_base_mod_union, windows_image_preparation,
   ue4ss_current_runtime_readiness, representative_family_milestones). 333 top-level keys untouched otherwise.
3. Located retained extracted mod sources via targeted
   `rg --hidden --no-ignore -l 'RegisterKeyBind|FindFirstOf|FindFunction|StaticClass|GetAllActors' … -g '*.lua'`
   and directory-specific `find`. Verified the Item Spawner member by SHA-256 re-hash
   (f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658 == checkpoint value).
4. Static analysis only: full numbered read of Item Spawner (746 lines) and infinite-stamina (106 lines);
   pattern-with-line-number extraction (`rg -n`) for mod-menu (3198 lines) and minimap (4271 lines);
   per-mod grep of the eight ue4ss example mods and the UEHelpers prerequisite surface.
5. DLL import evidence: parsed the frozen llvm-readobj outputs
   (`.superpowers/native-dependency-spike/windows-image-original-inputs-1/final-1/{dwmapi.dll,UE4SS.dll}.llvm.txt`)
   with awk over `Import {}` blocks, cross-checked against
   `outputs/2026-10-04-windows-image/original-input-comparison.json` and
   `outputs/2026-10-04-pe-dependency-runtime/RETAINED-IMPORT-COVERAGE.json`.
6. No mod code or game asset was executed; no network; no console contact; nothing outside the output directory
   was written; the checkpoint and all sources/archives/receipts were left untouched.

## Key findings

1. **The Item Spawner source IS retained intact** — `.superpowers/native-observations/item-spawner-real-mod-intake-1/member-00000000.bin`
   (zip member `TownfallItemSpawner/Scripts/main.lua`), hash re-verified this session. The historical F7 failure
   line in the checkpoint (main.lua:695 `ExecuteInGameThread` nil) matches the retained text exactly (line 695
   inside the `RegisterKeyBind(Key.F7, …)` block at 694–696). Six keybinds at 694/698/703/708/713/722 all wrap
   their bodies in `ExecuteInGameThread` — that one global is the first hard gate.
2. The first-visible-milestone intersection (RUNTIME-REQUIREMENTS.md) is deliberately narrow: Item Spawner uses
   NO recurring scheduler, no NotifyOnNewObject, no LoopAsync — unlike minimap (`LoopInGameThreadWithDelay(1, M.frame)`
   line 4034; `LoopInGameThreadWithDelay(TICK_MS, safeTick)` line 4268) and mod-menu (2429–2444, 3194–3196).
   Beyond the gate it needs: FindFirstOf/StaticFindObject/StaticConstructObject/FName/FText/LoadAsset, reflected
   property read/write incl. nested chains, 12-arg `LineTraceSingle` with a Lua-table out-parameter,
   `BeginDeferredActorSpawnFromClass`/`FinishSpawningActor`, and a full UMG construction/setter family
   (detailed table with line cites in MOD-CENSUS §1).
3. Dependency marking (per TEAM_PLAN): live module supplier = worker-1; validated layouts/callable addresses =
   worker-2; game-thread delivery + reflection path = worker-3 (consumes 1+2); new scope items (LoadAsset, real
   input source, map hooks, out-param marshalling, FName→string live path, spawn/GC ownership, UMG family) =
   missing-new. Host-side already available: module loader, genuine null/invalid UObjects, UEHelpers inventory,
   keybind registry (6/6), ExecuteAsync/ExecuteWithDelay/LoopAsync caller-driven queue.
4. Only **two** real-mod PE DLLs exist in the corpus and both come from `ue4ss-v1.zip` (provenance inputs.json):
   the proxy `dwmapi.dll` (135 imports/13 libs) and `ue4ss/UE4SS.dll` (634 imports/30 libs, static TLS directory
   present). There is NO retained third-party native gameplay DLL mod — recorded as a census gap, not generalized.
5. Service ranking from actual imports (SERVICES-BACKLOG): InitOnce pair (worker-4's named start), mutex +
   WaitForSingleObject(Ex) waits, broad file-I/O family (observed in BOTH DLLs), CRT startup block
   (_initterm/_seh_filter_dll/etc. — gate for real MSVC DLL bodies), static TLS/FLS, module-enumeration/PSAPI,
   virtual memory/mappings, SEH64 unwind, console, MSVCP140 stdlib. Honest negatives: **events, semaphores and
   version APIs are NOT observed in any retained mod import** — do not build them next on this census's strength.
6. Current exact-name coverage of the census is 5/135 (dwmapi) and 14/634 (UE4SS) import slots
   (RETAINED-IMPORT-COVERAGE.json); the new sync slice's fixture-only APIs are separated from census-observed
   ones per the CRITICAL.md note.

## Assumptions

- "Representative mods" = the six named in the task if retained + retained native DLLs; movement-speed turned out
  to be an IoStore PAK mod (not INI), and the only retained INI evidence is the performance-pak's Windows
  `Engine.ini` fragment (`[SystemSettings] r.FidelityFX.FI.Enabled=1`, `r.FidelityFX.FSR3.UseNativeDX12=1`) —
  family milestone "ini: install/launch demonstrated" refers to that earlier install, whose PS5 config-path
  adaptation remains open.
- Line citations refer to the retained on-disk copies hash-pinned in CLAIM.json; the Item Spawner `.bin` file is
  plain ASCII Lua (verified by od) despite its extension.
- Worker-3 was added as a marker (the task offered worker-1/worker-2/missing-new) because TEAM_PLAN assigns the
  game-thread/Lua delivery adapter to worker-3; this makes the mapping truthful rather than a new scope claim.
- Ranking priorities P1–P4 in SERVICES-BACKLOG are derived from census observation + TEAM_PLAN worker-4 direction,
  not from any executed DLL.

## Gaps / limitations

- No retained third-party native DLL mod; no Skyrim/other-engine corpus; ReadMe texts of movement-speed and
  performance zips not extracted (hashes recorded in their intake receipts).
- `.ucas/.utoc` container contents were not decoded (worker-6 territory; static bytes only here).
- The 3198-line mod-menu and 4271-line minimap scripts were API-inventoried by targeted pattern extraction, not
  full reads; any API outside the searched pattern set (RegisterKeyBind*, Execute*, Find*, Static*, LoadAsset,
  FName/FText, IsValid, Get*/NotifyOn*/require, Loop*, RegisterLoadMap*) could be missed. The two fully-read
  scripts (Item Spawner, infinite-stamina) carry complete inventories.
- Whether `GetEngine`-style globals beyond the searched set appear in mod-menu/minimap was not exhaustively
  verified (e.g. `GetUnrealEnv` was searched and not found).

## Bottom line for the controller

The Item Spawner milestone is blocked first by `ExecuteInGameThread` (worker-3 adapter over worker-1 live facts),
then by the reflection/struct/out-param/UMG services needing worker-2 layouts; the script itself is fully
retained and needs zero per-mod editing. The Windows-service next slice should be InitOnce → mutex+waits →
file I/O → CRT startup, with events/semaphores/version APIs explicitly deferred as not census-observed.
