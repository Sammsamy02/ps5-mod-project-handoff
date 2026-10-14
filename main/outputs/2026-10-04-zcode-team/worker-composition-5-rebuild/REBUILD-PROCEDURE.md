# REBUILD-PROCEDURE — composition-5 (module_start wiring into the composed runtime)

Worker: worker-composition-5-rebuild · 2026-10-04 · proposal/procedure only
(no build executed here; the wiring delivery had not landed when this was
prepared — everything below is ready so the rebuild is ONE command once it does).

## 0. Where the current module_entry actually lives (read-back findings)

The composition-4 composed runtime object does **not** contain `module_entry`:

* `…ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-4/run-3/runtime.map`
  (the `ld.lld -r` link map, 368 input objects) has **no** `module_start` /
  `module_entry` / `module_stop` input — the composed object stops at the
  supplier/ack/harness/bootstrap pieces.
* The current module entry is compiled at **PRX-packaging time**, by the fixed
  packaging script `outputs/2026-10-04-zcode-team/worker-prx-link-gaps/package_prx.py`
  (sha256 `e6fbe6c5011b498e0c26ee88bdc6c84aacab204b6ccad5696a78353afef5c313`),
  inside `link_pipeline()`:
  * `BOOTSTRAP_UNITS` entry `"lifecycle/module_entry.c"` (line ~134) resolved
    against `PIN["lineage"]` =
    `.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1`.
  * Confirmed in the packaging record
    `worker-composition-4-fix/prx-candidate-composition4-1/build-receipt.json`
    (`commands`): `prospero-clang … -c …/native-full-runtime-aligned-entry-1/lifecycle/module_entry.c -o …/build/module_entry.o`.
  * That lineage file is byte-identical (`diff` clean) to
    `outputs/ps5-mod-bridge/native-runtime-startup/lifecycle/module_entry.c`.
* So "rebuilding the composition with the wiring" means: **replace the
  packaging-time entry unit**; every composition unit stays unchanged.

Supporting facts verified for the wired entry:

* All four startup-chain symbols are **already defined** by the composed
  run-3 object (`run-3/exports.stdout`): `mfa_activate` (T),
  `RC::Platform::NativeModSource{C1,C2}` (T),
  `modbridge::native::PrepareActivatedUE4SSRuntime(…)` (T),
  `modbridge::native::RuntimeOwner::start/stop/initialize` (T). A wired entry
  that calls only these resolves them internally → **zero new undefined
  imports** → the packaging import pre-check stays green.
* `modbridge_ctor_phase` / `modbridge_ctor_value` (defined today only in
  `module_entry.c`) are **not referenced** by the composed object
  (`run-3/undefined.stdout` has no `modbridge_ctor*`), so a rewritten entry may
  drop them without creating imports. (The driver records this as a note, not
  a gate.)
* The packaging static check requires the `force_align_arg_pointer`
  realignment prologue bytes at `module_start`/`module_stop` in the FINAL PRX
  (`entry_evidence()` in package_prx.py) — the new entry MUST keep
  `__attribute__((visibility("default"), force_align_arg_pointer))` on both
  functions. The driver enforces this pre-flight (text-level) and the
  packaging script enforces it byte-level.

## 1. Pre-conditions (all must hold before running anything)

| # | item | expected |
|---|---|---|
| P1 | wiring delivery | `outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_module_entry.c` exists (delivered by worker-module-start-wiring; only its CLAIM.json existed at preparation time) |
| P2 | entry attestation | the wiring worker's REPORT.md states the file's sha256; you pass it as `--entry-sha256`. No rebuild without attestation |
| P3 | fixed composition script | `worker-composition-4-fix/build_runtime_composition4_fixed.py`, sha256 `36cc8199b7e5a2528badd4da53ff3750075cbbe4d76ca115490b50a008ed8ff3` (driver re-checks) |
| P4 | fixed packaging script | `worker-prx-link-gaps/package_prx.py`, sha256 `e6fbe6c5011b498e0c26ee88bdc6c84aacab204b6ccad5696a78353afef5c313` (driver re-checks) |
| P5 | composition-4 run | `…/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-4/run-3/` intact (receipt + 117,779,552-byte object, sha `45dd9901…`) — needed only for `--reuse-composition-run` |
| P6 | entry export surface | `int module_start(…)` and `int module_stop(…)` both defined, `force_align_arg_pointer` present on both |

## 2. The one-step rebuild (when the wiring lands)

From the workspace root `/WORKSPACE/.codex/worktrees/translation-core/what`:

```bash
python3 outputs/2026-10-04-zcode-team/worker-composition-5-rebuild/build_runtime_composition5.py \
  --entry-source outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_module_entry.c \
  --entry-sha256 <sha256-from-wiring-REPORT>
```

This runs, in order:

1. **Gate 0** — sha-verify both fixed scripts (P3, P4).
2. **Gate 1** — entry attestation + pre-flight (P2, P6): digest must match
   `--entry-sha256`; both entry exports present; alignment attribute present.
   Evidence → `worker-composition-5-rebuild/staging/entry-preflight.json`.
3. **Phase A (default)** — re-run the UNMODIFIED fixed composition script with
   the exact APPLY-ORDER Step 2 argv (identical staging copies, extra units,
   receipts; only `--output` changes) into
   `…/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-5/run-1`.
   All hash gates of the composition script itself apply; expected result:
   `NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED`, 368 units,
   15 fresh — the same composed runtime as composition-4 (all units unchanged;
   the object digest may differ only via embedded build paths).
4. **Gate 2** — chain-symbol availability: `llvm-nm --extern-only
   --defined-only` on the fresh composed object must define (exact or mangled
   prefix): `mfa_activate`,
   `_ZN9modbridge6native12RuntimeOwner5startEv`,
   `_ZN9modbridge6native28PrepareActivatedUE4SSRuntimeE`,
   `_ZN2RC8Platform15NativeModSourceC`. Evidence →
   `staging/chain-symbols.json`. Add more with `--require-symbol`.
5. **Phase B** — build the symlink-overlay lineage mirror
   `worker-composition-5-rebuild/staging/lineage-overlay-1/` (everything
   symlinked to the real `native-full-runtime-aligned-entry-1` except
   `lifecycle/`, which is a real dir of symlinks plus the attested entry copied
   to `lifecycle/runtime_module_entry.c` — relative `#include "../…"` paths keep
   resolving). If the entry includes headers from outside the lineage, pass
   `--stage-header REL=SRC` (e.g.
   `--stage-header runtime_ack_supplier.hpp=outputs/…/new-units/runtime_ack_supplier.hpp`);
   the overlay root is on the `-I` path and relative layouts are preserved.
6. **Phase B exec** — load the UNMODIFIED packaging script and adjust only its
   PIN block (the exact `package_composition4.py` precedent), plus one
   same-class pinned-values adjustment: `BOOTSTRAP_UNITS`
   `"lifecycle/module_entry.c"` → `"lifecycle/runtime_module_entry.c"`, and
   `PIN['lineage']` → the overlay. Also pinned:
   `composition_dir` → the composition-5 run dir,
   `composition_object_{sha256,bytes}` → from its receipt,
   `composition_checkpoint` → `zcode_supplier_composition_5`,
   `ledger` → provisional mirror
   `worker-composition-5-rebuild/transport-ledger.composition5-mirror.json`
   (real ledger stays controller-owned).
   Then `package_prx.py --link --out worker-composition-5-rebuild/prx-candidate-composition5-1`.

**Faster variant (packaging-only):** because the composition units are
unchanged, Phase A may be skipped and the attested composition-4 run-3 object
re-used:

```bash
python3 outputs/2026-10-04-zcode-team/worker-composition-5-rebuild/build_runtime_composition5.py \
  --entry-source outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_module_entry.c \
  --entry-sha256 <sha256-from-wiring-REPORT> \
  --reuse-composition-run
```

The driver still re-verifies the run-3 receipt status and object hash before
use. Default full variant is preferred for a clean lineage from a fresh run
dir; both produce the same composed input.

## 3. What is replaced / what is untouched

| artifact | disposition |
|---|---|
| packaging entry unit `lifecycle/module_entry.c` (aligned-entry lineage) | REPLACED in the link by the staged `lifecycle/runtime_module_entry.c` — the on-disk lineage file is never modified (overlay copy only) |
| composed runtime (368 units: supplier, lmf extras, query harness, ack supplier, S2-arm initializer, ack-enabled `runtime_owner`/`ue4ss_backend`, all retained units) | UNCHANGED (same attested staging copies and receipts; fresh run dir under composition-5, or re-used run-3 with `--reuse-composition-run`) |
| fixed build + packaging scripts | executed UNMODIFIED (sha-gated); only PIN values / the one BOOTSTRAP_UNITS list entry are adjusted by the driver, the documented composition-3/4 pinned-values pattern |
| real transport ledger | untouched; provisional mirror written under this worker's dir; controller must reconcile `zcode_supplier_composition_5` |
| anything else on disk | untouched; run/candidate dirs must not pre-exist (script-owned `exist_ok=False` semantics preserved) |

## 4. Verification checklist (post-build, all in the packaging output)

The packaging script's own fail-closed checks ARE the acceptance set (17/17,
gaps `[]`, same complete set as the accepted composition-4/3 precedent):

| check | expectation | failure meaning |
|---|---|---|
| `receipt.json` (composition run) | status `NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED`, object hash matches | composition inputs drifted → reconcile, never bypass |
| lineage verification | `LINEAGE_VERIFIED`; `bootstrap_units_all_present` — now attests the STAGED entry digest | wiring delivery changed after attestation → stop |
| combined-object pre-check | unresolved set == frozen 191 imports + 5 extended (`sceKernelDlsym`, `sceKernelGetModuleInfo`, `sceKernelLoadStartModule`, `sceKernelStopUnloadModule`, `qsort`); `unexpected`/`missing`/`weak_unexpected` all empty | the wired entry introduced a NEW import (e.g. a libc call not in the contract) → the entry must be corrected, not the gate |
| entry alignment evidence | `force_align_arg_pointer` realignment prologue found at BOTH `module_start` and `module_stop` in the final PRX | entry lost the alignment attribute → correct the entry |
| static checks | 17/17 PASS, `gaps: []` (`STATIC_CHECKS_PASSED`) | any gap → fail closed, no promote |
| candidate PRX | `prx-candidate-composition5-1/modbridge_supplier_composition.prx`; record sha256 + bytes in the run notes | — |

Then, BEFORE any in-game claim: this procedure produces a **candidate PRX
only**. In-game evidence still requires the separate controller-owned
console rung steps (APPLY-ORDER Step 4 pattern). No completion or in-game
claim may be made from this rebuild alone.

## 5. Failure playbook (no bypasses exist)

* `entry attestation mismatch` → the delivered file differs from the REPORT
  digest; get a fresh attestation from worker-module-start-wiring.
* `entry does not define both module_start and module_stop` / alignment
  pre-flight → the wiring delivery is incomplete; send back with this check's
  evidence (`staging/entry-preflight.json`).
* `startup-chain symbol NOT defined` → the composition lost a chain provider
  (bootstrap replacement or extra unit missing in this tree); reconcile the
  composition first — do NOT let the entry import the symbol.
* `combined object unresolved set differs` (in packaging) → same class as
  above at byte level; the entry references something outside the frozen
  contract; correct the entry.
* `lineage gaps` / `bootstrap_units_all_present` false → overlay staging
  problem; inspect `staging/lineage-overlay-1/`.
* Any fixed-script sha drift (P3/P4) → someone edited a pinned script; stop
  and reconcile provenance first.
* Interrupted run → delete ONLY the not-yet-attested fresh run/candidate dir
  this driver created (they are refused if pre-existing), fix the cause, re-run.

## 6. Provenance / non-interference

* Writes: only `outputs/2026-10-04-zcode-team/worker-composition-5-rebuild/`
  (this procedure, driver, staging, mirror, candidate dir) plus the
  script-owned fresh run dir `ue4ss-native-supplier-composition-5/run-1`.
* Reads elsewhere; nothing outside the write set is modified.
* No console contact, no target execution, no nested subagents, no network.
* Raw hashes quoted in this file were read back from the on-disk artifacts
  during preparation (run-3 receipt/exports/map, packaging build-receipt,
  lineage vs outputs lifecycle `diff`).
