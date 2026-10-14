# DRYBUILD-RECEIPT — comp6 rehearsal build (worker-comp6-drybuild)

2026-10-04/05 UTC. REHEARSAL: the REPLACE-MECHANICS.md §2 procedure executed
end-to-end with the D2 scaffold (Diff A adapter + Diff B
PsMappedImageCaller.hpp + Diff C OwnedMappedImageCaller.hpp), producing a real
changed composition, a real changed PRX candidate, and a full composition-5
gate receipt — entirely inside this worker dir. No console contact, no nested
subagents, no frozen-tree writes, pinned composition script and live
composition-5 driver unmodified (shas re-verified at the end).

Consumed D2 state: `outputs/2026-10-04-zcode-controller/comp6-integration/`
as of 21:11 (adapter had absorbed the configured-roots "Resolution B" opt-in;
Ps/Owned headers unchanged since 20:53). Configured-roots-only files
(`NativeLoaderContext.cpp/.hpp`, `UE4SSProgram.cpp`,
`Platform-NativeProgramContext.hpp`) were NOT staged — they need Procedure B
and are deferred, exactly as tasked ("dry build proves the pipeline with D2
only").

## 1. Command + hash ledger (chronological)

| # | step | command (root = `/WORKSPACE/.codex/worktrees/translation-core/what`) | key result | sha256 |
|---|---|---|---|---|
| 0 | staging | snapshot D2 cpp+hpp+Ps+Owned into `staging-d1/`; append rehearsal markers; convert 6 include hops to quoted (see `staging-d1/STAGING-DIFFS.md` — every staged copy differs from its source by ONLY the intended edits) | 9 staged files | see §2 table |
| 0b | pre-flight | run-3 `extra-lmf-loader-adapter` argv with source swapped to staging copy, `-fsyntax-only` | PASS (rc=0) | — |
| 1 | driver copy | copy `worker-composition-5-rebuild/build_runtime_composition5.py` (`aaf47bdaf752e62ff845e37828cda4a89a0a612d47809ab2eb99aa5a6dcd9904`) + 7 documented edits (docstring) → `build_runtime_composition6_dry1.py` | Procedure B copy | `e0f494e6750fdc11016c572bb9e9fccb0091077ca8588bb74b0763e323662a8f` |
| 2 | Run A Phase A | `python3 …/worker-comp6-drybuild/build_runtime_composition6_dry1.py --composition-run …/worker-comp6-drybuild/ue4ss-native-supplier-composition-6-dry1/run-1 --out …/prx-candidate-composition6-dry1-a` (first packaging attempt failed at bridge TU — see §5) | composition SUCCEEDED: status `NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED`, 368 units (15 fresh), chain gate 14/14 | object `ef19a4b89bdac29a6fb1e63282fd56ad8af947cf583b99b8ef08475f34f3a8b4`, 117,781,064 bytes; receipt `ea32f3f418ed52e59ecabb65c2177f8b5b5276b23f38e8af9a6591091b848c65` |
| 3 | Run A packaging | same driver, `--reuse-composition-run` (re-verified run-1 object hash), + 21 `--stage-header` args (`staging/extra-stage-headers.args`), overlay `staging/lineage-overlay-dry1` | PRX produced; packager `STATIC_CHECKS_PASSED` (20 checks), `gaps: []` | wrapped `35ba028e9dd3ed918ede03eb3b26712cb9135005695ee0198ff24d9978b46537` 30,792,663 B; bare `0dede0ee1eeeaf2346cc207f3b9add35ed7dead7b12901254d41bedf161e9190` 30,774,360 B |
| 4 | Run B (D1) | same driver, fresh `run-2` (independent Phase A) + `--overlay-dir …/lineage-overlay-dry1-b`, out `-b` | run-2 object BIT-IDENTICAL to run-1; candidate-b PRX BIT-IDENTICAL to candidate-a | object + PRX shas identical to Run A; run-2 receipt `10804a2ffcd74c8dd7c75c4a70aa6fbacd95308413a3cfead3d79052d98751e1` |
| 5 | gate | `python3 outputs/2026-10-04-zcode-team/worker-composition5-gate/gate_check_composition5.py --build-dir …/prx-candidate-composition6-dry1-a --compare-dir …/prx-candidate-composition6-dry1-b --json …/worker-comp6-drybuild/gate-receipt-composition6-dry1.json` | **exit 0 — all 8 checks PASS, verdict GO (offline gate)** | receipt `7f5906c3cf49ad65d1cec331446b32bd2e4ab005c97d64d3fa13c119f15a5b66` |

Integrity re-verification after everything: pinned composition script still
`36cc8199…` (matches its own gate + driver pin), live composition-5 driver
still `aaf47bda…`, D2 scaffold files byte-identical to the consumed shas,
real transport ledger untouched (only read; mirror written under this worker
dir, key `zcode_supplier_composition_6_dry1` in file
`transport-ledger.composition5-mirror.json` — filename kept from the copied
driver; key is the bumped one, no real-entry collision).

## 2. Delta table — dry1 vs composition-4 run-3 (baseline `45dd9901db7d626e…`, 117,779,552 B)

| dimension | result |
|---|---|
| fresh units bit-identical to run-3 | **14 of 15** (unreal-unrealinitializer, native-ownedmappedscansource, native-psscanbinding, native-ownedmappedcandidateselection, native-loadercontext, native-programfactory, native-ownedserializedelfsource, lmf-live-module-facts, lmf-sce-ops, lmf-base-scan, native-query-harness, module-runtime-ack-supplier, bootstrap runtime_owner, bootstrap ue4ss_backend) |
| units changed | **`lmf-loader-adapter` ONLY** (430,608 → 431,840 B): D2 Diff A readability filter + Resolution B opt-in + markers |
| composed object | `45dd9901…` → `ef19a4b8…` (+1,512 B); 368 units, 353 retained (same counts) |
| defined-symbol delta (composed object, llvm-nm extern-only defined-only) | **+3 / −0**: `_ZN2RC8Platform14comp6_drybuild22adapter_staging_markerEv` (T), `_ZN14comp6_drybuild28WCD_PS_DIFF_B_STAGING_MARKERE` (R), `_ZN14comp6_drybuild31WCD_OWNED_DIFF_C_STAGING_MARKERE` (R) — all three are the declared rehearsal markers; NO other additions, NO removals |
| adapter-unit undefined symbols | identical to run-3's adapter unit |
| imports (undefined-delta gate, S:167–179 vs notify-cancel base) | **current set identical to run-3** (198 symbols); added-vs-base = `['sceKernelGetModuleInfo']`, `baseline_extended: true` — exactly run-3's delta; removed `[]`; changed bindings `{}` |
| packaging import census (I1) | 196 = expected 196 (delta 0) |
| gate verdicts | W1 PASS, W2 PASS (module_start → StartActivatedRuntimeWithAck via bridge r11), W3 PASS (module_start+module_stop global/default, entry r3), I1 PASS, D1 PASS (byte-identical double build), F1 PASS, S1 PASS (bare 30,774,360 vs baseline 30,771,688 = +0.01%), P1 PASS (20 static checks) → **GO (offline gate)** |

## 3. Proof that Diffs A/B/C are IN the object

- **Diff A** (adapter readability filter + Resolution B): the staged adapter
  copy IS D2 content + one appended marker function
  (`staging-d1/STAGING-DIFFS.md`); `lmf-loader-adapter.o` is the only changed
  unit; marker `adapter_staging_marker` + its string are in the composed
  object (llvm-nm + `strings`).
- **Diff B** (PsMappedImageCaller.hpp): compile-time `#error` guards in the
  staged lmf hpp (`WCD_DRYBUILD_STAGING_PS_HEADER`) passed → the staging
  header won resolution (R3 trick); `lmf-loader-adapter.d` line 9 lists
  `staging-d1/PsMappedImageCaller.hpp` and ZERO frozen
  `ps-scan-abi-1/include/PsMappedImageCaller.hpp` references remain; marker
  string `…PsMappedImageCaller.hpp staging override (D2 Diff B text_base-absence
  rule) compiled into this object` is IN the composed object.
- **Diff C** (OwnedMappedImageCaller.hpp): same via
  `WCD_DRYBUILD_STAGING_OWNED_HEADER`; `lmf-loader-adapter.d` lines 8/849 list
  the staging copy (reached through BOTH `OwnedMappedResolverCollector.hpp`
  and `OwnedSerializedElfSource.hpp`); marker string IN the object.
- All 9 staged files were auto-pinned into
  `receipt.extra_pinned_dependencies` (S:128, same as R3) with the shas in §2
  of APPLY-ORDER.
- HONEST LIMIT: Diff B/C are constructor-body guards. The adapter TU never
  constructs those callers, so no ctor machine code exists in THIS object to
  disassemble; "in the object" is proven at the level the R3 precedent
  establishes — the D2 header text (verbatim + marker, proven by diff) is
  what the TU compiled, via guard + `.d` + marker-string evidence. Ctor-body
  codegen for the D2 headers exists only in TUs that construct callers
  (packaging bridge compiles the FROZEN headers today; see §6 step 4).

## 4. Scope compliance

- Writes: `outputs/2026-10-04-zcode-team/worker-comp6-drybuild/` ONLY.
- Frozen spike trees: consumed read-only (staging copies snapshot content;
  nothing written under `.superpowers/`).
- Pinned composition script: unmodified, sha-gated by the driver on every run.
- Live composition-5 driver: unmodified (`aaf47bda…` verified after all runs);
  this rehearsal used its own Procedure B copy.
- Real transport ledger: read-only; mirror entry uses bumped checkpoint id
  `zcode_supplier_composition_6_dry1`.
- Failed/partial attempts preserved, not hidden: `run-a.console.log` (compose
  failure: missing intercept), `run-a-retry.console.log` (packaging failure:
  bridge header pin drift), `prx-candidate-composition6-dry1-a-failed1/`
  (packaging failure: r11 bridge overlay-header gap, GAPS.json).

## 5. Failures hit during the rehearsal (all fail-closed, all resolved)

1. Missing intercept header (`OwnedSerializedElfSource.hpp`) → frozen-twin
   redefinition; fixed by staging the 4th direct includer.
2. D2 headers self-close their namespace; appended marker blocks initially
   added an extra brace — caught by the syntax-only pre-flight before any
   full run.
3. `runtime_start_bridge.h` pin drift (`c47f7184…` stale default vs
   `ecada77e…` current; the real comp5 build staged the current content via
   `--stage-header` override — proven by its overlay manifest). Resolved by
   driver-copy edit 7.
4. r11 bridge include closure wider than the composition-5 default overlay
   staging list → first packaging attempt failed
   (`live_module_base_scan.hpp` not found). Resolved with 21 explicit
   `--stage-header` args (Step 2.5 in APPLY-ORDER), discovered by iterating
   the recorded bridge argv to convergence; syntax-clean under the packaging
   flags `-Wall -Wextra -Werror`.

## 6. EXACT remaining steps for the REAL comp6 build (one-shot checklist)

Precondition: the configured-roots diffs are merged into
`outputs/2026-10-04-zcode-controller/comp6-integration/` and the controller
blesses the merged scaffold (record its shas). Then:

1. **Refresh staging from the merged scaffold** (this worker's `staging-d1/`
   pattern): copy merged `lmf_to_native_loader_facts.cpp/.hpp`,
   `PsMappedImageCaller.hpp`, `OwnedMappedImageCaller.hpp`; re-apply (a) the
   two quoted-include conversions + `#error` guards in the staged lmf hpp,
   (b) the quoted `PsMappedImageCaller.hpp` include in the staged Owned
   header, (c) decide marker policy — for the REAL build DROP the three
   rehearsal markers (they add 3 public symbols to the composed object);
   the include-override proof then rests on guards (keep the `#error`
   guards, which add no symbols) + `.d` paths. Re-run the 4 frozen intercept
   copies step unchanged (NativeLoaderContext.hpp, OwnedMappedScanSource.hpp,
   OwnedMappedCandidateSelection.hpp, OwnedMappedResolverCollector.hpp,
   OwnedSerializedElfSource.hpp — quote-conversions only).
   Regenerate `STAGING-DIFFS.md` + shas.
2. **Syntax-only pre-flight** (exact run-3 `extra-lmf-loader-adapter` argv,
   source swapped, `-fsyntax-only`, `LLVM_CONFIG` env from the receipt) —
   gate before burning a composition run.
3. **Configured-roots Procedure B** (REQUIRED for the deferred files):
   `NativeLoaderContext.cpp`, `NativeProgramFactory.cpp`-style changes and
   `UE4SSProgram.cpp` cannot be reached by argv (REPLACE-MECHANICS §1).
   Copy the pinned composition script, make the minimal `new_sources` /
   loop edits for exactly the units the merged scaffold changes
   (`native-loadercontext` at minimum; `program` needs the heavier
   S:93-loop extension for UE4SSProgram.cpp), re-pin the new script sha in
   BOTH the driver copy's `COMPOSE4_FIXED`/`COMPOSE4_FIXED_SHA` (D:66–67
   semantics) and re-derive the affected-inventory check expectation
   (S:57–61 headers list may need the merged
   `Platform/NativeProgramContext.hpp` reality reconciled). If
   `get_game_executable_path`/UE4SSProgram changes are part of
   configured-roots, schedule them as their own bounded change — the
   extra-unit override CANNOT replace `program`'s 83 strong symbols
   (proven T1).
4. **Decide the bridge/header coupling**: the packaging bridge TU compiles
   the FROZEN `PsMappedImageCaller.hpp`/`OwnedMappedImageCaller.hpp`
   flat-staged in the overlay (Step 2.5 mechanism,
   `staging/extra-stage-headers.args`). If the real comp6 must ALSO give the
   bridge the D2/merged header vintages (e.g. so the bridge may pass
   `allow_configured_roots_for_missing_executable_path`, which the r11
   bridge does NOT reference today), repoint those two `--stage-header`
   sources at the merged scaffold copies and re-run the bridge closure
   pre-flight. Otherwise keep frozen vintages (current rehearsal semantics).
5. **Driver copy**: start from this rehearsal's
   `build_runtime_composition6_dry1.py` (7 edits documented); bump checkpoint
   id to the real `zcode_supplier_composition_6`; keep edit 7 (bridge header
   `ecada77e…`) unless the wiring worker re-delivers the header; keep the 21
   `--stage-header` args (Step 2.5) unless the controller folds them into
   `DEFAULT_STAGE_HEADERS` upstream.
6. **Run**: fresh composition run (driver Phase A) → chain gate → overlay →
   packaging; then a second independent run for D1; then the gate checker
   with `--compare-dir` (all three commands verbatim as §1 rows 2/4/5 with
   real-build dirs). Expect I1=196 to HOLD (the rehearsal proved the D2-only
   composition adds zero imports); any drift requires
   `IMPORT-DELTA-JUSTIFIED.md` per GATE-CHECKS.md.
7. **Record**: APPLY-ORDER doc with the final argv + shas (this file's
   pattern), per-run receipts, gate receipt, and a delta table vs run-3 in
   this receipt's §2 format. No console staging without the GATE-CHECKS.md
   GO conditions (including human review of W2 evidence).

## 7. Deliverables index

- `CLAIM.json` (first) · this file · `APPLY-ORDER-comp6-dry1.md` · `REPORT.md` (last)
- `staging-d1/` — 9 staged copies + `STAGING-DIFFS.md` (provenance diffs)
- `build_runtime_composition6_dry1.py` — Procedure B driver copy
- `staging/` — `extra-stage-headers.args` + `.json` (21 bridge-closure
  headers with shas), `overlay-manifest.json`, `chain-symbols.json`,
  `entry-preflight.json`, compose logs, overlays `lineage-overlay-dry1`/`-b`
- `ue4ss-native-supplier-composition-6-dry1/run-1|run-2` — composition runs
  (`receipt.json`, `undefined-delta.json`, `commands.json`, per-unit
  `.o/.d`, `runtime.map`)
- `prx-candidate-composition6-dry1-a|-b` — packaged candidates
  (`build-receipt.json`, `verification-evidence/`, PRX + bare-original)
- `prx-candidate-composition6-dry1-a-failed1` — preserved failed attempt
- `gate-receipt-composition6-dry1.json` — full 8-check receipt, GO
- `transport-ledger.composition5-mirror.json` — mirror with bumped key
  `zcode_supplier_composition_6_dry1` (file name inherited from the copied
  driver; no real-ledger contact)
