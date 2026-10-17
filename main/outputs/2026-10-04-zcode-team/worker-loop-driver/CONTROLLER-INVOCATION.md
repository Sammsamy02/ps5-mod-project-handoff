# CONTROLLER-INVOCATION — loop_driver.py real run

Worker: worker-loop-driver (author of the AUTOMATIC repair-loop runner).
Driver: `outputs/2026-10-04-zcode-team/worker-loop-driver/loop_driver.py`
Host-only. Never stages, never injects, never console-dispatches.

## 0. Preconditions (controller owns all of them)

1. The current edit round has SETTLED. The self-test (2026-10-06T00:4xZ) caught
   edits still landing live: seedvalidation-copy-2/3 moved
   `db07ac60 -> 8a9a1c83` between snapshot and check. Do not rebaseline or
   run --full mid-edit.
2. Stage-1 known defects are resolved or consciously deferred (they are NOT
   whitespace-class; the driver will stop on them — that is by contract):
   - initializer TU: `NativeQueryHarness.hpp` file not found
     (units/UnrealInitializer.cpp:60 under the pinned base-receipt argv of
     unit `unreal-unrealinitializer`; the composition-safe resolution is a
     controller decision — e.g. drop-in header beside the unit, quote-include
     resolving from the includer dir — never a driver-side edit).
   - bridge TU (59db9dcd, newer than the ec53442b v5 note):
     `SeedValidation.hpp:377 missing field 'failure_class' initializer` under
     `-Wextra -Werror`, and `runtime_start_bridge...cpp:826 expected
     unqualified-id` colliding with the SDK `errno` macro.
3. Baseline re-snapshot (conscious, after the round settles):

   ```
   python3 outputs/2026-10-04-zcode-team/worker-loop-driver/loop_driver.py --rebaseline
   ```

   (archives the old baseline under `baseline/`, records old->new sha moves in
   `logs/rebaseline.log`). Cheap preflight without composition:

   ```
   python3 outputs/2026-10-04-zcode-team/worker-loop-driver/loop_driver.py --cycle settle-check
   ```

## 1. THE real-run command (exact)

```
cd /WORKSPACE/.codex/worktrees/translation-core/what
python3 outputs/2026-10-04-zcode-team/worker-loop-driver/loop_driver.py \
    --cycle launch11-full-<N> --full --lineage-ready
```

- `--full` — stage 3 (launch11_one_pass.py --full: unit sha verification,
  admission gate, receipt regen, prx-candidate-l11-1 and -2, ODR, finalize,
  gate v2 x2, determinism twin, accessor verification). Default OFF; without
  it stage 3 is a RECORDED refusal, never executed.
- `--lineage-ready` — controller asserts worker-luamod-lineage pins are done;
  enables stage 2 (regression_preflight, `--exec` mode because --full).
  Verified green lineage-only (exit 0, 4/4 checks verified) in self-test
  part D.
- Optional: `--auto-repair` arms the whitespace-only repair loop (missing EOF
  newline, trailing whitespace on raw-string-free files; ledgered, backed up,
  re-proven; any other class = STOP). `--no-cache` bypasses stage caching.

## 2. Gate discipline for the new `syscall` undefined symbol (coordinator note)

The v5 bridge edit introduces EXACTLY ONE new undefined symbol, `syscall`
(`::syscall(SYS_mincore,...)` in `bridge_kernel_range_verdict`). SDK target
libc declares it (`target/lib/libc.a` syscalls.o, `T syscall`).

- `--extend-undefined-baseline` is ALREADY ARMED: launch11_one_pass ->
  build_runtime_composition6_l11.py `compose_argv` passes it to the fixed
  edition unconditionally; that is the mechanism that records
  added-symbol-resolves-to-pinned-SDK-library. No new CLI flag exists or is
  needed for the runtime-object delta gate.
- If an imports/delta gate refuses the new symbol and the armed baseline
  extension does NOT cover the refusing gate (e.g. the packaging import
  contract / gate v2 D-checks, which this flag does not govern): the gate is
  NEVER edited; STOP and return to the controller for a conscious recorded
  decision.
- Stage 4 of the driver records a post-run `undefined_delta_observation`
  (added symbols + expected_bootstrap_resolutions + providers per run dir)
  and a `syscall_note` stating where `syscall` actually landed.

## 3. Expected receipts on success (all under
`outputs/2026-10-04-zcode-controller/launch11-combined/` unless noted)

| receipt | produced by | expectation |
|---|---|---|
| `prx-candidate-l11-1/`, `prx-candidate-l11-2/` | the two passes | fresh dirs (old ones moved `-aside-<ts>`) |
| `gate-receipt-l11-prx-candidate-l11-1.json` | gate v2 | verdict GO, or `NO-GO (failed: ['D1'])` (runner-documented pre-twin acceptance) |
| `gate-receipt-l11-prx-candidate-l11-2.json` | gate v2 (compare-dir = candidate-1) | verdict `GO (offline gate)`, `failed_checks: []` |
| run dirs `run-l11-1/`, `run-l11-2/` (under `.superpowers/.../ue4ss-native-supplier-composition-6/`) | fixed edition | `receipt.json` runtime_sha256 EQUAL across both (determinism) |
| `receipts/luamod-comparator.json` | runner post-build | luamod unit object == M-0 comparator 813b249d... |
| `receipts/accessor-verification-prx-candidate-l11-{1,2}.json` | runner | verdict PASS x3 classes |
| `receipts/odr-l11.json` | ODR gate | pass |
| `comp6-integration/change-manifest.finalized.json` | finalize | refreshed |
| driver report `outputs/2026-10-04-zcode-team/worker-loop-driver/results/loop-report-launch11-full-<N>.{json,md}` | the driver | exit 0; stage rows measured; stage-4 checks all `[x]` |

Driver exit codes: 0 ok (skips/refusals are recorded, never silent);
1 stage failed after bounded repair; 2 lineage drift (six-file manifest or
fail-closed gate); 3 contract error.

## 4. Timing

Every stage is timed through `worker-stage-metrics/timing_hooks.py`
(modbridge.stage-timing/v1) into the loop report: canonical `tests`
(stage 2), `compilation` (stage 3; packaging measured inside the
single-process runner and recorded as such — never double-counted),
`staging`/`injection`/`initialization` recorded `not_reached` (console-side;
epoch_view.py owns those numbers), custom `custom-cheap-prereq` (stage 1) and
`custom-gate-receipts` (stage 4). Self-test measured stage 1 at ~5.9 s
(5 syntax targets: 1.17-2.23 s per big TU, 0.15-0.26 s header/bridge).

## 5. Cache

`results/cache/stage1-cheap|stage3-composition/<key>.json`, keyed by input
sha sets. Unchanged inputs reuse and PRESERVE the previous verified evidence
(cache_hit + preserved_from_utc recorded; no re-measurement). Any controller
edit to the six files, the runner, the drivers, or EDITIONS.json changes a
key and forces a real re-run.
