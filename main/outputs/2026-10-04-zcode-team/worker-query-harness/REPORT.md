# REPORT — worker-query-harness (harmless-query milestone harness)

Deliverable: `native-query-harness` as two files (`src/NativeQueryHarness.hpp` +
`src/NativeQueryHarness.cpp`), a plug-in `--extra-unit` for the composed UE
runtime, implementing the PRIMARY milestone candidate of
`outputs/2026-10-04-zcode-team/worker-3-gamethread/MILESTONE-TEST-DESIGN.md`
(read-only `UObjectArray::GetNumElements()` twice on consecutive genuine
game-thread Ticks, through the published identity), with distinct refusal
outcomes, a fixed-size POD record with no Tick-path allocation, off-thread
JSONL formatting, host tests, and a target-side extra-unit compile probe.

Scope honesty: everything in this report is HOST and COMPILE evidence. Nothing
here executed against the real game, a console, the network, or the composed
runtime at runtime. Per MILESTONE-TEST-DESIGN.md §3, none of this counts as
the milestone itself; §"Acceptance" below defines what the controller must
additionally observe in-game.

## 1. Files (all inside this output dir)

| File | Role | SHA256 |
|---|---|---|
| `src/NativeQueryHarness.hpp` | Self-contained contract + state machine (standard library only) | `5e3abfa713a58294d7ab46b6bffa0e0fab6b78158de17a80210866a541d14930` |
| `src/NativeQueryHarness.cpp` | Implementation + default engine binding (`RC::Unreal` probes + `UObjectArray::GetNumElements`) | `1b9cec495343d5c80a2d096519128ad887c927f1376e3de722232ac8fadadd45` |
| `tests/host_test.cpp` | Host test driver (both layouts, all refusals, no-leak, immutability, concurrency) | `108b09bd49aeb1a9356b33cc7c5c9505a70a87972eeae9262e4d7c92b451013f` |
| `tests/run_host_tests.sh` | Reproducible host build+run | `9e43e5fb811dfcfd4a0fd7a05ef9ebe574cc7f392b9e556ae601551de228f8b6` |
| `tests/extra_unit_compile_probe.py` | Extra-unit compile probe (pinned prospero-clang++, receipt argv template) | `fbff5ff26117834dd67086662d0742aef26a6a7b402471eb9edae7d691f261f0` |
| `tests/target-probe/native-query-harness.o` | Resulting Prospero object (70,816 bytes) | `1eb11c7aaaab89e7ae2f1007db8186687fce273945955d7f44c85dd1cebaed8d` |
| `tests/target-probe/symbol-classification.json` | Unit-undefined vs composed-defined vs SDK-baseline symbols | — |

## 2. Design summary

Namespace `RC::Platform::QueryHarness`, class `Harness` (process singleton via
`Global()`), driven through three channels:

1. **Wiring time (serialized startup, before ticks)**
   - `InstallEngineBindings(EngineBindings)` — injectable seams:
     `is_game_thread_initialized`, `is_in_game_thread_raw`,
     `game_thread_id_key` (0 == unknown), `read_num_elements`. Target default
     (`MakeDefaultEngineBindings()`): `RC::Unreal::IsGameThreadInitialized`,
     `IsInGameThreadRaw`, `GetGameThreadIdRaw` (coherent
     `UnrealInitializer.hpp:226-234`) and `RC::Unreal::UObjectArray::GetNumElements`
     (`UObjectArray.cpp:394` in the composed units). Null members and double
     install are refused; host builds refuse the default binding outright (no
     silent stub).
   - `ArmLayoutGate(LayoutGateFacts)` — the S2 gate: engine major/minor,
     non_shipping/stats axes (0/1 facts only), the validated-profile storage
     claim (fixed/chunked, informational) and a bounded provenance label.
     Double arm, unresolved identity and empty labels are refused.
2. **Game-thread Tick entry: `OnEngineTickDispatch() noexcept`**
   Check order: terminal → bindings present → layout gate armed → identity
   known → in game thread → identity-key stable across the pair → the single
   engine read `read_num_elements()`. Outcomes (`QueryStatus`):
   - Four distinct non-terminal refusals (counted, never recorded as query
     records): `RefusedBindingsAbsent`, `RefusedLayoutGateClosed`,
     `RefusedIdentityUnknown`, `RefusedWrongThread`.
   - `Pending` after the first accepted count > 10000 (staged outside the
     published record).
   - Terminal `InconclusiveBelowThreshold` (count ≤ 10000 — inconclusive,
     NOT refuted, per the initializer heuristic) or `Completed` (both counts
     recorded, plus delta and monotonicity verdict Equal / NonDecreasing /
     Decreasing — decreasing is recorded honestly, never masked).
3. **Controller side (any thread)**
   - `TrySnapshot(QueryResultRecord&)` — true only for a TERMINAL record;
     a visible record is never partial. Single-writer seqlock (monotonic
     version, bounded 128 retries). The Tick path performs zero allocation,
     zero I/O; the only heap-free JSONL formatter
     (`FormatRecordJsonLine`, hand-rolled integer writer into a 640-byte
     caller buffer) is documented off-thread-only and runs on a snapshot.
   - `SnapshotDispatchCounters()` — design §2.2's refusal-evidence channel:
     per-refusal-kind counts, last refusal + sequence, staged resets
     (relaxed monotonic loads; explicitly not a coherent instant).

**No-Tick-leak rule**: any refusal while a first count is staged discards it,
returns the phase to `Idle` and bumps `staged_resets`; therefore both recorded
counts always come from consecutive accepted Ticks under a continuously open
gate on one stable identity (a published-identity key flip between the reads
is itself refused as `RefusedWrongThread`), and
`record.refusals_between_counts == 0` holds by construction.

Record: 128-byte POD (`static_assert`s: standard layout, trivially copyable,
fixed size), fields include engine identity + axes + storage claim + count
source, both counts, delta, verdict, threshold, identity key, tick counters
and gate provenance label.

## 3. Extra-unit wiring with the composition script

`build_runtime.py` (ue4ss-native-tick-file-owner-1) mechanism used as-is:

```
python3 .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/build_runtime.py \
  --output <fresh-dir> --rust-receipt <frozen> --host-receipt <current-passing> \
  --extra-unit native-query-harness=<ABS>/outputs/2026-10-04-zcode-team/worker-query-harness/src/NativeQueryHarness.cpp \
  --extend-undefined-baseline
```

Facts verified by the probe (`tests/extra_unit_compile_probe.py`, mirrors
`build_runtime.py` lines 55-73 exactly: loader-context `argv` template, source
swap, `-o`/`-MF` into the output dir, `argv[1:1] = ['-I', tick-file-owner-1/include,
'-I', tick-witness-1/include]`, same `LLVM_CONFIG` env handoff):

- Compile **exit 0** with the pinned
  `outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang++`
  (`-std=c++23`, full template define set); object 70,816 bytes, depfile
  emitted. No `--extra-define` needed.
- The cpp's include of its own header is QUOTED (`#include "NativeQueryHarness.hpp"`),
  so no include-path addition is required beyond the template.
- Undefined-symbol classification (`symbol-classification.json`): 22 unit-level
  undefined symbols; 18 resolved by the composed runtime's own units —
  including exactly the four engine seams
  `RC::Unreal::FUObjectArray::GetNumElements`,
  `RC::Unreal::IsInGameThreadRaw`, `RC::Unreal::GetGameThreadIdRaw`,
  `RC::Unreal::IsGameThreadInitialized` — plus standard C++/ABI bits; the
  remaining 4 (`__cxa_atexit`, `__dso_handle`, `memcpy`, `sched_yield`) are
  already in the frozen 197-symbol SDK baseline; **0 new undefined symbols
  after composition estimate**, i.e. the `--extend-undefined-baseline` gate
  passes without adding any SDK import.
- Dependency pinning: every header the unit touches (coherent
  `UnrealInitializer.hpp`, `UObjectArray.hpp` and their transitive closure)
  is already in the base build's pin set (the six affected original units
  depend on the same closure and passed the same check), so the extra-unit
  "Unpinned extra project dependency" gate is satisfied.

Controller wiring obligations at runtime (the harness leaves them to the
existing owners, it does not add callers):
1. `QueryHarness::Global().InstallDefaultEngineBindings()` once, before
   `RuntimeOwner::start()`.
2. In the serialized startup path, immediately after a successful
   `PublishStagedNativeLayoutMaps(Version::Major, Version::Minor, axes)`
   (the S2 GAP wiring INTEGRATION-MAP.md already assigns to the controller):
   `ArmLayoutGate({major, minor, non_shipping, stats, storage-claim-from-profile,
   "PublishStagedNativeLayoutMaps"})`. If the publish call THROWS, do NOT arm —
   the harness then refuses every dispatch with `RefusedLayoutGateClosed`.
3. Route the existing game-thread Tick dispatch (the serialized
   Tick/ProcessEvent route that publishes
   `NativeThreadDetail::Identity::publish_current()`) to also call
   `QueryHarness::Global().OnEngineTickDispatch()` on that same callback
   thread, after the publisher ran (a dispatch before publication is refused
   as `RefusedIdentityUnknown` and counted — which is design §2.1's
   pre-publication guard evidence).
4. The controller's driver pulls `TrySnapshot`/`SnapshotDispatchCounters`
   from its own (non-game) thread and writes the JSONL via
   `FormatRecordJsonLine` into the existing console evidence path.

## 4. Host test results

`tests/run_host_tests.sh` → **HOST_TESTS_PASSED: 358 checks, 0 failures**
(clang++ -std=c++20 -Wall -Wextra -DNATIVE_QUERY_HARNESS_HOST_TEST; the same
two source files as the extra unit, engine seams injected):

- Both storage-layout claims driven end-to-end: `chunked_array` with axes
  (non_shipping=1, stats=0) → Completed/Equal; `fixed_array` with axes
  (non_shipping=0, stats=1) → Completed/NonDecreasing.
- Every refusal path: bindings absent (repeatable), gate closed, identity
  unknown (pre-publication), wrong thread — each returns its distinct status,
  leaves `phase()==Idle`, publishes NO record, and advances only its counter
  + `last_refusal`.
- No-leak proofs: wrong-thread dispatch BETWEEN the two reads, an identity-key
  flip between the reads, and a gate disarm between the reads each discard the
  staged count (`staged_resets==1`, `TrySnapshot()==false`), after which a
  fresh consecutive pair completes with the LATER counts and
  `refusals_between_counts==0`.
- Threshold semantics: first count exactly 10000 → terminal Inconclusive with
  1 count; second count 9999 after 12000 → terminal Inconclusive with both
  counts, delta −2001 and Decreasing verdict recorded honestly.
- Terminal immutability: 5 further ticks leave the record byte-identical,
  `ticks_accepted` stays 2, the engine read is not re-invoked.
- Concurrency: 200 writer lifecycles vs. a snapshotting reader thread — every
  delivered record complete/terminal (0 torn or invalid), channel live
  (~700 terminal snapshots per run; 5/5 consecutive full-suite runs passed,
  e.g. 686/683/760/681/706).
- JSONL line emitted and asserted (engine "5.6", axes, storage, hex key,
  counts, delta, verdict, ticks, gate source), sample in the test output.
- Install/arm guards: null seams, double install, double arm, unresolved
  engine identity and empty provenance labels all throw.

## 5. Acceptance criteria: rung 1 → 2 (what the controller must observe in-game)

The harness moves the milestone from rung 1 (host plug-in tests) to rung 2
(real-process evidence) ONLY when ALL of the following come from the REAL
Townfall process through the existing console evidence path — the JSONL line
produced by the controller's driver from `TrySnapshot`, plus counters:

1. A `status":"completed"` record with `count_source":"engine_uobject_array"`,
   `counts.recorded":2`, both counts `> 10000`, and `monotonic` Equal or
   NonDecreasing (a Decreasing or `inconclusive_below_threshold` record does
   NOT advance the rung; it is honest negative evidence, not a failure of the
   harness).
2. `game_thread_id_key` non-zero and equal for both reads
   (`identity_checks":2`), consistent with the identity published by the first
   genuine `UGameEngine::Tick` (`IsInGameThreadRaw()==true` world).
3. Counter-channel evidence of the guard, not just the happy path: at least
   one `refused_wrong_thread` (or `refused_identity_unknown`) count from a
   real non-game-thread dispatch attempt, and zero
   `refused_layout_gate_closed` on the accepted path (the gate was armed by a
   genuinely successful `PublishStagedNativeLayoutMaps`).
4. The record's `engine`/"axes"/storage facts equal the validated profile and
   the real scan result (5.6 + resolved axes), and the gate provenance label
   names the publish call.
5. Lifetime preserved after the query: `RuntimeOwner::stop()` returns true,
   joins, reaches Stopped/Failed without hang or crash in the bounded window
   (design §2.5) — with the harness itself adding no Tick-path work after
   terminal (`ticks_accepted` frozen at 2 is part of that evidence).
6. Provenance chain per design §2.6 (live-image scan or
   `TickSerializedElfWitness` provenance; validated layout origin, bounded
   widths, confidence ≥ threshold).

## 6. What remains gated on in-game facts (not provable here)

- That `UObjectArray::GetNumElements()` through the published maps returns
  plausible values on the real process (any count, stability, monotonicity).
- That the first genuine Tick dispatch ordering (publisher → harness) holds
  in the real hook chain; only the refusal outcome of a mis-ordering is
  host-proven.
- The S2 GAP production caller itself (`PublishStagedNativeLayoutMaps` has no
  production caller yet — INTEGRATION-MAP.md Seam S2 GAP); until the
  controller wires it, every on-target dispatch is correctly refused.
- S1 live-module facts, witness-route admission, PRX packaging, load, and any
  console behavior: all untouched and untested by this worker.
- The FName roundtrip secondary candidate (design §1): not implemented here,
  remains optional and separate.

## 7. Deviations from the brief (recorded, brief vs. actual headers)

1. **S2 gate is armed by wiring, not queried at Tick time.**
   `NativeLayoutBinding.hpp` exposes only `StageNativeLayoutFiles` /
   `SetResolvedNativeLayoutAxes` / `PublishStagedNativeLayoutMaps`; the
   `Published`/`Staged` state is file-internal with no public query (verified
   in `ue4ss-native-layout-binding-1/src/NativeLayoutBinding.cpp`), and that
   include dir is not on the extra-unit template path. The gate is therefore
   armed by the controller in the same serialized startup path, immediately
   after a successful publish call (which throws on every refusal mode), and
   the harness refuses all dispatches whenever arming is absent. This matches
   MILESTONE-TEST-DESIGN.md §4 ("gate stays DISARMED until both seam
   resolutions are ok; host adapters mirror them").
2. **Threshold strictness.** The brief says "count is not >10000"; the
   initializer's wait loop uses `GetNumElements() >= 10000`. The harness
   requires strictly greater for Completed and treats exactly 10000 as
   Inconclusive (tested); the record carries `threshold":10000` so the
   controller can re-evaluate the boundary either way.
3. **"Both layouts"** is realized as both GUObjectArray storage-layout claims
   (fixed/chunked) AND both S2 axes combinations, because the actual
   fixed/chunked dispatch lives inside the engine's
   `TUObjectArray::GetNumElements()` (version-driven), not in this harness —
   the storage field is the validated profile's claim recorded as evidence.
4. **`UObjectArray` is an alias** of `FUObjectArray` in `RC::Unreal`
   (upstream + coherent headers); the default binding binds
   `RC::Unreal::UObjectArray::GetNumElements` accordingly.
5. **Concession to the seqlock idiom**: the snapshot copy of a trivially
   copyable POD under a version recheck is the standard seqlock pattern; it
   is the accepted single-writer discipline here (writer is only the
   admitted game thread), noted for the reviewer.

## 8. Provenance

Written by worker-query-harness on 2026-10-04 inside
`outputs/2026-10-04-zcode-team/worker-query-harness/` only; no existing file
was modified; no network, console, or target execution; ≤45 tool calls; no
nested subagents. The compile probe executed only the pinned local
prospero-clang++/llvm-nm from existing receipts.
