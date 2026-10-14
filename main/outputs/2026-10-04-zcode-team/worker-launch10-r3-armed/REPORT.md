# REPORT — worker-launch10-r3-armed (2026-10-04)

Mode: HOST BUILD ONLY. No console contact, no nested subagents, nothing staged to any
console, nothing executed in-game. All writes confined to this worker's dir plus its
script-owned run/overlay/candidate dirs. Deliverable order honored: CLAIM.json first
(written before any build work, amended with measured verdicts), candidate + receipts,
this REPORT last. Tool calls: ~45 (within the 40–80 budget).

## 1. Task and verdict

Compose the launch-10 R3-ARMED candidate for the BOTH_FAIL matrix rows (armed SeedTable +
DIFF 1 luamod FuncPtr-marshal + DIFF 2 rev B initializer R3-mode fold 1), verify
17/17 statics / imports / determinism / gate v2, and honestly check the DIFF-1/DIFF-2
unit argv integration against the frozen-script constraint.

**Verdict: the DIFF-1 half is NOT composable through the frozen composition-6 fixed
script — three independent blockers, exact citations below. The buildable candidate was
built and fully verified: the INITIALIZER-ONLY R3-ARMED composition (armed SeedTable +
DIFF-2 rev B in fold 1, no luamod change), two full one-passes, byte-identical, gate v2
GO 15/15.** It is a packaging-level composition candidate for the BOTH_FAIL branch, NOT
a launch-grade GO: fold 1 defers the game-thread pump to the R3 marshal drain and
nothing in this candidate drains it (see §5).

## 2. The DIFF-1 blockers (the honest check, exact)

1. **Frozen-script eligibility (decisive).** `worker-comp6-driver/build_runtime_composition6_fixed.py`
   (sha-pinned `8dd92a6f…`, frozen) admits `--replace-runtime-unit` targets only from
   (a) the hard-asserted affected set `{unreal-unrealinitializer, native-ownedmappedscansource,
   native-psscanbinding, native-ownedmappedcandidateselection, native-loadercontext,
   native-programfactory}` — any other affected inventory raises "Affected header
   inventory changed" — or (b) the attested retained allow-list `RETAINED_REPLACEABLE={'program'}`
   (SEC32-3). The composed unit `luamod` (source `ue4ss-native-notify-cancel-1/src/LuaMod.cpp`)
   is in neither: a replacement spec raises `ValueError('Bad replace-runtime-unit spec…')`.
   No other recompile channel exists: in-place spike edits are refused by the unit source
   pin (`Changed input`), and an rr-admitted in-place edit would still link the STALE
   pinned luamod object (non-affected units are never recompiled). There is also no -D
   injection for replaced runtime units (`--extra-define` is extra-units-only) — which is
   why fold-1 arming uses the documented in-file fallback flip (ARM-DISARM §1 step 2,
   receipt-covered).
2. **DIFF-1 base skew vs the composed unit (independent).** DIFF-1 is authored on the
   PORT-TREE bases `LuaMod.cpp a2d07f7c…` / `SettingsManager.hpp a9b4abbe…` (measured on
   `native-coherent-runtime-1/source/port/ue4ss/…`). The composed luamod unit compiles a
   DIFFERENT lineage: the notify-cancel overlay copy `0f0697a3…` (attested by
   `ue4ss-native-notify-cancel-1/plan.json overlay_source_sha256`, the base receipt, and
   every composed run receipt). DIFF-1 needs a NEW anchored rebase onto that lineage with
   new ids (same class as the §4-d initializer skew the rebase worker closed) — never a
   hand-patch, and not this worker's authorization.
3. **Shared-header recompile set.** DIFF-1's SettingsManager.hpp enum append (base
   `a9b4abbe…` IS the include-tree file the composed units consume) has the documented
   recompile set luamod + settingsmanager + ue4ssprogram + luamodref (+ includers). The
   frozen script recompiles none of them from changed headers; their pinned objects stay
   stale, so the enum could not reach any compiled TU through this script at all.

**L11-5 note (recorded, not binding here):** the shared-site drift the rebase documented
(rev B anchors vs the L11-5 gate-first flow's rewrites of the two EngineTick sites) does
NOT bind on the launch-10 staging — launch-10's staging is RAW `c85c42f7…` + the adjacent
armed `SeedTable.h` with no `apply_gate.py` rewrite, and rev B applied cleanly
(dry-run clean; applied bytes == the rebase worker's anchored patched copy, `72247480…`).
The binding blocker for DIFF-1 on THIS launch is blocker 1, not site ownership.

## 3. What was built

`staging/initializer-l10-r3/`: armed staging copied verbatim from the prebuild
(`UnrealInitializer.cpp c85c42f7…`, `SeedTable.h 59effe6e…` 4/4 armed, `SeedValidation.hpp
b8d851e9…`), then DIFF-2 rev B (`68d0c006…`) applied by `patch -p1`, then exactly ONE
line changed: line 179 `#define UE4SS_R3_MARSHAL_MODE 0` → `1` (the ARM-DISARM §1.2
arming fallback; the staged file is `8b0c7e99…`). Fold arming is proven equivalent to the
build define: `receipts/fold-equivalence.json` — flip-vs-flag compile to IDENTICAL
objects (`f5aa9efa…`) under the exact composition initializer argv (186 tokens + the 4
prepends); rev-B fold 0 on the same staging reproduces the prebuild's armed object
`983b18e2…` byte-exactly (fold-0 content-neutrality, now also proven on the armed table).

Driver: `build_runtime_composition6_l10r3.py` — d1fix-lineage copy with exactly three
tagged edits (diff vs the prebuild driver verified at 25 changed lines):
- **L10R3 E1** MY → this worker dir (scope isolation).
- **L10R3 E2** initializer `--replace-runtime-unit` → this worker's staged armed+R3 copy.
- **L10R3 E3** (found during the build, in-memory only): `package_prx.py` locates the
  workspace root by walking ancestors of its `__file__` to the first dir containing
  `.superpowers` — and a **stray `outputs/2026-10-04-zcode-team/.superpowers/` dir
  (created 2026-10-05 05:42 by another session; not this worker's file, left untouched)**
  stops that walk at TEAM and misroots every PIN path (first run-a attempt: 12 lineage
  gaps, preserved unchanged in `logs/run-a.console.log`). E3 points the exec-time
  `__file__` at a virtual path under the REAL workspace `.superpowers`; the loader still
  reads the sha-pinned packaging bytes (`e6fbe6c5…`, checked); nothing on disk changes.
  The build receipt `script` field records the virtual origin. The OWNER OF THAT STRAY
  DIR SHOULD REMOVE OR RENAME IT — it will misroot every future in-process exec of
  `package_prx.py` (and any other `_find_root`-style walker) from TEAM paths.

## 4. Verification results (all measured, evidence in this dir)

| check | result |
|---|---|
| Static packaging | `STATIC_CHECKS_PASSED`, 20 named checks failing none, in BOTH candidates (`verification-evidence/static-checks.json`); 17 PASS lines per packaging console log |
| Imports | 196 == 196, `imports_equal_extended_contract` PASS; combined-supplier.o UND set (211) IDENTICAL to the prebuild candidate's; Phase-A lineage delta `+[sceKernelGetModuleInfo]` = one of the 5 pinned EXTENDED_IMPORTS (same delta as every prebuild run); gate I1+ declared +[]/-[] == actual +[]/-[] |
| Determinism | fresh compose twice → composed object `4be218ef…` (117,740,160 B) bit-identical; bare-original `89b27917…`, bare-sdk `ed24b87b…`, wrapped fself `379eb490…` (30,805,575 B), combined-supplier.o `70e036e7…` all byte-identical across passes |
| Gate v2 (manifest mode vs frozen r11) | **GO, 15/15** — W1 W2 W3 I1 D1 F1 S1 P1 + M0 M1 I1+ B1 B2 B3 D2 (`gate-receipt-launch10-r3armed-a-vs-b.json`, exit 0); S1 size 0.06% vs comp-4-era baseline |
| Unit delta vs prebuild | 368/368 unit names equal; **367/368 source+object hashes identical**; only `unreal-unrealinitializer` differs (source `c85c42f7…` → `8b0c7e99…`, object `983b18e2…` → `f5aa9efa…`, 1,925,952 → 1,817,256 B — the R3 compile-outs net of the R3 additions) |
| Armed + R3 content | armed banner, `slot 3 gmalloc bound at`, `[R3] refused:`, `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal` all present; Shape-A refusal string absent; the three `R3Marshal*` bridges DEFINED in the composed object; all 4 SeedTable constants exported; bridge object byte-identical to the prebuild's (`16331ebe…`) |
| Receipts | replace-receipt 99 files = prebuild lineage with ONLY the 3 staging admissions swapped to this dir (96 shared hashes identical, zero drift); extra-receipt byte-equal to the prebuild's |

**Two measured corrections to inherited claims** (recorded in
`receipts/initializer-content-checks.json`, not copied): (a) `Phase 2: Verifying FName
constructor…` is ABSENT from this fold-1 object (present in prebuild fold 0) — the R3
early-return compiles out the whole FName verification path, per design; (b)
`[Seeds] REFUTED` has 0 hits in BOTH this object AND the prebuild's on-disk armed
initializer object — the prebuild CLAIM's "present: 1" did not reproduce against their
artifact; this worker does not inherit that number.

## 5. Honesty bounds

- **No launch claim.** Fold 1 compiles out the FName-ctor verification detour and BOTH
  EngineTick registrations, sets `pump_pending`, and prints the literal deferral line.
  The pump consumer is LuaMod DIFF 1's drain — not in this candidate (§2) — and the
  calibration service (R3D §5.2 item 4) does not exist. In-game this candidate is
  EXPECTED to print the deferral line and never register the EngineTick pump. It exists
  so the composition shape, fold arming, receipt walk, packaging, imports and gate are
  PROVEN ahead of a controller decision on (i) DIFF-1 composition mechanics (a
  comp-7-class driver clone or an attested allow-list extension) and (ii) the DIFF-1
  rebase onto the `0f0697a3…` notify-cancel LuaMod lineage.
- FName verification is NOT performed under R3 mode (latch settled, loud refusal, FC-1
  validator is the designated future replacement). F-1 remains open.
- `[Seeds] REFUTED` remains a possible early terminal; the armed walk re-opens FLAG-2 /
  FLAG-1 as launch verdicts, exactly as the prebuild recorded. No kernel behavior, seed
  validity or gameplay quality is claimed anywhere; all evidence is packaging-level.
- The LAUNCH10-MATRIX file was not edited (not this worker's file); the blockers above
  are this dir's CLAIM.json + this REPORT for the matrix owner.

## 6. File inventory (this dir)

| file | what |
|---|---|
| `CLAIM.json` | written FIRST; composition decision + blockers + measured verdicts |
| `staging/initializer-l10-r3/` | armed staging + DIFF-2 rev B + the one-line fold-1 flip (`UnrealInitializer.cpp.r3fallback0` = pre-flip copy kept for the diff) |
| `build_runtime_composition6_l10r3.py` | d1fix-lineage driver, L10R3 E1/E2/E3 only |
| `fold_equivalence_precheck.py` | flip==flag + fold-0 content-neutrality proof |
| `receipts/fold-equivalence.json` | E1/E2 PASS, E3 recorded |
| `receipts/composition6-replace-receipt.json` / `-extra-…` | regenerated, prebuild-lineage-exact |
| `receipts/initializer-content-checks.json` | armed/R3 string + symbol census + UND notes |
| `composition-change-manifest-launch10-r3armed.json` | manifest_id `composition6-launch10-r3armed-initonly-1` |
| `gate-receipt-launch10-r3armed-a-vs-b.json` | gate v2 GO 15/15 |
| `prx-candidate-composition6-l10r3-a/` | **the candidate** (deliverable) |
| `prx-candidate-composition6-l10r3-b/` | byte-identical determinism reproduction |
| `ue4ss-native-supplier-composition-6-l10r3/run-a|run-b` | compose runs |
| `staging/lineage-overlay-2-l10r3-a|b` | packaging overlays |
| `logs/run-a.console.log` | PRESERVED first-attempt failure (12 lineage gaps, the stray-dir misroot) |
| `logs/run-a-repackage.console.log`, `logs/run-b.console.log` | the two passing passes |
