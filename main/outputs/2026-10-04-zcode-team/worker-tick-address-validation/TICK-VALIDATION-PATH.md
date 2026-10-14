# TICK-VALIDATION-PATH.md — derivation chain from resolved base to the address PLH patches

Worker: worker-tick-address-validation, 2026-10-04.
Composed runtime under audit: composition-5 (base object `ue4ss-native-supplier-composition-4/run-3`,
see `outputs/2026-10-04-zcode-team/worker-composition-5-rebuild/build_runtime_composition5.py:25-27`),
entry `runtime_module_entry_r3.c` + bridge `runtime_start_bridge_r3.cpp`
(`build_runtime_composition5.py:60-69`, measured SHAs at :66 and :69).
Header set actually consumed is proven by
`.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-4/run-3/unreal-unrealinitializer.d`
(it resolves `native-coherent-runtime-1/source/port/unreal/include/Unreal/Hooks/Internal/DetourInstance.hpp`
and `.../Unreal/UEngine.hpp`; all file:line citations below are from that exact copy unless noted).

## 0. Starting point: the anchor (given, verified by composition-5 r3)

- `runtime_start_bridge_r3.cpp:113-123` — the two data-window anchors (AES S-box head at
  eboot p_vaddr `0x7dd2a40`, GNU build-id note at `0xbc5f250`), candidate base `kCandidateBase=0x400000`,
  extent `0xbc602c8`.
- `runtime_start_bridge_r3.cpp:131-156` (`window_anchor_verify`): fresh VMMAP via the proven
  `query_vmentry` op, both windows must be inside ONE readable mapping each and byte-compare exactly.
- `runtime_start_bridge_r3.cpp:219-241` — `scan_image_base` wrapper: original header scan keeps first
  shot; on anchor failure (exec-only eboot header page) the window pair plants `chosen_base=0x400000`.
- `runtime_start_bridge_r3.cpp:253` — `options.allow_vm_scan_base_as_module_image_base = true`
  (the explicit opt-in documented in `lmf_to_native_loader_facts.hpp:11-16`), then
  `TryMakeNativeLoaderFactsFromSupplier` at `runtime_start_bridge_r3.cpp:255-257`.
  Result: `NativeLoaderFacts` with `images[MainExe].base = 0x400000`
  (`lmf_to_native_loader_facts.hpp:33-35`; ELF `mapped.image_base` stays 0 by ABI contract, :28-31).

From here the runtime does NOT use a stored Tick constant. The address PLH patches is derived at
run time on the init thread by two independent in-process routes plus a dormant third.

## 1. module_start -> init thread (where derivation runs)

1. `runtime_module_entry_r3.c` -> `modbridge_runtime_start_with_activation`
   (`runtime_start_bridge_r3.cpp:182-333`).
2. Stage 2 builds the context: `MakeNativeProgramContext(std::move(converted.facts))` — the **1-arg**
   overload (`runtime_start_bridge_r3.cpp:277-278`).
3. `src/NativeLoaderContext.cpp:4-6` (consumed unit, run-3 receipt "native-loadercontext"):
   the 1-arg overload forwards `serialized_elf = {}` (empty vector) to the 2-arg overload.
4. `src/NativeLoaderContext.cpp:19`: constructs
   `OwnedMappedScanSource(snapshot, facts.mapped, facts.scan_policy, serialized_elf)`.
   `facts.scan_policy` is the default `CandidateSelectionPolicy::OriginalOnly`
   (`include/NativeLoaderContext.hpp:13`), and the LMF adapter deliberately never changes it
   (`lmf_to_native_loader_facts.hpp:68-69`).
5. `src/OwnedMappedScanSource.cpp:7-12`: with empty `serialized`, `serialized_` stays null.
   The context is stored via `config.NativeScanSource = m_native_context.scanner_source`
   (`ue4ss-native-ps-scan-provider-1/src/UE4SSProgram.cpp:812`).
6. `StartActivatedRuntimeWithAck` (`runtime_start_bridge_r3.cpp:289-291`) ->
   `PrepareActivatedUE4SSRuntime` (`include/ue4ss_backend.hpp:23`) -> `UE4SSBackend`
   (`.../native-coherent-pad-runtime-1/build-1/source/ue4ss_backend.cpp:16-57`) ->
   `RuntimeOwner::start()` spawns the init thread
   (`.../native-coherent-runtime-1/source/bootstrap/runtime_owner.cpp:31-41`, composed copy with ack,
   pinned at `outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-runtime-1/source/bootstrap/runtime_owner.cpp`).
   `module_start` returns `MRS_OK` immediately (`runtime_start_bridge.h:36`); all derivation below
   happens on that init thread inside `program_->init()` (`ue4ss_backend.cpp:59-70`).

## 2. Primary route A: mapped-image reference scan (no ELF file needed)

7. `UE4SSProgram::init()` (`port/ue4ss/src/UE4SSProgram.cpp:600-620`) -> `setup_unreal()` (:930)
   -> `Unreal::UnrealInitializer::Initialize(config)` (:1265) -> `PreInitialize` guards
   (staged `UnrealInitializer.cpp:993-1000`: throws `std::logic_error` if `NativeScanSource` is
   missing or does not retain `ModuleSource`) -> `ScanGame()` (:581-846).
8. `ScanGame` sets `config.gameengine_tick = !UnrealConfig.ScanOverrides.gameengine_tick`
   (staged `:594`) and calls `::ps_scan(&ctx,&results)` (staged `:614`).
9. `PsScanBinding.cpp:3-6`: `ps_scan` -> `scan_owned_source(config.NativeScanSource, ...)`.
10. `src/OwnedMappedScanSource.cpp:13-29` `collect_with_evidence` ->
    `collect_owned_mapped_candidates` -> `collect_owned_mapped`
    (`ue4ss-native-mapped-candidate-selection-1/src/OwnedMappedResolverCollector.cpp:28-103`):
    slot 10 (`gameengine_tick`) is produced by `ps_resolve_mapped_core_v1(&image.view(), id=7, &value)`
    (`provider_slots[]{6,0,2,1,3,4,10}`, collector cpp `:30` + loop `:33-36`).
11. Rust side: `ue4ss-native-scan-snapshot-1/crate/src/mapped_api.rs:50` — `7 => image.resolve_ugameengine_tick()`
    -> `src/game_loop.rs:4-10`: scan UTF-16 `"causeevent="` / `"CAUSEEVENT "` strings, find their
    rip-relative xrefs, map every xref to its enclosing root function (`get_root_function`), then
    `ensure_one` — exactly ONE distinct root function or `ResolveError`.
    This is a reference-anchored scan, NOT a byte signature and NOT an export lookup
    (export resolver is deliberately absent: `lmf_to_native_loader_facts.hpp:65-67`,
    `ModuleSnapshot::FindExport` throws `std::logic_error`).
12. Candidate admission validation (collector cpp, case 0 `:41-51`):
    `value.reserved` must be 0 and the address must satisfy `image.permits_code(value.address)`
    (slot 10 is in the code-permit branch `:45-47`); otherwise the slot is `RejectedAddress` /
    `InvalidResult` and never published. Statuses: 3=NoMatch, 4=Ambiguous (`:48-52`).
13. Slot 10 is OPTIONAL: `MappedRequiredSlots = 0x5d`
    (`include/OwnedMappedResolverCollector.hpp:25`) covers bits {0,2,3,4,6} only
    (g_uobject_array, fname_ctor, gmalloc, static_construct_object_internal, engine_version).
    A missing Tick alone does NOT fail the scan (`succeeded()` at :23).
14. Back in `ScanGame`: `UEngine::TickInternal.assign_address(results.gameengine_tick)`
    (staged `:816-818`). If the scan missed, this assigns NULL — `Function::assign_address(void*)`
    only sets `m_is_ready = func_address` (`port/first/include/Function/Function.hpp:57-64`),
    i.e. nullness is the ONLY gate here.

## 3. Primary route B: live vtable of the GameEngine instance

15. `PostInitialize` (staged `:385`, called at `:1573`) locates a live `UGameEngine`:
    `GetInstanceFromClass("GameEngine", "/Script/Engine.Default__GameEngine")` (instance-first,
    CDO fallback) and THROWS if absent (staged `:1264-1269`).
16. `OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL(UEngine, Tick, GameEngine)`
    (staged `:1315`) is the lambda at
    `port/unreal/include/Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtualBaseVC.hpp:118-131`:
    looks up `"Tick"` in `UEngine::VTableLayoutMap` (engine-version dependent; returns nullptr if
    absent), reads the vtable pointer out of the live object, loads the entry at the layout offset,
    and `ASM::resolve_function_address_from_potential_jmp`.
17. Cross-check + selection (staged `:1315-1378`): vtable vs scan are both logged (`:1318`);
    disagreement only WARNS (`:1321-1323`), it never refuses.
    `UnrealConfig.EngineTickResolveMethod` defaults to `FunctionResolveMethod::Scan`
    (`port/unreal/include/Unreal/UnrealInitializer.hpp:146`), so: prefer scan address, fall back to
    vtable address (staged `:1352-1364`); `VTable` mode would prefer vtable (`:1366-1378`).
    Whichever wins is re-`assign_address`-ed into `UEngine::TickInternal`.

## 4. Dormant route C: serialized-ELF witness (ps_ugameengine_tick_file_witness_v1)

18. `src/OwnedMappedScanSource.cpp:18` — the witness path fires ONLY if ALL hold:
    `serialized_` non-null AND `policy_ == SelectUniqueAfterNoMatch` AND `config.gameengine_tick`
    AND `!image_error` AND slot-10 `status==NoMatch` AND `endpoint_status==3`.
    In the composed r3 runtime BOTH prerequisites are absent: bytes empty (step 3) and policy
    `OriginalOnly` (step 4). An AMBIGUOUS scan (status 4) also never reaches it — only NoMatch/3.
19. When it does fire: `src/OwnedSerializedElfSource.cpp:92-111` `tick()` —
    `ps_ugameengine_tick_file_witness_v1(bytes_.data(), bytes_.size(), ...)` capacity query then fill
    (`:95-99`), header sanity `version==1, flags==1, witness_count<=8192` (`:97`),
    every witness revalidated against the LIVE image under one read lease
    (kind-1 source-bytes memcmp `:102` via `source_matches` `:68-77`; kind-2 RELATIVE64 slot must
    match a parsed R_X86_64_RELATIVE row and the live 8 bytes `:103-106`), then
    `translate(header.linked_tick,1,true)` + `image.permits_code` (`:109`).
20. File-side resolution is `ue4ss-native-tick-witness-1/crate/src/elf_tick.rs`:
    `resolve_with_witness` (`:196-211`) refuses W+X spans, records whole R-X RO load spans as
    SOURCE_BYTES, then `resolve_image` (`:213-230`): requires the strings `"causeevent="`,
    `"CAUSEEVENT "`, `"UEngine::Tick"`, `"Pure virtual not implemented (%s)"`, `"GameEngine"`,
    `"/Script/Engine"`; relocation-backed vftable roots; pure-virtual-stub CFG; constructor CFG with
    `("/Script/Engine","GameEngine")` call-argument constants; vftable table shape (>=12 entries,
    zeroed 16-byte headers); slot-index correlation; >1 candidate = Ambiguous.
    Translation to runtime: `runtime_base_ + (linked - low_)` where `low_` is the lowest
    non-zero-flag PT_LOAD vaddr (`OwnedSerializedElfSource.cpp:34, 62-64`).

## 5. From resolved address to the patch write

21. Registration: `Hook::RegisterEngineTickPreCallback(HookedEngineTick, ...)` (declared
    `port/unreal/include/Unreal/Hooks/Hooks.hpp:241`; PROSPERO call sites staged `:1383-1387` and
    `:1394-1397`). The detour target metadata is `&UEngine::TickInternal` +
    `GlobalConfig.bHookEngineTick` (`port/unreal/include/Unreal/Hooks/Internal/Metadata.hpp:196-204`).
22. `AddCallback` calls `EnsureHooked()` immediately
    (`port/unreal/include/Unreal/Hooks/Internal/DetourInstance.hpp:281-290`; doc :279). So the patch
    happens at registration time on the init thread, before `mods-started`.
23. `EnsureHooked` (`DetourInstance.hpp:232-238`) -> `InstallHook()` (:149-171):
    guards are `PLHDetour` already present (:151), `*bShouldHook` (:154-157), and
    `TargetFunction->is_ready()` (:158-162 — nullness only, Function.hpp:103-105). Then
    `PLH::x64Detour(bit_cast<uint64_t>(TargetFunction->get_function_address()), &StaticHookFn, &Trampoline)`
    (:164-167) — construction only, no write yet.
24. `ActivateHook()` (:174-186) -> `PLH::x64Detour::hook()`
    (`ue4ss-native-owned-default-detour-1/port/x64Detour.cpp:348-545`):
    disassembles 100 bytes at the target (:351), refuses if nothing decodes (:354-357),
    `followJmp` (:359-363), allocates the owned jump island (:365),
    computes the prologue to overwrite (`calcNearestSz` :413-419, "Function too small to hook
    safely!" refuse), `expandProlSelfJmps` (:423-427 refuse), `makeTrampoline` (:433-436 refuse),
    PROSPERO-owned patch: shape-checks the bounded jump (:383-408), `MemoryProtector(m_fnAddress,...,RWX)`
    (:502-503, refuse if `!isGood()`), `mem_copy` of the jmp+nops over the prologue (:505-512) with
    byte-exact rollback on throw (:513-541).

## 6. What makes a bad address REFUSE instead of crash (task 2 answer)

Validation that EXISTS before the write:
- span/permission: `image.permits_code()` on the scan candidate (collector cpp :45-47) and on the
  witness translation (`OwnedSerializedElfSource.cpp:109`); vtable addresses are NOT span-checked.
- uniqueness: `ensure_one` root-function uniqueness in the scan (game_loop.rs:10) and
  candidate uniqueness in the ELF proof (elf_tick.rs:226-229).
- nullness: `is_ready()` (DetourInstance.hpp:158) — an unresolved Tick refuses loudly (below).
- decodability/shape: PLH refuses undecodable, too-small, jmp-tangled, or unprotectable targets
  (x64Detour.cpp :354-357, :413-427, :502-503).

Validation that DOES NOT exist:
- No prologue byte-comparison against a known Tick prologue anywhere in the chain
  (grep across the consumed units: none; the only prologue byte checks are PLH's own decoder).
- No function-boundary check that the resolved address is a function START (PLH even follows
  leading jmps, x64Detour.cpp:359-363, actively tolerating non-start addresses).
- No semantic check that the target IS UGameEngine::Tick: vtable-vs-scan disagreement warns and
  proceeds (staged :1321-1323).
- `Function::assign_address` accepts ANY non-null pointer (Function.hpp:57-64).

LOUD refusal path (null/unready/PLH-refused): `AddCallback` gets `EnsureHooked()==false` ->
`ERROR_ID` (DetourInstance.hpp:284-288) -> staged `:1385-1387` throws
"Native game-thread initializer callback registration failed" -> caught inside
`UE4SSProgram::init()` PROSPERO handlers, error recorded, `shutdown_native_program()`, init returns
(UE4SSProgram.cpp:693-714) -> `UE4SSBackend::initialize` rethrows the stored startup error
(ue4ss_backend.cpp:66-70) -> `RuntimeOwner::initialize` catches and emits
`ack_->failed(1, detail)` + `RuntimeState::Failed` (composed runtime_owner.cpp:112-123; sink emits
the literal state line `"failed"` at :154-174).
NOTE: `mods-started` cannot mask this — `on_engine_initialized` (which emits `engine-ready` +
`mods-started`, ue4ss_backend.cpp:31-45) runs at UE4SSProgram.cpp:652-661, strictly AFTER
`setup_unreal()` returned, i.e. after tick registration succeeded.

SILENT danger path (wrong-but-plausible address): passes nullness + permits_code + PLH decode ->
patch is written -> registration succeeds -> `mods-started` fires. If the patched bytes are hot
game code, the next call crashes the game (no further acks). If they are cold/never-executed code,
the game runs on but no Tick callback ever fires — status stays `mods-started` with a zeroing query
harness counter. A WRONG TICK ADDRESS IS NOT SYSTEMATICALLY REFUSED. Say it loudly: nothing in the
r3 runtime proves the patched address is UGameEngine::Tick; the only real protections are the
scan's string-reference uniqueness and the live vtable entry, plus PLH's structural refusals.
