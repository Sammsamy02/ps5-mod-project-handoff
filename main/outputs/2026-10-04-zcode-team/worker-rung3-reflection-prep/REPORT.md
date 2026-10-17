# REPORT — worker-rung3-reflection-prep (2026-10-04)

Task: define rung 3 (harmless reflected access) for the composition-5 runtime,
trace the reflection READ path in the port tree with file:line, pick the
minimal evidence-clean Lua probe, and write the verification recipe.
Proposal-only; no console contact; no nested subagents; writes only in this
directory.

Deliverables: `CLAIM.json` (written first), `RUNG3-RECIPE.md`, this report.

## 1. What was produced

- **Rung-3 definition** (RUNG3-RECIPE.md section 1): resolve + read-only
  access + harmless termination, for engine 5.6.1 SHIPPING / build
  04bb4330... on prospero-x86_64-sysv (identity from the promoted pair,
  `worker-emit-final-pair/emission-final/MemberVariableLayout.ini`).
- **Full read-path trace** (section 2): Lua `FindFirstOf` / `GetPropertyValue`
  from registration to the guarded dereference, every hop cited file:line.
- **Guard identification** (section 3): four refuse-don't-crash layers, with
  the width-table gate at `NativeLayoutBinding.cpp:87-92` + `:105-116` as the
  staging-time barrier and the generated-accessor missing-key throw as the
  access-time barrier.
- **Probe choice** (section 4): `FindFirstOf("GameEngine")` + `IsValid()` +
  twin-find stability, with the class-name evidence audit that justifies
  "GameEngine" as the ONLY usable class name today.
- **Verification recipe** (sections 5-8): additive `Rung3Probe` Lua mod
  carrier, C1/C2 observables, pass criteria, 11 discriminated failure
  signatures, binding sequencing.

## 2. Key findings

1. **The read path is index-based and does not need FName strings.**
   `FindFirstOf` matches via `FName::Equals` = ComparisonIndex equality only
   (`port/unreal/include/Unreal/NameTypes.hpp:345-348`), and the Lua
   `GetPropertyValue` lookup compares FNames the same way
   (`port/unreal/src/UStruct.cpp:155-168`). The only engine call on the find
   path is the FName string constructor (FC-1 candidate, default `FNAME_Add`,
   `NameTypes.hpp:307`) — for an already-present name like "GameEngine" it is
   a pure find.

2. **The silent-FName hazard.** If `FName::ConstructorInternal` is unresolved,
   `construct_with_string` returns silently (`NameTypes.hpp:241`) leaving
   FName index 0 — the search then harmlessly matches nothing and the mod
   still receives a nullptr-safe invalid userdata (`LuaMod.cpp:1650-1654`).
   A negative find is therefore AMBIGUOUS without same-launch FC-1 probe
   records; the recipe makes FC-1 a binding disambiguator (failure signature
   F1).

3. **The promoted pair is rung-1→2 only.** It stages exactly FUObjectArray
   (10 keys incl. UEP_TotalSize) + TUObjectArray (7 keys) with identity
   5.6.1-0+++UE5 / 04bb4330... / NonShipping=0 / Stats=0. All rung-3 member
   families (UObjectBase/UStruct/UClass/FField/FProperty) are refused by the
   assembler until the batch2 R1-R5 reads land
   (`worker-staged-pair-live/RUNG3-REQUIRED-READS.md:4-9`). Under this pair,
   any reflected accessor call throws the typed missing-key refusal
   (`generated_include/MemberVariableLayout_SrcWrapper_UObjectBase.hpp:42-56`)
   — safe, but unable to advance rung 3. Attempting the probe before the
   extended pair is sequenced work, not a shortcut.

4. **The width table blocks the property-read step today.** Per the 419-entry
   compiled contract, `FField::NamePrivate` / `FField::Next` / `Owner` have NO
   compiled accessor widths (`worker-2-layout-facts/profiles/
   townfall-ps5-ue56.profile.json:447-455`) and widthless members stay skipped
   (`RUNG3-REQUIRED-READS.md:73-75`). Since `UStruct::FindProperty` walks the
   FField `Next` chain, the property-read step cannot work until those widths
   exist in the compiled contract — an additional dependency beyond the staged
   pair. `UObjectBase` is fully widthed (5/5, incl. ClassPrivate 8 /
   NamePrivate 8), so the FIND step is unblocked once its offsets are staged.

5. **Class-name evidence audit (per instruction).** No GUA snapshot record in
   any retained facts/results file lists an object class name: ingame v1/v2/v3
   records cover vmmap/module-list/layout predicates (module-list names are
   empty strings), and the GUA validation read only array-header words
   (`worker-anchor-validation/REPORT.md:75`). The single evidenced class is
   `UGameEngine` — serialized-ELF symbol `ps_ugameengine_tick_file_image` at
   vaddr `0x2fac8c0` (profile `GameEngineTick.gameengine_tick`) plus rung-1
   acceptance (Tick route). Hence `FindFirstOf("GameEngine")`, nothing else.

6. **Read-time offset dereference is unguarded by design — the safety comes
   from the staged gates.** `data = base + GetOffset_Internal()` adds no bounds
   check (`LuaUObject.cpp:2348`); the guard that makes a bad offset refuse
   rather than crash is upstream: width-table staging gate
   (`NativeLayoutBinding.cpp:87-92`, `:105-116`), publication agreement gate
   (`:175-182`), and the missing-key accessor throw. This is exactly the
   property the recipe's failure signatures F4-F7 are built on.

## 3. Evidence locations cited (most load-bearing)

- Port tree: `PORT/ue4ss/src/Mod/LuaMod.cpp:1529,1632`; `PORT/ue4ss/src/
  UE4SSProgram.cpp:896-928,940,1704-1711`; `PORT/ue4ss/src/
  NativeLayoutBinding.cpp:44-59,87-92,105-116,131-164,175-199`;
  `PORT/ue4ss/include/LuaType/LuaUObject.hpp:620-623,761-774,791-840,975-1001`;
  `PORT/ue4ss/src/LuaType/LuaUObject.cpp:598-602,2303-2370`;
  `PORT/unreal/src/UObjectGlobals.cpp:434-504,745-765`;
  `PORT/unreal/src/UObjectArray.cpp:394-396`; `PORT/unreal/src/
  UnrealInitializer.cpp:108-111`; `PORT/unreal/src/UStruct.cpp:155-168`;
  `PORT/unreal/include/Unreal/NameTypes.hpp:228,239-255,307,345-348`;
  `PORT/unreal/src/NameTypes.cpp:18,68-99`;
  `PORT/unreal/include/Unreal/PropertyMacros.hpp:97-124`.
- Facts/artifacts: `worker-emit-final-pair/emission-final/*.ini`;
  `worker-staged-pair-live/RUNG3-REQUIRED-READS.md`;
  `worker-activation-bundle/LAYOUT-GATE.md`;
  `worker-acceptance-evidence/EVIDENCE-SPEC.md` (rung 3 = :233-325);
  `worker-2-layout-facts/profiles/townfall-ps5-ue56.profile.json`
  (:354-378 GameEngineTick, :405-500 widths);
  `worker-settings-bundle-merge/STAGING-COMMANDS.md:22-24,42-60` +
  `staged-package/ue4ss/UE4SS-settings.ini:10`;
  `worker-itemspawner-lua-audit/LUA-API-MATRIX.md:26-28`;
  `outputs/2026-10-04-zcode-controller/console/ingame-v{1,2,3}-*.jsonl`
  (no class names); `worker-anchor-validation/REPORT.md:75`;
  `worker-query-harness/REPORT.md:190`.

## 4. Honest limitations

- No engine or console execution: this is a source trace over retained
  evidence; the probe has never run.
- `FUObjectHashTables::IsAvailable()`'s body and `GetAccessorWidth`'s table
  (`NativeLayoutWidths.hpp`) are not present in this worktree — they are
  supplied by the composition build; the width content is characterized from
  the compiled-port contract in the worker-2 profile, not read from the build
  header.
- `IsValid()`'s listener-map membership test may report false for objects
  found through the non-caching `FindFirstOf` path; the recipe compensates
  with the twin-find stability signal and records both.
- The exact settings-mode (hash vs forced-GUA iteration) the composition-5
  runtime runs under is not re-verified from the live build; the staged
  settings say `bUseUObjectArrayCache=false`.
- Rung-3 remains UNREACHED; nothing here rates any quality score.
