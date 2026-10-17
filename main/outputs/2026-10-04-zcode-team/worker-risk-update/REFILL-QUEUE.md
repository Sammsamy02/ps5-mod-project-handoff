# REFILL-QUEUE.md — worker-risk-update (team 2026-10-04, evening)

Prioritized remaining useful INDEPENDENT work for freed worker slots, drawn
from `worker-integration-checklist/INTEGRATION-CHECKLIST.md`, the register's
unresolved items (see `RISK-REGISTER-V2.md`), and today's delivered-worker
residuals. Each entry is worker-sized, proposal/verification scoped, and
independently actionable unless a prerequisite is named.

**Do-not-duplicate — in flight at queue time (CLAIM only, no REPORT yet):**
`worker-c2-alt-correspondence`, `worker-composition-4-fix`,
`worker-rung3-chunk-probe`. Delivered today and closed (no re-run):
`worker-mutex-sdk-retry`, `worker-fileio-apply`, `worker-console-staging-bundle`,
`worker-loadlist-final`, `worker-dll-mod-acquire`, `worker-d1-chain-relink`,
`worker-fname-runtime-probe` (pending controller record/verification only),
`worker-composition-4` (failed-documented; superseded by composition-4-fix).

Controller-owned console steps (NOT worker tasks; listed so slots are not
wasted on them): STOR the relinked 51-tuple payload per
`worker-d1-chain-relink/STAGING-MANIFEST-REPIN.md` +
`worker-console-staging-bundle/MASTER-STAGING-PACKAGE.md` (Gate A0 first);
next console session per `worker-post-promotion-runtime/RUNTIME-STAGING-PLAN`
and `worker-e2e-console-dry` GO gates; game launches are always user actions.

---

## P0 — critical-path unblockers

**Q1. Independent terminal review of composition-4-fix receipts.**
- Task: when `worker-composition-4-fix/REPORT.md` lands, verify: new script
  copy hash-attested (never the frozen `7a83cb38…` original), run-2 fresh
  dir, run-receipt status `NATIVE_SELECTED_TICK_FILE_WITNESS_RECOMPSED`-
  equivalent (368 units expected), final `.o` hash/bytes recorded, and the
  undefined-delta equals EXACTLY `[sceKernelGetModuleInfo, clock_gettime,
  getpid]` (nm-verified). Confirm run-1 failure artifact untouched and
  spike-tree writes still limited to the five hash-verified include/
  headers.
- Priority: P0 (gates the whole compose→stage→arm→query ladder;
  R-COMP4-01).
- Prerequisite: composition-4-fix REPORT delivery. One reviewer only;
  do not run the fix worker's commands again.

**Q2. E2E transcript-pairing checker (host-only).**
- Task: author a checker that validates a candidate `e2e-status.jsonl` /
  staging manifest BEFORE console dispatch: every `begin` has an `end`
  (F-R2 semantics), payload identity = size + import count + import-name
  set with sha as secondary (F-R3 rule), and manifest hashes byte-match
  local files. Run it against `STAGING-MANIFEST-REPIN.md` and
  `MASTER-STAGING-PACKAGE.md` manifests as its first real input; report
  any mismatch as findings, never as edits.
- Priority: P0 (cheapest way to protect the upcoming console E2E).
- Prerequisite: none.

## P1 — near-term ladder support

**Q3. C2-alternative receipt verification slot.**
- Task: when `worker-c2-alt-correspondence` delivers, independently check
  `candidate-ranges.json`: DATA segments only; exclusions hold
  (relocation-dependent bytes, `.data`/`.bss`, probe self-copies); every
  range's live address maps into a span the igf6 mapscan marked
  readable/trusted; readable-mapping verification is specified as
  same-invocation. Confirm Tick/SCO is still marked UNRESOLVED.
- Priority: P1 (unblocks promotion decision C2; R-EXEC-01).
- Prerequisite: c2-alt-correspondence REPORT. Do not start early; do not
  design a competing check.

**Q4. Validate the in-game log parser against the real console logs (R-LOG-01).**
- Task: run `python3 -m unittest discover -s tests` for
  `worker-ingame-log-parser` against the real igf4/igf5/igf6 JSONL records;
  verify the comma-tolerant rule and extraction heuristics on real bytes;
  deliver a verdict (adopt / fix-list) with per-rule evidence.
- Priority: P1 (evidence tooling trust for the next console session).
- Prerequisite: real logs available in the workspace (they are — igf
  lineage records); if a specific log is only in the controller console
  dir, request fetch via controller, no direct console contact.

**Q5. FC-1 (FName ctor) interpretation matrix for the next probe iteration.**
- Task: write the bounded interpretation matrix for the FC-1 runtime probe
  (mprotect-first technique, candidate runtime `0x63d590`): expected
  record sequence, per-outcome verdicts, rollback; fold-in spec for the
  next igf-lineage build (igf7-prep), coordinating with
  `worker-rung3-chunk-probe`'s add-on module so ONE launch serves both.
- Priority: P1 (FName P2 single lead; two probes, one launch).
- Prerequisite: controller verifies `worker-fname-runtime-probe` artifact
  hashes first (delivery is pending verification per CURRENT-STATE).

**Q6. FileIO console target runbook + interpretation matrix.**
- Task: mirror `worker-mutex-sdk-target/CONSOLE-TARGET-RUNBOOK.md` for the
  fileio family: staged-session design exercising the 16 census names +
  4 fail-fast stubs on console, per-name expected outcomes, bounded
  rollback; note fsync/O_DIRECTORY semantics probes (R-LIBC-01 overlap).
  Proposal-only; no console execution.
- Priority: P1 (mutex SDK candidate exists; fileio rides the same session).
- Prerequisite: none (inputs: `worker-fileio-apply` delivery, mutex
  runbook).

## P2 — queued, independent

**Q7. Staging-docs cross-lint.**
- Task: reconcile the three execution documents for the next console
  session — `worker-e2e-console-dry/CONSOLE-E2E-EXECUTION.md`,
  `worker-console-staging-bundle/MASTER-STAGING-PACKAGE.md`,
  `worker-d1-chain-relink/STAGING-MANIFEST-REPIN.md` — into one
  contradiction list (payload identity, loadlist ordering, freshness
  reset, rollback). Output: findings only; owners decide edits.
- Priority: P2 (prevents conflicting instructions at a user-coordinated
  session).
- Prerequisite: none.

**Q8. CreateMutexA A→W shim proposal + CP_ACP decision memo (R-MUT-01).**
- Task: draft the shim design (census slot, CP_ACP mapping options with
  evidence, test plan against the mutex family suite) as a proposal for
  the controller's policy decision. No code application.
- Priority: P2.
- Prerequisite: none.

**Q9. Oodle presence check in the two complete container pairs (R-CONTC-01).**
- Task: offline, bounded inspection of the two complete utoc/pak pairs for
  Oodle-compressed entries; classification-spec-conformant report; no byte
  semantics guesses.
- Priority: P2.
- Prerequisite: none.

**Q10. CRT census gap enumeration (R-CRT-01).**
- Task: from existing artifacts, write the exact missing-inputs list for
  `worker-crt-startup-design` (baseline-dll-census.json, entry-adapter
  interface, owner composition sources marked not-integrated) so the
  controller can deliver them in one pass.
- Priority: P2.
- Prerequisite: none.

**Q11. App records/UI audit re-run against the authoritative worktree.**
- Task: `zcode_worker_e2e_records_ui` audited an export copy; produce a
  diff/verification spec (or execute the read-only diff) of `app.js` /
  `index.html` against the authoritative worktree files and state whether
  the audit findings transfer.
- Priority: P2.
- Prerequisite: none (read-only on the app files).

**Q12. Pad hardware-test pre-flight checklist (R-PAD-01).**
- Task: from `worker-real-input-pad`'s staged package, write the bounded
  VISIBILITY-TEST pre-flight checklist (staged files + hashes, expected
  record sequence, per-failure meaning, rollback to no-pad state) so the
  first hardware read is one console-session step.
- Priority: P2.
- Prerequisite: none.

## P3 — explicitly deferred

**Q13. Streaming zip→mfb variant design proposal (R-BOUND-01).**
- Priority: P3 — commission only if the corpus demands it post-E2E;
  proposal-only, no bounds change applied.
- Prerequisite: E2E milestone passed.

**Q14. ARGV_ECHO PROBE-PLAN addendum (R-ARGV-01 closure note).**
- Task: record the crash attribution (double-dispatch) and
  "attribution-only, no re-dispatch without reconciliation" status in the
  probe plan so no future slot re-stages it blindly.
- Priority: P3 (small doc fix).
- Prerequisite: none.

**Q15. Mutex+fileio joint console session plan (R-SER-01 successor).**
- Task: single staged-session design covering both SDK candidates (joint
  report path, per-family interpretation matrices, shared rollback) to
  avoid two separate console sessions.
- Priority: P3 until a staging window is agreed; then promote to P1.
- Prerequisite: controller staging-window decision.

---

Queue discipline: refill completed slots top-down; skip any entry whose
prerequisite is unmet; never duplicate an in-flight worker; all outputs
proposal/verification-only under each new worker's own results directory;
CLAIM.json first, REPORT.md last; no console, no network.
