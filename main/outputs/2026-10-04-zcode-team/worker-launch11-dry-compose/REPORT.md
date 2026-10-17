# REPORT — worker-launch11-dry-compose

2026-10-04 · Host build only; no console contact, no nested subagents; nothing staged,
launched, armed, or folded ON. Deliverable order honored: `CLAIM.json` first (updated
with measured verdicts after the passes), this `REPORT.md` last.

## 1. The task, in one line

The launch-11 matrix's ledger says the vtable backend seam and the walk-site RX guard are
mechanically compatible (§4a) and pins R3-ARMED to BOTH_FAIL rows — but no row had ever
composed a build with TWO successor patches simultaneously. This worker composed the
most demanding compatible pair — backend PREPARED-UNARMED + walk-site guard, both
verdict-gated default-OFF — through the d1fix-lineage one-pass into its own candidate,
and proved the composition with receipts, a determinism twin, and a GO gate.

## 2. What was composed

| patch | staged form | provenance assertion |
|---|---|---|
| vtable backend PREPARED-UNARMED (`unreal-unrealinitializer` + seamed siblings + refusing D-1) | disarmed, `UE4SS_DISPATCH_DATA_BACKEND=0` | staging copy of the L11PB vehicle, 6/6 shas verified against the L11PB manifest (`39fe33f3…` initializer, `59effe6e…` armed SeedTable carried, etc.) |
| walk-site RX guard (`asmhelper-asmhelper`) | **fold-0** (define ABSENT: loud refusal, zero syscall text — the only fold admissible without a D2 verdict) | WST diff `42baeebd…` applied to a staging copy of the port unit; result byte-identical to WST's `ASMHelper.guarded.cpp` `694af016…` |

Wiring: the guarded unit enters the link as an attested RETAINED-unit replacement via the
sanctioned new fixed-script edition — `build_runtime_composition6_fixed_drycompose.py`
(`8b70520ee3306ac6…` = frozen `8dd92a6fe4b71cf1…` + ONE tagged `RETAINED_REPLACEABLE`
edit, diff-verified to contain nothing else) driven by
`build_runtime_composition6_drycompose.py` (`330b76daf4d6356d…` = the L11PB driver
`ba0d899e…` + tagged DRYCOMPOSE E0-E3, diff-verified). The SEC32-3 argv-drift assertion
enforced matrix §3.2's law mechanically: only source/-o/-MF redirected under the real
148-arg unit argv. The shared `coherent-full-1/asmhelper-asmhelper.o` (`68d22a1c…`) was
re-verified untouched.

## 3. Measured results (both passes)

- **Object delta EXACTLY as declared**: 368/368 unit names equal vs the launch-10 run
  receipt; source deltas exactly `{unreal-unrealinitializer, asmhelper-asmhelper}`;
  object delta exactly `{asmhelper-asmhelper}` — initializer object byte-identical
  `983b18e24d32058f…` == launch-10's; asmhelper object byte-identical
  `81dd832bd59fab43…` == WST's fold-0 compile-gate object (independent real-argv
  reproduction, twice).
- **Imports 196 unchanged**: composed UND set IDENTICAL to launch-10's (198 == 198, set
  equality; `sceKernelGetModuleInfo` is launch-10's own pre-existing extended-baseline
  entry, same libkernel providers); gate I1/I1+ declared +[]/-[] == actual +[]/-[].
- **Static checks**: printed gate 17/17 PASS; `static-checks.json` 20/20 PASS
  (`STATIC_CHECKS_PASSED`), check set identical to the L11PB candidate — both passes.
- **Determinism pair**: independent fresh compose+package — composed
  `49fd49e73dafb145…`, wrapped fself `d94bb99a33f7ff55…`, bare `acf00edd23c8fe6e…`,
  all equal across run-a/run-b.
- **Gate v2 vs frozen r11**: **GO (offline gate), 15/15, failed_checks []** with the
  manifest (`9435ec8a4ad6e7ca…`) declaring BOTH units
  (`gate-receipt-launch11-drycompose-a-vs-r11.json`, `67ef64e5…`).
- Composed runtime `49fd49e73dafb145df77140f83259fe0e390b8efcebe5fe604b7be4d47068e09`
  (117,842,752 B; +11,392 B vs launch-10 = the fold-0 guarded object — the declared,
  expected difference; "unchanged" means imports/UND identity, not object identity).

Candidates: `prx-candidate-composition6-drycompose-a` (+ `-b` determinism twin); runs
`ue4ss-native-supplier-composition-6-drycompose/run-a|run-b`.

## 4. What this unblocks mechanically (see MATRIX-ROW-NOTE.md)

The guard-unit wiring for every guard row (L11-1..L11-5/L11-8's fold-ABSENT form), the
first two-patch composition precedent for the matrix's §2 rows, and a one-define-flip
path to L11-1's composition. NOT unblocked: any armed form (ledger-g member-type arming
edition, R2, D-1), the fold-1 verdict gate, DIFF-2 rebase, PLH absorption, cc1 adoption.

## 5. Failure history (preserved; never replayed blind)

Three receipt attempts preserved (`*.firstattempt/secondattempt/thirdattempt.json`):
(1) `--dependency-d` replaces the default walk list → 33-file receipt; (2) missing the 4
spike Resolution-B header admissions → compose attempt refused at the host-receipt layer
(`Changed input: …NativeLoaderContext.hpp`; the rr layer is the designed reconciliation);
the fixed script died pre-mkdir, so no rejected run dir exists; (3) parity-diff caught
the swapped admissions before any build. First gate attempt NO-GO
(`gate-receipt-launch11-drycompose-a-vs-l10.json`, `5dba72a0…`, unchanged): D1
`--compare-dir` mode asserts byte-identity to the compare candidate (prebuild semantics)
and M1 attests every `sha256_after` key — corrected by the gate's own
`determinism.json` path (measured run-a/run-b) and manifest hygiene; evidence-form
fixes, composition unchanged.

## 6. New mechanical facts + pre-existing incident

- **`__FILE_NAME__` basename pin**: the unit embeds its source basename (no debug info);
  a wrong basename yields a different object (`f34b667e…` vs `81dd832b…` for identical
  code). Fold rows must keep the staged basename `ASMHelper.guarded.cpp`.
- **PRE-EXISTING shared-tree mutation observed, preserved untouched**:
  `coherent-full-1/asmhelper-asmhelper.d` now begins with WST compile-gate fold1 paths —
  `compile_gate.sh` redirected `-c`/`-o` but not `-MF` (sha now `e346c81ad4b9c645…`).
  Harmless to every compose (0 affected-header matches; only the `.o` is hash-pinned)
  but it contradicts WST REPORT §5's "nothing in the spike trees is modified"; owner
  reconciliation (exact-argv reproduction, L11PB §6 pattern) or controller decision —
  never a silent overwrite.

## 7. Honesty bounds

No in-game, verdict, kernel-behavior, or quality claim of any kind; fold-0 and the
disarmed seam are inert by construction and measured at the import/UND level, which is a
packaging-level statement. WST, L11PB, and LAUNCH11-MATRIX non-claims carry over
unchanged. The controller owns every verdict-gated act.

## 8. Deliverables (this dir)

`CLAIM.json` (first) · this `REPORT.md` (last) · `TWO-PATCH-COMPOSITION-RECEIPT.md` ·
`MATRIX-ROW-NOTE.md` · candidates `prx-candidate-composition6-drycompose-a/-b` · runs
`run-a/-b` · receipts (`composition6-replace-receipt.json` `147d9ecf…` 103 files +
3 preserved attempts; `composition6-extra-receipt.json` `0528d3d6…` byte-equal to
L11PB/launch-10) · manifest + GO gate receipt (+ preserved NO-GO) · driver + edition
(both diff-verified) · `staging/initializer-drycompose/` + `staging/asmhelper-guard/`
(baseline copy, guarded source, diff, probe object/.d) · `logs/`.
