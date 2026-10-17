# REPORT.md — worker-risk-update (team 2026-10-04)

Task: refresh the worker-risk-e2e risk register with today's session
outcomes and produce the refill queue for freed worker slots. Delivered in
this directory: `CLAIM.json` (first), `RISK-REGISTER-V2.md`,
`REFILL-QUEUE.md`, this `REPORT.md` (last). 14 tool calls total of 40;
read-only outside `worker-risk-update/`; no console, no network, no nested
subagents.

## 1. Sources used

- `worker-risk-e2e/RISK-REGISTER.md` (the 26-row register: 9 mandated + 17
  supplementary — note: the task brief said 24 rows; the file has 26; all
  26 are updated).
- `worker-checkpoint-reconcile/RECONCILIATION.md` + `CURRENT-STATE.md`
  (~21:30Z snapshot).
- Owning workers' reports: `worker-mutex-sdk-retry` (SDK composition SUCCESS,
  81/81 commands, console candidate), `worker-fileio-apply` (7 diffs applied,
  host PASS first run, SDK composition with `--fileio` PASS),
  `worker-composition-4` (FAILED at delivered-script gate; full root cause;
  run-1 salvage), `worker-composition-4-fix/CLAIM.json` (bootstrap-units
  diagnosis, in flight), `worker-d1-chain-relink` (51-tuple merged chain
  built + regressions + STAGING-MANIFEST-REPIN), `worker-igf5-diagnosis-
  rebuild` (igf6 rebuild, two-run determinism, safety rationale now in
  doubt), `worker-c2-alt-correspondence/CLAIM.json` (in flight; exec-span
  read crash fact), `worker-console-staging-bundle`, `worker-loadlist-final`,
  `worker-dll-mod-acquire`, `worker-fname-runtime-probe` (all delivered),
  `worker-fname-fc1-prep` (/tmp recurrence evidence), `worker-integration-
  checklist/INTEGRATION-CHECKLIST.md`.

## 2. Register updates (RISK-REGISTER-V2.md)

Statuses: 5 RESOLVED(-HOST-SIDE), 1 SUPERSEDED, 12 ADVANCED, 7
OPEN-UNCHANGED, 1 MOSTLY-RESOLVED split handled inline (R-IDENT-01,
R-LAYOUT-01 as MOSTLY RESOLVED).

- RESOLVED: R-ARGV-01 (crash attributed to argv double-dispatch; console
  recovered), R-V3-01 (SUPERSEDED by the v3 MAPPING_BREAKTHROUGH + igf
  ladder), R-D1-01 + R-R2C-01 (canonical 51-tuple merged chain built,
  regressed, re-pinned — host-side), R-SER-01 (joint fileio+mutex host
  green + SDK PASS), R-PRX-01 (PRX route proven in-game via igf lineage).
- ADVANCED (notable): R-DRV-01 (green paths prove the happy case; loop
  hardening still unapplied), R-ACK-01/R-GT-01 (units compiled + attested,
  blocked one level up at composition), R-IDENT-01/R-LAYOUT-01 (axes
  resolved, GUA live-validated, staged pair stageable; rung-3 + Tick/SCO
  residuals named), R-CONTC-01 (pak 4699-vs-4544 settled), R-ACT-01
  (worker-8 redo 61/61 honest capabilities), R-G1-02/R-G3-01 (mitigations
  designed, executions pending), R-LOG-01 (real logs now exist to validate
  against).
- OPEN-UNCHANGED: R-RW-01, R-E2E-FR3 (practice adopted), R-PAD-01,
  R-PLD-01 (re-confirmed by igf5's zero-record lesson), R-LIBC-01,
  R-BOUND-01, R-CRT-01, R-MUT-01.
- NEW risks (section C): **R-EXEC-01** exec-only page enforcement — igf6
  died at its first exec-span read, mechanism NOT localized per user
  amendment, C2 blocked, data-segment alternative in flight, no broad
  investigation/repeated text-reads per user directive; **R-FTP-02** FTP
  caching — task-named; nondeterministic staleness would invalidate the
  RETR-readback staging oracle both ways (distinct from R-FTP-01's
  deterministic size law); mitigations: per-attempt receipts, never
  re-pin-to-match, unique per-attempt destination names; **R-COMP4-01**
  composition-4 bootstrap-replacement architecture defect — pin-walk
  ordering (rr built after the fresh-unit loop) plus the wider
  `--replace-bootstrap-unit` gap; blocks the whole compose→arm→query
  ladder; fix owned by in-flight composition-4-fix; **R-TMP-01** /tmp as
  report/probe sink recurring (fname-fc1-prep today; INITONCE lesson
  restated; standing rule: evidence sinks are owned outputs/ dirs on host,
  /data on console).

## 3. Refill queue (REFILL-QUEUE.md)

15 entries, prioritized, each with task/priority/prerequisite: Q1
composition-4-fix terminal receipt review (P0), Q2 E2E transcript-pairing
checker (P0), Q3 C2-alt receipt verification (P1), Q4 real-log parser
validation (P1), Q5 FC-1 interpretation matrix (P1), Q6 fileio console
target runbook (P1), Q7 staging-docs cross-lint (P2), Q8 CreateMutexA shim
proposal (P2), Q9 Oodle presence check (P2), Q10 CRT census gap list (P2),
Q11 app UI audit re-verify vs authoritative worktree (P2), Q12 pad
hardware-test pre-flight (P2), Q13 streaming bridge proposal (P3), Q14
ARGV_ECHO closure addendum (P3), Q15 joint mutex+fileio console session
plan (P3). Explicit do-not-duplicate list for the three in-flight workers;
controller-owned console steps listed so worker slots are not spent on them.

## 4. Honest limits

- No evidence was re-executed or re-hashed this session; all "verified"
  statements cite the owning worker's own receipts (the original register's
  re-verification notes are carried forward as historical).
- R-FTP-02 is recorded as task-named by the controller brief; no caching
  mechanism is demonstrated in the workspace — the row exists to keep the
  oracle from silently depending on one.
- Likelihood/impact are qualitative judgments, not measurements. Nothing
  here claims console acceptance, rung advancement, or milestone passage;
  console-side application of the D1 relink and all gate G0–G6 executions
  remain controller/user-coordinated steps.
