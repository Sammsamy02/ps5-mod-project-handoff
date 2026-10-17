# MATRIX-ROW-NOTE — what the two-patch dry composition unblocks mechanically

Worker: worker-launch11-dry-compose · 2026-10-04. For the LAUNCH11-MATRIX owner
(worker-launch11-integration-matrix) and the controller. Companion to
`TWO-PATCH-COMPOSITION-RECEIPT.md`. "Unblock mechanically" means: the composition
STEP is now proven end-to-end with receipts and a GO gate; the verdict gates
themselves are NOT moved — every consuming row keeps its own evidence gates.

## Unblocked (mechanically proven by this build)

1. **The guard-unit wiring for EVERY guard row (L11-1, L11-2, L11-3, and the
   fold-ABSENT rows L11-5/L11-8).** The previously unproven step of matrix §3.2/§4f —
   explicit asmhelper unit rebuild + receipt attestation actually reaching the link —
   is now a delivered, diff-verified mechanism: the new fixed-script edition
   (`8b70520e…` = frozen `8dd92a6f…` + the single `asmhelper-asmhelper`
   RETAINED_REPLACEABLE admission) + `--replace-runtime-unit asmhelper-asmhelper=<staged
   guarded cpp>` + the replace-receipt `--file` admission of source AND object. The
   WST-1 silent-stale-link hazard now has a measured, gate-visible answer.
2. **The ledger-4a pair co-staged in one candidate** — the most demanding compatible
   combination (backend seam × guard) composes, passes 17/17 printed static checks
   (20/20 in the evidence file), keeps imports 196 == 196 and the composed UND set
   byte-identical to launch-10's, with a determinism twin and a GO gate receipt whose
   manifest declares BOTH units. The matrix's §2 rows can now cite a TWO-patch
   composition precedent, not just single-unit ones.
3. **L11-1 (CONSOLIDATE) reduces to ONE define flip.** Its composition is exactly this
   candidate with the fold define `-DRC_ASM_WALK_RX_GUARD=1` (guard ACTIVE) — same
   edition, same receipts pattern, same manifest structure; the fold-1 object
   (`6604ce2d…`, WST-measured) swaps in through the identical retained-replacement
   path. Note the `__FILE_NAME__` basename pin (receipt §4): keep the staged basename
   `ASMHelper.guarded.cpp` to reproduce WST's measured fold objects byte-for-byte.
4. **L11-2/L11-3's base is proven**: guard + PREPARED-UNARMED backend vehicle compose;
   the PLH Prospero layer absorption (§3.4) is the remaining untested addition for
   those rows (PLH REPORT was still pending at matrix time — re-verify box 8 first).
5. **L11-5/L11-8's guard unit is already in their shipping form.** Fold-0 IS the
   fold-ABSENT form those rows mandate (byte-identical `81dd832b…`, zero syscall
   text); their remaining steps stay §3.5 (gate + DIFF-2 REBASE + arm + cc1 adoption).

## NOT unblocked (unchanged gates — do not cite this build for these)

- **Any fold-1 or armed form**: the guard ACTIVE fold needs the D2 verdict (WST-5:
  admissible only when seq5 `rx_result.mprotect_rc == 0`); the backend ARM needs the
  ledger-g fixed-script decision + ODR walk (a DIFFERENT, larger edition than mine —
  arming changes the PLHDetour member type across two TUs; my edition only widens a
  replacement allow-list) + R2 capture + the D-1 design. L11-4 remains blocked exactly
  as the matrix says.
- **R3 DIFF-2 rebase** (§4-d), **cc1 ini adoption** (§4-i), **PLH verdict
  dependencies** (CLAIM-5) — untouched by this build.
- **Staging/launch**: the controller owns every verdict-gated act; this candidate is a
  dry-compose proposal (`prx-candidate-composition6-drycompose-a`, wrapped fself
  `d94bb99a…`), never a staging instruction.

## Suggested matrix deltas (for the matrix owner to decide)

- §3.2's step 2/3 can now name a concrete mechanism: "the retained-replacement path of
  a fixed-script edition admitting `asmhelper-asmhelper`" with this worker's edition +
  receipt as the pattern (new revision = new ids per row instance).
- §4-g's edition pattern has a second, smaller instance: allow-list widening (this
  build) vs member-type arming (still open for L11-4).
- Box 8 successor-artifact hashes gain: WST fold-0 object independently reproduced in
  composition (`81dd832b…`, twice), composed two-patch runtime `49fd49e7…`.
