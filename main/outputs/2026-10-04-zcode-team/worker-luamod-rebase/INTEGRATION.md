# INTEGRATION — controller runbook for the L10LM (DIFF-1 rebased) composition

worker-luamod-rebase, 2026-10-04. Everything is staged under
`outputs/2026-10-04-zcode-team/worker-luamod-rebase/` (below: `LM =` that dir).
The controller builds; this worker did NOT run the compose/package. All shas are
sha256; full pin list in `LM/receipts/sha-manifest.json`.

## 0. What gets composed

The BOTH_FAIL-branch R3-armed candidate, now WITH the luamod unit replaced by
the rebased DIFF-1 (executor-tag formulation — no SettingsManager.hpp change):

- initializer: the r3-armed armed + DIFF-2-rev-B FOLD-1 copy, byte-verified
  re-stage (`8b0c7e99...`; SeedTable `59effe6e...`, SeedValidation `b8d851e9...`)
- luamod: `staging/luamod-l10rebase/LuaMod.cpp` `4540593ac9e66cdc`
  (= overlay lineage `0f0697a3...` + DIFF-1 luamod hunks + the L10LM
  executor-tag formulation; see REBASE.md)
- everything else: the exact launch-10/r3-armed lineage (program replacement,
  bootstrap replacements, extra units, wired entry `c97055f4...`, bridge
  `10385e22...` — all inherited from the l10r3 driver defaults).

## 1. Sha gates BEFORE anything runs

```bash
LM=outputs/2026-10-04-zcode-team/worker-luamod-rebase
cd /WORKSPACE/.codex/worktrees/translation-core/what

shasum -a 256 $LM/build_runtime_composition6_fixed_l10lm.py   # expect 9de2c4c8ccfbbdf8...
shasum -a 256 $LM/build_runtime_composition6_l10lm.py         # expect 9b332fc56ededa74...
shasum -a 256 $LM/staging/luamod-l10rebase/LuaMod.cpp         # expect 4540593ac9e66cdc...
shasum -a 256 $LM/staging/initializer-l10-r3/UnrealInitializer.cpp  # expect 8b0c7e9905ce4a85...
shasum -a 256 $LM/staging/initializer-l10-r3/SeedTable.h      # expect 59effe6e4cd34eb0...
shasum -a 256 $LM/receipts/composition6-replace-receipt.json  # expect 27c1b3b0dc8d646a...

# lineage pins the driver itself re-checks (fail-closed anyway):
shasum -a 256 outputs/2026-10-04-zcode-team/worker-prx-link-gaps/package_prx.py  # e6fbe6c5011b498e...
shasum -a 256 outputs/2026-10-04-zcode-team/worker-comp6-driver/build_runtime_composition6_fixed.py  # 8dd92a6fe4b71cf1... (FROZEN, unchanged)
```

Lineage diff checks (one line / the six tagged sites only):

```bash
diff outputs/2026-10-04-zcode-team/worker-comp6-driver/build_runtime_composition6_fixed.py \
     $LM/build_runtime_composition6_fixed_l10lm.py     # EXACTLY line 52 (L10LM E0)
diff outputs/2026-10-04-zcode-team/worker-launch10-r3-armed/build_runtime_composition6_l10r3.py \
     $LM/build_runtime_composition6_l10lm.py           # docstring + L10LM E0/E1/E2/E3/E4/E5 only
```

## 2. Optional host re-verification of the staged luamod source (no composition)

```bash
python3 $LM/compile_gate.py     # L10LM-COMPILE-GATE-2, ~1-2 min, writes LM/receipts/compile-gate.json
```

Expect: `gate_pass: true`; control `a9716f55...` vs pinned `2371839b...` =
exactly the 6 attributed +0x30 bytes; variantB `5595feb7...` == variantA
`5595feb7...`; no new UND symbols. (The gate rewrites only `LM/receipts/`.)

## 3. The build (one-pass, then determinism pass)

The driver refuses pre-existing run/out dirs — pick fresh ones per pass, exactly
like the r3-armed run-a/run-b discipline:

```bash
cd /WORKSPACE/.codex/worktrees/translation-core/what
python3 $LM/build_runtime_composition6_l10lm.py \
  --composition-run $LM/ue4ss-native-supplier-composition-6-l10lm/run-a \
  --out $LM/prx-candidate-composition6-l10lm-a
# ... then a second full pass with run-b / prx-candidate-composition6-l10lm-b
```

All other arguments have pinned defaults inside the driver (replace-receipt
defaults to `LM/receipts/composition6-replace-receipt.json` via L10LM E5;
entry/bridge shas are the r3-armed pins). What the driver does, in order:
compose-fixed sha check → ODR gate → receipt preflight (coverage +
drift re-measure) → edited-landed gate → entry/bridge preflight →
compose (the L10LM E0 edition; expect TWO retained replacements — `program`
AND `luamod` — plus the affected six) → chain-symbols gate → overlay →
package (`package_prx.py` `e6fbe6c5...`, sha-checked, with the inherited
in-memory `__file__` point-in).

Fail-fast map (first-run risks specific to this staging):

| symptom | meaning |
|---|---|
| `Retained-unit replacement source must be covered by the --replace-receipt` | the receipt lost the luamod entry — re-run `python3 $LM/regen_receipt_l10lm.py` and re-hash |
| `Retained-unit replacement argv drifted beyond source/-o/-MF redirection` | the base receipt's luamod argv changed — STOP, reconcile (nothing in this staging should cause it) |
| `Changed input` on any pin | something outside `LM/` drifted since 2026-10-04 — STOP and reconcile; do not edit pins |
| `Affected header inventory changed` | would mean someone edited the affected-six headers — NOT expected; the L10LM E0 edition leaves the line-125 assertion untouched |
| stray-team-dir misroot in packaging (12 lineage gaps, r3-armed run-a precedent) | already handled in-memory by L10LM E4; if it recurs, apply the r3-armed remedy (remove/rename `outputs/2026-10-04-zcode-team/.superpowers/`) |

## 4. Expected unit delta vs the r3-armed run

368/368 unit names equal; exactly TWO units differ:

| unit | source | object |
|---|---|---|
| `unreal-unrealinitializer` | unchanged vs r3-armed (`8b0c7e99...`) | `f5aa9efa...` (same as r3-armed) |
| `luamod` | `0f0697a3...` → `4540593a...` | NEW (pinned `2371839b...` → fresh; host gate measured the fresh unit's UND set == pinned's, 565 == 565, so the candidate's import contract should hold — I1/I1+ will measure it for real) |

Determinism: two passes must be byte-identical at every artifact level
(composed object, bare-original, bare-sdk, wrapped fself, combined-supplier.o).

## 5. Manifest + gate v2 (controller-owned steps, r3-armed precedent)

1. Derive `LM/composition-change-manifest-launch10-l10lm.json` from
   `worker-launch10-r3-armed/composition-change-manifest-launch10-r3armed.json`:
   - new `manifest_id` (e.g. `composition6-launch10-l10lm-diff1-1`);
   - `changed_units`: the SAME initializer entry (paths re-pointed to
     `LM/staging/initializer-l10-r3/`) PLUS a second entry
     `{"unit":"luamod","role":"replace","source_paths":[LM/staging/luamod-l10rebase/LuaMod.cpp],"sha256_after":{...:4540593a...}}`
     and a note that DIFF-1 is rebased per `LM/REBASE.md` with the executor-tag
     formulation (no SettingsManager.hpp change — zero shared-header recompile
     set);
   - `import_delta`: declared empty both directions (rationale unchanged, plus:
     the fresh luamod UND set == the pinned luamod UND set, measured);
   - `resolution_b` / `d2_text_base`: carry verbatim from the r3-armed manifest.
2. Run `worker-gate-evolution/gate_check_composition5_v2.py` in manifest mode vs
   the frozen r11 baseline, as the r3-armed worker did; expect the same 15/15
   check families (W1 W2 W3 I1 D1 F1 S1 P1 + M0 M1 I1+ B1 B2 B3 D2).

## 6. Known design consequence (decide BEFORE treating this as launch-grade)

The DIFF-1 drain cluster is link-inert as authored (file-static, no TU-internal
caller): the composed candidate will carry the R3 readiness/ladder/table surface
but NOT an armed drain — the initializer's deferred pump still has no consumer
until a caller for `arm_funcptr_marshal_drain` exists. Options belong to the
controller/authoring worker (non-static linkage + a reachable declaration vs an
in-TU caller); see REPORT.md §5. Also note the recorded stale-layout exposure
class for retained units (REPORT.md §4): this admission REPAIRS the luamod
instance of it.
