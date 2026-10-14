# REPORT — worker-evidence-gap-fix-2 (2026-10-05, LOCAL ONLY)

Task: fix the nine REAL-DATA classifier gaps (G1-G9) from
`worker-verdict-first-real/LAUNCH10-RESULTS.md` §5 in
`outputs/2026-10-04-zcode-team/worker-launch10-evidence-upgrade/evidence.sh`,
rerun the three real runs (the regression corpus) plus the 94-check host suite,
SAME-or-BETTER-labeled verdicts, no re-baselining.

## Result: DONE — 8 of 9 gap groups fixed, all suites green

Script sha256: `76559126…` (pre, = prior worker's post — no upstream drift)
→ `7b03ca9503a2cd229d391e3f116d27e308b9de39124ee438711319554412d7ee`.
Machine diff + pre-patch snapshot in this directory.

## Verdict changes on the real runs (SAME-or-BETTER constraint met)

| run | before | after | why better-labeled |
|---|---|---|---|
| run1 (pid 317) | `FAILED_code3` | `FAILED_E2_START` | oracle E2 EXPECTED-REFUSAL named; rung honestly reserved (no status capture ⇒ E4/NO-CRASH unevidenced; NEXT prints "quartet=yes, fresh=unknown … rung claim reserved") |
| run2 (pid 341) | `FAILED_code-8` | `FAILED_B7_MFA` | oracle B7 EXPECTED-REFUSAL, FAIL-with-named-cause, nothing downstream ran |
| run3 (pid 355) | `FAILED_code3` | `MINIMAL_ACCEPT_E2_REFUSAL` | the classifier now reproduces the controller's own §2 DECLARATION: session quartet proven (extent 4/4, anchor 7, intervals 1, spans 6, cfg 2), no D6, no crash tail, fresh failed status matching verbatim — with the E4 'activated' O_TRUNC caveat printed |

Classification substance unchanged: same refusal lines, same last-failure-line
precedence, all pinned-detail arms and every other verdict branch byte-equal in
behavior (rehearsal five sets A-E reproduce their verdicts exactly).

## Per-gap status

- **G1** FIXED — `FAILED_E2_START` / `FAILED_B7_MFA` / `MINIMAL_ACCEPT_E2_REFUSAL`
  verdict names + `ORACLE-CLASS` line + `refusal_class` JSON field; refusal lines no
  longer staged as `S9_START_ACK` (mfa → `S2_ACTIVATION`, code=3 → `S9_START_REFUSED`).
- **G2** FIXED — per-pid/session split: SDIAGSUM = file totals (sessions, pid_first/
  pid_last, baseline_cut), PIDSUM per session (26 fields incl. mfa int32, extent_events,
  quartet inputs, spanpub, stat_ownmod), PIDCHAIN + PIDJSON per session; STIME deltas
  never cross sessions (boundary marker + reset tested); optional sdiag baseline cut
  (`--baseline-sdiag-bytes` / baseline-dir ref file), default OFF since launch-10
  pinned no sdiag baseline.
- **G3** FIXED — JSON tail now carries refusal_class/oracle_class, the sdiag object
  (current session's full value set incl. predicted pair, quartet flag) and the
  per-session `sessions[]` array; validated with python3 json.loads on all 8 outputs.
- **G7** FIXED as note — run1's vmentry=22761 > 16384 (walk_stop=0) now reported as a
  diag-note + `vm_capacity` JSON; no verdict impact (oracle C1/S3 reconciliation stays
  open with the controller).
- **G4** FIXED as note — S11_STOP-after-refusal annotated (fires run1/run3/set D).
- **G5** FIXED — `--sdiag FILE` alias, tested with run1's original documented name.
- **G6** FIXED (scoped) — failure lines grouped by r4 pid + cumulative-window caveat
  note; full per-session log windowing deliberately not attempted (not cheap; r4
  prefixes exist only on failure lines).
- **G8** FIXED as note — exepath_source=0 + stat_ownmod=1 C7 nuance recorded against
  the row (fires on both real boots).
- **G9** FIXED as note — spanpub env 64-bit low/high split + cap=1 saturation note.

Skipped (reported): E1 unpinned-code=2 verdict naming (not an observed class; pinned
code=2 details already map); nothing else from the results doc §5 left unfixed.

## Regression evidence

- Real-run corpus rerun: run1/run2/run3 rc=0, empty stderr, zero WARN; fixture shas
  after the reruns still equal LAUNCH10-RESULTS §6 provenance (4a4e0cf7…, d5ad9fa0…,
  e40bf151…, 9aeeceeb…, 264ba150…, f5ba1226…) — fixtures untouched.
- Host suite: `sh host-test/run_fixture_tests.sh` → rc=0, **94 passed / 0 failed**
  (all prior assertions green; no assertions removed or weakened; none added — the
  existing 94 already pin the formats G2/G3 had to preserve, e.g. SDIAGSUM substrings
  and STIME "(first t_ms)").
- Rehearsal five sets: A/E `MODS_STARTED`, B/C `FAILED_STATUS`, D
  `FAILED_S6_CONVERSION` — identical pre/post; zero WARN.
- No-re-baselining: filtered diff for `BASE_*` / `PIN_*_SHA` / `BASE_SESS_*` = 0 lines;
  new `BASE_SDIAG` defaults 0 (cut OFF).
- JSON tail: valid JSON on run1/run2/run3 + rehearsal A-E.

## Honest bounds

All verification is host-side against the preserved real-console fetches; no console
contact, no FTP. `MINIMAL_ACCEPT_E2_REFUSAL` is a classifier LABEL reproducing the
controller's oracle-based declaration — it does not upgrade evidence: the E4
`activated` line is still not literally on file for run3, and the label prints that
caveat plus the belt-and-braces (fetch runtime-status.txt immediately on a failed run).
The G7 capacity question (run1: 22761 walked vs pinned 16384 with clean stop) remains
OPEN for reconciliation — the classifier only reports it honestly now. New display
token `S9_START_REFUSED` is introduced for START-stage refusal lines; STAGE-TABLE.md
was not edited (outside my owned files) — documented here instead.
