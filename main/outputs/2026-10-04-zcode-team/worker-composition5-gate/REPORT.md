# REPORT — worker-composition5-gate

**Verdict: OFFLINE ACCEPTANCE GATE DELIVERED AND NEGATIVE-CONTROL VALIDATED.**
The gate is runnable now; the moment worker-module-start-wiring's
`runtime_module_entry.c` and worker-composition-5-rebuild's candidate land, one
command gives PASS/FAIL per check in minutes. No console contact, no network,
writes confined to this worker dir.

## 1. Deliverables (this dir, in required order)

| file | role |
|---|---|
| `CLAIM.json` | written first; scope, inputs, negative-control contract |
| `WIRING-SPEC.md` | exact call chain, every linkable symbol with file:line citations |
| `GATE-CHECKS.md` | scriptable checklist + GO/NO-GO staging criteria + launch-readiness conditions |
| `gate_check_composition5.py` | the checker (stdlib-only, read-only on the build dir, exit 0/1) |
| `gate-receipt-composition4.json` | negative-control receipt (comp4 → NO-GO, W2 FAIL, as required) |
| `REPORT.md` | this file, last |

## 2. Wiring review findings (task 1) — see WIRING-SPEC.md for citations

- The composed object **already defines** the entire startup chain. Actual
  linkable symbols in `worker-composition-4-fix/prx-candidate-composition4-1/build/combined-supplier.o`
  (and `run-3/exports.stdout`): `mfa_activate` (C linkage),
  `_ZN9modbridge6native28StartActivatedRuntimeWithAckE…PKc` (one-call helper),
  `_ZN9modbridge6native28PrepareActivatedUE4SSRuntimeE…RKNS0_16RuntimeAckConfigE`
  (**4-arg ack-enabled form — no `activation_root` parameter**; the 5-arg form
  in `ps5-mod-bridge/.../bootstrap/ue4ss_backend.hpp:25-28` is a NEWER source
  revision than the composed unit at
  `worker-composition-3/bootstrap-stage/native-coherent-pad-runtime-1/build-1/source/ue4ss_backend.cpp:84-88`
  — a wiring that tries to call the 5-arg form will NOT link),
  `_ZN9modbridge6native12RuntimeOwner5startEv`,
  `_ZN9modbridge6native27MakeActivatedUE4SSModSourceE…`,
  `_ZN2RC8Platform15NativeModSourceC1/C2E…`. Calling these adds ZERO new
  imports; the frozen 191+5 census stays green.
- Required order: `mfa_activate(&cfg,&out,&err)` →
  `StartActivatedRuntimeWithAck(context, ActivatedMods{raw}, expected_title,
  mod_root)` — the helper internally does MakeRuntimeAckConfig →
  `InstallDefaultEngineBindings` (exactly once, BEFORE start) →
  `PrepareActivatedUE4SSRuntime` (assigns `context.mod_source` = the
  NativeModSource step, opens the ack sink, constructs the UE4SS program with
  buffered mods) → `owner->start()`. Direct variant (helper skipped) is
  acceptable but must reproduce that ordering exactly.
- The returned owner must be retained (file static) for `module_stop` →
  `StopRuntimeWithAck`/`owner->stop()`; never stop from the init thread.
- New entry MUST keep `visibility("default"), force_align_arg_pointer` on both
  `module_start` and `module_stop` (byte-level packaging check
  `entry_alignment_evidence`, `package_prx.py:980-988`).
- `modbridge_ctor_phase`/`modbridge_ctor_value` may be dropped (unreferenced by
  the composed object — verified against `run-3/undefined.stdout`).

## 3. Gate design (task 2) — see GATE-CHECKS.md

Eight checks: W1 chain symbols defined, W2 module_start CALLS the chain
(ELF64 `.rela.text` relocation parse + `objdump -dr` disassembly evidence into
the receipt — definitions alone are not wiring), W3 entry exports, I1 import
census == 196 (or written justification), D1 determinism double-build
byte-identical, F1 fail-open diagnostic markers, S1 size sanity (baseline
30,771,688 bytes ±5%), P1 all 20 sharpprospero static checks + packager receipt
fields (`explicit_init=module_start`, `explicit_fini=module_stop`,
`loader_array_ownership=false`, `runtime_or_hooks_executed=false`).

## 4. Negative control (task 3) — receipt `gate-receipt-composition4.json`

`gate_check_composition5.py --build-dir
…/worker-composition-4-fix/prx-candidate-composition4-1` → **NO-GO, exit 1**:

| check | result | note |
|---|---|---|
| W1 chain-symbols-defined | PASS | comp4 defines the chain — presence alone, which is exactly why W1 is insufficient |
| **W2 module_start-references-chain** | **FAIL** | `module_entry.o` call refs: none of `mfa_activate`/helper/{prepare+start}; disassembly evidence in receipt shows the guard/probe entry only |
| W3 entry-exports-intact | PASS | module_start/module_stop global+default |
| I1 import-census-unchanged | PASS | imports=196, delta 0, contract check PASS |
| D1 determinism-double-build | FAIL | comp4 was never double-built (expected; comp5 must evidence it) |
| F1 fail-open-diagnostic-markers | PASS | state markers + channel present |
| S1 size-sanity | PASS | 30,771,688 bytes, 0.00% delta, byte-identical to baseline sha |
| P1 packager-static-checks | PASS | STATIC_CHECKS_PASSED, 20/20, receipt fields ok |

Parser positive-control (read-only): the same relocation parser run against
`run-3/module-runtime-ack-supplier.o` finds exactly
`_ZN9modbridge6native12RuntimeOwner5startEv` and
`_ZN9modbridge6native28PrepareActivatedUE4SSRuntimeE…` call relocations — the
W2 logic demonstrably detects chain references where they exist and their
absence where they do not.

## 5. GO/NO-GO (task 4) — see GATE-CHECKS.md for the full table

- **GO** = gate exit 0 + human-reviewed W2 disassembly matching WIRING-SPEC
  order + attested entry sha256 unchanged + D1 evidenced.
- **NO-GO** on any FAIL; explicitly also on: prepare-without-start (reproduces
  the comp4 defect one level deeper), owner-not-retained for module_stop,
  attestation drift, unexplained size delta, missing determinism.
- **Launch-readiness** is deliberately NOT claimable offline: loader accepts,
  diagnostic/ack states (`activated`→`engine_ready`→`mods_started`) observed,
  game stays playable (fail-open held), and a UE4SS observable side effect —
  those are the on-console conditions after staging.

## 6. Honest scope

- No composition-5 candidate exists yet (both delivering workers still pending
  at gate-prep time), so the gate was validated against composition-4 only —
  by design: comp4 is the negative control.
- The gate is static/offline: it proves structure, linkage, imports,
  determinism and packaging acceptance. It cannot prove on-console behavior;
  nothing here claims runtime success or completion.
- D1/S1 baselines assume the candidate dir shape of the accepted comp4
  precedent; if the rebuild's layout differs materially, only `--build-dir`
  discovery logic would need a path update, not the checks.
- Call budget well under the 80-call bound; all existing files outside this
  worker dir left untouched.
