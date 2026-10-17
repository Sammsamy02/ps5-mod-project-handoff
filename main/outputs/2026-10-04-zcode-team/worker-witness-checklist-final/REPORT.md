# REPORT — worker-witness-checklist-final

2026-10-05 · LOCAL ONLY, read-only outside
`outputs/2026-10-04-zcode-team/worker-witness-checklist-final/`. Deliverable
order honored: CLAIM.json, WITNESS-FINAL.md, this REPORT.md.

## 1. The task, in one line

Update the post-build witness checklist from the launch10-final-a era to the
FINAL launch-10 unit set (launch10-final-b-and-v4) so the pass-time witness
verifies the right things: the 7-string composed-object set, the fresh-unit
receipts, the verifier-complement checks (gate GO x2, determinism, stage-header
pins, undefined-delta shape, 5-record artifacts receipt), and the v4 fold as an
explicit precondition.

## 2. What was produced

- **WITNESS-FINAL.md** — the executable checklist: §1 per-artifact string
  table (1a-1f, narrow vs UTF-16LE encodings pinned per row), §2 fresh-unit
  verification from the run receipts with argv-drift recomputation, §3 the
  verifier-complement table (explicitly scoped against what
  `launch10_one_pass.py::verify_accessors` already proves), §4 the v4-pending
  preconditions, §5 the SPAWN-CAPABILITY verdict-language discipline, §6 the
  pass-time receipts index.
- **CLAIM.json** — pre-registered expectations with every sha/fragment pinned,
  plus honesty notes.

## 3. Key findings made while reading (they shaped the checklist)

1. **The pass has NOT completed.** `launch10-build/receipts/` holds only an
   OLD accessor-verification receipt (prior verifier revision, verdict
   FAIL-MISSING-PRX, error strings that no longer exist in the current
   verify_accessors) and `prx-candidate-l10-1/` is a failed-attempt remnant
   (build-receipt + GAPS.json only). Six run attempts are preserved
   (`run-l10-1-failed{2..6}-*`, aside-* dirs). Everything below is therefore
   PRE-REGISTERED, and the checklist tells the witness to evaluate only the
   final run-l10-{1,2} artifacts.
2. **"7 strings" reconciled.** The launch10-final-b units contain exactly 7
   seed-class guard refusal strings: 5 slot refusal messages (3x "refusing
   reads" slots 0/1/3, 2x "refusing bind" slots 2/4 — the prior witness's
   "5 strings", measured 3/2/5 fragments in the gate object) + the classifier
   Refused name ("unmapped/prot-none gap (refused)") + the terminal
   "[Seeds] REFUTED: " wrapper.
3. **v4 is NOT yet folded.** final-units SeedValidation.hpp (6e8f8c4a...) still
   carries v3 ("at or past CurrentBlock", publish_nonimage_readable_spans
   primary); the v4 staging hashes b84f8585 (header) / 4a2b9400 (initializer).
   EDITION.json still reads "launch10-final-b ... wrapper validation pending".
   Hence §4 lists the fold (engine predicate string, entry-0 None anchor,
   candidate-image-resident fixture PASS receipt, pin/EDITION refresh) as
   PRECONDITIONS the controller must confirm before the witness runs — the
   runner itself refuses drift if the pins are not refreshed, which makes
   "fold not done" a pre-composition failure, never a witness matter.
4. **"BOTH 53b1188f AND 67170b21" is a provenance pair, not two live pins.**
   53b1188f (anchor-cap-only staged header) was superseded IN PLACE by
   67170b21, which contains BOTH constants (kMaxAnchorMappings 8→16 verified
   at line 99, kMaxVmMappings 1024→16384 at line 91 of the on-disk staged
   header). One --stage-header pin can carry one sha, so the checklist
   witnesses: the 67170b21 pin + measured sha in overlay-manifest.json, BOTH
   constants in the overlay copy, and the 53b1188f lineage recorded in the
   receipt chain (EDITION.json edit #4 text, anchor-cap-analysis,
   LAUNCH-DAY-10.md rev-b bullet). Honesty note: the full 64-hex of
   53b1188f4fef... is no longer pinned anywhere on disk.
5. **Fresh-unit set and the enforcing gate.** The current compose_argv
   redirects 5 retained units: {luamod, cppmod, settings, asmhelper-asmhelper,
   program} (task's fresh set = the first 4). The fixed edition SEC32-3
   assertion refuses any argv drift beyond source/-o/-MF — measured precedent:
   `run-l10-1-failed4-argvdrift`. The witness RECOMPUTES the drift from the
   run receipt vs the base receipt rather than trusting the print.
   `run-l10-1-failed6-lineage/receipt.json` confirmed the receipt field shapes
   (freshly_compiled_units, retained_units_replaced, selected_runtime_units)
   and that its undefined-delta.json predates the expected_bootstrap_resolutions
   edit — only the FINAL runs' delta files count.
6. **Walkguard reachability discipline.** The guarded unit's five reason
   strings will all appear in the composed object as bytes; at fold-0 the only
   runtime-reachable refusal is guard_off_no_rx_verdict (three reasons sit
   behind the RC_ASM_WALK_RX_GUARD gate; page_range_invalid is unreachable
   from the sole call site). Object-identity anchors pinned: fold-0 gate
   object 81dd832b... (must equal the run receipt's asmhelper object_sha256),
   fold-1 6604ce2d... must NOT appear. W1 = the Output STR line → UTF-16LE;
   W2 + reasons → narrow.
7. **v4 receipt staleness.** worker-fname-validator-v4's PASS receipt records
   fixture-suite sha 98774321... while the on-disk suite hashes 674897f0...
   (as that worker's own REPORT §2 records). Flagged for fold review.

## 4. Scope discipline

No staged file, driver, receipt, candidate, or composition run was touched;
the only writes are the three deliverables in this directory. No console
contact, no network, no provider calls. No quality, in-game, verdict, or
completion claim is made; per SPAWN-CAPABILITY.md the candidate is
LOAD-CAPABLE, NOT SPAWN-CAPABLE — the checklist's §5 pins the verdict-language
consequences (first-OPTIONS silence expected; the loud walk refusal is the
designed fold-0 NO-CRASH posture; visible spawn is not a launch-10 verdict
input).
