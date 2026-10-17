# WIRING — exact S2 arm, disarm-on-throw, and module_start ack supplier

Worker: worker-query-s2-arm, 2026-10-04. Every signature below was read from
the actual composed sources; the patched initializer compile-probes clean under
the exact affected-unit target argv (probe/initializer-probe/, exit 0) and
imports exactly the four harness symbols. Task-2/3/4 deliverables in one page;
the ready-to-apply artifacts are the sibling files.

## 1. The exact one-line arm call (task 2)

```cpp
RC::Platform::QueryHarness::Global().ArmLayoutGate({static_cast<uint32_t>(Version::Major), static_cast<uint32_t>(Version::Minor), static_cast<uint8_t>(Version::NonShipping ? 1 : 0), static_cast<uint8_t>(Version::DebugBuild ? 1 : 0), RC::Platform::QueryHarness::StorageLayout::Unknown, "PublishStagedNativeLayoutMaps"});
```

File: `.superpowers/native-dependency-spike/ue4ss-native-tick-initializer-retry-1/src/UnrealInitializer.cpp`
(unit `unreal-unrealinitializer`, pinned at `66691a88…` in the composition base
receipt). Function: `ScanGame()`.

Placement: as the FIRST statement after the genuinely successful publish.
Textually it sits immediately after the whole block

```cpp
        InitializeVersionedContainer();
#ifndef __PROSPERO__
        // Exact native offsets already include target configuration effects.
        ApplyDebugBuildOffsets();
#endif
```

i.e. right after that closing `#endif`, inside `#ifdef __PROSPERO__`. On the
Prospero target the `#ifndef` block preprocesses to NOTHING, so the arm call is
the statement IMMEDIATELY following `InitializeVersionedContainer();` in the
preprocessed serialized sequence — exactly the worker-query-harness-integration
S2-ARM.md placement (publish throws -> arm unreachable -> gate stays closed and
every dispatch is refused `RefusedLayoutGateClosed` + counted; publish returns
-> gate genuinely armed). Guarded `#ifdef __PROSPERO__` because only that
target selects `NativeProvidedVirtualVC` -> `PublishStagedNativeLayoutMaps`;
arming on a target whose publisher never ran would be a false record.

Field discipline (why each argument):

| Field | Value | Why |
|---|---|---|
| `engine_major`/`engine_minor` | `static_cast<uint32_t>(Version::Major/Minor)` | exactly the values the publish consumed (`(uint32_t)Version::Major, …` per the binding's publish path); equals the staged identity or publish threw. Both non-zero is also the harness's resolved-identity precondition |
| `non_shipping` | `static_cast<uint8_t>(Version::NonShipping ? 1 : 0)` | identical axis passed by `set_virtual_offsets()`; 0/1 fact (harness refuses >1) |
| `stats` | `static_cast<uint8_t>(Version::DebugBuild ? 1 : 0)` | the stats axis as consumed by the existing publish path |
| `storage` | `StorageLayout::Unknown` | DEVIATION from S2-ARM.md's `ChunkedArray` example, recorded honestly: the validated profile (`worker-2-layout-facts/profiles/townfall-ps5-ue56.profile.json`) supplies NO GUObjectArray storage claim ("member layout offsets: never supplied/resolved"). The field is informational; the real fixed/chunked dispatch is engine-version-driven inside `TUObjectArray::GetNumElements()` |
| `source` | `"PublishStagedNativeLayoutMaps"` | exact provenance label the record carries (empty labels refused) |

## 2. Disarm-on-throw catch-alls (task 3)

Two catch-alls, both in the same file, both `#ifdef __PROSPERO__`:

a) ScanGame-level — opens immediately after the arm line, closes right after
`StaticStorage::bScanFullyCompleted = true;`. Covers the second-pass scan and
scan-completion marking:

```cpp
        try
        {
            … second-pass scan …
        }
        catch (...)
        {
            RC::Platform::QueryHarness::Global().DisarmLayoutGate();
            throw;
        }
```

b) Initialize-level — opens right after the `if (!StaticStorage::bScanFullyCompleted) { ScanGame(); }`
block in `UnrealInitializer::Initialize` (which, on this target, is the whole
serialized startup through `PostInitialize`/engine-readiness), closes after
`PostInitialize(UnrealConfig);`:

```cpp
        try
        {
            … FName verification, object-construction waits, hook resolution …
        }
        catch (...)
        {
            RC::Platform::QueryHarness::Global().DisarmLayoutGate();
            throw;
        }
```

Semantics (identical to S2-ARM.md §4): publish throws -> arm unreachable -> gate
closed; post-arm startup throws -> explicit disarm -> gate closed (`throw;`
preserves the original failure for the existing startup retry/cancellation);
full success -> gate stays armed for the process lifetime. `DisarmLayoutGate()`
is noexcept/idempotent; a disarm is not sticky (host-proven: re-arm succeeds,
double arm still refuses `std::logic_error`). The wrapped regions are NOT
re-indented — byte-minimal reviewable diff against the pinned base.

## 3. Persistent tick dispatch route (companion, required for two consecutive reads)

`HookedEngineTick` (the `GameThreadInitializer` pre-callback) self-retires on
its first successful game-thread execution (`CallbackIterationData.RemoveSelf()`),
so it can never deliver two consecutive engine ticks. The patch registers one
additional persistent pre-callback immediately after the initializer callback's
registration (same `#ifdef __PROSPERO__` branch, same ERROR_ID throw
discipline):

```cpp
    static auto QueryHarnessEngineTickDispatch([[maybe_unused]] Hook::TCallbackIterationData<void>& CallbackIterationData, [[maybe_unused]] UEngine*, [[maybe_unused]] float, [[maybe_unused]] bool) -> void
    {
        RC::Platform::QueryHarness::Global().OnEngineTickDispatch();
    }
…
            const auto query_dispatch_callback = Hook::RegisterEngineTickPreCallback(QueryHarnessEngineTickDispatch, {false, false, STR("UE4SS"), STR("QueryHarnessDispatch")});
            if (query_dispatch_callback == Hook::ERROR_ID)
            {
                throw std::runtime_error("Query harness tick dispatch callback registration failed");
            }
```

Registration order: `GameThreadInitializer` first (publishes identity), then
`QueryHarnessDispatch` — on every later genuine engine tick the harness sees a
known identity on the game thread. Prerequisite 1 of S2-ARM.md §5
(`InstallDefaultEngineBindings()` exactly once, before `RuntimeOwner::start()`)
is satisfied by the supplier below (module_start thread, before start());
prerequisite 3 (controller driver: TrySnapshot / SnapshotDispatchCounters /
FormatRecordJsonLine) remains the controller's console-evidence step and needs
no target code.

## 4. module_start RuntimeAckConfig supplier (task 4)

Files: `new-units/runtime_ack_supplier.hpp` + `new-units/runtime_ack_supplier.cpp`
(composition-4 extra unit `module-runtime-ack-supplier`). Implements
worker-ack-integration-deep WIRING-SPEC S1/S2/S3 verbatim:

```cpp
RuntimeAckConfig runtime_ack = MakeRuntimeAckConfig(mod_root);
//   status_root = the EXACT mfa_config::root string (never activation_root)
//   pid         = static_cast<long>(getpid())          (frozen import contract)
//   now_ms      = clock_gettime(CLOCK_MONOTONIC) -> ms (vendored posix_now_ms shape)
RC::Platform::QueryHarness::Global().InstallDefaultEngineBindings();   // once, before start
auto owner = PrepareActivatedUE4SSRuntime(std::move(context), std::move(activation),
                                          expected_title, runtime_ack); // 4-arg composition-3 closure
owner->start();          // ack sink already opened inside prepare, during module_start
…
StopRuntimeWithAck(owner);   // module_stop: stop() joins + emits one 'stopped'; dtor closes
```

The game-bound entry (future unit) keeps exactly two obligations: call
`mfa_activate` with its `mfa_config` and hand this supplier the activation plus
the same `root` string. The sink itself is opened inside
`PrepareActivatedUE4SSRuntime` after `MakeActivatedUE4SSModSource` validates —
one sink per process, opened during module_start while /data is reachable.

Host-proven (host-tests/, 41 checks, exit 0): config birth facts, the real
sink file chain (`session\tpid\tnow_ms` header + activated/engine-ready/
mods-started + single stopped), real `RuntimeOwner` start/stop with a stub
backend, the arm-line facts end-to-end, and the disarm semantics.
Target-proven (probe/supplier-probe/, exit 0): the unit compiles clean under
the exact extra-unit argv and imports only composed-unit, harness-unit, and
frozen-import-contract symbols (`getpid`, `clock_gettime` measured with the
same four libkernel providers as composition-3's `sceKernelGetModuleInfo`).
