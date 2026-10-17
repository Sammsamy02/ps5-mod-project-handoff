# REPORT — worker-composition-5-rebuild

**Verdict: COMPOSITION-5 REBUILD FULLY PREPARED.** The wiring delivery
(worker-module-start-wiring) had NOT landed when this worker ran (only its
CLAIM.json exists), so no build was executed. Everything that can be prepared
was prepared and mechanically rehearsed: the one-step build driver, the exact
rebuild procedure, and every gate the rebuild will hit. When the wiring lands,
the rebuild is ONE command.

## 1. Key read-back finding (corrects the task's framing, documented honestly)

The composition-4 composed runtime object does **not** contain module_entry,
and the composition build script has no entry hook to wrap:

* `…/ue4ss-native-supplier-composition-4/run-3/runtime.map` (368-object link
  map) contains NO `module_entry`/`module_start` input — grep clean.
* The current module entry is compiled at **PRX-packaging time** by
  `worker-prx-link-gaps/package_prx.py` (sha `e6fbe6c5…`, verified on disk)
  from the `BOOTSTRAP_UNITS` entry `"lifecycle/module_entry.c"` resolved
  against `PIN["lineage"]` = `native-full-runtime-aligned-entry-1`
  (compile command visible in `worker-composition-4-fix/
  prx-candidate-composition4-1/build-receipt.json`).
* That lineage file is byte-identical (`diff` clean) to
  `outputs/ps5-mod-bridge/native-runtime-startup/lifecycle/module_entry.c`
  (the probe-lifecycle entry: guard, diagnostic channel, `probe_lifecycle_start`
  — no startup-chain invocation, exactly the "no wiring" state described).

Therefore composition-5 = **same composition, replaced packaging-time entry
unit**, using the established pinned-values adjustment pattern.

## 2. Facts verified that make the wiring safe to link (evidence, not hope)

* All four startup-chain symbols are already DEFINED by the composed run-3
  object (`run-3/exports.stdout`): `mfa_activate` (T),
  `RC::Platform::NativeModSource{C1,C2}` (T),
  `modbridge::native::PrepareActivatedUE4SSRuntime(…)` (T),
  `modbridge::native::RuntimeOwner::start/stop/initialize` (T). A wired entry
  calling these adds ZERO new undefined imports — the packaging import
  pre-check (frozen 191 + 5 extended) stays green.
* `modbridge_ctor_phase`/`modbridge_ctor_value` (today defined only in
  module_entry.c) are NOT referenced by the composed object
  (`run-3/undefined.stdout`) — the new entry may drop them without creating
  imports (recorded as a note, not a gate).
* The packaging static check measures the `force_align_arg_pointer`
  realignment prologue bytes at `module_start`/`module_stop` in the FINAL PRX —
  the new entry must keep the attribute on both exports (driver gates this
  pre-flight; packaging enforces it byte-level).

## 3. Deliverables (this dir)

* `CLAIM.json` (written first), `REPORT.md` (this file, last).
* `build_runtime_composition5.py` — the one-step driver (syntax-checked,
  gates dry-run):
  * Gate 0: sha-verify both fixed scripts before load/exec
    (`36cc8199…` composition, `e6fbe6c5…` packaging — both verified matching
    on disk during preparation).
  * Gate 1: entry attestation (`--entry-sha256` from the wiring REPORT) +
    export-surface pre-flight → `staging/entry-preflight.json`.
  * Phase A (default): re-run the UNMODIFIED fixed composition script with the
    verbatim APPLY-ORDER Step 2 argv (only `--output` changes) into
    `ue4ss-native-supplier-composition-5/run-1` — all units unchanged, all of
    the script's own hash gates intact. `--reuse-composition-run` instead
    re-verifies and re-uses attested run-3 (object is unchanged by definition).
  * Gate 2: `llvm-nm` proof that every required chain symbol (4 defaults,
    extensible via `--require-symbol`) is DEFINED by the fresh composed object.
  * Phase B: symlink-overlay lineage mirror (relative `#include "../…"`
    resolves; optional `--stage-header REL=SRC` for headers outside the
    lineage), then exec the UNMODIFIED packaging script with PIN adjustments
    (`composition_dir/object sha/bytes/checkpoint
    zcode_supplier_composition_5/provisional ledger mirror/lineage→overlay`)
    plus one same-class pinned-values adjustment: `BOOTSTRAP_UNITS`
    `lifecycle/module_entry.c` → `lifecycle/runtime_module_entry.c`.
    Output: `prx-candidate-composition5-1/modbridge_supplier_composition.prx`.
* `REBUILD-PROCEDURE.md` — full procedure: pre-conditions, one-step commands
  (full + packaging-only variant), replace/untouched table, verification
  checklist (17/17 static checks = the accepted composition-4/3 set),
  failure playbook, provenance/non-interference.

## 4. Mechanics rehearsed (no build performed)

Using the real fixed packaging module and the real (unwired) entry as a
stand-in, then fully cleaned up: `BOOTSTRAP_UNITS` swap OK (10 units, in-place);
overlay symlink tree built — `lifecycle/../probe/probe_api.h` resolves to the
real file and all three lineage `-I` dirs are usable; pathlib join semantics
confirmed. Rehearsal overlay deleted; the driver refuses a pre-existing real
overlay (`lineage-overlay-1`), so the later real run is unblocked. Driver
missing-delivery path exits cleanly with a pointed message.

## 5. Honest scope

* No wiring delivery existed → no composition-5 run, no PRX candidate, no
  ledger claim. The provisional `zcode_supplier_composition_5` ledger entry is
  written only by the driver at real build time, into a MIRROR file; the real
  ledger stays controller-owned.
* No console contact, no target execution, no nested subagents, no network.
  Writes confined to this worker dir (+ the script-owned fresh run/candidate
  dirs at real build time).
* In-game evidence remains a separate controller-owned step after a 17/17-PASS
  candidate exists; nothing here claims runtime success.
* Call budget: well under the 60-call bound.
