# S2-ARM — arming the query-harness layout gate after `PublishStagedNativeLayoutMaps`

Worker: worker-query-harness-integration, 2026-10-04. Proposal-only controller
wiring for the NEXT composed runtime (supplier-composition line). Every
signature below was read from the actual sources; no file outside this output
dir was modified. Scope honesty: this is a wiring spec, not a performed
integration — the publish call still has no production caller in the current
composed object (INTEGRATION-MAP.md SEAM S2 GAP), and until it does, every
on-target harness dispatch is correctly refused.

## 1. The seam (what already exists)

`RC::Platform::NativeLayouts::PublishStagedNativeLayoutMaps(uint32_t major, uint32_t minor, bool non_shipping, bool stats)`
- declared: `.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/include/NativeLayoutBinding.hpp:10`
- implemented: `.../ue4ss-native-layout-binding-1/src/NativeLayoutBinding.cpp:172`
- throws `runtime_error` on EVERY refusal mode: not staged; axes unresolved;
  published-facts disagreement; staged-identity disagreement with resolved
  engine facts. Same-bundle/same-facts retry is the only retry.

In the next composed runtime the publisher is reached through the existing
production-adjacent path (no new caller of the raw API needed):

```
UnrealInitializer::Initialize (serialized startup, init thread)
  -> InitializeVersionedContainer()
       selects NativeProvidedVirtualVC on Prospero (Container.cpp.patch)
       -> NativeProvidedVirtualVC::set_virtual_offsets()      // NativeProvidedVirtualVC.hpp:13-18
            -> PublishStagedNativeLayoutMaps(
                   (uint32_t)Version::Major, (uint32_t)Version::Minor,
                   Version::NonShipping, Version::DebugBuild); // THROWS on any refusal
```

Supporting wired facts (unchanged): files staged by
`UE4SSProgram::load_unreal_offsets_from_file()` (UE4SSProgram.cpp:908); axes
resolved by `SetResolvedNativeLayoutAxes(non_shipping, stats)` after the first
scan pass (UnrealInitializer.cpp:832); version facts set at
UnrealInitializer.cpp:695-698. Startup is the only publisher; staged maps are
process-lifetime singletons under `BindingMutex`; accessors never run
concurrently with publication.

## 2. The harness gate (what is being armed)

`RC::Platform::QueryHarness::Global()` (singleton), wiring-time API from
`NativeQueryHarness.hpp`:

```cpp
struct LayoutGateFacts {
    uint32_t engine_major{};
    uint32_t engine_minor{};
    uint8_t  non_shipping{};   // 0/1 fact only
    uint8_t  stats{};          // 0/1 fact only
    StorageLayout storage{};   // Unknown=0, FixedArray=1, ChunkedArray=2
    char     source[kSourceLabelCapacity]{};  // provenance label
};
auto ArmLayoutGate(const LayoutGateFacts&) -> void;    // throws std::logic_error on double arm
auto DisarmLayoutGate() noexcept -> void;
```

At Tick time the gate is checked (not queried from the layout binding): with
the gate closed, `OnEngineTickDispatch()` returns `RefusedLayoutGateClosed`,
counts the refusal, and stages nothing. That is the honest substitute for the
binding's file-internal `Published`/`Staged` state, which has no public query.

## 3. The one-line call site (bootstrap context)

Placement: in `UnrealInitializer::Initialize`, as the statement IMMEDIATELY
after the `InitializeVersionedContainer();` call — the same serialized startup
statement sequence whose Prospero branch just executed
`set_virtual_offsets()` -> publish. If the publish threw,
`InitializeVersionedContainer()` propagated the exception and this line is
never reached; if it returned, the publish genuinely succeeded.

```cpp
RC::Platform::QueryHarness::Global().ArmLayoutGate({Version::Major, Version::Minor, static_cast<uint8_t>(Version::NonShipping ? 1 : 0), static_cast<uint8_t>(Version::DebugBuild ? 1 : 0), /*validated-profile storage claim*/ RC::Platform::QueryHarness::StorageLayout::ChunkedArray, "PublishStagedNativeLayoutMaps"});
```

Field discipline (one line, but not arbitrary):

| Field | Value | Why |
|---|---|---|
| `engine_major`/`engine_minor` | `Version::Major`/`Version::Minor` | exactly the values the publish call just passed; they equal the staged-identity facts or publish would have thrown |
| `non_shipping` | `Version::NonShipping ? 1 : 0` | identical axis passed by `set_virtual_offsets()`; 0/1 fact only |
| `stats` | `Version::DebugBuild ? 1 : 0` | the stats axis as consumed by the existing publish path (same variable it passes as `stats`) |
| `storage` | the validated profile's GUObjectArray storage claim from worker-2's profile — `ChunkedArray` above is the 5.6-profile example, substitute the profile's actual claim | recorded as evidence; the real fixed/chunked dispatch is engine-version-driven inside `TUObjectArray::GetNumElements()` and is NOT controlled by this field |
| `source` | `"PublishStagedNativeLayoutMaps"` | the exact provenance label the record carries; empty labels are refused by the harness |

Guarantees of this placement:
- Arm happens ONLY after a genuinely successful publish (a throwing publish
  leaves the gate closed; the harness then refuses every dispatch with
  `RefusedLayoutGateClosed`, each one counted — that counter channel is the
  negative-evidence proof that the gate was never armed by a failed publish).
- No double arm: `ArmLayoutGate` throws on double arm, and this statement runs
  exactly once per process because `InitializeVersionedContainer()` runs once
  in serialized startup (publish itself refuses re-publication; its only legal
  same-facts retry can only follow a FAILED publish, i.e. a state where
  arming never happened).
- Serialized-startup ordering preserved: arm precedes `RuntimeOwner::start()` /
  `UE4SSProgram::init()` tick flow and precedes the first genuine
  `UGameEngine::Tick` that publishes `NativeThreadDetail::Identity`; a
  mis-ordered dispatch is refused `RefusedIdentityUnknown` and counted.

## 4. Disarm-on-throw (required companion)

`DisarmLayoutGate()` is `noexcept` and idempotent. Wrap the REMAINDER of the
serialized startup after the arm line so a later startup failure can never
leave a half-initialized runtime advertising an open gate:

```cpp
try {
    // ... remaining serialized startup through engine-readiness ...
} catch (...) {
    RC::Platform::QueryHarness::Global().DisarmLayoutGate();
    throw;   // preserve original failure; gate closed => all dispatches refused+counted
}
```

Semantics summary:
- publish throws -> arm line unreachable -> gate closed (no disarm needed).
- post-arm startup throws -> explicit disarm in the catch -> gate closed.
- full success -> gate stays armed for the process lifetime; there is no
  target-side re-arm/retry path (`ResetForHostTest` is host/teardown only).

## 5. Prerequisite wiring (unchanged obligations from worker-query-harness)

1. `QueryHarness::Global().InstallDefaultEngineBindings()` exactly once,
   before `RuntimeOwner::start()` (host builds refuse the default binding; on
   target it binds the four engine seams
   `IsGameThreadInitialized`/`IsInGameThreadRaw`/`GetGameThreadIdRaw`/
   `UObjectArray::GetNumElements`, all verified defined by the composed
   supplier-composition-1 units — see this worker's classification: 18/22
   unit undefineds resolved by composed units incl. all four seams).
2. Route the serialized game-thread Tick/ProcessEvent dispatch (the same
   callback that runs after `NativeThreadDetail::Identity::publish_current()`)
   to also call `QueryHarness::Global().OnEngineTickDispatch()` on that thread.
3. Controller driver (non-game thread): `TrySnapshot` for the terminal record,
   `SnapshotDispatchCounters` for the refusal evidence, JSONL via
   `FormatRecordJsonLine` into the existing console evidence path.

## 6. Composition note for the next build

The unit compiles cleanly under the existing extra-unit mechanism against the
supplier-composition-1 baseline with ZERO new undefined symbols (measured;
`target-probe-supplier-composition-1/symbol-classification-vs-supplier-composition-1.json`
in this dir), so the arming wiring adds no SDK import and needs no
`--extra-define`; compose with
`build_runtime.py --extra-unit native-query-harness=<harness>/src/NativeQueryHarness.cpp --extend-undefined-baseline`
(the gate is expected to pass with an empty added set; the actual composition
and link remain the controller's serialized step).
