# APPLY-ORDER — composition-4 one-shot (controller)

Everything in this directory is a ready-to-apply patch set for the FINAL
composed runtime. All gates stay intact: nothing here modifies a pinned file,
a frozen receipt, or a pinned build script on disk; the composition consumes
hash-attested staging copies (the exact, previously measured
worker-composition-3 reconciliation precedent). No step touches a console.

## Step 0 — verify base state

```
shasum -a 256 .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-3/run-1/coherent-tick-file-witness-runtime.o
# expect e9679aa5b3bc3a38140c4dd4a63b566158bded3eb0a4387d4cc0e073b2d86350  (117712568 bytes)
shasum -a 256 .superpowers/native-dependency-spike/ue4ss-native-tick-initializer-retry-1/src/UnrealInitializer.cpp
# expect 66691a8860ce4230b65cfebc78b0e2e0fc7be283f86a24969261028e79b94b6b  (pre-patch pin)
shasum -a 256 outputs/2026-10-04-zcode-team/worker-query-s2-arm/staging/UnrealInitializer.cpp
# expect 933d08a2f3e4dea1f80d216bd99779871b6e5b3ae1f26028d357d824051e2542  (patched staging copy)
shasum -a 256 outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/build/build_runtime_composition4.py
# expect 7a83cb3830777f86c228930929537d7dcfef742acd175b6c081f26c022f0d645
```

Review `patches/01-unreal-initializer-s2-arm.diff` (122 lines) and
`composition4-vs-composition3-build-script.diff` (70 lines) before applying.
The staged initializer is byte-complete in `../staging/`; the diff is for review.

## Step 1 — stage five headers into the extra-unit include dir (new files only)

```
WHAT=<workspace root>; TEAM=$WHAT/outputs/2026-10-04-zcode-team; WINC=$WHAT/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include
cp $TEAM/worker-query-harness/src/NativeQueryHarness.hpp                                                        $WINC/
cp $TEAM/worker-composition-3/bootstrap-stage/native-coherent-runtime-1/source/bootstrap/runtime_owner.hpp      $WINC/
cp $TEAM/worker-composition-3/bootstrap-stage/native-coherent-runtime-1/source/bootstrap/activated_mod_source.hpp $WINC/
cp $TEAM/worker-composition-3/bootstrap-stage/native-coherent-runtime-1/source/bootstrap/ue4ss_backend.hpp      $WINC/
cp $TEAM/worker-installer-ps5io/vendored/modforge_activation.h                                                  $WINC/
```

These five names do not exist in `include/` today (verified), are flat files
(the build script's `include/*` glob pins and hash-checks them), and shadow
nothing (no affected unit includes these names). Required hashes are pinned in
`receipts/composition4-replace-receipt.json` — a drift refuses the build.

## Step 2 — compose (the one-shot command)

Relative to the workspace root:

```
python3 outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/build/build_runtime_composition4.py \
  --output .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-4/run-1 \
  --rust-receipt .superpowers/native-dependency-spike/ue4ss-native-tick-witness-1/native-1/receipt.json \
  --host-receipt .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/host-final-1/receipt.json \
  --extra-unit lmf-live-module-facts=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_facts.cpp \
  --extra-unit lmf-sce-ops=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_os_ops_sce.cpp \
  --extra-unit lmf-loader-adapter=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp \
  --extra-unit lmf-base-scan=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_base_scan.cpp \
  --extra-unit native-query-harness=outputs/2026-10-04-zcode-team/worker-query-harness/src/NativeQueryHarness.cpp \
  --extra-unit module-runtime-ack-supplier=outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.cpp \
  --extra-define LMF_DISABLE_DL_RETENTION \
  --extra-receipt outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/receipts/composition4-extra-receipt.json \
  --extend-undefined-baseline \
  --replace-bootstrap-unit runtime_owner=outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-runtime-1/source/bootstrap/runtime_owner.cpp \
  --replace-bootstrap-unit ue4ss_backend=outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-pad-runtime-1/build-1/source/ue4ss_backend.cpp \
  --replace-receipt outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/receipts/composition4-replace-receipt.json \
  --replace-runtime-unit unreal-unrealinitializer=outputs/2026-10-04-zcode-team/worker-query-s2-arm/staging/UnrealInitializer.cpp
```

Gate expectations (measured by this worker's probes; STOP and reconcile on any
other result — never bypass):

| gate | expectation |
|---|---|
| run status | `NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED` |
| runtime_compilation_units | 368 (366 from composition-3 + harness + supplier; the initializer was already among the 7 affected fresh units, now consumed from the attested staging copy with `runtime_replaced: true`) |
| freshly_compiled_units | 15 (composition-3's 13 + the 2 new extra units) |
| undefined delta added | `[clock_gettime, getpid]` ONLY — both measured with providers `libkernel.so, libkernel_stub_weak.so, libkernel_sys.so, libkernel_web.so` (same provider family as composition-3's `sceKernelGetModuleInfo`); `memset` and every string/exception symbol is already in the composed baseline |
| undefined delta removed/changed | `[]` / `{}` |

## Step 3 — packaging (controller serialized step, unchanged mechanics)

Re-run `package_prx.py --verify-only` then `--link` against the composition-4
run dir, using the same documented pinned-values adjustment pattern as
`worker-composition-3/package_composition3.py` (composition_dir -> `…composition-4/run-1`,
object sha/bytes from the run receipt, checkpoint `zcode_supplier_composition_4`,
real-ledger entry still owed by the controller). Static-check expectations are
unchanged (20/20 PASS, gaps `[]`).

## Step 4 — in-game evidence (rung 1 -> 2)

The acceptance bar is worker-query-harness REPORT.md §5, unchanged. The
negative-evidence channel: any tick dispatch while the gate is closed (startup
failed, or pre-arm) must show up ONLY as `refused_layout_gate_closed` counters
with no partial record; a successful publish arms the gate and the first two
post-publication ticks carry the two accepted reads. Controller driver duties
(TrySnapshot / SnapshotDispatchCounters / FormatRecordJsonLine into the console
evidence path) are unchanged and need no target code from this patch set.

## Provenance / non-interference

- The pinned spike `UnrealInitializer.cpp` stays byte-identical on disk
  (`66691a88…`); all frozen `validate_inputs` pins keep passing.
- `build_runtime.py` (pinned) and `build_runtime_composition3.py` are not
  modified; composition-4 runs from the new generated copy (diff for review in
  this directory).
- The spike tree receives ONLY the five new flat include files (Step 1).
- `runtime-ack.diff` bundle-01 canonical-tree state (composition-3 REPORT §1)
  is untouched by this patch set.
