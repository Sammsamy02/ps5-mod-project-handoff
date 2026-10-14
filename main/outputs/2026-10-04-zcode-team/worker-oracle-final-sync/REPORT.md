# REPORT — worker-oracle-final-sync

2026-10-04 (inspection of artifacts dated 2026-10-05 local). Deliverable order
per packet: `CLAIM.json` first, `ORACLE-SUPPLEMENT.md` (the deliverable the
controller merges), this `REPORT.md` last. LOCAL ONLY: this worker wrote
nothing outside `outputs/2026-10-04-zcode-team/worker-oracle-final-sync/`;
ORACLE.md is untouched (the supplement is a PATCH PROPOSAL).

## 1. Task in one line

The launch-10 acceptance oracle (83 predictions) was written against the
pre-fold unit set; the FINAL build (EDITION `launch10-final-a` + walkguard
edit 5) gained five deltas. Tabulate exactly those deltas, every line pinned
to the FINAL source files.

## 2. Method and evidence chain

1. Read the oracle (`worker-launch10-acceptance-criteria/ORACLE.md`) end to
   end for the rows each delta touches (C1-C13, D1-D7, F14/F15/F29/F30,
   verdict clauses, NO-CRASH invariant).
2. Verified the walkguard fold ACTUALLY shipped, not just staged: the l10
   driver hardcodes the retained replacement
   `asmhelper-asmhelper=…/ASMHelper.guarded.cpp`
   (`build_runtime_composition6_l10.py:351`); the composed run receipt
   (`…/run-l10-1[-aside-*]/receipt.json`) records source `694af016…` → object
   `81dd832b…` — byte-identical to WST's fold-0 compile-gate object (0
   `syscall` instructions). So "fold-0 = guard ACTIVE, RX-walk fold inactive"
   means: the guarded unit wraps every walk read and refuses loudly; the
   mprotect transition fold is not in the binary. Verified across five run
   receipts (failed2/failed6 show the pre-guard object `68d22a1c…`, the four
   bef38f16 runs all show `81dd832b…`).
3. Pinned the pump-arm route: `OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL`
   (UnrealVirtualBaseVC.hpp:118-134; vtable data read at :132, guarded text
   read at :133 via `resolve_function_address_from_potential_jmp`) and every
   walk call site in `final-units/UnrealInitializer.cpp` (grep: :1819, :1828,
   :1851, :1936, :1945, :1954, :1963, :1972, :1981, :2049, :2087) — first live
   walk = LoadMap at :1819, matching the task's pump-arm pins (~1819/~1892).
4. Enumerated every `diagnostic_event` in the consolidated bridge (61 sites)
   and the exact emission order in `modbridge_runtime_start_with_activation`
   (register → spanpub publish → scan → coverage gate → capture return → gap
   refusal → conversion → cfg → lmf block), with constants
   `kCandidateBase=0x400000` (:220), `kImageExtentVaddr=0xbc602c8` (:228),
   `MRS_ERR_CONVERSION=1` (runtime_start_bridge.h:37).
5. Capacity: `staging/live_module_facts.hpp` (`67170b21…`) :91 `kMaxVmMappings
   =16384` (+ :99 `kMaxAnchorMappings=16`), bridge `kBridgeVmmapEntries=16384`
   (`CB:240`), pool shape strings in `final-units/SeedValidation.hpp`
   (:254-287) and their consumer in the initializer (:864-939).
6. `linker_provided`: read from the surviving spanpub-merge receipt state
   (`run-l10-1-aside-1791209468/undefined-delta.json`).

## 3. Drift / incident findings (READ-ONLY; owner reconciliation pending — I fixed nothing)

- **Bridge file drift after the final build.** EDITION.json pins the bridge at
  `fc06fe52…`; the byte-exact surviving copy is
  `worker-comp6-driver/staging/lineage-overlay-2-aside-1791208925/lifecycle/runtime_start_bridge.cpp`.
  The live `launch10-build/{final-units,bridge-merge}/`
  `runtime_start_bridge_r14_diag_spanpub.cpp` copies were edited at 09:07
  (sha `f823fcb5…`): the repair removes fc06fe52's stray file-scope publish
  hunk (fc06fe52:697-702) and fixes the publish call arity — but BOTH versions
  still miss the closing `}` of `bridge_readable_span_at` (after its
  `return 1;`, before the `bridge_capture_envelope` comment block). The
  supplement therefore pins all content claims to `fc06fe52` (the as-built
  bytes) and flags the live-copy drift for the controller.
- **The as-built bridge TU failed its packaging compile.**
  `prx-candidate-l10-1-aside-1791209686/GAPS.json` records 6 errors against
  `lineage-overlay-2/lifecycle/runtime_start_bridge.cpp` (= fc06fe52): the
  file-scope publish call (:701: a type specifier / undeclared `mappings` /
  undeclared `extent_begin`), the extraneous `}` (:702), and
  `'SeedValidation.hpp' file not found` (:712 — the packaging include path
  lacks final-units/). Later packaging attempts fail EARLIER, at the lineage
  check `undefined_delta_added_matches_checkpoint_claim`
  (`prx-candidate-l10-1-failed6` and `-aside-1791209839` GAPS.json), so the
  09:07 repair has not been compile-exercised and the missing `}` is very
  likely the next error. Consequence carried into the supplement: the five
  `bridge_spanpub_*` tags are prediction-pinned "if any emit", and
  `linker_provided` vs `added` bookkeeping differs between the completed run
  state and the newest re-run (composed object byte-identical `bef38f16…`
  throughout — the difference is delta bookkeeping only, which is exactly the
  S5 evidence-reader line).
- **Live concurrent movement.** Run dirs were renamed aside
  (`-aside-1791209468/-1791209686/-1791209839`, new SPAWN-CAPABILITY.md) while
  this worker read them. All reads were read-only; where a path vanished
  mid-task the aside copy with identical content was used. No dispatch, no
  replay, no writes outside my output dir.

## 4. What the supplement changes in the oracle (merge map)

- NEW rows W1-W7 (walk-guard refusal strings) + a successor note on F29/F30
  and the NO-CRASH invariant (the pump-arm refusal replaces the igf6 crash
  shape; crash there = REGRESSION).
- C8/C9 unconditional; C11 tags live-on-failure / absent-on-success
  unconditional; C12 resolved to the L3 gate shape (gap events during capture,
  code=1, NO lmf_* on that path); C13 unchanged.
- NEW spanpub marker row (bridge_spanpub_count/total/cap/env_low/env_high) in
  the stage table after lease registration; predicted values anchor_count=7,
  spans=6, reg=6, intervals_found=1 restated as unconditional.
- F14 success text superseded; F15 refusal format superseded (still
  value-carrying, still GOOD-capped); NEW span-table-unavailable refusal
  class; NEW SV-F shape strings incl. the snapshot-truncated vs not-covered
  split.
- Capacity section: complete capture EXPECTED (stop=0, truncated=false);
  vm_total>16384 = the new truncation refusal class; kMaxAnchorMappings=16
  confirms D3's folded cap.
- Layout freshen: explicit zero-impact statement.
- One-line linker_provided note for the evidence reader.

## 5. Honesty bounds

Everything is source-derived and receipt-cross-checked; nothing is an outcome
promise and only the real console launch advances a verdict. The fold-0
zero-syscall property is cited from worker-launch10-walkguard-prep's measured
probes, not re-measured here. The spanpub marker emissions and the bridge TU's
compile state are explicitly bounded (see section 3). No quality score of any
kind is claimed; no claim in this packet exceeds the evidence it pins.
