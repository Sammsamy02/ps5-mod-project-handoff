# REPORT — worker-r3-diff2-rebase (2026-10-04)

Mode: DIFF REBASE + COMPILE GATE ONLY. No console contact, no nested subagents, nothing
staged into any composition, nothing executed on a console; the spike tree, every staging
tree and the superseded rev-A diff were modified NOWHERE — all work ran on hash-pinned
copies under this worker's directory (the control compile reads the attested staging file
in place, read-only, under its exact attested argv). Deliverable order honored: CLAIM.json
first, rebase script + diff + receipts, MATRIX-NOTE-SKEW-CLOSED.md, this REPORT last.
Tool calls: ~42 (within the 30–60 budget).

## 1. Task and verdict

LAUNCH11-MATRIX §4-d measured a BASE SKEW: DIFF 2 (initializer R3-marshal mode) was
authored on the query-s2-arm staging initializer `933d08a2…` while the live merge lineage
is `c85c42f7…` (worker-initializer-integration/staging), rebase MANDATORY before any L11
R3-ARMED build. **The skew is closed.** Deliverable: DIFF 2 **rev B**
(`diffs/r3-initializer-r3-marshal-mode.revB.diff`, sha256 `68d0c0064de69284…`, 6 hunks)
anchored on the pinned `c85c42f7aaab0630…` staging bytes, compile-gated BOTH folds under
the exact composition initializer argv, with the fold-0 object-identity check PASSING.

## 2. How the rebase was done

- **Anchored replayer** (`rebase_diff2.py`): base pin verified (c85c42f7… == replace-
  receipt pin), rev-A diff pin verified (d556c4e7…, superseded, untouched), then 5
  anchored splices covering the 6 rev-A hunks — each anchor occurrence-count == 1
  (receipts/anchors.json; drift aborts, never hand-patch). `diff -u` from the anchored
  output IS the deliverable; `patch -p1 --dry-run` clean and a real `patch -p1` output is
  sha256-identical to the anchored output (receipts/patch-dryrun.txt).
- **All 6 hunks ported**: (1) `UE4SS_R3_MARSHAL_MODE` define (in-file fallback 0) +
  constexpr `R3MarshalGate` + `[[maybe_unused]]` on `HookedEngineTick`; (2) attr on
  `QueryHarnessEngineTickDispatch`; (3) FName-ctor PLH-detour compile-out (data-side
  address override kept, verification latch settled + notify so the blocking wait is
  unreachable, loud refusal, honest NOT-verified label, FC-1 delegation); (4/5) both
  `__PROSPERO__` EngineTick registrations compiled out with the literal line
  `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal` +
  `pump_pending` set (the DIFF 1 seam) and the doubled refusal count; (6) exported
  bridges `R3MarshalPublishGameThreadIdentity / R3MarshalQueryHarnessDispatch /
  R3MarshalPumpPending` at the RC::Unreal tail, bodies verbatim from rev A.
- **Drift reconciliation (seed paths + Shape-A)**: the merged lineage's seed consumption
  composes with the compile-outs — the seed path skips the detour verifier when
  `SeedArming::bSeedBoundFNameCtor` is set and settles the SAME
  `FNameVerificationStatus` latch rev B settles (armed-seed, Shape-A-refused and
  R3-mode launches all compose); the EngineTick registration block is byte-identical
  across both lineages (seed work changed only the resolve branch above it), so the
  deferral anchors cleanly; with armed seeds the vtable tick fallback binds and the
  deferral replaces only the registration step. Bridge-comment line references updated
  to the merge-lineage line numbers.

## 3. Rev-B design delta (recorded, intended): hard preprocessor fold

Rev A's additions were `if constexpr`-shaped, so its fold 0 was only KIND-parity (its
bridges and `pump_pending` still emitted). This session's gate demanded fold 0 =
byte-faithful with object identity, so rev B guards EVERY addition on
`UE4SS_R3_MARSHAL_MODE` (same fallback-0 semantics; the constexpr gate lives inside the
guard unchanged). Consequences:

- Fold 0 preprocessed token stream is IDENTICAL to the unpatched base (verified), and
  fold 1's armed semantics are rev A's exactly (gate value true, same compile-outs).
- **Link contract change**: a DISARMED initializer no longer defines the `R3Marshal*`
  bridge symbols; DIFF 1's LuaMod references them under `__PROSPERO__`, so mismatched
  folds now fail the LINK loudly. ARM-DISARM §2.3's pairing law is now linker-enforced;
  L11 rows arm both defines together (§3.5 step 3). Recorded in CLAIM.json non-claims
  and the matrix note — not hidden.

## 4. Compile gate (receipts/compile-gate.json, fold0-object-parity.json)

Exact composition initializer argv: notify-cancel build-1 receipt argv for
`unreal-unrealinitializer` (186 args) + the 4 args the composition script prepends at
argv[1:1] (`-I ue4ss-native-tick-file-owner-1/include -I
ue4ss-native-tick-witness-1/include`, per build_runtime_composition4_fixed.py:94),
source/-o/-MF redirected, LLVM_CONFIG env, prospero-clang++ -std=c++23 -O0. The control
compiles the attested c85c42f7 file at its home path (its staging-local SeedTable.h /
SeedValidation.hpp resolve adjacent, as attested); patched copies get one terminal `-I`
to that staging dir (names unique in the include chain).

| variant | exit | object |
|---|---|---|
| control (unpatched) | 0 | `fe639a85…` — byte-exact reproduction of the attested object |
| fold 0 (rev B, no define) | 0 | `fe639a85…` — **sha256-identical to the unpatched object** |
| fold 1 (`-DUE4SS_R3_MARSHAL_MODE=1`) | 0 | `7f058ed8…` — differs by design; carries the deferral/refusal wide strings (UTF-16LE) and the three `R3Marshal*` symbols; fold 0 carries none |
| -Werror x3 | 1 | the PINNED BASELINE itself does not pass -Werror (pre-existing promoted diagnostics); kind parity: fold 0 AND fold 1 add zero new kinds, lose none |

Corroboration: -E outputs (linemarkers + blank lines stripped) are token-identical for
control vs fold 0. Textual check: zero base lines missing in the patched file; the only
additions are the guarded R3 blocks, comments, and three `[[maybe_unused]]` attr lines.
Gate verdict: **PASS** on all five checks.

## 5. Honest limits / handoff

- **-c only**: no link, no composition run, no boot. Cross-TU bridge resolution
  (LuaMod DIFF 1 <-> initializer rev B, same-ABI signatures) and the DIFF 1 rebase
  status (its bases `a2d07f7c…` / `a9b4abbe…` are NOT part of the §4-d skew) are
  unchanged, unverified here.
- **§4-e ownership still decided at merge time**: rev B anchors on the RAW c85c42f7
  bytes. L11-5's gate-first flow rewrites the two shared sites, so rev B's two site
  anchors will drift there — the expected detector; the gated-bytes variant is a
  composition-time re-anchor with new ids, never a hand-patch. Site ownership recorded
  in the rev-B deferral comment (DIFF 2 owns the two sites; the gate owns every other
  E-site).
- **L11PB vehicle note**: the launch11-vtable-prebuild vehicle is `39fe33f3…`
  (= c85c42f7… + 2 include lines + armed SeedTable.launch10.h). rev B's anchors are on
  c85c42f7… text; the 2 include lines sit outside every anchor, so rev B is expected to
  apply there, but that variant is NOT compile-gated in this session — gate it under the
  row's argv before any armed build.
- FName verification remains NOT performed under R3 mode (latch settled, loud line,
  FC-1 validator is the designated future replacement). F-1 remains open; nothing here
  claims any in-game marshal behavior.
- The matrix file itself was not edited (not this worker's file); the closure note for
  its owner is `MATRIX-NOTE-SKEW-CLOSED.md`.

## 6. File inventory (this worker's dir only)

| file | what |
|---|---|
| `CLAIM.json` | RR-1..RR-5 + non-claims + base pins (written FIRST) |
| `rebase_diff2.py` | anchored replayer (pins, count==1 anchors, abort on drift) |
| `diffs/r3-initializer-r3-marshal-mode.revB.diff` | the rebased DIFF 2 (rev B, 6 hunks) |
| `compile_gate_revb.py` | both-fold gate under the exact composition argv |
| `receipts/anchors.json` | 5/5 anchors occurrence-count 1, no aborts |
| `receipts/compile-gate.json` + `compile-gate-run.txt` | full gate results + human log |
| `receipts/fold0-object-parity.json` | the §4-d-closing object-identity receipt |
| `receipts/patch-dryrun.txt` | patch -p1 dry-run + applied-output hash match |
| `receipts/diff-hashes.json` | rev B + superseded rev A hashes |
| `MATRIX-NOTE-SKEW-CLOSED.md` | adoption note for the LAUNCH11-MATRIX owner |
| `work/` | hash-pinned base/patched copies + all gate outputs (argvs, stderr, objects) |
