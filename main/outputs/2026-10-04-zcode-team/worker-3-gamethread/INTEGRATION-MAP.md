# INTEGRATION-MAP — Existing UE4SS integration path and the seams for worker-1/worker-2

Authoritative root: `/WORKSPACE/.codex/worktrees/translation-core/what`
(all paths below relative to it, unless absolute)

Purpose: the exact, already-built path from UE4SSProgram construction to the
serialized Tick witness and the activation owner, with the PRECISE seams where
(a) a genuine owning live-module facts supplier (worker-1) and (b) a validated
Townfall UE5.6 layout profile (worker-2) attach — plug-in, not rewrite.

```
[worker-1: live-module supplier]                [worker-2: layout profile]
        |                                               |
        v                                               v
  S1 NativeLoaderFacts                        S2 StageNativeLayoutFiles /
     (MakeNativeProgramContext)                  SetResolvedNativeLayoutAxes /
        |                                        PublishStagedNativeLayoutMaps
        v                                               |
  NativeProgramContext {module_source, scanner_source, mod_source, ...}   |
        |                                               |
        +-------------------+---------------------------+
                            |
   modbridge::native::PrepareDeferredUE4SSRuntime / PrepareActivatedUE4SSRuntime
        |  (UE4SSBackend: policy=Deferred, wraps on_engine_initialized)
        v
   RuntimeOwner.start() -> init thread -> UE4SSProgram::init()
        |
        v
   UE4SSProgram::setup_unreal() -> UnrealInitializer::PreInitialize/Initialize
        |   (refuses missing ModuleSource / scanner-owner mismatch)      |
        |                                                                |
        v                                                                v
   ScanGame() via OwnedMappedScanSource (ps_scan binding)    Version axes + versioned container
        |   (original no-match -> serialized Tick witness)   (S3 evidence route: first genuine
        v                                                   Tick publishes game-thread identity)
   fire_unreal_init_for_cpp_mods -> setup_unreal_properties -> share_lua_functions
        |
        v
   on_engine_initialized (S4 activation: Lua mods started, EngineInitialized observed)
```

## 1. Path stages with exact file+function references

### Stage 1 — Loader facts -> NativeProgramContext (S1 seam entry)

Files:
- `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/NativeLoaderContext.hpp`
- `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/NativeLoaderContext.cpp`
- `outputs/2026-10-04-tick-file-owner/README.md` (owner-facing description)

Function: `RC::Platform::MakeNativeProgramContext(NativeLoaderFacts facts, const std::vector<uint8_t>& serialized_elf) -> NativeProgramContext` (NativeLoaderContext.cpp:7).

What it does today, in order:
1. `if (facts.mapped.format != PS_MAPPED_IMAGE_ELF64) throw std::invalid_argument("Native PS5 program requires caller-supplied ELF64 mapped image facts")` (NativeLoaderContext.cpp:10-11).
2. `Unreal::Platform::ModuleSnapshot::Create(std::move(facts.modules))` — ModuleSnapshot ctor
   (`.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/PlatformModuleSnapshot.hpp`):
   refuses a null `ModuleInput::lifetime`, missing `images[ScanTarget::MainExe]`,
   a monolithic input that names more than MainExe, empty readable_ranges,
   and invalid extents. `find_export` may be absent (FindExport then throws
   "Native module export resolver is not configured" on first use).
3. Fills `NativeProgramContext{module_file_path, game_executable_path, module_source=snapshot, input_source}`.
4. `ValidateNativeProgramContext(context)`
   (`.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/Platform/NativeProgramContext.hpp`):
   absolute/normalized/filename-bearing paths without NUL; `module_source` must
   be present; both paths must name existing regular files (a file-availability
   gate only — "It establishes neither image identity nor a mapping-to-path
   relationship").
5. `context.scanner_source = std::make_shared<const Unreal::Platform::OwnedMappedScanSource>(snapshot, facts.mapped, facts.scan_policy, serialized_elf)` (NativeLoaderContext.cpp:19).

The 1-argument overload delegates with an empty serialized ELF (no witness route).

`NativeLoaderFacts` fields consumed (NativeLoaderContext.hpp:7-14):
- `module_file_path`, `game_executable_path` — caller-bound to the retained snapshot;
- `modules: Unreal::Platform::ModuleInput` — `lifetime` (shared owner retaining ALL
  described bytes, registrations, resolver state and export callback lifetime),
  `monolithic`, `images` (at least `MainExe`: `{base, size, readable_ranges}`),
  `find_export`;
- `mapped: Unreal::Platform::OwnedMappedImageFacts` — `format`, `text_base`,
  `image_base`, `eh_frame`, `eh_frame_hdr`, `pe_exception_directory`
  (`PsMappedExtent`s; MainExe metadata extents must describe supplied actual
  mapped bytes, "missing metadata is not invented");
- `input_source: std::shared_ptr<Input::PlatformInputSource>` (optional);
- `scan_policy: CandidateSelectionPolicy` (default `OriginalOnly`).

Lifetime ownership: the caller-owned `ModuleInput::lifetime` is the single
owner of the mapped bytes; `ModuleSnapshot` and `OwnedMappedScanSource`
retain `shared_ptr<const ModuleSnapshot>` handles to it. Per
checkpoint `native_startup_lifetime`: loaded code references are retained for
process lifetime (capacity 64) — the supplier must not revoke bytes while any
engine callback can still run.

### Stage 2 — Serialized Tick witness (inside the scanner source)

Files:
- `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/OwnedMappedScanSource.hpp`, `OwnedSerializedElfSource.hpp`, `OwnedMappedCandidateSelection.hpp`
- `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/OwnedMappedScanSource.cpp`, `OwnedSerializedElfSource.cpp`
- ps_scan binding: `.superpowers/native-dependency-spike/ue4ss-native-ps-scan-provider-1/src/PsScanBinding.cpp` (`scan_owned_source`)

Data flow: `OwnedMappedScanSource(snapshot, facts, policy, serialized_elf)` holds
`std::shared_ptr<const OwnedSerializedElfSource> serialized_`. During
`collect(...)`/`collect_with_evidence(...)`, after the ORIGINAL mapped resolver
report (verbatim, authority preserved: success/ambiguity/error), under an
explicit `CandidateSelectionPolicy::SelectUniqueAfterNoMatch` (and only after a
genuine original NoMatch / endpoint status 3 without image error),
`OwnedSerializedElfSource::tick(const OwnedMappedImageCaller&, uint64_t& runtime_address) const noexcept -> int32_t`
revalidates every relocation-aware proof witness under the same read lease and
yields a bounded mapped RX candidate with provenance
`MappedResultProvenance::TickSerializedElfWitness`. Guards that stand:
"A path, hash, asserted bias or caller boolean cannot establish correspondence";
"SCE executable flags, file-only loader metadata, existing TLS, unrelated
imports, modified BSS and stricter RELRO are covered by the owned host fixture".
The optional serialized source is privately copied and tied to ONE exact
retained snapshot by checked immutable-span correspondence
(`OwnedSerializedElfSource::Create`, `matches`).

The scan source becomes the selected native `ps_scan` backend through
`scan_owned_source(source, expected_snapshot, PsCtx*, PsScanResults*) -> bool`,
which refuses when `expected != source->source_owner()` (see Stage 4 admission).

### Stage 3 — Program construction (modbridge bootstrap)

Files:
- `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/ue4ss_backend.{hpp,cpp}`
- `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/runtime_owner.{hpp,cpp}`
- `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/activated_mod_source.{hpp,cpp}`
- Factory alternative: `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/NativeProgramFactory.hpp` (`CreateNativeUE4SSProgram(NativeLoaderFacts, options)`)

`PrepareDeferredUE4SSRuntime(RC::Platform::NativeProgramContext context)` and
`PrepareActivatedUE4SSRuntime(context, ActivatedMods, expected_title, activation_root)`
(ue4ss_backend.cpp:61-77) build `UE4SSBackend`, which:
1. `ValidateNativeProgramContext(context)` (again, narrow gate);
2. if buffered mods + `context.mod_source`: `ConfigureRetainedPS5PadInput(*context.mod_source, context.input_source)`;
3. forces `mod_startup_policy = Deferred`;
4. chains `context.on_engine_initialized = [signal, previous]{ previous(); signal->ready(); }`
   — the previous real callback is preserved and runs first;
5. constructs the REAL `RC::UE4SSProgram(std::move(context), {})` (constructor
   performs settings/path/log setup; `native_startup_error_message()` non-null
   -> `std::runtime_error("Native UE4SS construction rejected: ...")`);
6. buffered mode additionally calls `program_->start_native_mods()` eagerly
   (native C++ entries loaded/resolved pre-Unreal; Lua main.lua still deferred).

`RuntimeOwner` (runtime_owner.cpp) owns the backend and the init thread:
- `start()`: Prepared -> Starting, spawns `RuntimeOwner::initialize` (Failure -> Failed + error string).
- `stop()`: refuses SELF-JOIN (`signals_->initializer == std::this_thread::get_id()` -> return false, runtime_owner.cpp:46); sets Stopping (unless Failed/Stopped), `backend_->request_stop()`, joins the thread, Stopped. A late `ready` cannot overwrite Stopping/Stopped (guarded at runtime_owner.cpp:83-84).
- `initialize()` noexcept: `backend_->initialize(ready_cb)`; if init returns without a stop request -> Failed "Native runtime init returned without a stop request" (honesty guard).
- destructor: `if (!stop()) std::terminate()` — no detached-thread fallback.
- Backend contract: `initialize` blocks until shutdown; `request_stop()` thread-safe noexcept; backend destruction requires separately-established engine-hook quiescence ("stop() cooperatively requests shutdown and joins its owned init thread; it does not establish safe engine unhook/unload").

### Stage 4 — UE4SSProgram::init -> UnrealInitializer (admission gates + scan)

Files (canonical coherent source, selected by the tick-file-owner composition):
- `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/UE4SSProgram.cpp`
- `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/UnrealInitializer.cpp`
- `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UnrealInitializer.hpp`

Key sites:
- `UE4SSProgram::setup_unreal()` — `config.ModuleSource = m_native_context.module_source; config.NativeScanSource = m_native_context.scanner_source;` (UE4SSProgram.cpp:946-947). All hook flags, `FNameToStringMethod`, `DebugBuild`, `Stats`, `StartupWait` flow from `UnrealInitializer::Config`.
- `UnrealInitializer::PreInitialize` — HARD REFUSALS (UnrealInitializer.cpp:940-947):
  - `!UnrealConfig.ModuleSource` -> `std::logic_error("Native module source is not configured")`;
  - on `__PROSPERO__`: `!NativeScanSource || NativeScanSource->source_owner() != ModuleSource` -> `"Native mapped scan source must retain the configured module owner"` (the S1/S2 anti-pretender guard: scanner and module bytes must be the SAME owner).
- `UnrealInitializer::Initialize` — `ScanGame()` (single/multi-pass via the selected ps_scan binding), `VerifyFNameConstructor()` (initializer retry + FName wait cooperating with shutdown/deadline — checkpoint `native_ue4ss_activation_takeover.source_lineage`), `Output "Using engine version: {}.{}"`, then bounded waits for object construction (`GetNumElements() >= 10000`) with `CheckStartupWait()` (StartupWaitControl: cooperative cancellation + 5-minute deadline, poll slices preserving the original retry interval — checkpoint `native_ue4ss_bootstrap`).
- Version facts: first scan pass sets `Version::Major/Minor = results.engine_version.{major,minor}` when `config.engine_version` (UnrealInitializer.cpp:695-698). Build axes: `results.build_configuration` (0 not detected, 1 shipping, 2 development, 3 test) and `results.stats` (0 not detected, 1 off, 2 on).

### Stage 5 — Activation owner and EngineInitialized (S4 seam)

- `UE4SSProgram::init()` runs the engine bootstrap; on `__PROSPERO__`, after
  `fire_unreal_init_for_cpp_mods()`, `setup_unreal_properties()`,
  `share_lua_functions()`, it stores `m_is_program_started` and fires
  `m_native_context.on_engine_initialized()` under
  `m_engine_ready_callback_mutex` ONLY if `!m_shutdown_requested`
  (UE4SSProgram.cpp:654-661); if shutdown was requested it calls
  `shutdown_native_program()` and returns (no late activation).
- Mod discovery and Lua entry: `m_native_context.mod_source`
  (`RC::Platform::NativeModSource`, from
  `MakeActivatedUE4SSModSource(activation, expected_title, activation_root)`,
  activated_mod_source.hpp:28-30) supplies immutable logical `ue4ss/` keys:
  settings, `MemberVariableLayout.ini`/`VTableLayout.ini` (UE4SSProgram.cpp:899-902),
  enabled `startup_mod_names()` (UE4SSProgram.cpp:2604-2607) and the Lua mod list
  (UE4SSProgram.cpp:1742-1744). Lua entry/require/loadfile/dofile come through
  the real luamod file functions (checkpoint `native_lua_action_lifetime`:
  actions serialized with close, shared ProcessEvent registration;
  `native_mod_load_order`: discovery and dlopen only after engine init).
- Game-thread identity (S3 evidence route):
  `.superpowers/native-dependency-spike/ue4ss-native-game-thread-1/include/Unreal/Platform/NativeGameThreadIdentity.hpp`
  — `RC::Unreal::NativeThreadDetail::Identity::publish_current()` returns
  Published/AlreadyCurrent/DifferentThread/Unavailable; `id()` THROWS
  "GetGameThreadId called too early, not yet initialized by UGameEngine::Tick";
  `in_current_raw()`/`id_raw()` are nonthrowing and REFUSE admission when the
  identity is unknown. The genuine Tick publisher sets it on the first genuine
  `UGameEngine::Tick`; ProcessEvent action dispatch refuses unknown/other
  threads (checkpoint `native_game_thread_identity`).

## 2. THE SEAMS

### SEAM S1 — live-module facts (worker-1 attaches HERE)

- API: `RC::Platform::NativeLoaderFacts` -> `MakeNativeProgramContext(NativeLoaderFacts, serialized_elf = {})` (NativeLoaderContext.hpp:20-24).
- Consumes (worker-1 must produce): a genuine retained in-process mapping set
  expressed as `ModuleInput` (lifetime owner + MainExe image {base,size,
  readable_ranges} + optional find_export), `OwnedMappedImageFacts` for THAT
  SAME image (format must equal `PS_MAPPED_IMAGE_ELF64`, text_base, image_base,
  eh_frame/eh_frame_hdr extents describing actual mapped bytes), and the two
  bound paths. Optionally the serialized original ELF bytes to open the Stage-2
  witness route.
- Ownership boundary: worker-1's `ModuleInput::lifetime` owns the bytes for the
  whole process; `MakeNativeProgramContext` only wraps it (snapshot +
  scan-source share the owner). The bootstrap/factory consume the resulting
  `NativeProgramContext` — `UE4SSBackend` and `UE4SSProgram` retain
  `module_source`/`scanner_source` via `m_native_context` for program lifetime.
- Existing refusals that make the seam plug-in safe (do NOT relax):
  format!=ELF64 (NativeLoaderContext.cpp:10), ModuleSnapshot ctor validations,
  `ValidateNativeProgramContext` path/file gates,
  PreInitialize owner-identity guard (UnrealInitializer.cpp:946).
  A supplied path/hash/base-address/flag is NOT a supplier and will not pass
  these gates to a live result.

### SEAM S2 — validated layout facts (worker-2 attaches HERE)

- API (all exist in production source):
  - `RC::Platform::NativeLayouts::StageNativeLayoutFiles(const std::filesystem::path&)` — called by `UE4SSProgram::load_unreal_offsets_from_file()` on `__PROSPERO__` with `m_working_directory` (UE4SSProgram.cpp:908); stages the member/vtable layout pair, one loaded lifetime, refuses restaging.
  - `SetResolvedNativeLayoutAxes(std::optional<bool> non_shipping, std::optional<bool> stats)` — called by UnrealInitializer after the first scan pass (UnrealInitializer.cpp:832); axes come from forced settings or scan detector outputs; UNKNOWN detector outputs remain UNKNOWN nullopt ("shipping/STATS defaults do not establish target layout facts").
  - `PublishStagedNativeLayoutMaps(uint32_t major, uint32_t minor, bool non_shipping, bool stats)` — `.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/src/NativeLayoutBinding.cpp:172`; refuses: not staged; axes unresolved; published-facts disagreement; staged-identity disagreement with resolved engine facts (`id.engine_major != major || ...` -> runtime_error). Same-bundle/same-facts retry is the ONLY retry.
  - Width bound: `GetAccessorWidth(class_name, canonical_key)` (`.superpowers/native-dependency-spike/ue4ss-native-layout-widths-2/include/NativeLayoutWidths.hpp`) — compiled-port accessor widths bound caller-declared sizes; unknown widths are rejected, no native PDB defaults.
- GAP (recorded): `PublishStagedNativeLayoutMaps` currently has NO production
  caller — only Stage and SetResolvedNativeLayoutAxes are wired
  (UE4SSProgram.cpp:908, UnrealInitializer.cpp:832). The final publication step
  must be invoked with `Version::Major/Minor` (from the version scan result,
  UnrealInitializer.cpp:695-698) and the resolved axes when
  `InitializeVersionedContainer()` selects `NativeProvidedVirtualVC` on
  Prospero (Container selection patch:
  `.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/Container.cpp.patch`).
  Worker-2's profile supplies the validated pair contents + identity (engine
  major/minor + axes); the controller wires the publish call — nothing else
  changes.
- Ownership: staged maps are process-lifetime singletons guarded by
  `BindingMutex`; startup is the only publisher; accessors never run
  concurrently with publication.

### SEAM S3 — game-thread evidence route (milestone observation point)

- `RC::Unreal::GetGameThreadId()/IsInGameThread()/IsGameThreadInitialized()` (UnrealInitializer.hpp:198-209) backed by `NativeThreadDetail::Identity` (first genuine Tick publisher).
- The milestone query (see MILESTONE-TEST-DESIGN.md) is delivered through the
  same serialized Tick/ProcessEvent dispatch that this identity guards:
  unknown thread or wrong thread => dispatch refuses; no Tick leaves work
  pending.

### SEAM S4 — activation owner

- `PrepareActivatedUE4SSRuntime(context, ActivatedMods, expected_title, activation_root)`
  (ue4ss_backend.hpp:22-24). Consumes: a validated `NativeProgramContext`
  WITHOUT `mod_source` (backend refuses a context that already has one,
  ue4ss_backend.cpp:70-71), the Claude-side activation
  (`mfa_activation`), a caller-supplied `expected_title` (game/profile fact,
  never inferred from the mod's load list), and `activation_root` (only when
  native entries are listed). Engine readiness only via the real
  `on_engine_initialized` chain (Stage 3 step 4 + Stage 5).

## 3. What already works and MUST be preserved (do-not-regress list)

1. NameTypes conversion fallback: engine-owned Kismet `Conv_NameToString`
   (`FNameToStringMethod::Conv_NameToString`); the absent SkySphereMesh anchor
   is saved and optional (checkpoint `optional_resolver_route_correction`:
   FNameToString and GameEngine::Tick are OPTIONAL scan slots, required scan
   mask 0x5d).
2. FName native output ABI correction: explicit-output constructor lowering
   `void fn(ptr output, ptr text, i32 mode)` (checkpoint
   `fname_native_constructor_abi`).
3. Initializer retry + FName wait cooperating with shutdown/deadline
   (StartupWaitControl, 5-minute cooperative deadline, poll slices preserving
   original retry interval).
4. Cancellation/deadline/affinity guards: RuntimeOwner self-join refusal,
   late-ready protection, "init returned without stop request" failure,
   Identity publish/AlreadyCurrent/DifferentThread semantics, ProcessEvent
   dispatch refusing unknown/other threads, no Tick leaves work pending.
5. Serialized Tick witness route: original report authority (success/
   ambiguity/error preserved verbatim), SelectUniqueAfterNoMatch-only witness
   admission, single read lease for all witnesses, bounded mapped RX candidate
   only (never a callable pointer claim).
6. Activation-owned Program/settings/Lua path: real
   Program/settings/inventory/Lua entry/require/loadfile/dofile, immutable
   `ue4ss/` logical keys, no disk fallback, no per-mod rewrites; serialized
   action close, callback retirement destroying callables before return
   (checkpoint `native_synchronous_callback_retirement`).

## 4. Provenance note

References were verified by direct read of the files above on 2026-10-04.
The composed relocatable object for this path is
`.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/native-1/coherent-tick-file-witness-runtime.o`
(SHA256 cf93b8f5b528d0bc8307d76255b1958fbcb992e990f2633e78e8016f75ebfbe0,
117,254,296 bytes, 360 runtime + 3 bootstrap units, 197 undefined native
symbols unchanged). None of it has executed against the real game
(`target_execution: false` throughout); this map describes SOURCE structure,
not runtime proof.
