# REPORT.md — worker-console-safety (team 2026-10-04)

Deliverables in this directory (in write order): `CLAIM.json` (first),
`CONSOLE-SAFETY-PROCEDURE.md` (the standing controller reference), this
`REPORT.md` (last).

## What was done

1. Read the controller's console record for the day:
   `outputs/2026-10-04-zcode-controller/CONSOLE-LOG.md` (items 1–10, both
   sessions) and `CONTROLLER.md` (standing gates).
2. Read the checkpoint console-crash entries directly from
   `.superpowers/native-observations/transport-experiments-progress.json`:
   `zcode_console_unreachable` (18:44:27Z), `zcode_console_crash_2`
   (20:43:17Z), `zcode_igf5_diagnosis` (20:45:27Z + user correction
   20:49:22Z), `zcode_igf6_RESULTS` (21:13:50Z + `accurate_recording`
   amendment 21:17:12Z), plus the full `zcode_*` console-action timeline
   15:29Z–21:39Z to complete the catalog.
3. Read the reconciliation snapshot
   (`worker-checkpoint-reconcile/RECONCILIATION.md` items 8/9/13,
   `CURRENT-STATE.md` "Console safety state").
4. Read worker-risk-update's delivered findings (`REPORT.md`,
   `RISK-REGISTER-V2.md`; `REFILL-QUEUE.md` present) — the rows that bind
   console work: R-ARGV-01 (RESOLVED), R-PLD-01, R-FTP-01, R-FTP-02,
   R-EXEC-01, R-TMP-01, R-E2E-FR3, R-G1-02, R-G3-01.
5. Read the crash-specific worker evidence: `worker-argv-probe/REPORT.md`
   and `worker-igf5-diagnosis-rebuild/REPORT.md` + `STAGE-PLAN-IGF6.md`.
6. Produced `CONSOLE-SAFETY-PROCEDURE.md` with all six mandated sections:
   (a) pre-dispatch checklist, (b) dispatch discipline, (c) post-dispatch
   monitoring, (d) FTP read discipline (caching), (e) crash recovery,
   (f) known-prohibited actions — each rule citing the crash/failure/risk
   row that earned it — plus Appendix A (full console action catalog),
   Appendix B (crash-precedent table: preceding actions -> outcome ->
   attribution -> rule), Appendix C (source index).

## Key findings

- **Crash 1 attribution is settled evidence for the dispatch rules.** The
  checkpoint records dispatch #1 OK, dispatch #2 FIVE SECONDS later hitting
  the 30s timeout, then total console loss (ports 8084/2121 + ping),
  user-confirmed PS5 crash, same-evening recovery. This yields the two
  hard rules: one dispatch per authorization; >=30s minimum between any two
  dispatches; a dispatch timeout is a crash signal, never a retry trigger.
- **Crash 2 is NOT attributed and the procedure says so.** It occurred after
  igf5 staging + ploader ARMING, pre-launch; three suspects were recorded and
  none was confirmed (the user correction explicitly notes the igd1 control
  did not isolate the igf5 cause). The Townfall crash in the igf5 launch
  attempt (zero records + console + game lost) makes the inline syscall shim
  (SYS_mprotect=74, 2 sites) the leading hypothesis only. Result: inline
  syscalls in ploader modules are prohibited outright (build gates assert
  zero `0F 05` sites), while arming is treated as a real action with its own
  crash surface — console state is verified after staging/arming before any
  further step.
- **igf6 interruption** (after mapscan, before the first exec-span read,
  module-contained) has NO localized mechanism per the user amendment; the
  procedure encodes the user directive verbatim: no broad investigation, no
  repeated text-read attempts, and any future exec-page probe requires a new
  falsifiable design plus user coordination. The design lesson that DID hold:
  novel operations placed after write-through identity records made the crash
  point readable by absence.
- **FTP caching (R-FTP-02) is task-named, not demonstrated.** The procedure
  implements the brief's minimum (wait 15–30s, fresh connection, size-change
  check) plus the register's mitigations (sha against THIS attempt's receipt,
  never re-pin-to-match, unique per-attempt destination names, mtime/
  stale-JSONL-rename as primary freshness oracle, explicit caching probe on
  repeated mismatch) — labeled as precautionary oracle hygiene.
- **Non-crash failures are in the catalog too**: INITONCE c49108fe's /tmp
  report sink (FTP-refused, results lost — the /data-sink rule) and the v3
  zero-record staging (the zero-records-=-stop rule).

## Honest bounds

- Audit is document-level: no evidence was re-executed, re-hashed, or
  re-captured; all crash attributions are quoted from the checkpoint entries
  and owning worker reports with their own honesty caveats intact (igf5 cause
  "leading hypothesis, not confirmed"; igf6 mechanism "not localized").
- The catalog merges two sources (CONSOLE-LOG.md for controller actions,
  checkpoint entries for user-coordinated launches); a few igf4/igf5 staging
  timestamps exist only at minute resolution ("~20:4x") because the
  checkpoints record them inside narrative entries, not dedicated keys.
- No console contact, no network, no nested subagents; every file outside
  `worker-console-safety/` was read-only.

## Budget

15 tool calls used of 50 (CLAIM written at call 15; procedure at 16; this
REPORT at 17).
