# REPORT — worker-entry-review

Worker: worker-entry-review · 2026-10-04 · independent, read-only pre-launch review.
Output dir: `outputs/2026-10-04-zcode-team/worker-entry-review/` (this directory; the only location written).
Deliverables in order: `CLAIM.json` → `REVIEW-VERDICT.md` (full findings, evidence, diffs) → this `REPORT.md`.

## What was reviewed

- `outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_module_entry.c` — sha256 `1c041f8bef1607994ce905591f7e9ed91326465e45081da55e56d6ea2e501b80`, 371 lines (verified against the claimed sha; the host-test staged copy is byte-identical).
- `outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge.cpp` (`1889bc7d…`, 237 lines) and `.h` (`ecada77e…`, 82 lines).
- Against: `worker-composition5-gate/WIRING-SPEC.md`, the original `outputs/ps5-mod-bridge/native-runtime-startup/lifecycle/module_entry.c` (+ `lifecycle.c/.h`, `probe/probe_api.h`, `diagnostic.h`), `modforge_activation.h/.c`, the composed-unit headers the bridge calls (`runtime_ack_supplier`, `runtime_owner`, `activated_mod_source`, `live_module_facts`, `lmf_to_native_loader_facts`, `NativeLoaderContext`, `ue4ss_backend`), the frozen import contract, `package_prx.py`, `gate_check_composition5.py`, and the wiring worker's host tests.

## Verdict: GO-WITH-FIXES

**0 P0, 1 P1, 3 P2, 5 P3.** The entry itself is sound and launch-ready in behavior; the P1 is a gate/integration blocker, not a code defect in the wiring.

## Checklist summary (full evidence in REVIEW-VERDICT.md)

1. **Probe-path equivalence — PASS** with one documented drift (P3-1): guard, `-20/-21/-22`, constructor invocation, ops table, and stop ordering are line-identical to the original; on a failed probe the diagnostic stream now emits `probe_lifecycle_result`+`runtime_start_skipped` instead of `module_start_return(result)`. Return codes/state/effects unchanged.
2. **WIRING-SPEC order — PASS exactly**: `mfa_activate` once (zero-default config) → bridge: capture (VMMAP-first) → convert with explicit `allow_vm_scan_base_as_module_image_base = true` opt-in → `MakeNativeProgramContext` → `StartActivatedRuntimeWithAck` (internally verified: `MakeRuntimeAckConfig` → `InstallDefaultEngineBindings` → 4-arg `PrepareActivatedUE4SSRuntime` → `owner->start()`); `module_stop` → `StopRuntimeWithAck` when the owner exists. All four hardcoded path/title constants verified against package-profile.json and RUNTIME-STAGING-PLAN.md.
3. **Fail-open — PASS**: every wiring failure returns 0 from `module_start` with all three evidence channels; activation is adopted by the bridge on every reachable exit and never released by the entry. Caveats: P2-1 (status write can truncate the runtime's own ack session on a stage-3/stop exception; `mfa_status_open` is `O_TRUNC`), P2-2 (bridge early-return paths contradict its own ownership contract; unreachable from this entry).
4. **Import surface — PASS**: the frozen contract measured at exactly 191 rows and contains `getpid`, `clock_gettime`, `sceKernelOpen/Write/Close` (and even `memset`); no memset/memcpy/printf/snprintf construct or emission (target flags `-O1 -ffreestanding -fno-builtin`; `nm -u` of the host-compiled entry object shows none), and all `mfa_*`/chain symbols are defined inside the composed object.
5. **Threading/lifetime — PASS**: wiring on the loader thread before any runtime thread exists; `runtime_started` atomic single-shot; `runtime_active` only touched under `entry_guard`; owner `stop()` always joins from the loader thread.
6. **Buffer/format safety — PASS**: all appends bounded, all detail buffers (`160/192/128`) NUL-terminated on every branch, `INT32_MIN` handled, `mfa_error` reads defended by full zero-init.
7. **Zero-init loops — PASS**: byte loops are aliasing- and alignment-safe on x86-64; padding zeroing harmless; no memset synthesis under the frozen flags.
8. **Packaging — PASS on code** (`force_align_arg_pointer` active on both exports target-side, both exports defined, no C++ in the `.c`), plus explicit integration requirements: the bridge must be added as a SECOND packaging unit (`BOOTSTRAP_UNITS` + prospero-clang++ path exists), its seven+ composed headers staged from the exact composed provenance, and gate W2 amended.

## Findings

| ID | Sev | Summary | Where |
|----|-----|---------|-------|
| P1-1 | P1 | Gate W2 scans only the object defining `module_start`; with the bridge as a separate TU the `StartActivatedRuntimeWithAck` relocation is absent there → W2 fails a correct wiring. Amend `find_entry_objects()` (diff in verdict) before the composition-5 gate run. | `gate_check_composition5.py:381-438` vs entry:281 / bridge:193 |
| P2-1 | P2 | `write_status_failed` opens `runtime-status.txt` with `O_TRUNC` on paths where the runtime's own ack session may exist (MRS_ERR_START after the prepare sink opened; stop-exception path) — truncates the runtime's evidence, contradicts the entry comment and one-sink rule; sink fd also leaks (no dtor close). Fix: skip the status write when `code == MRS_ERR_START` (diff in verdict). | entry:196-197, 221-226; `modforge_activation.c:609`; `runtime_owner.cpp:71-75` |
| P2-2 | P2 | Bridge leaks a non-NULL activation on its own "bad arguments"/"already started" returns, contradicting `runtime_start_bridge.h:53-57`. Unreachable from this entry; fix by adopting before the checks (diff in verdict). | bridge:130-147 |
| P2-3 | P2 | Bridge TU has zero compile evidence (host tests fake it); signatures verified statically against the exact composed headers and `nm combined-supplier.o` (all match). Integration must compile it with prospero-clang++ against hash-pinned header copies (divergent copies exist across worker dirs). | `host_test/fakes/bridge_host.c` |
| P3-1 | P3 | Diagnostic drift on probe failure (see item 1); re-baseline receipts grepping `module_start_return`. | entry:318,322,330 |
| P3-2 | P3 | `MRS_ERR_STOP_EXN == MRS_ERR_CONTEXT == 2`; evidence ambiguity. | `runtime_start_bridge.h:38,41` |
| P3-3 | P3 | `owner->start()` false (thread-spawn failure) still yields `MRS_OK`/`runtime_start_ok`; ack file honestly shows `failed`. Triage note, not a defect. | `runtime_ack_supplier.cpp:93`; `runtime_owner.cpp:31-40` |
| P3-4 | P3 | Bridge includes `ue4ss_backend.hpp` without using it — extra header-staging burden. | bridge:34 |
| P3-5 | P3 | Stack-local `LiveModuleOsOps` borrowed by the retained supplier; safe in the current VmScan route, dangles if a future chain calls supplier ops post-start. Record-only. | bridge:153; `live_module_facts.hpp:207` |

## Required actions before first real launch

1. Amend `gate_check_composition5.py` W2 (P1-1) — otherwise the gate run will fail a correct composition.
2. Integrate: swap `lifecycle/module_entry.c` for the delivered entry, append `runtime_start_bridge.cpp` to `BOOTSTRAP_UNITS`, stage hash-pinned composed headers into an existing `-I` dir, run the fail-closed unresolved-set pre-check (P2-3 + item-8 requirements).
3. Apply the two entry/bridge diffs (P2-1, P2-2) or explicitly accept the narrow evidence-loss/contract windows with a written decision.
4. Re-baseline any probe receipt/playbook that matches `module_start_return` on failed probes (P3-1).

## Honesty notes

- Static review only: no target compilation, no console contact, no execution of the reviewed units. Host-test and composed-object evidence were read, not re-run.
- The "byte-for-byte probe behavior" claim holds for returns/state/side effects but not for the diagnostic event stream (P3-1).
- All findings are reproducible from the file:line citations in REVIEW-VERDICT.md; proposed fixes are diffs in that document, not applied anywhere.
