# REPORT — worker-composition-3 (composition-3 recomposition + ack-enabled packaging)

Workspace root for all relative paths: `/WORKSPACE/.codex/worktrees/translation-core/what`
Worker output dir: `outputs/2026-10-04-zcode-team/worker-composition-3/`
Date: 2026-10-04. No console contact, no target execution, no nested subagents.

## 0. Result summary

| Step | Result |
|---|---|
| Read WIRING-SPEC (`worker-ack-integration-deep/controller-patch/02-composition-wiring/WIRING-SPEC.md`) | done |
| Bundle 01 -> canonical outputs bootstrap tree | APPLIED, 4/4 hashes verified |
| Bundle 02 -> spike tree per REPLACE-MANIFEST | APPLIED + verified 4/4, then RECONCILED (see §3): spike tree restored to pinned pre-bundle hashes; composition consumed byte-identical hash-attested staging copies |
| Recomposition (build_runtime.py + PROPOSAL mechanism, x4 extra units, LMF_DISABLE_DL_RETENTION, supplier receipt, extended undefined baseline) | SUCCEEDED: `NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED`, 366 units, 117,712,568 bytes |
| Undefined delta gate | added = `[sceKernelGetModuleInfo]` ONLY — identical to composition-2's set; the ack bootstrap units added ZERO new imports; removed none; changed bindings none |
| Packaging (fixed package_prx.py sha e6fbe6c5, --link vs composition-3 object) | SUCCEEDED: 20/20 static checks PASS, gaps `[]` |

## 1. Bundle 01 — canonical outputs bootstrap tree (APPLIED, permanent)

Payload (bundle dir) pre-verified == COPY-MANIFEST `bundle_sha256`, then copied to
`outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/`. Post-apply state (all match manifest):

| file | sha256 (post == bundle_sha256) |
|---|---|
| runtime_owner.hpp | `5428f8c322c3aaf9761188f53255fea6cb5aedabb5f60bd7c26ba89953dcbd2a` |
| runtime_owner.cpp | `54c56c49d0ad89fd6456f58d99f765027f6e16e0bfdbaf8b8e1e044f268aa629` |
| ue4ss_backend.hpp | `081b20ef91c18351ceb9543fa563718d3403098d8dfa143bb5a52e52738d7892` |
| ue4ss_backend.cpp | `ce6dd23b1395b0a89bf32a85efd97478c423e8c206590e209601922907044d47` |

Pre-images preserved under `worker-composition-3/pre-images/`.
Note: `outputs/ps5-mod-bridge/native-runtime-startup/SOURCES.sha256` does not list these four
files (grep: no matches), so no stale-manifest side effect found. Nothing in the composition
pins the canonical tree (verified: no `.d` in composition-2 references `native-runtime-startup`).

## 2. Bundle 02 — REPLACE-MANIFEST verification (all 6 entries checked)

Applied from `controller-patch/02-composition-wiring/destination-relative/.superpowers/...`
(that dir contains only hidden `.superpowers`, hence "empty" to plain `ls`). Verified 4/4 replaced
files == `bundle_sha256` (`runtime_owner.hpp` 5428f8c3…, `runtime_owner.cpp` 54c56c49…,
`ue4ss_backend.hpp` 7f932a58…, pad `ue4ss_backend.cpp` 1d58f9a5…), and the 2 UNCHANGED files
still at pinned hashes (`activated_mod_source.hpp` e7d92aaf…, `.cpp` 51edda55…).

## 3. Reconciliation: why the composition consumed staging copies

Two FROZEN gates pin the spike bootstrap bytes, and bundle 02 mutates exactly those paths:

1. `native-coherent-runtime-1/build_runtime.py::validate_inputs()` asserts
   `native-coherent-runtime-1/source/bootstrap/runtime_owner.cpp` at its pre-ack hash —
   measured failure: `AssertionError: ('input pin mismatch', …/runtime_owner.cpp)`.
2. `ue4ss-native-tick-file-owner-1/build_runtime.py` line 43 checks base
   `bootstrap_units` sources: `runtime_owner.cpp` @ c259e0ef…, pad `ue4ss_backend.cpp` @ 62313cc1…
   (verified in `.superpowers/native-dependency-spike/ue4ss-native-notify-cancel-1/build-1/receipt.json`).

Per WIRING-SPEC ("If the gate reports anything else: STOP and reconcile; never bypass") no gate
was bypassed or edited. Reconciliation, exactly matching worker-ack-integration-deep's own
measured precedent (evidence compiled "against a destination-mirroring staging tree",
`evidence/dest-stage` + DEST-COMPILE.json):

- The 4 spike files were RESTORED to their manifest `current_sha256` pre-bundle bytes
  (restored hashes verified: 1f894c3d…, c259e0ef…, 166de1c2…, 62313cc1…).
- The ack bytes (byte-identical to bundle 02 payloads, hashes re-verified) were consumed from
  `worker-composition-3/bootstrap-stage/native-coherent-runtime-1/source/bootstrap/` and
  `.../native-coherent-pad-runtime-1/build-1/source/`, attested in
  `worker-composition-3/replace-receipt.json` (7 files, includes the unchanged
  `activated_mod_source.hpp` + pad headers).
- Net effect on the composition is identical to a spike-tree apply: the replacement units are
  compiled from the ack bytes; the receipt records `source_sha256` == REPLACE-MANIFEST hashes.
- CONTROLLER ACTION OUTSTANDING: re-applying bundle 02 to the spike tree requires updating the
  pinned `validate_inputs()` pins in `native-coherent-runtime-1/build_runtime.py` (a controller
  pinned-values patch). Until then the spike tree must stay at pre-ack hashes or every
  recomposition will fail at `validate_inputs`.

## 4. Recomposition run

Mechanism: `worker-composition-3/build_runtime_composition3.py` — a generated copy of the pinned
`ue4ss-native-tick-file-owner-1/build_runtime.py` (original sha256
`f9ca46fb56d1b01ee43464e74f4086aaa7940ada4988fe68f4e82a79a2161a03`) plus ONLY the bounded
`--replace-bootstrap-unit` / `--replace-receipt` mechanism from
`build_runtime-bootstrap-replace.PROPOSAL.md` (destination-mirroring `-I` rewrite included).
Full diff vs the pinned original: `evidence/build_runtime_composition3.diff` (53 lines: docstring,
2 new argparse flags, 1 mechanism block; all frozen gates byte-identical).

Command (one invocation; relative to workspace root):

```
python3 outputs/2026-10-04-zcode-team/worker-composition-3/build_runtime_composition3.py \
  --output .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-3/run-1 \
  --rust-receipt .superpowers/native-dependency-spike/ue4ss-native-tick-witness-1/native-1/receipt.json \
  --host-receipt .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/host-final-1/receipt.json \
  --extra-unit lmf-live-module-facts=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_facts.cpp \
  --extra-unit lmf-sce-ops=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_os_ops_sce.cpp \
  --extra-unit lmf-loader-adapter=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp \
  --extra-unit lmf-base-scan=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_base_scan.cpp \
  --extra-define LMF_DISABLE_DL_RETENTION \
  --extra-receipt outputs/2026-10-04-zcode-team/worker-supplier-ingame-update/build-receipts/run-1/receipt.json \
  --extend-undefined-baseline \
  --replace-bootstrap-unit runtime_owner=outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-runtime-1/source/bootstrap/runtime_owner.cpp \
  --replace-bootstrap-unit ue4ss_backend=outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-pad-runtime-1/build-1/source/ue4ss_backend.cpp \
  --replace-receipt outputs/2026-10-04-zcode-team/worker-composition-3/replace-receipt.json
```

Failed attempts (both failed BEFORE the output dir was created; no state mutated; logs in
`evidence/failed-attempt-1-include-drift/` after the third attempt hit an include-resolution
drift — staged pad cpp pulled OLD spike headers via `-I`; fixed by the attested `-I` mirror map,
the same technique as the dest-stage evidence):
1. exit 1 `Changed input: …/outputs/2026-10-04-zcode-team/ue4ss-native-notify-cancel-1/…` — copy's
   path-derivation bug (W/S/ROOT anchored off `__file__`); fixed by pinning W to the original dir.
2. exit 1 `input pin mismatch …/runtime_owner.cpp` — the §3 frozen-gate conflict; fixed by restore+staging.

## 5. Run verification (all measured from `run-1/`)

| item | value |
|---|---|
| receipt `status` | `NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED` |
| runtime object | `run-1/coherent-tick-file-witness-runtime.o` |
| runtime_sha256 (receipt == recomputed on disk) | `e9679aa5b3bc3a38140c4dd4a63b566158bded3eb0a4387d4cc0e073b2d86350` |
| runtime_bytes | `117712568` |
| runtime_compilation_units | `366` (353 retained + 13 fresh; PROPOSAL predicted 364→366 for the 2-unit ack set) |
| freshly_compiled_units | the 11 composition-2 units + `runtime_owner` + `ue4ss_backend` |
| undefined delta | added `[sceKernelGetModuleInfo]`, removed `[]`, changed `{}`, current_count 198 — **identical added set to composition-2** (`baseline_extended: true`, providers: libkernel.so, libkernel_stub_weak.so, libkernel_sys.so, libkernel_web.so). The ack pair contributed zero new imports (WIRING-SPEC S5 expectation: added == []; actual delta is exactly the composition-2 extras delta, which this run inherits because it carries the same 4 extra units) |
| extra_receipt pinned | `worker-supplier-ingame-update/build-receipts/run-1/receipt.json` sha256 `3b0d0425796864143c6cb6cf77dee439d4b8e53d6e99e8c1fdd59e32eee441f1` |
| bootstrap_runtime_owner.o | sha256 `9677a7a03de741c75805bc42215671d6b7be274eae287587c5be74054bcb11f8` (source 54c56c49…, `bootstrap_replaced: true`) |
| bootstrap_ue4ss_backend.o | sha256 `e34067060054eb2b2ba8611d1209f48fc203ee566034cdfce6dee5f23ffde48a` (source 1d58f9a5…, `bootstrap_replaced: true`) |
| extra_pinned_dependencies | 66 (same count as composition-2) |
| commands recorded | 48 (all argv in `run-1/commands.json`) |
| `linked_prx` / `target_executed` / `console_contact` | false / false / false |

Ack symbols verified `--extern-only --defined-only` on the composed object (T = defined):
`RuntimeAckSink::Open(RuntimeAckConfig const&)`, `emit`, `engine_ready`, `mods_started`,
`activated`, `stopped`, `failed(int, char const*)`, `close`; ack-enabled
`RuntimeOwner(std::unique_ptr<RuntimeBackend>, std::shared_ptr<RuntimeAckSink>)` ctor (C1+C2);
4-arg `PrepareActivatedUE4SSRuntime(NativeProgramContext, unique_ptr<mfa_activation>,
string_view, RuntimeAckConfig const&)` — exactly the WIRING-SPEC S1 closure.

## 6. Packaging run (fixed script, --link, vs composition-3 object)

Script: `outputs/2026-10-04-zcode-team/worker-prx-link-gaps/package_prx.py`, sha256 verified at
load == `e6fbe6c5011b498e0c26ee88bdc6c84aacab204b6ccad5696a78353afef5c313`, executed UNMODIFIED.
Driver: `worker-composition-3/package_composition3.py` — applies only the documented
pinned-values adjustment (the same pattern that produced the fixed script itself):
`composition_dir` -> `ue4ss-native-supplier-composition-3/run-1`, object sha/bytes from the run
receipt, checkpoint `zcode_supplier_composition_3`, and a PROVISIONAL mirrored ledger
(`evidence/transport-ledger.composition3-mirror.json`; the real ledger
`.superpowers/native-observations/transport-experiments-progress.json` has no `_3` entry and is
read-only for this worker — controller must reconcile).

Command: `python3 outputs/2026-10-04-zcode-team/worker-composition-3/package_composition3.py`
(internally `package_prx.py --link --out …/worker-composition-3/prx-candidate-composition3-1`).

Pipeline executed by the fixed script: 10 bootstrap units compiled (prospero-clang, -Werror),
supplier_import_extension.S, `ld.lld -r` partial link with the composition-3 object, extended
196-import contract, dotnet Packager, finalize_sdk 0x11000000, make_fself.

Result: `STATIC_CHECKS_PASSED` — **20/20 checks PASS** (`verification-evidence/static-checks.json`;
named checks incl. elf_module_type_flags, pt_sce_module_unique, sdk_stamp_0x11000000,
explicit_dt_init_dt_fini, loader_owns_no_arrays, init_fini_slots, eh_frame/hdr/gcc_except_table,
imports_equal_extended_contract, linker_provided_dynamic_not_imported,
imports_resolve_to_sdk_or_linker, entry_alignment_evidence), `build-receipt.json` gaps `[]`.

Candidate: `worker-composition-3/prx-candidate-composition3-1/modbridge_supplier_composition.prx`
30,756,791 bytes, sha256 `b8bfae77c8d147b54864a6fc5ac1f24355ba3fb13c79b0cd92e11bc7e36aab35`
(bare-original pre-fself sha256 `1857597cd43534302b97ff4bdc7bef6963b6cabc72c2d5c73e65e25bd6f0e28d`;
build-receipt sha256 `283bedf2926f357c259e9770d6d06c936147aa68e2a4443f42227a43b9bba0c4`).

## 7. Limits (stated honestly)

- Static verification only: no in-game load, execution, promotion, or visual/audio claim.
  `promotion_allowed` stays false per the package profile.
- The module_start `RuntimeAckConfig` supplier (WIRING-SPEC S1/S2/S3) is future work, authored
  with the next game-bound caller unit; nothing calls `PrepareActivatedUE4SSRuntime` yet.
- Spike tree carries PRE-ack bootstrap hashes (see §3); bundle 01 is applied permanently; the
  controller still owes: (a) the runtime-1 pinned-values patch + bundle-02 re-apply if the wired
  spike tree is desired, (b) the real-ledger `zcode_supplier_composition_3` entry.
- Pre-flight/driver helper scripts live in this worker dir; the two pinned build scripts and the
  fixed packaging script were never modified on disk.

## 8. Files written by this worker

Under `outputs/2026-10-04-zcode-team/worker-composition-3/`: CLAIM.json, REPORT.md (this file),
`build_runtime_composition3.py`, `package_composition3.py`, `replace-receipt.json`,
`bootstrap-stage/` (7 attested files), `pre-images/` (8 files), `preflight/` (compiles + .d),
`evidence/` (build_runtime_composition3.diff, failed-attempt-1-include-drift/,
transport-ledger.composition3-mirror.json), `prx-candidate-composition3-1/` (candidate + receipts).
Task-authorized artifacts outside the worker dir: the composition-3 run dir
`.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-3/run-1/`
and bundle 01's four canonical-tree replacements.
