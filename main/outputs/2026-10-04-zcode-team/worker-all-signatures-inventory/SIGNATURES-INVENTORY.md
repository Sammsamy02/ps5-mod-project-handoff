# SIGNATURES-INVENTORY — every AOB/scan/binding dependency on startup -> engine-ready -> mods-started -> first-F7

Worker: worker-all-signatures-inventory, 2026-10-04. Static source reading only; no console
contact; no nested subagents; nothing executed. Every claim cites `file:line`. Workspace root
`/WORKSPACE/.codex/worktrees/translation-core/what` is abbreviated `W:`.

Provenance: the composed-runtime source copies of both Signatures.cpp files are byte-identical
across all trees (md5 `2ee019cd…` ue4ss, `0b07233c…` unreal), so citations use the canonical
reconstruction `W:.superpowers/native-dependency-spike/native-coherent-runtime-1/source-reconstruction-final-4/`
(abbreviated `SRC:`). The consumed staged initializer is
`W:outputs/2026-10-04-zcode-team/worker-query-s2-arm/staging/UnrealInitializer.cpp` (abbreviated
`SINI:`), the tree SLOTS-FEASIBILITY names as staged. Scan-source stack:
`ue4ss-native-tick-file-owner-1` compiled against `ue4ss-native-ps-scan-provider-1/include`;
collector/caller: `ue4ss-native-mapped-candidate-selection-1`. These provenance lines follow
`W:outputs/2026-10-04-zcode-team/worker-required-slots-feasibility/SLOTS-FEASIBILITY.md:10-15`.

Verdict vocabulary: `bound-by-seed` (the seed-table/initializer proposal work covers this
dependency), `dead-needs-remedy` (exec-dead as composed and the seed work does NOT cover it),
`works-data-side` (satisfied without reading/executing game code), `not-on-path` (unreachable,
absent, or harmlessly inert on this path).

---

## 0. Executive picture

There are exactly **three binding pipelines** in the composed runtime, plus **settings/layout
data**:

1. **ps_scan arm** (Rust mapped-image resolvers behind `ps_scan`, `SINI:571`; ABI
   `W:outputs/2026-10-04-zcode-team/worker-composition-5-rebuild/staging/lineage-overlay-1/PsScanAbi.h:33-67`).
   13 config slots (`PsScanAbi.h:24-38`), 7 genuine providers (`provider_slots[]{6,0,2,1,3,4,10}`,
   `ue4ss-native-mapped-candidate-selection-1/src/OwnedMappedResolverCollector.cpp:39`),
   required mask `0x5d` = slots {0,2,3,4,6}
   (`OwnedMappedResolverCollector.hpp:25`, per-bit decomposition in SLOTS-FEASIBILITY §0).
   **Structurally dead end to end on this target** (G0 facts, G1 construction, G2 Rust view gate,
   G3 measured anchors, G4 admission — SLOTS-FEASIBILITY §0, verdict at :21-68).
2. **Lua AOB override passes** (`DoScan` -> `SinglePassScanner::start_scan` over
   `config.ModuleSource->ReadableRanges()`, `SRC:port/unreal/src/Signatures.cpp:78-84,143-149`).
   Driven by `UE4SS_Signatures/*.lua` scripts; **none are shipped** in any staged root (search of
   `worker-ue4ss-settings/staged`, `worker-loadlist-final/final-package`,
   `worker-settings-bundle-merge/staged-package` returned zero `UE4SS_Signatures` files), so all
   12 `ScanOverrides` are empty and both passes are inert as composed.
3. **Non-scan binders**: live-object vtable walks (`GET_ADDRESS_OF_UNREAL_VIRTUAL`,
   `SRC:port/unreal/include/Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtualBaseVC.hpp:104-134`),
   reflection-based location (KismetStringLibrary, `SINI:1075-1124`), layout data files
   (`MemberVariableLayout.ini` / `VTableLayout.ini`, shipped in
   `worker-loadlist-final/final-package/ue4ss/`), and settings forces
   (`EngineVersionOverride`/`DebugBuild`/`Stats` ini keys,
   `SRC:port/ue4ss/src/SettingsManager.cpp:171-187`).

Everything exec-side (pattern anchors, xrefs, disassembly evidence, detour patches) is dead or
unproven against the trusted exec-only/unreadable text record
`[0x400000,0x787c000)` prot 0x4 (SLOTS-FEASIBILITY §1 table; §6 non-claims).

---

## 1. STAGE A — startup (module_start -> bridge Stage 2 scan-source construction)

| # | Dependency | Where it scans / what it binds | Consumer on path | Scan target | Verdict |
|---|---|---|---|---|---|
| A1 | Owned mapped **scan-source construction** | `OwnedMappedScanSource` ctor eagerly builds `OwnedMappedImageCaller` (`ue4ss-native-tick-file-owner-1/src/OwnedMappedScanSource.cpp:10`; provider copy `ue4ss-native-ps-scan-provider-1/src/OwnedMappedScanSource.cpp:6-7`), which throws `"Mapped text base must be registered readable executable bytes"` unless `permits_code(text_base)` (`OwnedMappedImageCaller.hpp:57`; `permits` false for 0 at `:33`); bridge registers R-only spans (`worker-module-start-wiring/runtime_start_bridge_r11.cpp:483`; "Exec-only text is NOT registered readable" `:449-450`) | `MakeNativeProgramContext` at Stage 2 (`runtime_start_bridge_r11.cpp:623-624`) -> `MRS_ERR_CONTEXT` (`:658-661`) — startup dies here before any scan | exec (needs X-flagged text_base) | **dead-needs-remedy** — G0+G1; seed diff alone does not defuse this; the remedy must make scan-source construction lazy/skippable when seeds are armed (SLOTS-FEASIBILITY §4c note at :350-352) |
| A2 | **ps_scan loop** itself | `while(true) ::ps_scan(&ctx,&results)` (`SINI:610-633`), throw `"PS scan timed out"` at `:630` after `SecondsToScanBeforeGivingUp=120` (staged ini `worker-ue4ss-settings/staged/ue4ss/UE4SS-settings.ini:11`) | feeds every slot binding in §2 | exec (Rust view gate `mapped_image.rs:48-52`, `mapped_api.rs:46`) | **dead-needs-remedy** — seed diff gates the four required flags false (`SINI:585-589`) and skips/tolerates the arm; `unavailable_owned_mapped` clears only REQUESTED fields (`OwnedMappedResolverCollector.cpp:24-29`) so seeded results survive (SLOTS-FEASIBILITY §4b, :322-327) |

---

## 2. STAGE B — init-thread: `ScanGame()` (`SINI:576-927`), all 13 ps_scan slots

Config derivation `SINI:585-596`; note `ftext_fstring` is **never assigned** (defaults false;
struct field `SINI:525`, result field `:558`). Consumers below cite the exact assignment lines.

| Slot | Field (PsScanAbi.h:24-38 order) | Consumer (binding) | Scan target class | Verdict as composed |
|---|---|---|---|---|
| 0 | `g_uobject_array` (REQUIRED) | `UObjectArray::SetupGUObjectArrayAddress` `SINI:893-895` — UObject iteration/GC foundation; `GetNumElements` consumed in init wait `SINI:1060` | data RESULT, code EVIDENCE (0/9 anchors exist, SLOTS-FEASIBILITY §2 slot 0) | dead-as-composed -> **bound-by-seed** (data addr; `permits_pointer_slot` validation is code-read-free, `OwnedMappedImageCaller.hpp:71-73`) |
| 1 | `fname_tostring_fstring` | `FName::ToStringInternal.assign_address` `SINI:808-811`; but `DefaultFNameToStringMethod=Scan` with unbound internal falls back to `Conv_NameToStringInternal` (`SRC:port/unreal/src/NameTypes.cpp:130-146`), located by reflection at `SINI:1093-1110` | code EVIDENCE (xrefs) | **works-data-side** via Conv_NameToString fallback (a CALL into exec text, not a read); slot itself not required |
| 2 | `fname_ctor_wchar` (REQUIRED) | `FName::ConstructorInternal.assign_address` `SINI:885-888`; string ctor silently no-ops unbound (`SRC:port/unreal/include/Unreal/NameTypes.hpp:246-248`, per worker-fname-ctor-audit §1) | code (data head + code body; 5-string intersection impossible at runtime; G4 `permits_code`) | dead-as-composed -> **bound-by-seed** (derived constant `base+0x23d590`, candidate chain + pool corroboration in `worker-fname-ctor-audit/REPORT.md` §3; constant must bypass G4 by direct consumer assignment, SLOTS-FEASIBILITY §4b) — **but see FLAG-1: the verifier (C1) is not covered by the seed** |
| 3 | `gmalloc` (REQUIRED) | `GMalloc = bit_cast<FMalloc**>(results.gmalloc)` `SINI:889-891`; enforced by throw `"PostInitialize: GMalloc is uninitialized."` `SINI:387-389` | data RESULT, code EVIDENCE (0/14 anchors, strings absent; deletefile anchor stranded) | dead-as-composed -> **bound-by-seed** (data pointer slot + optional `[addr]` live-pointer data probe, SLOTS-FEASIBILITY §2 slot 3 / §4b) |
| 4 | `static_construct_object_internal` (REQUIRED) | `UObjectGlobals::SetupStaticConstructObjectInternalAddress` `SINI:812-815`; unbound -> `StaticConstructObject` returns nullptr silently (`SRC:port/unreal/src/UObjectGlobals.cpp:70-77,84-91`) | code (0/4 anchors; readable string anchor stranded at xref step; G4) | dead-as-composed -> **bound-by-seed** (code constant assigned at consumer, bypassing G4; SLOTS-FEASIBILITY §2 slot 4 / §5) |
| 5 | `ftext_fstring` | **no assignment consumer anywhere**; only side effect: `!ctx.config.ftext_fstring` makes the second-pass DoScan gate always true `SINI:907-911` | n/a | **not-on-path** (inert) — see FLAG-4 latent trap |
| 6 | `engine_version` (REQUIRED) | `Version::Major/Minor = results.engine_version` `SINI:718-722` -> `InitializeVersionedContainer()` `SINI:857` -> `PublishStagedNativeLayoutMaps` + S2 layout gate `SINI:863-876` | code + data (42 UTF-16 hits all false positives; store scan needs disassembly; SLOTS-FEASIBILITY §2 slot 6) | dead-as-composed -> **bound-by-seed via settings** (`[EngineVersionOverride] MajorVersion/MinorVersion`, `SRC:port/ue4ss/src/SettingsManager.cpp:171-173`; removes bit 6 from the required mask) — **remedy diff NOT yet in the staged ini** (`worker-ue4ss-settings/staged/ue4ss/UE4SS-settings.ini` has no such section) |
| 7 | `fuobject_hash_tables_get` | `FUObjectHashTables::SetupGetAddress` `SINI:901-904` (result null-guarded) | code | **works-data-side** as graceful degradation: unbound -> `IsAvailable()` false -> GUObjectArray iteration fallback (`SRC:port/unreal/src/UObjectHashTables.cpp:133`; `SRC:port/unreal/src/UnrealInitializer.cpp:110`) |
| 8 | `gnatives` | `GNatives_Internal = results.gnatives` `SINI:897-899` | code | **not-on-path** with loud-soft degradation: startup warning only (`SRC:port/ue4ss/src/UE4SSProgram.cpp:1317-1319`); `FFrame::Step` errors and returns when null (`SRC:port/unreal/src/FFrame.cpp:206-213`); guarded also at `SRC:port/unreal/src/UFunctionStructs.cpp:210,301` — costs BP-step functionality at F7, never blocks startup |
| 9 | `console_manager_singleton` | **zero consumers** (only the logging Lua override, `SRC:port/ue4ss/src/Signatures.cpp:391-394`; console keys zeroed per `validate_native_services`, engine-tick-fallback REPORT §3) | n/a | **not-on-path** |
| 10 | `gameengine_tick` | `UEngine::TickInternal.assign_address` `SINI:816-819`, then resolve block `SINI:1312-1379` and callback registration `SINI:1383-1398` | code EVIDENCE (causeevent xrefs die at code read), vtable fallback data-side | scan arm dead; **binding works-data-side** via vtable fallback (`EngineTickResolveMethod` default Scan falls back, `SRC:port/unreal/include/Unreal/Hooks/Internal/Metadata.hpp` chain + `worker-tick-witness-design/TICK-WITNESS-DESIGN.md:363-371`) — **but see FLAG-2 (jmp probe read) and FLAG-1-class (PLH patch unproven)** |
| 11 | `build_configuration` | Detect switch `SINI:745-760`, `native_non_shipping` `SINI:842-846` | unknown stays 0 -> assumes shipping + warning (`SINI:746-751`) | **works-data-side** (unknown-default + `DebugBuild=` ini force, `SettingsManager.cpp:179-187`) |
| 12 | `stats` | Detect switch `SINI:774-806`, `native_stats` `SINI:847-854` | unknown stays 0 -> warning "explicit Stats setting" (`SINI:785`) | **works-data-side** (unknown-default + `Stats=` ini force) |

### 2b. The two Lua DoScan arms and all 12 signature scripts (`SRC:port/ue4ss/src/Signatures.cpp`, read completely, 507 lines)

`setup_lua_scan_overrides` (`:171-506`) is called from `SRC:port/ue4ss/src/UE4SSProgram.cpp:1017`.
Arms: first pass `SINI:823-829` -> `ScanForGameFunctionsAndData` (`SRC:port/unreal/src/Signatures.cpp:30-98`:
overrides pulled `:47-71`, scan over `ReadableRanges()` `:78-84`); second pass `SINI:907-915` ->
`ScanForGUObjectArray` (`SRC:port/unreal/src/Signatures.cpp:99-156`: overrides pulled `:108-139`,
scan `:143-149`). Every script binds through `scan_from_lua_script` (`:54-169`, native-source
preload path `:69-88`).

| Script | Lines (ue4ss Signatures.cpp) | What it binds | Who consumes the binding | Status on this path |
|---|---|---|---|---|
| `GUObjectArray.lua` | 177-197 | `UObjectArray::SetupGUObjectArrayAddress` `:187` | slot 0 consumer | script absent -> override empty; slot served by seed design |
| `FName_ToString.lua` | 199-220 | `FName::ToStringInternal` `:209` | `ToStringInternalWrapper`, NameTypes.cpp:130 | script absent -> **not-on-path** (Conv_NameToString fallback owns it) |
| `FName_Constructor.lua` | 222-285 | `VerifyFNameConstructor(address)` `:242`, `ConstructorInternal.assign_address` `:255,:264`, ByteProperty probe `:248` | slot 2 consumer + NameTypes.hpp ctors | script absent -> **not-on-path**; binding owned by derived constant |
| `GMalloc.lua` / `FMemory_Free.lua` | 287-311 | `Unreal::GMalloc` `:301` | slot 3 consumer + PostInitialize throw | script absent -> not-on-path |
| `StaticConstructObject.lua` | 313-336 | `SetupStaticConstructObjectInternalAddress` `:325` | slot 4 consumer | script absent -> not-on-path |
| `GUObjectHashTables.lua` | 338-359 | `FUObjectHashTables::SetupGetAddress` `:348` | slot 7 consumer | script absent -> not-on-path (graceful fallback) |
| `GNatives.lua` | 361-381 | `GNatives_Internal` `:371` | slot 8 consumer | script absent -> not-on-path |
| `ConsoleManager.lua` | 383-404 | nothing (logs address only) `:391-394` | none | **not-on-path** (dead upstream too) |
| `ProcessLocalScriptFunction.lua` | 406-430 | `ProcessLocalScriptFunctionInternal` `:419` | built-in derivation skipped when override present `SINI:1516` | script absent -> not-on-path; built-in derivation owns it (FLAG-3) |
| `ProcessInternal.lua` | 432-453 | `ProcessInternalInternal` `:442` | built-in derivation skipped when override present `SINI:1508` | script absent -> not-on-path; built-in derivation owns it (FLAG-3) |
| `CallFunctionByNameWithArguments.lua` | 455-479 | `CallFunctionByNameWithArgumentsInternal` `:468` | FExec/console path (console keys zeroed) | **not-on-path** |
| `GameEngineTick.lua` | 481-505 | `UEngine::TickInternal` `:494` | slot 10 consumer | script absent -> not-on-path; vtable fallback owns it |

Because no scripts ship, every `config.X` in `SINI:585-594` is `true` (slot requested), the
required mask is the full `{0,2,3,4,6}`, and both DoScan arms run with empty override sets
(no-op scans). **As composed today nothing required is bound**; all bindings rest on the
seed/settings remedies landing.

---

## 3. STAGE C — init-thread: `Initialize`/`PostInitialize` (`SINI:1020-1574`)

| # | Dependency | Lines | Scan target | Verdict |
|---|---|---|---|---|
| C1 | **FName constructor verification hook** | `VerifyFNameConstructor` `SINI:929-988`; called `SINI:1044-1047`; `RegisterFNameConstructorPostCallback` `:945-953` -> `EDetourTarget::FNameConstructor` detour (`SRC:port/unreal/include/Unreal/Hooks/Internal/Metadata.hpp:139+` case `:123` in the § listing) -> `AddCallback`->`EnsureHooked` (`DetourInstance.hpp:279-284`) -> `PLH::x64Detour` (`DetourInstance.hpp:166-169`); registration failure throws `SINI:954-958` | code READ+WRITE at ctor | **dead-needs-remedy — FLAG-1, NEW and uncovered by seeds.** worker-fname-ctor-audit §2 already ruled the detour-based verifier "exec-blocked on this target; not usable as the validator". The seed remedy binds the ctor but leaves this throw armed; remedy sketch: integrate the prepared FC-1 validator (`worker-fname-runtime-probe`: mprotect-first 64-byte read at 0x63d590 + 9 call-site windows, sys_mprotect=74, 16 KiB pages) in place of the detour, or gate verification on recorded seed provenance |
| C2 | KismetStringLibrary / Conv_NameToString / CDO location | `SINI:1056-1124` (hard wait loops) | data (GUObjectArray iteration) | **works-data-side** (depends only on slot 0 binding) |
| C3 | `UEngine::LoadMap` vtable walk | `SINI:1303-1311` | data entry read + 16-byte code probe | **works-data-side** address; see FLAG-2 |
| C4 | `UEngine::Tick` resolve + hook | `SINI:1312-1401` | vtable fallback data-side; PLH patch code | binding **works-data-side**; hook install **dead-needs-remedy** (FLAG-1 class: `x64Detour::hook()` disassembles 100 bytes, `MemoryProtector RWX` + `writeEncoding`, `ue4ss-native-polyhook-sources-1/port/polyhook/sources/x64Detour.cpp:322-397`; never exercised in-game in the composed runtime; failure is LOUD — `ERROR_ID` -> throw `SINI:1384-1387` -> `failed` ack, worker-tick-address-validation §5) |
| C5 | `AGameModeBase::InitGameState` vtable | `SINI:1403-1411` | same class as C3 | works-data-side + FLAG-2 |
| C6 | `AActor::BeginPlay` / `EndPlay` / `Tick` vtables | `SINI:1412-1438` | same | works-data-side + FLAG-2 |
| C7 | `UGameViewportClient::Tick` vtable | `SINI:1439-1447` (disabled if instance not found `:1295-1300`) | same | works-data-side + FLAG-2 |
| C8 | `UObject::ProcessEvent` vtable | `SINI:1448-1456` (`bHookUObjectProcessEvent` default true, `SettingsManager.hpp:128`) | same | works-data-side + FLAG-2; detour patch on-demand (mod `RegisterHook`) carries FLAG-1 class risk at F7 |
| C9 | `UObject::ProcessConsoleExec` / `UStruct::Link` vtables | `SINI:1457-1474` | same | works-data-side + FLAG-2; consumers console-only -> effectively not-on-path |
| C10 | **ProcessInternal built-in derivation** | `SINI:1497-1513`: `ExecuteUbergraph` UFunction -> `GetFuncPtr()` (data) -> `resolve_function_address_from_potential_jmp` `:1506` | **code read (16 bytes) + Zydis decode of 164 code bytes** `SINI:1527-1555` | **dead-needs-remedy — FLAG-3, NEW.** All hook flags default true (`SettingsManager.hpp:116-130`), staged ini has no `[Hooks]` section, so this runs at init; on prot-0x4 text the reads fault (crash-shaped, not loud-throw). Remedy: offline derivation (we hold the image; FC-1 window decoder proves feasibility) or `HookProcessInternal/HookProcessLocalScriptFunction=false` in the staged ini |
| C11 | Required-object waits, TypeChecker | `SINI:1127-1241,1476-1495` | data (GUObjectArray) | **works-data-side** (slot 0 dependent) |
| C12 | `MemberVariableLayout.ini` / `VTableLayout.ini` | `SRC:port/ue4ss/src/UE4SSProgram.cpp:901-940,1024`; `SRC:port/unreal/src/NativeLayoutBinding.cpp:132`; files shipped in `worker-loadlist-final/final-package/ue4ss/` | data files | **works-data-side** — this is the port's real "MemberVariable overrides" surface (no AOB anywhere) |
| C13 | GMalloc enforcement | `SINI:387-392` | data | slot-3 dependent; bound-by-seed |
| C14 | Profile/layout publication + S2 gate | `SINI:855-876` (`PublishStagedNativeLayoutMaps`, `townfall-ps5-ue56.profile.json` pins 5.6) | data (validated profile) | **works-data-side** once slot 6 is settings-served |

## 3b. Where the task's expected names actually live (GObjects/GNames/GEngine/GWorld/ProcessEvent/func mirrors)

- **GObjects**: bound as `GUObjectArray` (slot 0 / `GUObjectArray.lua`). No separate GObjects script exists.
- **GNames**: no script, no slot. The FName pool is derived offline: singleton at file-image
  `0x9365ec0` / runtime `0x9765ec0` with structural corroboration
  (`worker-fname-ctor-audit/REPORT.md` §3). Seed-design input only.
- **GEngine / GWorld**: **no extern bindings, no scripts, no consumers** in the composed port
  (grep over `SRC:port/unreal` returned no `extern … GEngine/GWorld` bindings); `FWorldContext`
  exists only as a data header. **not-on-path.**
- **ProcessEvent**: vtable-walk binding (C8), not an AOB script.
- **func mirrors**: `FFrame::Step` GNatives dispatch (C-slot 8) and `UFunction::GetFuncPtr`
  (data-side member read, used at `SINI:1505`) — both data-side consumers.
- **MemberVariable overrides**: `MemberVariableLayout.ini` (C12). No AOB surface.

---

## 4. STAGE D — event-loop: Lua VM + mods start -> `mods-started` ack

| # | Dependency | Evidence | Verdict |
|---|---|---|---|
| D1 | Lua VM bootstrap, module start | event loop -> module start ordering `worker-engine-tick-fallback/REPORT.md` §2 (`UE4SSProgram.cpp:673->1514->1476->1483-1487`); no AOB in Lua bootstrap | **works-data-side** (no scan dependencies); reachability gated by C1/C4 |
| D2 | FName ctor use during mod start (e.g. `FindFirstOf("ItemDatabaseSubsystem")` gate) | `worker-fname-ctor-audit` §1: silent no-op -> nullptr, no diagnostic | **bound-by-seed** (ctor constant); failure mode silent — mitigation is the FC-1 validator, FLAG-1 |

## 5. STAGE E — first-F7 press (Lua hotkey callback execution; `F7` is a Lua-mod keybind, `SRC:port/ue4ss/src/Mod/LuaMod.cpp:553`)

| # | Dependency | Consumer | Scan target | Verdict |
|---|---|---|---|---|
| E1 | FName string ctor | every `Find*` from Lua (NameTypes.hpp:246-267) | code addr (derived) | **bound-by-seed** |
| E2 | `StaticConstructObject` (spawn) | `UObjectGlobals.cpp:70-91`, silent nullptr when unbound | code addr | **bound-by-seed** (slot 4 constant) |
| E3 | GNatives / `FFrame::Step` (BP call stepping) | `FFrame.cpp:206-213` | data ptr table | **not-on-path** as a blocker (graceful error; functional loss only) |
| E4 | ProcessEvent detour (mod `RegisterHook`) | C8 binding; PLH patch on demand | code READ+WRITE | **dead-needs-remedy until FLAG-1-class patch route is proven** (loud failure) |
| E5 | ProcessInternal/ProcessLocalScriptFunction detours (custom events, BP hooks) | `SINI:1497-1557` derivation + on-demand hooks | code READ (+WRITE on demand) | **dead-needs-remedy** (FLAG-3 for derivation; FLAG-1 class for patch) |
| E6 | `CallFunctionByNameWithArguments` / ProcessConsoleExec / ULocalPlayerExec | FExec console chain (console keys zeroed) | n/a | **not-on-path** |
| E7 | `ExecuteInGameThread` EngineTick closures | engine-tick-fallback REPORT §1-2 (closures drain per tick; hard dependence at `UE4SSProgram.cpp:1276-1291`) | hook already live from C4 | **works-data-side** once C4 lands; no new scan dependency |
| E8 | Property access / reflection from Lua | layout maps (C14) + MemberVariableLayout.ini (C12) | data | **works-data-side** |

---

## 6. COMPLETE verdict roll-up (sorted by path stage)

| Stage | Dependency | Verdict |
|---|---|---|
| startup | A1 scan-source construction (G0/G1) | dead-needs-remedy (must be part of the seed diff: lazy/skip) |
| startup | A2 ps_scan loop (G2/G3/G4) | dead-needs-remedy (seed diff gates + skip) |
| init | slot 0 GUObjectArray | dead-as-composed -> bound-by-seed |
| init | slot 1 FName::ToString | works-data-side (Conv_NameToString fallback) |
| init | slot 2 FName ctor | dead-as-composed -> bound-by-seed (binding) / **C1 verifier dead-needs-remedy (FLAG-1)** |
| init | slot 3 GMalloc | dead-as-composed -> bound-by-seed |
| init | slot 4 StaticConstructObject | dead-as-composed -> bound-by-seed |
| init | slot 5 FText | not-on-path (FLAG-4 latent trap) |
| init | slot 6 engine_version | dead-as-composed -> bound-by-seed via settings (diff not yet staged) |
| init | slot 7 GUObjectHashTables | works-data-side (graceful fallback) |
| init | slot 8 GNatives | not-on-path (graceful degradation) |
| init | slot 9 ConsoleManager | not-on-path |
| init | slot 10 GameEngineTick | binding works-data-side (vtable); patch dead-needs-remedy (FLAG-1 class) |
| init | slots 11/12 build_configuration/stats | works-data-side (unknown-defaults + ini forces) |
| init | 12 Lua signature scripts + 2 DoScan arms | not-on-path (absent; inert) |
| init | 10 vtable walks (C3-C9) | works-data-side addresses; FLAG-2 probe read |
| init | ProcessInternal derivation (C10) | dead-needs-remedy (FLAG-3, NEW) |
| init | layout inis + profile publication (C12/C14) | works-data-side |
| event-loop | Lua mod start (D1/D2) | works-data-side; reachability gated by C1/C4 |
| F7 | E1/E2 | bound-by-seed |
| F7 | E3 | not-on-path (graceful) |
| F7 | E4/E5 | dead-needs-remedy until patch route proven |
| F7 | E6 | not-on-path |
| F7 | E7/E8 | works-data-side |

## 7. LOUD FLAGS (new or expanded beyond the known ps_scan/FName/tick triad)

**FLAG-1 — every detour patch is a code READ+WRITE, and the composed runtime has never
installed one on this console.** `TDetourInstance::InstallHook` builds `PLH::x64Detour`
(`DetourInstance.hpp:166-169`); `x64Detour::hook()` disassembles up to 100 bytes at the target,
`MemoryProtector(…, RWX)` then `writeEncoding` writes the jmp
(`ue4ss-native-polyhook-sources-1/port/polyhook/sources/x64Detour.cpp:322-397`). Two
engine-ready-structural hooks arm this at init: FName-constructor verification (C1, throw at
`SINI:954-958`) and EngineTick (C4, throw at `SINI:1384-1387`). Prior in-game probes only ever
*read* text, via mprotect-add-READ (`worker-fname-runtime-probe/REPORT.md`: "mprotect-first …
add READ keep EXEC", SYS_mprotect=74); no evidence exists that PROT_WRITE on text succeeds.
Remedy sketch: (a) one bounded feasibility probe mprotect-RWX on a single text page (FC1-A
pattern); (b) if W refused, a vtable-patch redirect for the tick (vtables live in serviced
writable memory — data-side) replacing PLH for `EDetourTarget::EngineTick`; (c) replace the
FName verifier with the already-prepared FC-1 validator instead of a detour.

**FLAG-2 — all ten vtable walks carry a 16-byte code read.**
`GET_ADDRESS_OF_UNREAL_VIRTUAL`/`OPTIONAL_…` end in
`ASM::resolve_function_address_from_potential_jmp` (`UnrealVirtualBaseVC.hpp:115,133`), which
decodes the first instruction AT the entry (`SRC:port/first/src/ASMHelper/ASMHelper.cpp:9-22,49-69`;
no PROSPERO conditional exists in that file). Under the trusted prot-0x4 record this read faults
— crash-shaped, contradicting the "route B needs no readable text" framing
(TICK-WITNESS-DESIGN.md:363-371) unless text becomes readable. Remedy sketch: offline
no-thunk verification of the predicted entries (we hold the image; the FC-1 window decoder is
the precedent) + a composed PROSPERO ASMHelper variant that skips/guards the probe, or
mprotect-first before the walk.

**FLAG-3 — ProcessInternal/ProcessLocalScriptFunction built-in derivation reads code at init.**
With all hook flags default-true and no overrides, `SINI:1497-1557` reads 164 bytes through
Zydis at the ProcessInternal address (after a 16-byte probe at `:1506`). Never previously
enumerated as a scan dependency; dead on exec-only text and crash-shaped. Remedy: settings
(`HookProcessInternal=false`, `HookProcessLocalScriptFunction=false` in the staged ini) or
offline-derived constants assigned like the seeds.

**FLAG-4 — `ftext_fstring` is never set** (`SINI:585-596` omits it), so the second-pass DoScan
gate `SINI:907-911` is always true. Harmless today (no scripts -> no-op scan), but a latent
trap: shipping any `UE4SS_Signatures/*.lua` later silently re-arms AOB code readers. Remedy:
set `config.ftext_fstring = true;` (or remove the field from the gate) in the seed diff.

**FLAG-5 — nothing is actually staged yet.** `worker-seed-table-derivation/` contains
CLAIM.json only (no SEED-TABLE.json / SEED-INTEGRATION.md); the staged ini lacks
`[EngineVersionOverride]`; the seed initializer diff is proposal-grade. Until those land, the
required mask is {0,2,3,4,6} and startup dies at A1/A2 exactly as SLOTS-FEASIBILITY predicts.

## 8. Explicit non-claims

- No claim that any seed constant, including the derived FName ctor `0x23d590` chain, is
  identity-proven; those remain "identity and ABI unverified" per their deriving workers.
- No claim about runtime behavior never exercised: PLH installs, vtable walk reads, and the
  ProcessInternal derivation have no in-game evidence in the composed runtime.
- The exec-only/unreadable premise is the project's trusted live record (SLOTS-FEASIBILITY §6);
  nothing here re-proves or refutes it — FLAG-1/2/3 are conditional on it and must be
  re-evaluated if a feasibility probe shows text readable.
- All findings are static reading of already-retained sources; no frozen or wiring file was
  modified; no console contact.
