# REPORT — worker-launch10-walkguard-prep

2026-10-05 · Host build only; no console contact, no nested subagents; nothing
staged, launched, armed, or folded ON. Deliverable order honored: `CLAIM.json`
first (updated in place with measured verdicts), this `REPORT.md` last,
`RECEIPTS.txt` pinning sha256 of every deliverable.

## 1. The task, in one line

The launch-10 matrix R1/R4 rows' next step, prepared: the ARMED launch-10 base
(4/4 `SeedTable.h` `59effe6e...`, U-SWAP-1) composed WITH the
worker-walk-site-transitions walk-site RX guard wired into the link — built
PREPARED at fold DEFAULT-OFF (the IGW probe is PARKED for this run, no D2
verdict exists), exactly the launch-11 PREPARED-UNARMED pattern, through the
d1fix-lineage one-pass into this worker's own candidate, plus the MEASURED
exact one-define-flip that activates the fold once a `W_WORKS` /
`W_FAILS_R_WORKS` verdict arrives.

## 2. What was composed

| patch | staged form | provenance |
|---|---|---|
| armed initializer (`unreal-unrealinitializer`) | ARMED — launch-10's U-SWAP-1 carried unchanged | byte-identical copy of `worker-launch10-prebuild/staging/initializer-launch10` (`UnrealInitializer.cpp` `c85c42f7...`, `SeedTable.h` `59effe6e...` 4/4 gmalloc 0x930add8 `kRequiredSetComplete=true`, `SeedValidation.hpp` `b8d851e9...`), re-verified pre- and post-run |
| walk-site RX guard (`asmhelper-asmhelper`) | fold-0, define ABSENT (loud refusal, zero syscall text — the only fold admissible without a D2 verdict) | WST diff `42baeebd...` applied to MY copy of the pristine port unit `63fd324d...`; result byte-identical to WST's `ASMHelper.guarded.cpp` `694af016...`; basename kept `ASMHelper.guarded.cpp` (the `__FILE_NAME__` pin) |

Wiring (the launch-11 two-patch pattern, re-instanced over the ARMED base):

- **Edition**: `build_runtime_composition6_fixed_walkguard.py`
  (`1e210eb99db21fef...` = frozen `8dd92a6f...` + ONE tagged `WGP E1`
  `RETAINED_REPLACEABLE` edit admitting `asmhelper-asmhelper`; diff-verified
  to contain nothing else).
- **Driver**: `build_runtime_composition6_walkguard.py`
  (`7251945474f740f7...` = the launch-10-prebuild driver `007ac4df...` + WGP
  E0-E4, diff-verified: docstring, edition pointer, `MY` scope isolation,
  initializer path, asmhelper spec — plus **WGP E4**, a new incident fix,
  §6 below). The SEC32-3 argv-drift assertion enforced the matrix law
  mechanically: ONLY source/-o/-MF redirected under the real 148-arg unit
  argv, both passes.
- **Receipt**: `receipts/composition6-replace-receipt.json`
  (`ab9381dae4598b3e...`, 100 files — launch-10's lineage file-for-file with
  identical hashes on all shared paths + `--file` admissions of MY 3
  armed-staging files, MY staged guarded source, and the same 4 Resolution-B
  headers; the first attempt without the header admissions is preserved as
  `.firstattempt.json`). Extra receipt `0528d3d6...` byte-equal to the shared
  lineage's. Shared `coherent-full-1/asmhelper-asmhelper.o` (`68d22a1c...`),
  the port unit, the frozen edition inputs and the launch-10 staging
  re-verified untouched after both passes.

## 3. Measured results

- **Object delta EXACTLY as declared** (both passes): 368/368 unit names equal
  vs the launch-10 run receipt; initializer source sha EQUAL `c85c42f7...`
  (path-only delta to MY staging); **object delta exactly
  `{asmhelper-asmhelper}`** — initializer object byte-identical
  `983b18e2...`, guard object byte-identical to WST's fold-0 compile-gate
  object `81dd832b...` (5th independent real-argv build of the same bytes:
  WST gate, L11PB probe, launch-11 compose ×2, mine ×2).
- **Cross-lineage byte identity**: composed runtime
  `49fd49e73dafb145df77140f83259fe0e390b8efcebe5fe604b7be4d47068e09`
  (117,842,752 B) — byte-identical to launch-11 dry-compose's composed object
  although built from a DIFFERENT staging lineage (their L11PB vehicle vs my
  launch-10 armed copy). +11,392 B vs launch-10's `d6214abd...` = exactly the
  fold-0 guarded object, the declared and expected difference.
- **Static checks**: printed gate 17/17 PASS (`logs/run-a2.console.log`,
  `logs/run-b.console.log`); `verification-evidence/static-checks.json`
  STATIC_CHECKS_PASSED 20/20 both candidates, check set identical to
  launch-10's.
- **Imports 196**: supplier-contract counts identical (196 strong / 6 weak /
  0 missing / 0 conflicts), import names list-equal vs launch-10's candidate;
  composed UND set IDENTICAL to launch-10's (198 == 198, set equality);
  gate I1+ declared +[]/-[] == actual +[]/-[] vs frozen r11.
- **Determinism**: run-b (fresh compose + package) reproduced run-a's composed
  object bit-identically; candidates a/b byte-identical at bare-original
  `acf00edd...`, bare-sdk `d262350a...`, wrapped fself `d94bb99a...`
  (30,822,599 B), combined-supplier.o `9394fbae...`; gate D1 PASS.
- **Gate v2 manifest mode vs frozen r11**: **GO (offline gate), 15/15**
  (`gate-receipt-launch10-walkguard-a-vs-b.json`, `21df37d5...`, exit 0) —
  W1 W2 W3 I1 D1 F1 S1 P1 + M0 M1 I1+ B1 B2 B3 D2. B2 PASS with the declared
  inherited r11-relative bridge removal matching the actual delta exactly;
  bridge object sha-identical to launch-10's (13/14 candidate build objects
  identical, only `combined-supplier.o` differs — imports-evidence
  provenance, the launch-10-vs-launch-9 pattern).
- **Candidates**: `prx-candidate-composition6-l10wg-a` (deliverable; `-b` is
  the byte-identical determinism twin). Runs
  `ue4ss-native-supplier-composition-6-l10wg/run-a|run-b`.

## 4. The exact one-define-flip to activate (verdict-gated, controller-owned)

MEASURED in `staging/asmhelper-guard/probe/` (throwaway objects, real
148-arg argv, never linked into any candidate):

| probe | form | result |
|---|---|---|
| 0 | guarded source, define ABSENT (this candidate's form) | rc=0, `81dd832b...`, **0** `syscall` instructions |
| 1 | guarded source + `-DRC_ASM_WALK_RX_GUARD=1` | rc=0, `6604ce2d...` == WST's fold-1, **1** syscall; argc=149 |
| 2 | guarded source with the ONE fold-default token flipped (`#define RC_ASM_WALK_RX_GUARD 0` → `1`) | rc=0, **argc=148 (composition-admissible)**, object `f5627427131f53ace920cfa178e4cb5711c02fde1b47eca5d1a45e1a4957a3df`, **1** syscall, RC::ASM export surface identical to probes 0/1 |
| 3 | pristine port unit (control) | rc=0 (object differs from the shipped shared object only by the redirected basename — expected) |

- **PRIMARY flip (no new code)**: in the staged `ASMHelper.guarded.cpp`,
  change `#define RC_ASM_WALK_RX_GUARD 0` → `#define RC_ASM_WALK_RX_GUARD 1`
  (one token, keep the basename), re-run this composition unchanged — same
  edition `1e210eb9...`, same receipts pattern (admit the flipped source via
  `--file`), declare object `f5627427...` in the manifest.
- **ALTERNATIVE flip (reproduces WST's measured fold-1 bytes `6604ce2d...`
  byte-for-byte)**: append `-DRC_ASM_WALK_RX_GUARD=1` to the retained
  replacement argv. This is REFUSED by the current edition's SEC32-3 drift
  assertion (measured: only source/-o/-MF may drift and an inserted token
  also changes argc — probe 1's argc=149), so it needs a further
  one-tagged-edit edition widening the drift set for `asmhelper-asmhelper`.
- **Gate** (WST-5): EITHER activation is admissible exactly when the IGW D2
  seq5 records `rx_result.mprotect_rc == 0` — matrix rows R1/R2 (`W_WORKS`)
  and R3/R4 (`W_FAILS_R_WORKS`); ABSENT/BOTH_FAIL/FATAL keep fold-0 or the
  compile-out posture. Runtime fail-closure (transition rc!=0 → loud refusal,
  NO read) remains the mis-fold backstop. The controller owns the act; this
  worker performs neither flip.

## 5. What this unblocks (and what it does not)

The R1/R4 rows now have their next-step candidate STAGED AND MEASURED: armed
table + guard unit co-composed, receipted, twin-built and gate-GO'd, with
activation reduced to the measured one-define-flip of §4. NOT unblocked: any
ACTIVE/armed fold (needs the IGW verdict, which is PARKED), the U-SWAP-4 PLH
per-page layer (separate, unbuilt), U-SWAP-2 armed backend, ledger-g arming,
DIFF-2 rebase, cc1 adoption. The controller reads LAUNCH10-MATRIX §4 boxes
1–6 before any staging decision; this pre-build does not substitute for that
lookup.

## 6. Failure history + new incident (preserved; never replayed blind)

- **Receipt first attempt** (`receipts/composition6-replace-receipt.json.firstattempt.json`,
  `36c018c2829ba710...`): 96 files — I mirrored launch-10's 3 initializer
  admissions + the guarded source but missed that launch-10/11 ALSO explicitly
  admitted the 4 Resolution-B headers; the diff vs launch-10's receipt caught
  it before any build.
- **Packaging first attempt** (`prx-candidate-composition6-l10wg-a.rootgap-firstattempt`):
  12 lineage-check gaps, fail-closed before any static check.
  **Root cause (NEW shared-tree incident, preserved untouched, owner
  reconciliation pending)**: a STRAY unowned
  `outputs/2026-10-04-zcode-team/.superpowers/native-dependency-spike/owned-dll-sdk-candidate-1/...`
  tree (20 KB, created 2026-10-05 05:42 by another worker) shadows
  `package_prx.py::_find_root()`, which walks up from the script location
  looking for `.superpowers` — so ROOT resolved one level too deep and every
  ROOT-derived pin missed. **Fix = WGP E4** (in the driver, in memory only;
  the frozen packaging file stays byte-identical, `e6fbe6c5...` re-verified):
  re-point the loaded module's ROOT/SPIKE/PIN paths onto the true root with
  per-path existence refusal — every lineage check still runs fail-closed
  against the real artifacts (`staging/wgp-e4-root-rebase.json` records the
  re-base). Until the owner/controller reconciles or removes the stray tree,
  EVERY comp6-lineage packaging run needs an equivalent re-base — launch-10's
  own driver would now fail the same way. I did NOT touch the stray tree
  (unowned content; same disposition as launch-11's pre-existing
  `asmhelper-asmhelper.d` mutation `e346c81a...`, unchanged by my runs).

## 7. Honesty bounds

No in-game, verdict, kernel-behavior, seed-validity, or quality claim of any
kind; nothing executed on target. Fold-0 and the armed table ship exactly as
their owners staged them — the guard unit is inert-by-construction at fold-0
(no mprotect text, WST-measured and re-measured here), and arming re-opens
FLAG-2 as the launch-10 verdict regardless of this candidate. All evidence is
packaging-level: byte identities, symbol/string censuses, gate receipts. Gate
S1 (0.12% vs the comp-4-era baseline) and D1 (double-build byte identity) are
not quality statements. The probe objects are host compiles of console-target
code — they prove the fold compiles and which form carries syscall text, not
console behavior. The `f5627427...` flip object is measured but NEVER linked;
the fold-1 object in a real activation must be re-measured by whoever runs
the flip.

## 8. Deliverables (this dir)

`CLAIM.json` (first) · this `REPORT.md` (last) · `RECEIPTS.txt` (sha256
pins) · `verification-receipt.json` + `verify_walkguard_composition.py` ·
candidates `prx-candidate-composition6-l10wg-a/-b` · runs
`run-a/-b` + overlays `lineage-overlay-2-l10wg-a2/-b` · receipts (replace
100-file + extra + preserved first attempt) · manifest + GO gate receipt ·
edition + driver (both diff-verified) ·
`staging/initializer-walkguard/` + `staging/asmhelper-guard/` (baseline copy,
diff, guarded source, probe dir + probe script) · `evidence/` snapshots ·
`logs/` · preserved first attempts.
