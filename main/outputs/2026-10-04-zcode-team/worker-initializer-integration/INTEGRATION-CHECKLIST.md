# INTEGRATION-CHECKLIST — what the one-pass driver adds beyond this worker's delivery

Worker-initializer-integration, 2026-10-04. The merged unit + seed table +
gates are delivered and compile-gated; the items below are the companion
steps the controller/one-pass driver owns, in order. Every item cites its
owner or authority.

## 0. What ships from THIS worker (done, receipts in this directory)

- [x] `staging/UnrealInitializer.cpp` — merged unit (Shape A arming, seed
      consumption, FNAME-VERDICT remedy, DoScan/ps_scan gates, FLAG-4 close,
      A-PI honoring citation, FName-detour skip, tick-fallback record,
      probe-verdict extension point `kExecWritabilityVerdictGating=false`).
      sha256 `c85c42f7…`.
- [x] `staging/SeedTable.h` — GENERATED from the landed
      `worker-seed-table-derivation/SEED-TABLE.json` (dialect path verified
      identical to canonical `SEED-TABLE.conformed.json`). sha256 `2cab0c52…`.
- [x] `staging/SeedValidation.hpp` — pure validation chain, host-tested ALL
      PASS (`receipts/host-tests.final.stdout`). sha256 `b8d851e9…`.
- [x] Compile gate: exact run-3 unit argv, exit 0, zero new warnings
      (`receipts/compile-gate.stderr`, object sha `fe639a85…`).
- [x] Composed-delta proof: composed undefined-delta identical to run-3's
      (`[sceKernelGetModuleInfo]`, nothing removed) — REPLACE-ARGV.md.
- [x] `receipts/replace-receipt.json` — hash-pinned over the three files above.

## 1. Driver adds: composition + packaging (Phase A + driver, per REPLACE-MECHANICS §2)

1. Fresh run dir; APPLY-ORDER argv with the two-flag delta from
   REPLACE-ARGV.md (replace-runtime-unit + replace-receipt redirected here;
   everything else verbatim, including the run-3 `--extra-*` set,
   `--extend-undefined-baseline`, and bootstrap flags).
2. Phase A runs the gates (input pins, fresh-dep pins incl. the two
   staging-local headers via the replace-receipt, `ld -r`, undefined-delta,
   Rust exports, final pin re-check) — expected: pass with delta
   `[sceKernelGetModuleInfo]` unchanged.
3. Driver invocation with `--composition-run <new dir>`: chain_symbols_gate
   re-verifies the startup chain against the NEW object (no new startup-chain
   symbols were added — `--require-symbol` unchanged).
4. Bump the transport-ledger checkpoint id (REPLACE-MECHANICS §2:
   `composition_checkpoint` → `zcode_supplier_composition_7` or next free) so
   composition-5/6 entries are not overwritten; new `--out` dir and
   lineage-overlay bump per the same table.
5. Record the exact new argv in APPLY-ORDER form next to the manifest.

## 2. Driver adds: the staged settings + ini package (NOT this worker's files)

6. Settings v3 package (worker-settings-v3-package): `[EngineVersionOverride]
   MajorVersion=5 MinorVersion=6` + `[Hooks] HookProcessInternal=0
   HookProcessLocalScriptFunction=0` — REQUIRED for launch 9: slot 6 leaves
   the required mask only via the settings route, and the FLAG-3 Zydis
   code-read block (staged `:1497-1557`, honored by this unit via the
   retained parse path SettingsManager.cpp:279-280 →
   UE4SSProgram.cpp:1222-1223) only skips when both keys are set.
   Manifest re-pin to the v3 ini sha (settings-v3 package, 1448 B,
   `d8faa4ec…`).
7. Ridealongs plan (owners: worker-ridealongs / exec-writability): IGW
   exec-write probe + fc1 validator + observer + qdrv1 ride the same launch;
   the ploader.ini ORDERING of the IGW line vs the composition's line is the
   ridealongs' decision — it decides whether the exec-writability verdict
   records exist by this module's `module_start` (input to §4 below).

## 3. Driver adds: expected launch-9 terminal (honest prediction, LAUNCH9-PREDICTION)

8. With this delivery, launch 9's init thread stops at I3 with the named
   `[Seeds] Required-slot seed table incomplete: … missing [gmalloc]` throw —
   LOUD, `failed(1,<msg>)` ack, module survives, NOTHING exec-side touched
   (the compiled object contains no post-arming path). E1-E6 still get their
   first real exercise (r11 lease, D2, configured roots).
9. Triage signature: the throw message names present/missing slots; do NOT
   treat it as a seed refutation (`[Seeds] REFUTED:` is a different, also-
   terminal message from the consume path).

## 4. Driver adds: the probe-verdict contract (next iteration, designed not armed)

10. Exec-writability worker delivers the IGW ladder verdict schema + the
    ridealongs-pinned verdict file path. THEN this unit's
    `SeedArming::kExecWritabilityVerdictGating` flips false→true and the
    bounded verdict read lands at ScanGame entry ({X->RX ok} gates the
    post-arm vtable walks; {R->RW ok} gates a vtable-swap tick backend;
    absent/unreadable → stays Shape A). Distinct worker-ID revision — no
    replay of identical failed requests.
11. A future gmalloc derivation (the one under-determined slot) lands as a
    seed-table update: regenerate SeedTable.h (one command:
    `python3 gen_seed_table.py worker-seed-table-derivation/SEED-TABLE.json
    staging/SeedTable.h`), re-hash `receipts/replace-receipt.json`,
    recompose. No merged-unit change; that launch then owns FLAG-2/FLAG-1 as
    its open verdicts (LAUNCH9 §4 #3/#4).

## 5. Regression/acceptance (per docs/QUALITY-RUBRIC discipline)

12. The new arms need their own Editor/PlayMode checks at composition time
    (worker-assigned): "seeds armed, scan skipped" path, the Shape-A refusal
    message, and the query-harness layout gate as the downstream witness.
    Critic scores never replace these regression tests.
13. Provenance stays "identity and ABI unverified" for slots 2/4 until the
    FC-1 / sco_anchor validators run in-game; slot 0 is live-validated class.
    No >8 claim is made anywhere in this delivery.
