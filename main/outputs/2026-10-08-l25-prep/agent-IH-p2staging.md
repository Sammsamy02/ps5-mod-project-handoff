# Agent IH — P2 build-staging prep (variant-B removal) — 2026-10-10

Battery rules honored: /tmp staging + plan-mode checks ONLY. NO builds, no
--execute, no console, no live-tree edits (verified post-session: all five
target files still hash to session-start values fe01e390/810efaae/0e3ecab0/
6ed7a208/aac03fbc; live stamp header untouched — plan-run stamp cleanup
happened only inside /tmp).

Staged deliverables: `outputs/2026-10-08-l25-prep/round5-staging/P2/`
(01/02/03 = FX's patches byte-identical; 02b/03b = IH rebase adaptations;
COMMAND-CARD.md = the AC sequence; evidence-PLAN-*.json = the four plan runs).

## Rebase verdict per patch (vs CURRENT tree, not FX's authoring bases)

- **01 build_variants.py — APPLIES CLEAN, no rebase needed.** The FY
  concurrency refactor (and HH/set-aside drift) did not touch any of the five
  regions: 5/5 hunks at `--fuzz=0`, offsets 0/+1. Delta byte-verified equal to
  the patch intent. Post sha d377802f (≠ FX §0's 53739fcf solely because the
  base moved 5618dd51→fe01e390). FY interplay checked by reading the patched
  main(): the concurrent execute path iterates the ACTIVE set, so A-only runs
  schedule 2 pass tasks and the guarded pair_receipt handles A-only; only
  cosmetic drift remains (G-concurrent-twins-budget note still lists B tasks).
- **02 tree+CHANGES — tree CLEAN (hashes match FX's §0 EXACTLY:
  UnrealInitializer 6a9087b5, LuaMod 5a6a06e6); CHANGES tail hunk REBASED.**
  4/5 CHANGES hunks (the two pin-row pairs) apply clean; the @863 tail hunk
  rejects because round5_row_sync + kb_tu_note + round4_setaside landed after
  FX's anchor. **02b** re-anchors it: identical fx_p2_row_sync entry appended
  as the new last key, comma moved to `"round4_setaside": true`. Pure
  re-anchoring; JSON validated; applies clean after 02.
- **03 l26_packaging_driver.py — hunks 1/3/4 CLEAN; hunks 2+5 REBASED (HH).**
  The post-HH sidecar changes interleave both regions (docstring item 6 sits
  between FX's item-5 anchor and the import; P12/P13 lines sit inside the
  expected-gates list). **03b** = HUNK A (item-5 docstring edit re-anchored on
  HH's `);`-terminated item 5 + FX's `import os` verbatim) and HUNK B (FX's
  conditional pair_gates byte-identical, composed WITH HH's unconditional
  P12/P13: gates=[…P5–P9]+pair_gates+[P12,P13]+FX's pair_gates_note). py_compile
  clean. P1 composition verified both directions (P1's packaging hunk is
  disjoint; applies on top of the full post-P2 driver).

## Plan-mode gate picture (all four runs rc=0; evidence JSONs staged)

- **Build default (B absent):** variants [A], 68 gates/413 steps (A+B baseline:
  74/823). G4-token-present ok=null planned-skip with FX's note;
  G4-ctoken-present true; G5 token_units_B=[]; G9/G10 planned-skip;
  `write-variant-pair-receipt` skipped → VARIANT-PAIR-RECEIPT.json NOT written;
  **G6 census 394/394 — census STAYS 394** (B shares A's units; 391+3
  NEW_UNITS per variant per pass), exactly as hypothesized. Q's copyin overlay
  composes cleanly on the PATCHED tree (mode patch-rebased-onto-revised-base,
  order gate true). Only ok=False gate: G2q-unrealinitializer.base — IDENTICAL
  to the live pre-P2 baseline plan and the last real execute (pre-existing
  expectation-not-invariant drift, not P2). G1 (19,044 pins) and G2r true.
- **Build L26_BUILD_B=1:** variants [A,B], 74/823 (exact baseline shape),
  pair receipt planned, **G4-token-present ok=FALSE** ("variant B active …
  token presence enforced at execute") — the loud-failure design works; the
  refusal raise fires at --execute.
- **Packaging default:** variants [A], 23 gates/4 steps, ZERO False gates;
  expected gates P5–P9+P12/P13 (no P10/P11), pair_gates_note records the P2
  skip. **Packaging L26_PACK_B=1:** [A,B], 37 gates/7 steps, P10/P11 restored.
- **Token census (FX §1.1 re-run, staged tree): 0 skip-token hits in any
  compiled source**; remains only in the drivers' TOKEN constant + CHANGES
  note text. Cleaner than FX NOTES §3 claimed: Q's frozen copyin-probe
  candidate ALSO has 0 (checked staged AND live).

## Honest notes

- Live tree is under ACTIVE parallel-agent drift: profile/townfall-build-facts.json
  drifted mid-session and was re-fixed by its owner; the COMMAND-CARD pre-flight
  re-hashes all five targets and re-checks the 02b anchor before apply.
- One /tmp-only staging incident, detected and corrected BY the gates
  themselves: a patch re-invocation silently reverse-applied the two tree
  files (G2 pins + token census caught it immediately); final staged state is
  hash-guarded. Zero live impact.
- BSD patch quirk documented in the card: `patch -R --fuzz=0` mis-handles
  02b's trailing no-newline context — plain `patch -R` (zero actual fuzz) or
  `git apply -R` reverses it exactly; full five-file pristine round-trip
  verified in /tmp.
- Staging mechanics (documented, not semantics): /tmp tree = APFS clones of
  l26-runtime/l25-base/l24-package + symlinked siblings; driver-input JSONs
  path-remapped to the staged root (l26-runtime + l24 prefixes only; the
  macOS /private/tmp resolved form). Plan receipts reflect the staged driver
  sha; all gate SEMANTICS unchanged.
- NO compile/build/package/console evidence exists for P2 — plan-mode only,
  per battery rules. Root's full rebuild + packaging verify (COMMAND-CARD §2)
  remains mandatory before any upload-grade use.

— agent IH (ZCode), 2026-10-10
