# REPORT — worker-query-harness-integration (re-dispatch)

Re-dispatched worker (prior same-role attempt died to a rate limit before any
delivery). Worker: integrates/verifies the delivered `worker-query-harness`
against the CURRENT composed runtime; proposal-only for controller wiring.
All work under `outputs/2026-10-04-zcode-team/worker-query-harness-integration/`;
the harness dir and every other existing file were read-only to this worker
(the host suite was re-run from a byte-identical copy inside this output dir —
hashes below prove identity). 20 tool calls total, no nested subagents, no
network, no console, no target execution.

## 0. Inputs verified

- `outputs/2026-10-04-zcode-controller/ZCODE-FORK-HANDOFF.md` read (integration
  queue step 1: candidate promotion -> staged pair -> query-harness rung-1->2).
- Checkpoint `zcode_worker_query_harness` and `zcode_supplier_composition_1`
  read from `.superpowers/native-observations/transport-experiments-progress.json`.
- Harness file hashes all match its REPORT:
  `NativeQueryHarness.hpp 5e3abfa7…`, `NativeQueryHarness.cpp 1b9cec49…`,
  `host_test.cpp 108b09bd…`, `run_host_tests.sh 9e43e5fb…`.
- **Composed object verified against checkpoint**:
  `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-1/run-1/coherent-tick-file-witness-runtime.o`
  = sha256 `fb2f4dd16a20997455b39e6857fd2bdf45842c691bd0124a448948ae5bcb775c`,
  117,624,920 bytes — exactly `zcode_supplier_composition_1.runtime_sha256` /
  `runtime_bytes`. The probe re-verifies this before every compile and aborts
  on drift.

## 1. T1 — 358-check host suite re-run: STILL GREEN

`bash tests/run_host_tests.sh` from this dir's `harness-copy/` (byte-identical
copy of `src/` + `tests/`; original dir untouched):

```
HOST_TESTS_PASSED: 358 checks, 0 failures   (1.33s wall)
concurrent> terminal_snapshots=692 torn_or_invalid=0
```

clang++ -std=c++20 -Wall -Wextra -DNATIVE_QUERY_HARNESS_HOST_TEST, same two
source files as the extra unit. JSONL sample re-emitted and asserted
(engine 5.6, axes non_shipping=1/stats=0, chunked_array, counts 12000/12000,
equal, identity_checks 2, gate_source PublishStagedNativeLayoutMaps). Copy
hash equality recorded in this report §0.

## 2. T2 — extra-unit compile vs the CURRENT supplier-composition-1 object

Probe: `extra_unit_compile_probe_supplier_composition_1.py` in this dir —
mechanism identical to the harness's `tests/extra_unit_compile_probe.py`
(loader-context `argv` template from the pinned
`ue4ss-native-notify-cancel-1/build-1/receipt.json`, source swap,
`argv[1:1] = ['-I', tick-file-owner-1/include, '-I', tick-witness-1/include]`,
`-MF` depfile, same `LLVM_CONFIG` handoff, pinned prospero-clang++), with the
classification baseline switched from the old `native-1/` object to the
supplier-composition-1 `run-1/` object.

Results (`target-probe-supplier-composition-1/`):

- Compile **exit 0**; object **70,816 bytes**, sha256
  `1eb11c7aaaab89e7ae2f1007db8186687fce273945955d7f44c85dd1cebaed8d` —
  byte-identical to the original probe's object (deterministic compile;
  expected, the compile does not read the composed object).
- Unit undefined symbols: 22 total.
  - **18 resolved by the composed runtime's own units**, including exactly the
    four engine seams `_ZN2RC6Unreal13FUObjectArray14GetNumElementsEv`,
    `_ZN2RC6Unreal17IsInGameThreadRawEv`, `_ZN2RC6Unreal18GetGameThreadIdRawEv`,
    `_ZN2RC6Unreal23IsGameThreadInitializedEv`, plus C++ ABI bits
    (`_Unwind_Resume`, logic_error, operator delete, exception machinery,
    guard acquire/release).
  - **4 already in the composed baseline**: `__cxa_atexit`, `__dso_handle`,
    `memcpy`, `sched_yield`.
- **Undefined delta vs the composed baseline: EMPTY — zero new undefined
  symbols after composition (prediction held).** Baseline = the composed
  object's own `nm --extern-only --undefined-only` set, measured 199 symbols,
  which cross-checks exactly against run-1's own receipt artifact
  `undefined-delta.json`: `current_count: 199`,
  `added: ["_DYNAMIC", "sceKernelGetModuleInfo"]` (the checkpoint-recorded
  197-symbol frozen SDK baseline + 2). Full lists in
  `target-probe-supplier-composition-1/symbol-classification-vs-supplier-composition-1.json`.
- Consequence: the harness composes into the CURRENT supplier-composition
  runtime through the existing
  `build_runtime.py --extra-unit native-query-harness=<harness>/src/NativeQueryHarness.cpp --extend-undefined-baseline`
  mechanism with an empty added set — no new SDK import, no `--extra-define`,
  all four engine seams resolve from composed units. The actual composition
  and link remain the controller's serialized step (not executed here).

## 3. T3 — S2-ARM.md (controller wiring)

Delivered: `S2-ARM.md` in this dir. Key content, all signatures read from
actual sources:

- **Seam**: `RC::Platform::NativeLayouts::PublishStagedNativeLayoutMaps(uint32_t major, uint32_t minor, bool non_shipping, bool stats)`
  (NativeLayoutBinding.hpp:10, impl NativeLayoutBinding.cpp:172; throws on
  every refusal mode). In the next composed runtime the publisher is reached
  via `InitializeVersionedContainer()` -> Prospero selection of
  `NativeProvidedVirtualVC` (Container.cpp.patch) ->
  `set_virtual_offsets()` (NativeProvidedVirtualVC.hpp:13-18) which calls
  publish with `(uint32_t)Version::Major, (uint32_t)Version::Minor,
  Version::NonShipping, Version::DebugBuild`.
- **One-line call site** in the bootstrap context: immediately after the
  `InitializeVersionedContainer();` statement in `UnrealInitializer::Initialize`
  (serialized startup; if publish threw, the line is never reached):
  `RC::Platform::QueryHarness::Global().ArmLayoutGate({Version::Major, Version::Minor, (uint8_t)(Version::NonShipping?1:0), (uint8_t)(Version::DebugBuild?1:0), <validated-profile StorageLayout>, "PublishStagedNativeLayoutMaps"});`
  — with a per-field discipline table (facts must equal the values just
  published; storage from worker-2's validated profile; label exact).
- **Disarm-on-throw**: publish-throw => arm unreachable => gate closed by
  construction (all dispatches refused `RefusedLayoutGateClosed`, counted);
  any post-arm startup throw => explicit
  `QueryHarness::Global().DisarmLayoutGate()` (noexcept, idempotent) in a
  catch-all that rethrows. No double arm is possible through this placement;
  there is no target-side re-arm path.
- Prerequisites restated: `InstallDefaultEngineBindings()` once before
  `RuntimeOwner::start()`; route `OnEngineTickDispatch()` on the serialized
  Tick dispatch after identity publication; controller driver pulls
  `TrySnapshot`/`SnapshotDispatchCounters` off-thread and emits JSONL through
  the existing console evidence path.

## 4. T4 — rung-1 → rung-2 acceptance bar (v3-promotion scenario, restated)

Rung advances from 1 (host plug-in tests, complete) to 2 (real-process
evidence) ONLY when ALL of the following come from the REAL Townfall process
through the console evidence path, after a v3 candidate is promoted to
validated layout facts by independent multi-anchor correspondence (incl. Tick
0x33ac8c0 / StaticConstructObject 0x890aa0 prologue byte-checks + segment
extent consistency — a predicate hit NEVER auto-establishes identity), the
pair is staged via the assembler, the runtime is composed with the harness
unit, and the S2 gate is armed per S2-ARM.md:

1. **The query record**: one terminal `status":"completed"` JSONL line from
   `TrySnapshot` with `count_source":"engine_uobject_array"`,
   `counts.recorded":2`, **both counts strictly > 10000** (exactly 10000 is
   `inconclusive_below_threshold` and does NOT advance the rung), and
   `monotonic` equal or non_decreasing — i.e. non-decreasing across the two
   CONSECUTIVE genuine ticks (the harness's no-leak rule guarantees
   consecutiveness: any refusal between reads discards the staged count and
   `refusals_between_counts` stays 0). `decreasing` is recorded honestly and
   does NOT advance the rung.
2. **Identity**: `identity_checks":2` — both reads under the same non-zero
   `game_thread_id_key`, consistent with the identity published by the first
   genuine `UGameEngine::Tick` (`IsInGameThreadRaw()==true` world).
3. **Refusal counters live, not just the happy path**: at least one real
   `refused_wrong_thread` (or `refused_identity_unknown`) count from a genuine
   non-game-thread dispatch attempt, and `refused_layout_gate_closed == 0` on
   the accepted path (proving the gate was armed by a genuinely successful
   `PublishStagedNativeLayoutMaps`, per S2-ARM.md).
4. **Facts match the validated profile**: engine major/minor equal the
   promoted facts (5.6), axes (`non_shipping`/`stats` 0/1) equal the resolved
   scan axes as published, storage equals the validated profile's GUObjectArray
   storage claim, and `gate_source":"PublishStagedNativeLayoutMaps"`.
5. **Lifetime preserved after the query**: `RuntimeOwner::stop()` returns
   true, joins, reaches Stopped/Failed within the bounded window; the harness
   is frozen post-terminal (`ticks_accepted` stays 2, no further engine reads).
6. **Provenance chain**: validated layout origin (v3 multi-anchor
   correspondence incl. segment extents), bounded accessor widths, confidence
   ≥ threshold; raw provenance preserved.

Standing honesty rules: candidate != validated; an Inconclusive or Decreasing
record is honest negative evidence, not a harness failure; unperformed runs
are never reported as passed; nothing here substitutes for the controller's
serialized composition, regression checks, or an actual console launch.

## 5. Files produced (all inside this output dir)

| File | Role |
|---|---|
| `CLAIM.json` | claim, written first |
| `harness-copy/src/*, harness-copy/tests/*` | byte-identical harness copy used for the suite run (hashes in §0) |
| `harness-copy/tests/host_test_bin` | rebuilt suite binary from the copy |
| `extra_unit_compile_probe_supplier_composition_1.py` | T2 probe (checkpoint-hash-gated) |
| `target-probe-supplier-composition-1/native-query-harness.o` | 70,816 B Prospero object (`1eb11c7a…`) |
| `target-probe-supplier-composition-1/symbol-classification-vs-supplier-composition-1.json` | 22 undefined / 18 composed-resolved / 4 baseline / delta EMPTY |
| `target-probe-supplier-composition-1/*.{stdout,stderr}, .d` | raw compile + nm provenance |
| `S2-ARM.md` | T3 controller wiring |
| `REPORT.md` | this file, written last |

## 6. Provenance

Written by worker-query-harness-integration on 2026-10-04 inside
`outputs/2026-10-04-zcode-team/worker-query-harness-integration/` only; no
existing file elsewhere modified; no network/console/target execution; 20 tool
calls; no nested subagents. Executed only: clang++ host suite (copy), pinned
prospero-clang++ compile, pinned llvm-nm, python3. Raw outputs preserved under
`target-probe-supplier-composition-1/`.
