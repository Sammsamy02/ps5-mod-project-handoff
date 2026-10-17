# TWO-PATCH-COMPOSITION-RECEIPT — worker-launch11-dry-compose, 2026-10-04

Host build only. No console contact, no nested subagents. PROPOSAL-ONLY: both successor
patches ship verdict-gated default-OFF; nothing here stages, launches, arms, or flips a
fold. Read `CLAIM.json` (first) and `REPORT.md` (last) alongside this receipt.

## 1. The composed pair (LAUNCH11-MATRIX §4 ledger pair "a", first two-patch build)

| patch | unit | tier | staged form | provenance |
|---|---|---|---|---|
| vtable backend PREPARED-UNARMED | `unreal-unrealinitializer` (+ seamed siblings, refusing D-1) | WRITE | disarmed (`UE4SS_DISPATCH_DATA_BACKEND=0`, PLH at all 5 sites, zero `0F 05` bytes) | byte-identical copy of `worker-launch11-vtable-prebuild/staging/initializer-vtable-pre/` — 6/6 shas verified vs the L11PB manifest before staging |
| walk-site RX guard | `asmhelper-asmhelper` | READ | **fold-0** (`RC_ASM_WALK_RX_GUARD` define ABSENT → loud refusal, zero syscall text; the only fold admissible without a D2 verdict, WST-5) | WST diff `42baeebd…` applied to a STAGING COPY of the port unit; result byte-identical to WST's `ASMHelper.guarded.cpp` `694af016…` |

## 2. Measured composition results (both passes a+b; evidence in this dir)

| check | result |
|---|---|
| composed units | 368/368 unit names equal vs the launch-10 run receipt |
| source delta | EXACTLY `['unreal-unrealinitializer', 'asmhelper-asmhelper']` — no third unit |
| object delta | EXACTLY `['asmhelper-asmhelper']` — initializer object byte-identical `983b18e24d32058f…` == launch-10's; asmhelper object byte-identical `81dd832bd59fab43…` == WST's fold-0 compile-gate object (independent real-argv reproduction) |
| composed UND set | IDENTICAL to launch-10's: 198 == 198, set equality (`sceKernelGetModuleInfo` is the pre-existing comp6-lineage extended baseline, present in launch-10's own run receipt delta with the same libkernel providers) |
| imports | 196 == 196 (gate I1/I1+: declared +[]/-[], actual +[]/-[], contract-names MATCH) |
| static packaging | printed gate: 17/17 PASS; `verification-evidence/static-checks.json`: 20/20 PASS, status `STATIC_CHECKS_PASSED`, check set identical to the L11PB candidate's — both passes |
| determinism pair | run-a vs run-b (independent fresh compose+package): composed object `49fd49e73dafb145…` equal, wrapped fself `d94bb99a33f7ff55…` equal, bare `acf00edd23c8fe6e…` equal |
| gate v2 (manifest mode, vs frozen r11 baseline) | **GO (offline gate), 15/15, failed_checks []** — `gate-receipt-launch11-drycompose-a-vs-r11.json` (`67ef64e57e9a72a2…`), manifest `composition-change-manifest-launch11-drycompose.json` (`9435ec8a4ad6e7ca…`) declaring BOTH units |
| composed runtime | `49fd49e73dafb145df77140f83259fe0e390b8efcebe5fe604b7be4d47068e09` (117,842,752 B; +11,392 B vs launch-10 — the fold-0 guarded asmhelper object; the expected, declared difference) |

## 3. The composition mechanics proven (previously unproven)

1. **Two successor patches in ONE d1fix-lineage one-pass** — no row had ever composed a
   build with two successor units staged simultaneously.
2. **The WST-1 stale-link law's wiring** (matrix §3.2 + §4f): the guarded asmhelper unit
   is wired as an attested RETAINED-unit replacement through the sanctioned new
   fixed-script edition — `build_runtime_composition6_fixed_drycompose.py`
   (`8b70520ee3306ac6…` = frozen `8dd92a6fe4b71cf1…` + ONE tagged `RETAINED_REPLACEABLE`
   edit admitting `asmhelper-asmhelper`, diff-verified to contain nothing else). The
   SEC32-3 argv-drift assertion enforced the matrix §3.2 law mechanically: ONLY
   source/-o/-MF redirected under the real 148-arg unit argv. The frozen script was NOT
   forked silently and the shared `coherent-full-1/asmhelper-asmhelper.o`
   (`68d22a1c…`, re-verified unchanged on disk) was NOT touched.
3. **Receipt admission family** (matrix §3.2 step 3 + task order): `--file` admissions for
   the patched ASMHelper AND the seamed headers; replace receipt
   `147d9ecfed432bdb…` (103 files) is file-for-file the L11PB lineage receipt (zero hash
   differences on the 96 shared paths) with exactly the expected 6-theirs → 7-mine staging
   swap; extra receipt `0528d3d60085abfb…` byte-equal to L11PB's and launch-10's. Three
   earlier attempts preserved (`*.firstattempt/secondattempt/thirdattempt.json`) — the
   failure history is §5.
4. **Determinism/gate evidence forms**: D1 in compare-dir mode asserts byte-identity to
   the compare candidate (correct for L11PB's byte-identical prebuild, impossible by
   design here); for an intentionally-different candidate the gate's own
   `determinism.json` path is the determinism evidence — written from the measured
   run-a/run-b pair (`prx-candidate-composition6-drycompose-a/determinism.json`,
   `eea3f2d7a33460ef…`).

## 4. New mechanical facts recorded for consuming rows

- **`__FILE_NAME__` basename pin**: the guarded unit embeds its source BASENAME (no `-g`;
  the only string difference between same-content objects compiled from different paths).
  A fold-0/fold-1 rebuild reproduces WST's measured objects (`81dd832b…`/`6604ce2d…`)
  byte-for-byte ONLY when the compiled file's basename is exactly
  `ASMHelper.guarded.cpp`. The composed pass satisfies this via the staging path
  (`staging/asmhelper-guard/ASMHelper.guarded.cpp`).
- **Fresh-dep coverage**: the guarded unit's 878-dep walk needs exactly ONE admission
  beyond the frozen pin set — the staged source itself; every header resolves into the
  existing pin set.
- **Gate semantics**: `gate_check_composition5_v2.py --compare-dir` D1 = byte-identity
  assertion (prebuild use); manifest-mode M1 attests every `sha256_after` key (provenance
  docs belong in justification text, not in `sha256_after`).

## 5. Failure history (preserved, never replayed blind)

- Receipt attempts 1-3 (`*.firstattempt/secondattempt/thirdattempt.json`): (1) passing
  `--dependency-d` REPLACES the default walk list → 33-file receipt (my invocation error);
  (2) correct walk but missing the 4 spike Resolution-B header admissions → compose
  attempt 1 refused at the host-receipt layer (`Changed input: …/NativeLoaderContext.hpp`,
  host pin `18c76cdc…` vs edited `d857bc0d…` — the rr layer is the designed reconciliation
  and my receipt omitted it); the fixed script died BEFORE creating the run dir (no
  rejected run dir exists); (3) query/ack admissions added but the 4 RB headers dropped in
  the same edit → caught by parity diff before any build. Final receipt is attempt 4.
- Gate attempt 1 NO-GO (`gate-receipt-launch11-drycompose-a-vs-l10.json`,
  `5dba72a0fd279b7d…`, preserved unchanged): D1 compare-dir mode + M1 `sha256_after`
  hygiene, both §4/§3 above. Never re-rated on unchanged evidence; both fixes are
  evidence-form corrections, not composition changes.

## 6. Pre-existing incident OBSERVED (not mine, preserved untouched)

`coherent-full-1/asmhelper-asmhelper.d` (the SHARED dependency file) currently begins with
`…/worker-walk-site-transitions/host-test/compile-gate/ASMHelper.guarded.fold1.o:` — WST's
compile gate redirected `-c`/`-o` but not `-MF` (compile_gate.sh), so the gate runs
overwrote the shared `.d` (sha now `e346c81ad4b9c645…`). Harmless to every compose (the
affected-set scan over it matches 0 of the 4 Resolution-B headers; only the `.o` is
hash-pinned), but it contradicts WST REPORT §5 "nothing in the spike trees is modified"
and the file should be reconciled by its owner (exact-argv reproduction like L11PB §6, or
controller decision), never silently. I did not modify it.

## 7. Honesty bounds

- No in-game claim, no verdict claim, no kernel-behavior claim; fold-0 and the disarmed
  seam are inert by construction and measured at the import/UND level, which is a
  packaging-level statement, not a behavior guarantee.
- The composed runtime hash DIFFERS from launch-10's `d6214abd…` by design (declared);
  "unchanged" here means imports/UND identity, not object identity.
- The controller owns every verdict-gated act (fold-1, backend arm, D-1, staging).
