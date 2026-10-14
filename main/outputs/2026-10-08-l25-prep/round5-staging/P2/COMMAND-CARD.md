# P2 (variant-B removal) — AC build-queue command card (staged by agent IH, 2026-10-10)

Source proposal: `parked/P2-variant-b-removal/` (agent FX). This staging
rebased FX's set onto the CURRENT tree state (post-FY build_variants.py
fe01e390…, post-HH l26_packaging_driver.py 810efaae…, post-round4-set-aside
CHANGES.json 0e3ecab0…). Everything below was APPLY-CHECKED + PLAN-CHECKED in
/tmp staging only; the live tree was never touched. No builds/console.

## 0. Patch set (this directory)

| file | role | rebase verdict |
|---|---|---|
| `01-build-variants-drop-b.patch` | FX original, byte-identical copy | APPLIES CLEAN to current `build_variants.py` (fe01e390…): 5/5 hunks, `--fuzz=0`, offsets 0/+1 lines (FY/HH additions are all BELOW the docstring region; none of the five regions changed). Post-state sha `d377802f…` (differs from FX NOTES §0's `53739fcf…` ONLY because the base moved 5618dd51→fe01e390; hunk delta byte-verified identical to intent). |
| `02-tree-dead-code.patch` | FX original, byte-identical copy | Tree hunks apply CLEAN (UnrealInitializer.cpp → `6a9087b5…`, LuaMod.cpp → `5a6a06e6…` — BOTH exactly match FX's §0 expected hashes; tree unchanged since FX). CHANGES.json: 4/5 hunks clean; the tail hunk (@863) REJECTS — live tail gained `round5_row_sync`/`kb_tu_note`/`round4_setaside` after FX's anchor. |
| `02b-changes-tail-rebased.patch` | IH rebase adaptation | Appends the byte-identical `fx_p2_row_sync` entry as the new LAST key (comma moved from FX's round4_row_sync anchor to `"round4_setaside": true`). Pure re-anchoring; applies clean at `--fuzz=0` after 02; JSON validated, no trailing newline preserved. Post CHANGES sha `6ac6cf57…` (live-path form). |
| `03-packaging-driver.patch` | FX original, byte-identical copy | Hunks 1/3/4 apply CLEAN (docstring header line, VARIANTS tuple → `B_PKG_VARIANT`/`L26_PACK_B`, pair_packaging_receipt guard). Hunks 2 (@22: docstring item-5 + `import os`) and 5 (@353: expected-gates list) REJECT — agent HH's sidecar changes (docstring item 6; P12/P13 gate lines) interleave both regions. |
| `03b-packaging-driver-postHH-rebase.patch` | IH rebase adaptation | HUNK A: FX's item-5 docstring edit re-anchored on the CURRENT item-5 text (which HH re-terminated `).` → `);` — FX's replacement lines keep the `;`), plus FX's `import os` unchanged. HUNK B: FX's conditional `pair_gates` (P10/P11 only when B active, byte-identical) composed WITH HH's unconditional P12/P13 entries: gates = […P5–P9]+pair_gates+[P12,P13] + FX's `pair_gates_note` verbatim. Post packaging-driver sha `6adfc6ee…`; py_compile clean. |
| `evidence-PLAN-*.json` | the four staged plan runs (see §4) | — |

P1 composition: P1's packaging hunk (lineage_overlays region, ~line 156) is
DISJOINT from every P2/03 hunk. Verified in staging BOTH directions: P2/03+03b
applies to the current (pre-P1) driver, and P1's hunk applies cleanly ON TOP of
the full post-P2 driver (py_compile OK). If AC runs P1 first, order P1 → 01 →
02(+02b) → 03(+03b) works unchanged.

## 1. APPLY SEQUENCE (root, live tree, from worktree root `what/`)

Pre-flight — verify the live files still match the states this staging was
checked against (parallel agents are ACTIVE; `profile/profiles/townfall-build-facts.json`
drifted and was re-fixed DURING this staging session):

```sh
cd /WORKSPACE/.codex/worktrees/translation-core/what
shasum -a 256 outputs/2026-10-08-l26-runtime/{build_variants.py,l26_packaging_driver.py} \
  outputs/2026-10-08-l26-runtime/typescan-r4/CHANGES.json \
  outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/{UnrealInitializer.cpp,LuaMod.cpp}
# expect: fe01e390… 810efaae… 0e3ecab0… 6ed7a208… aac03fbc…
# (if CHANGES.json moved again: re-check that the 02b tail anchor
#  '"round4_setaside": true' is still the last key's line — re-anchor if not)
```

Apply (each `patch` from `what/`; P=`outputs/2026-10-08-l25-prep/round5-staging/P2`):

```sh
/usr/bin/patch -p1 --fuzz=0 < $P/01-build-variants-drop-b.patch        # clean, no output beyond "patching file"
/usr/bin/patch -p1 --fuzz=0 < $P/02-tree-dead-code.patch               # EXPECT rc=1: "1 out of 5 hunks failed" (CHANGES tail) — tree hunks clean
/usr/bin/patch -p1 --fuzz=0 < $P/02b-changes-tail-rebased.patch        # clean; completes the CHANGES append
/usr/bin/patch -p1 --fuzz=0 < $P/03-packaging-driver.patch             # EXPECT rc=1: "2 out of 5 hunks failed" (hunks @22 + @353) — 1/3/4 clean
/usr/bin/patch -p1 --fuzz=0 < $P/03b-packaging-driver-postHH-rebase.patch   # clean; completes the packaging driver
rm outputs/2026-10-08-l26-runtime/l26_packaging_driver.py.rej outputs/2026-10-08-l26-runtime/typescan-r4/CHANGES.json.rej
python3 -m py_compile outputs/2026-10-08-l26-runtime/{build_variants.py,l26_packaging_driver.py}
```

Post-state hashes (live-path form): `build_variants.py d377802f…`,
`UnrealInitializer.cpp 6a9087b5…`, `LuaMod.cpp 5a6a06e6…`,
`CHANGES.json 6ac6cf57…`, `l26_packaging_driver.py 6adfc6ee…`.
(The CHANGES tail-appended `fx_p2_row_sync` entry is the row-sync — pins move
WITH the files in patch 02's four row hunks; no further row sync required.)

## 2. SNAPSHOT + BUILD + PACKAGE + VERIFY

```sh
# archive previous variant dirs (G3 dir-fresh refuses overwrite; frozen provenance never deleted)
mkdir -p outputs/2026-10-08-l26-build-variants/archive-p2-$(date -u +%Y%m%dT%H%M%SZ)
mv outputs/2026-10-08-l26-build-variants/l26-A outputs/2026-10-08-l26-build-variants/archive-p2-…/  # if present

# stamp provenance (agent FG discipline)
BRIDGE_SNAPSHOT_ID=<SNAPSHOT-ID from harvest/snapshot_tree.sh>

python3 outputs/2026-10-08-l26-runtime/build_variants.py --plan      # gate picture §4 must match BEFORE execute
L26_BUILD_B=1 python3 outputs/2026-10-08-l26-runtime/build_variants.py --plan   # optional loud-failure proof: G4-token-present ok=False
BRIDGE_SNAPSHOT_ID=… python3 outputs/2026-10-08-l26-runtime/build_variants.py --execute

python3 outputs/2026-10-08-l26-runtime/l26_packaging_driver.py --plan
python3 outputs/2026-10-08-l26-runtime/l26_packaging_driver.py --prepare
python3 outputs/2026-10-08-l26-runtime/l26_packaging_driver.py --execute
```

Execute-path note (FY concurrency): main() iterates the ACTIVE variant set —
an A-only `--execute` schedules 2 pass tasks (A1/A2), not 4; finisher + guarded
pair_receipt run as staged. The `G-concurrent-twins-budget` gate detail still
lists the A/A/B/B merge order in its note string (informational only, cosmetic).

## 3. RECEIPT-DIFF EXPECTATIONS (which files stop being written)

Default (A-only) runs STOP writing:
- build: `outputs/2026-10-08-l26-build-variants/VARIANT-PAIR-RECEIPT.json`
  (G9/G10 recorded ok=null "planned-skip"; step `write-variant-pair-receipt`
  `skipped:true`).
- packaging: `VARIANT-PAIR-PACKAGING-RECEIPT.json` and
  `FINAL-RECEIPT-VARIANT-B.json` (P10/P11 ok=null planned-skip at execute;
  expected-gates list drops P10/P11, keeps P12/P13).

Unchanged for A: `EXECUTION-RECEIPT.json`/`PLAN.json` (variants[] now A-only),
per-variant TWIN/SOURCE-SELECTION/CHANGE-SELECTION/marker receipts, G6 census
394 = 391+3 NEW_UNITS per variant per pass (B shares A's units — census STAYS
394 with single-variant; confirmed in staged plans), G7 import-delta, G8 twin
identity within A, P0–P9 + P12/P13 per-variant packaging gates, P8 239/2.
Frozen provenance never touched: `l26-B*` build dirs, `2026-10-08-l26-package-B*`.

Opt-in restores the full pair machinery: `L26_BUILD_B=1` build plan = 74
gates/823 steps with variants [A,B] and pair-receipt path written (staged
evidence) — but G4-token-present is ok=False (token absent from every compiled
source) and `--execute` then raises FX's refusal ("refusing to build a variant
B that would silently equal variant A"). Same shape on packaging side via
`L26_PACK_B=1` (P10/P11 active) — blocked upstream by the build refusal.

## 4. PLAN-MODE GATE PICTURE (actual staged output; evidence-PLAN-*.json)

Build default (B absent): rc=0 — `variants:[A]`, 68 gates, 413 steps,
affected 202, `G4-token-present ok=null` "variant B not in the active build
set (agent-FX P2 drop); skip-token presence not enforced this run",
`G4-ctoken-present ok=true`, `G5: token_units_B=[]`, `G9/G10 ok=null`
planned-skip, pair step skipped, census 394/394, copyin overlay mode
`patch-rebased-onto-revised-base` (Q's overlay composes CLEANLY on the patched
UnrealInitializer; G2q-order-def-before-use true). ONLY ok=False gate:
`G2q-copyin-probe-pin-unrealinitializer.base` — IDENTICAL to the live pre-P2
baseline plan and the last real execute (documented expectation-not-invariant
base drift; NOT P2-caused). G1 (19,044 pins) and G2r both true.

Build `L26_BUILD_B=1`: rc=0 — variants [A,B], 74 gates/823 steps (exactly the
live A+B baseline shape), `G4-token-present ok=FALSE` with note "variant B
active (L26_BUILD_B=1); token presence enforced at execute" (the loud-fail
evidence; the raise itself fires only at --execute), pair receipt planned.

Packaging default: rc=0 — variants [A], 23 gates/4 steps, expected gates
P5–P9 + P12/P13 only, `pair_gates_note: "P10/P11 skipped: variant B not in the
active packaging set (agent-FX P2 drop)"`, zero False gates.

Packaging `L26_PACK_B=1`: rc=0 — variants [A,B], 37 gates/7 steps, P10/P11
back in the expected-gates list, note "A/B pair gates active (B opted in)".

Token census (FX §1.1 re-run on the staged post-P2 tree): 0 occurrences of
`MODBRIDGE_L25_SKIP_DELETE_LISTENER` in ANY compiled source (typescan-r4
candidate src/include/generated_include + include/, profile, thread, NEW_UNITS
sources). Remaining occurrences: the two drivers' `TOKEN=` constant and
CHANGES.json note text (7) — neither compiled. Cleaner than FX NOTES §3
stated: Q's frozen `copyin-probe/candidate` copy ALSO has 0 occurrences
(verified staged AND live).

## 5. ROLLBACK (unapply)

Reverse order (each from `what/`):

```sh
/usr/bin/patch -R -p1 < $P/03b-packaging-driver-postHH-rebase.patch
/usr/bin/patch -R -p1 < $P/03-packaging-driver.patch        # EXPECT rc=1: the 2 never-applied hunks reject; the 3 applied ones reverse
/usr/bin/patch -R -p1 < $P/02b-changes-tail-rebased.patch   # NOTE: do NOT pass --fuzz=0 here (see quirk below); git apply -R also works
/usr/bin/patch -R -p1 < $P/02-tree-dead-code.patch          # EXPECT rc=1: tail hunk rejects (it rode 02b); tree+row hunks reverse
/usr/bin/patch -R -p1 < $P/01-build-variants-drop-b.patch
```

Verified in /tmp staging: all five files return byte-identical to pre-P2
(fe01e390… / 810efaae… / 0e3ecab0… / 6ed7a208… / aac03fbc…).
BSD-patch quirk (documented): `patch -R --fuzz=0` MIS-HANDLES 02b's trailing
"\ No newline" context and rejects a hunk that is actually present; plain
`patch -R` (zero actual fuzz) and `git apply -R` both reverse it exactly.
Alternative rollback: restore the archived pre-apply copies (snapshot before
applying is still the safest).
