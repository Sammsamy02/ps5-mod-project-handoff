# REPORT — worker-query-s2-arm (S2-arm + query-harness integration, ready to wire)

Workspace root for all relative paths: `/WORKSPACE/.codex/worktrees/translation-core/what`
Worker output dir: `outputs/2026-10-04-zcode-team/worker-query-s2-arm/`
Date: 2026-10-04. No console contact, no target execution, no network, no nested
subagents. Nothing outside this worker dir was written.

## 0. Result summary

| Task | Result |
|---|---|
| 1. Harness as extra unit vs composition-3 object `e9679aa5…` | PASSED: compiles clean under the exact extra-unit argv; 22 unit undefineds = 18 resolved by composed units + 4 already in the composed baseline; `NEW_UNDEFINED_AFTER_COMPOSITION_DELTA = []` (zero). Object byte-identical to the prior worker's probe (`1eb11c7a…`) — deterministic |
| 2. Exact one-line arm call + placement | DELIVERED (`integration-patch/WIRING.md` §1 + `patches/01-unreal-initializer-s2-arm.diff`) |
| 3. Disarm-on-throw catch-alls | DELIVERED (two exact try/catch blocks, same file; `WIRING.md` §2) |
| 4. module_start RuntimeAckConfig supplier | DELIVERED as the compilable extra unit `module-runtime-ack-supplier` (`new-units/`), WIRING-SPEC S1/S2/S3 verbatim; host tests 40/40, target compile probe clean |
| 5. Complete one-shot integration patch set | DELIVERED (`integration-patch/`): patched-init staging + review diff, new extra unit, generated composition-4 build script (+review diff), merged replace-receipt (20 files), extra-receipt, APPLY-ORDER.md with gate expectations |

## 1. Step 1 — harness probe vs the composition-3 object (measured)

`probe/extra_unit_compile_probe_composition_3.py` (same mechanism as the prior
worker's supplier-composition-1 probe, recheckpointed): composed object
`ue4ss-native-supplier-composition-3/run-1/coherent-tick-file-witness-runtime.o`
sha `e9679aa5b3bc3a38140c4dd4a63b566158bded3eb0a4387d4cc0e073b2d86350`,
117,712,568 bytes (checkpoint match). Result
(`probe/target-probe-composition-3/symbol-classification-vs-composition-3.json`):
`unit_undefined_count=22`, `resolved_by_composed_units=18`,
`already_in_composed_baseline=4`, delta EMPTY. The receipt cross-check reads the
run's own `undefined-delta.json` (8 current entries).

## 2. The S2-arm wiring (steps 2-3)

Delivered as a hash-attested staging copy of the composed initializer:
`staging/UnrealInitializer.cpp`
(sha `933d08a2f3e4dea1f80d216bd99779871b6e5b3ae1f26028d357d824051e2542`),
generated from the pinned base `66691a88…` by
`tools/make_initializer_staging.py` (every anchor asserted unique). Review diff:
`integration-patch/patches/01-unreal-initializer-s2-arm.diff` (122 lines).
Insertions, all `#ifdef __PROSPERO__`-guarded (a non-ProspERO compile stays
semantically identical):

1. `#include <NativeQueryHarness.hpp>` (self-contained harness contract header).
2. THE ONE-LINE ARM CALL — `WIRING.md` §1 has the exact line and the exact
   placement: first post-publish statement of `ScanGame()` (on Prospero the
   `#ifndef __PROSPERO__` offset block between it and
   `InitializeVersionedContainer();` preprocesses to nothing). Storage is
   `Unknown` — an honest recorded DEVIATION from S2-ARM.md's `ChunkedArray`
   example: the validated `townfall-ps5-ue56` profile supplies no
   GUObjectArray storage claim (verified in the profile JSON).
3. ScanGame-level try/catch-all with `DisarmLayoutGate(); throw;`.
4. Initialize-level try/catch-all covering serialized startup from after
   `ScanGame()` through `PostInitialize(UnrealConfig);` (engine-readiness).
5. The persistent per-tick dispatch route: recorded deviation from S2-ARM.md
   §5.2 ("the same callback that runs after publish_current()") — that callback
   (`HookedEngineTick`, `GameThreadInitializer`) SELF-RETires via
   `CallbackIterationData.RemoveSelf()` on its first successful game-thread
   execution (initializer source line 207 + comment at 1332-1333), so it can
   never observe two consecutive ticks. The patch registers one additional
   persistent `RegisterEngineTickPreCallback` (`QueryHarnessDispatch`) right
   after the initializer callback's registration, with the same ERROR_ID throw
   discipline; the thunk is `OnEngineTickDispatch()` only.
6. Dispatch thunk definition next to `HookedEngineTick`.

Compile-proven (`probe/compile_replaced_initializer_probe.py`, exit 0): the
staged source compiles clean under the EXACT affected-unit argv
(base-receipt `unreal-unrealinitializer` argv + the mechanism's `-I`
insertions; mirrored `-I` to the harness header per the dest-stage precedent).
`nm` classification: exactly four new imports — `Global`, `ArmLayoutGate`,
`DisarmLayoutGate`, `OnEngineTickDispatch` — all defined by the harness extra
unit at link. Dependency delta vs composition-3's real run-1 initializer .d:
plus `NativeQueryHarness.hpp` (in composition-4 it resolves as
`ue4ss-native-tick-file-owner-1/include/NativeQueryHarness.hpp`, flat file,
glob-pinned by the build script) plus the attested staged source itself;
nothing else.

## 3. The module_start ack supplier (step 4)

`integration-patch/new-units/runtime_ack_supplier.{hpp,cpp}` — extra unit
`module-runtime-ack-supplier`. `MakeRuntimeAckConfig` is the ONLY
`RuntimeAckConfig` birth site (status_root = exact `mfa_config::root` string;
`pid=getpid()`; `now_ms` from `clock_gettime(CLOCK_MONOTONIC)` in the vendored
`posix_now_ms` shape), `StartActivatedRuntimeWithAck` installs the harness
engine seams once and calls the 4-arg
`PrepareActivatedUE4SSRuntime(context, activation, expected_title, runtime_ack)`
then `start()`; `StopRuntimeWithAck` is the module_stop half. The future
game-bound entry keeps only `mfa_activate` + context construction (WIRING-SPEC
S5.5 unchanged). Target-proven (`probe/compile_supplier_extra_unit_probe.py`,
exit 0): clean under the exact extra-unit argv; every undefined import
classifies into composed units / the harness unit / the frozen import contract
(`probe/supplier-probe/classification.json`); `getpid` and `clock_gettime` were
nm-verified against the pinned SDK target libs with the same four providers as
composition-3's `sceKernelGetModuleInfo` (libkernel, libkernel_stub_weak,
libkernel_sys, libkernel_web).

## 4. Host-level testing (required before delivery)

`host-tests/run_host_tests.sh` -> `HOST_TESTS_PASSED`, 40/40 checks, exit 0
(`host-tests/host_test_output.txt`). It compiles the REAL canonical
`runtime_owner.cpp` (bundle-01 bytes == composition-3 staged bytes) and the
REAL vendored `modforge_activation.c` plus the supplier's host-visible half and
the harness in host-test mode. Coverage: ack-config birth facts (exact root
string, getpid, monotonic ms, empty/null degradation); the real sink file chain
(`modforge-runtime-status` header, `session\tpid\tnow_ms` line,
activated/engine-ready/mods-started, single `stopped` on double stop); real
`RuntimeOwner` start/stop/join over a lifecycle-correct stub backend (no failed
line on the clean path; engine-ready correctly absent — it is backend-owned);
the arm-line facts end-to-end to a terminal record (storage Unknown, axis 0/1
facts, provenance label, both consecutive counts); and the disarm semantics
(post-disarm dispatch refused `RefusedLayoutGateClosed` + counted, staged count
discarded to Idle, no partial record, disarm not sticky, double arm refused
`std::logic_error`).

## 5. The one-shot composition-4 patch set (step 5)

- `integration-patch/build/build_runtime_composition4.py`
  (sha `7a83cb3830777f86c228930929537d7dcfef742acd175b6c081f26c022f0d645`,
  py_compile OK) — generated copy of composition-3's build script plus ONLY
  (a) `--replace-runtime-unit` (bounded to freshly compiled affected units;
  `unreal-unrealinitializer` is the only target; refuses non-affected or
  already-staged targets; requires the receipt) and (b) the rr attestation
  fallback in the extras dependency loop, identical to the bootstrap loop's
  existing fallback (an unpinned `.superpowers` dependency is admitted ONLY
  with receipt-attested bytes — never silently). Frozen gates byte-identical;
  review diff `composition4-vs-composition3-build-script.diff` (70 lines).
- `integration-patch/receipts/composition4-replace-receipt.json` —
  composition-3's bootstrap receipt (byte-identical inclusion) + staged
  initializer + the five W/include staged headers + the seven port headers of
  the supplier closure (20 files, all hashes self-verified; the five W/include
  destinations intentionally refuse the build until Step-1 staging exists).
- `integration-patch/receipts/composition4-extra-receipt.json` — the two new
  extra units' sources (harness bytes unchanged from worker-query-harness).
- `integration-patch/APPLY-ORDER.md` — the one-shot command and gate
  expectations: units 368, fresh 15, undefined delta added exactly
  `[clock_gettime, getpid]`, removed/changed empty. Expected status string and
  the packaging/ledger notes for the controller.

## 6. Limits (stated honestly)

- Host + target COMPILE evidence only. The composition-4 link, packaging, and
  any console load remain the controller's serialized steps; nothing here has
  executed on a target.
- The harness's 358 host checks were not re-run (they write into
  worker-query-harness's tree, read-only for this worker); the deliverable
  bytes are hash-pinned unchanged, and this worker's 40-check suite covers the
  added wiring semantics on host.
- rung-1->2 acceptance (worker-query-harness REPORT.md §5) is untouched by
  this work and still requires real in-game evidence after the controller
  composes and runs.
- The probe compiles of the staged initializer/supplier used dest-stage
  mirrored `-I` paths (the real composition stages the headers into
  W/include); byte-identity of the staged headers is hash-enforced by the
  receipt either way.

## 7. Files written (all under the worker dir)

CLAIM.json, REPORT.md, `integration-patch/` (WIRING.md, APPLY-ORDER.md,
patches/01-unreal-initializer-s2-arm.diff, build/build_runtime_composition4.py
+ review diff, new-units/runtime_ack_supplier.{hpp,cpp},
receipts/*.json), `staging/UnrealInitializer.cpp` + `staging/include/` (5
mirrors), `host-tests/` (test + runner + captured output + bin),
`probe/` (3 probe scripts + initializer-probe/ + supplier-probe/ +
target-probe-composition-3/), `tools/` (3 generators).
