# CONSOLE-SAFETY-PROCEDURE.md — standing reference for all console interactions

Owner: controller (single session). Workers NEVER touch the console.
Effective: 2026-10-04, compiled by worker-console-safety from the day's full
console record. Every rule below cites the crash, failure, or risk row that
earned it. Nothing here is advisory unless marked so.

Console surfaces covered: payload manager `http://ps5-host-not-configured.invalid:8084`
(`/list_payloads`, `/manage:check`, `/manage:upload`, `/loadpayload:`),
anonymous FTP port 2121 (bounded RETR readback / result collection), ploader
arming + user-coordinated Townfall launches.

---

## 0. Standing gates (always in force, every session)

1. `game_file_mutations_permitted_now=false` (installation HOLD): no game-file
   writes, no autoload changes, no Townfall launch unless the user explicitly
   authorizes that specific launch for that specific session.
2. `console.new_dispatch_allowed_without_review=false`: every dispatch needs an
   explicit authorization (handoff NEXT_ACTIONS item or user directive) naming
   that exact reviewed candidate. Record the reconciliation when a gate is
   superseded; it stays in force for everything new.
3. One console actor: the controller. No worker performs uploads, dispatches,
   FTP reads, or ploader arming.
4. Evidence sinks are `/data/...` on console and owned `outputs/...` on host,
   NEVER `/tmp` (INITONCE c49108fe: FTP refuses /tmp reads — results lost;
   R-TMP-01).
5. Current prohibitions carried from today (section f): no double-dispatch,
   no inline syscalls in ploader modules, no exec-span reads from a ploader
   module, no ARGV_ECHO.elf re-dispatch, no ploader re-dispatch while console
   state is unreconciled after a crash.

---

## a. Pre-dispatch checklist (ALL boxes before any `/loadpayload` call)

1. **Console reachable.** `GET /list_payloads` returns 200 with a readable
   catalog. If not: STOP — go to (e). Never "try again in a minute" after a
   refused connection; a refused port after a dispatch is a crash signature.
2. **Dispatch authorized.** The exact artifact (build-id, sha256, size) is
   named in a handoff item or user directive; the candidate has passed its
   review gates (receipt, static checks, determinism where required).
3. **Artifact verified locally.** Local sha256 == build receipt. Any
   replacement of an owned console file preserves the old build locally with
   its receipt before overwrite; refuse divergent content (hash before
   replace).
4. **Upload verified per R-E2E-FR3/R-FTP-01.** After STOR/upload: wait 15–30s,
   fresh FTP connection, bounded RETR readback; first bytes `7f 45 4c 46` for
   ELF; size + sha256 equal to THIS attempt's receipt (details in d).
5. **Payload risk scan.** Zero raw `syscall` instructions (`0F 05`) in the
   final `.text`; imports limited to the receipted set; no unreviewed novel
   mechanism (new syscall, new driver, new page-permission operation).
6. **Novel operations LAST.** The record order puts any first-time operation
   (first exec-span read, first new import class, first direct hardware
   interaction) AFTER the write-through identity records (entry/startup/
   context), so a fault localizes by absence. This is what made igf6's crash
   point readable and igf5's recoverable-as-diagnosis.
7. **Result path sane.** `--report-path`/sink is `/data/...`; expected result
   filename recorded in the plan; any stale result JSONL RENAMED (kept, not
   deleted) before the run so this run's records are unambiguous
   (STAGE-PLAN-IGF6 step 3).
8. **Console state clean.** Manager responsive; previous run's records
   reconciled and copied; no unresolved crash, timeout, or anomaly since the
   last verified-good contact.

Dispatch is permitted only when 1–8 all hold. Otherwise stage-and-stop is the
correct outcome ("If the dispatch route misbehaves, stop and leave staged").

## b. Dispatch discipline

1. **ONE dispatch per authorization.** Never double-dispatch. Crash 1
   (18:44Z): ARGV_ECHO dispatch #1 OK, dispatch #2 five seconds later hit the
   30s timeout, then total console loss (ports + ping, user-confirmed PS5
   crash). The second dispatch of an unpaused payload is the only action ever
   observed to take the console down by itself.
2. **Minimum 30s between any two dispatches**, including deliberate
   determinism pairs — checkpoint action from crash 1: ">=30s gap, not 5s".
3. **Determinism is proven at build time, not by rapid re-dispatch.** Two
   byte-identical builds (igf6 pattern) replace the second on-console run.
   A second dispatch is a NEW decision requiring fresh authorization, a
   reconciled first result, and the 30s floor.
4. **Never dispatch when:** a previous dispatch's result is uncollected or
   unreconciled; the console crashed or "recovered" without a read-only
   verification pass since; the payload carries an unreviewed mechanism; or
   the only reason is "the first attempt probably didn't take".
5. **A dispatch timeout is a failure signal, not a slow start.** Dispatch #2's
   30s timeout in crash 1 was the first symptom of the console dying. On
   timeout: NO retry — go directly to (e).
6. **No re-dispatch of ARGV_ECHO.elf** until reconciled (R-ARGV-01 residual /
   R-G1-02); a single deliberate re-probe is designable but is not owed, and
   G1 is served by the config-file-driven entry design regardless.

## c. Post-dispatch monitoring

1. **Write the expected-evidence table BEFORE dispatch**: expected record
   sequence (seq1..N), markers, build-id, sink path, expected sizes. The igf6
   interpretation matrix is the model — decide what each outcome means before
   the run, including "records stop at seq N".
2. **First evidence check is freshness, not content**: result-file mtime
   changed / renamed stale file reappeared. Only then read content.
3. **Timeout before assuming failure:** allow >=30s (the observed dispatch
   bound) plus staging latency before the first FTP read. Do NOT poll RETR in
   a tight loop — every poll is console contact; wait on mtime.
4. **Records are the only truth (R-PLD-01).** ploader OK does NOT mean the
   module started — ploader cannot see module_start's return. Never claim
   "ran/loaded OK" from the dispatch HTTP 200 alone; judge only from the
   record stream.
5. **Zero records = stop.** The write-through sink writes seq 1–3 before
   anything novel runs; zero records plus any instability means module_start
   likely never ran or a ring-0-level event occurred (igf5: zero records +
   console + game loss). STOP, no re-launch, reconcile first.
6. **Partial records = localize by absence.** igf6 stopped after mapscan,
   before the first exec-span read — module-contained, game and console
   survived. Capture the console-side result (single RETR), copy + sha256
   locally under `outputs/2026-10-04-zcode-controller/console/`, and treat
   the first unexecuted novel operation as the prime suspect. Do NOT
   immediately re-dispatch the same binary to "confirm".
7. **Collect once, receipt forever.** One bounded RETR of the result JSONL;
   local copy + receipt (sha256) recorded in CONSOLE-LOG. Console-side
   records are never modified or truncated by the controller.

## d. FTP read discipline (the caching issue, R-FTP-02)

Status: the caching mechanism itself is NOT demonstrated — this row is
task-named by the controller brief because staleness symptoms were observed
and the whole staging oracle depends on RETR readback. Treat every rule here
as oracle hygiene, not proven behavior.

1. **After any STOR/upload, wait 15–30s before the verification RETR**, and
   make the readback on a **fresh connection** (no session reuse). The point
   is to give every caching layer time and no opportunity to serve the
   previous generation.
2. **Size-change check:** compare readback size against (i) THIS attempt's
   expected size and (ii) the pre-image/previous generation's size. A readback
   byte-size identical to the previous file is a staleness warning even if a
   hash happened to match — prefer unique per-attempt destination names
   (suffix or new path) for high-stakes payloads so a cache cannot alias
   generations.
3. **Verify sha256 against THIS attempt's receipt only.** Any mismatch is
   channel ambiguity: STOP. **Never re-pin the local receipt to match a
   readback** — that is the exact false-NOGO->re-pin hazard R-E2E-FR3 warns
   against, and equally never accept a false PASS from a stale readback of
   the previous file.
4. **Freshness oracle for run evidence:** result-file mtime + pre-launch stale
   JSONL rename are the PRIMARY freshness signals; RETR readback is secondary.
5. **R-FTP-01 size law stays binding:** RETR auto-decrypt is deterministic;
   payload readbacks must start `7f 45 4c 46` (inner ELF) and obey the
   segment-covered-prefix law. Keep it in every staging receipt.
6. **Escalation:** if a size/hash mismatch repeats, stop ALL staging and
   explicitly probe the caching behavior before staging anything else;
   document what was found.

## e. Crash recovery procedure

Trigger: dispatch timeout, refused ports (8084/2121), ping loss, user reports
restart, or any zero-record + instability outcome.

1. **STOP all console contact immediately.** No re-dispatch, no retry, no
   ploader contact, no ping loops ("DO NOT re-dispatch ploader or attempt any
   console contact until fully restarted and verified" — crash 2 action).
2. **Record the preceding actions while fresh**: last successful step, payload
   identity (build-id + sha256), dispatch count and times, staging and arming
   steps taken, expected evidence. Both of today's crash entries were
   written this way and that is what made attribution possible later.
3. **Hand power/network to the user.** Console power state, restart, and
   network are user-controlled. Wait for explicit user confirmation that the
   console is back (crash 1: user confirmed "it crashed").
4. **First contact after recovery is READ-ONLY, in this order:**
   a. `GET /list_payloads` — is the manager responsive?
   b. FTP read-only: does the expected result file exist? size? mtime? Did
      the pre-crash module write anything? (crash 1 action: check the argv
      result file BEFORE any new dispatch.)
   c. `/manage:check` + readback sha of the staged payload — unchanged since
      the receipt, or note divergence.
5. **Assess whether the module ran**, from records only: result JSONL present
   with expected records = ran to that point; partial = stopped at the first
   unexecuted novel op (igf6 pattern, module-contained); zero records = never
   got to the sink or ring-0 event (igf5 pattern). Write the assessment into
   the checkpoint/CONSOLE-LOG.
6. **Verify the load path before any novel payload.** Re-stage a known-good
   diagnostic (the igd1 pattern: loads and writes after crashes) if there is
   any doubt the loader/record path still works. Note honestly: igd1 success
   proves the path, it does NOT prove the crashed candidate innocent.
7. **Reconcile before resuming.** Only after: manager responsive + results
   reconciled + crash entry recorded with cause hypothesis (or honest
   "unlocalized") + fresh authorization → consider the next dispatch of a
   DIFFERENT reviewed candidate. Never blind-replay the crashed action.
8. **Close the loop:** append the crash + recovery to CONSOLE-LOG.md, update
   the risk register row (the `zcode_console_unreachable` resolved:true
   pattern), and carry any new prohibition into section f.

## f. Known-prohibited actions

| # | Prohibited | Earned by |
|---|---|---|
| 1 | Double-dispatch of any payload (any second dispatch <30s after a first) | Crash 1, 18:44Z — dispatch #2 5s after #1, console died |
| 2 | Inline `syscall` instructions (raw `0F 05`, e.g. SYS_mprotect=74) in ploader modules; build gates must assert ZERO syscall sites and forbid `sceKernelMprotect`/`igcar_sys_mprotect` identities | Crash 2 + Townfall crash — igf5 zero records, console + game lost; leading (never confirmed) suspect |
| 3 | Exec-span byte reads from a ploader module; no broad investigation of the igf6 mechanism; no repeated text-read attempts | igf6 interruption after mapscan, before first exec-span read; user directive 21:17:12Z. Any future exec-page probe needs a NEW falsifiable design + user coordination |
| 4 | Re-dispatch of ARGV_ECHO.elf until reconciled | Crash 1 attribution residual (R-ARGV-01/R-G1-02) |
| 5 | Dispatch without explicit review/authorization | Standing gate `console.new_dispatch_allowed_without_review=false` |
| 6 | Game-file writes, autoload changes, Townfall launches outside explicit user authorization | Installation HOLD, honored all day |
| 7 | `/tmp` as a report/probe sink on console or host | INITONCE c49108fe (unreadable results) + R-TMP-01 |
| 8 | Retry on dispatch timeout; blind replay of failed/uncertain operations | Crash 1 timeline; workspace rule: actual readback + reconciliation |
| 9 | Deleting or overwriting console-side evidence; deleting stale result JSONL (rename instead) | Evidence-preservation rules; igf5 page-cache loss showed how easily console-side records vanish |
| 10 | ploader re-dispatch while console state is unreconciled after a crash | CURRENT-STATE console-safety rule; crash 2 action |
| 11 | Treating ploader OK / dispatch 200 as "the module ran" | R-PLD-01 — records are the only truth |
| 12 | Re-pinning a local receipt/hash to match a divergent FTP readback | R-E2E-FR3 hazard via R-FTP-02 |

---

## Appendix A — Console action catalog, 2026-10-04 (complete, from CONSOLE-LOG.md + checkpoint)

Controller session 1 (handoff-directed, ~15:28Z local 10:28 CDT):
1. Reachability GET /list_payloads -> 200, catalog OK.
2. Upload ONCE: NEXT_DLL_THREADS.elf (6d295f56) via /manage:upload; catalog
   entry 10:28:12-0500.
3. Verification: /manage:check ok; /list_payloads shows payload path; anonymous
   FTP bounded RETR readback 240,960 B, sha256 0dc08b37... — byte-identical.
4. Dispatch ONCE: GET /loadpayload:...NEXT_DLL_THREADS.elf -> OK (single
   reconciled gate supersession).
5. FTP RETR result log /data/modbridge-owned-dll-candidate.jsonl (11,624 B,
   12 records; 4 new for 6d295f56, final PASSED); local copy + receipt.
6. Local evidence staging; no deletions or overwrites on console.

Controller session 2 (standing authorization, later):
7. Reachability re-check /list_payloads.
8. Upload once + dispatch once: INITONCE c49108fe as INITONCE_DLL_TEST.elf ->
   OK, but result UNREADABLE (composed without --report-path; logged to /tmp;
   FTP refuses). NON-CRASH FAILURE, controller error; build superseded.
9. Corrected INITONCE 1a6b37b7 uploaded over the same owned alias (flawed
   build preserved locally with receipt); readback verified bc0574d6...;
   dispatch once -> OK.
10. Results: 4 records for 1a6b37b7, final PASSED; receipt + local copy.

Checkpoint-recorded console events (user-coordinated launches / probes):
- 16:57Z ingame probe v1 staged; 17:19Z IN_GAME_RUN_COMPLETED, 7 records.
- 17:27Z ingame v2 full run, 56 records.
- 18:43Z v3 PRX final staged -> zero records (cause then unconfirmed);
  19:15Z v3 MAPPING_BREAKTHROUGH, 1,035 v3 records.
- ~18:44Z ARGV_ECHO.elf upload OK -> dispatch #1 OK -> dispatch #2 (5s apart)
  30s timeout -> console unreachable (crash 1); user confirmed PS5 crash;
  console recovered same evening.
- 19:59Z anchor validation results from live console records (GUA read+write).
- igf4 probe staged + launched in-game: loaded + logged (last known-good;
  rollback sha 4aa624cc...).
- ~20:4x igf5-4186048a6dd6 staged + PLOADER ARMED; 20:43:17Z console crash 2
  (pre-launch, cause uncertain); Townfall launch attempt -> Townfall ALSO
  crashed, igf5 produced ZERO records (crash 2 + game crash in sequence).
- 20:45Z igd1 known-good re-staged: loads and writes after crashes (load path
  functional; per user, does NOT isolate igf5's cause).
- igf6-ad9bda1fc50a staged (same-file igf5 replacement, stale JSONL renamed) +
  launched; 21:13:50Z results: entry/startup/mapscan clean (10,047 records,
  8,685 spans), interrupted after mapscan BEFORE first exec-span read;
  module-contained (game + console survived); 21:17:12Z user amendment:
  mechanism NOT localized.
- 21:33Z fileio console target run: PASSED after namespace-root fix.

Totals for the day: 6 manager reachability/check calls logged, 5 uploads
(NEXT_DLL_THREADS, INITONCE x2 over one alias, ARGV_ECHO, igf5/igf6 PRX
replacements, igd1 re-stage), 5 standalone dispatches + 4 user-coordinated
in-game launches (v1/v2/v3-lineage, igf4, igf5, igf6), ~12 FTP result
collection/readback operations, 2 ploader arming events, 2 console crashes +
1 game crash + 2 non-crash failures (INITONCE /tmp, v3 zero-records).

## Appendix B — Crash/failure precedents (what preceded -> outcome -> rule)

| Event | Immediately preceding actions | Outcome | Attribution status | Rule born |
|---|---|---|---|---|
| Crash 1 (18:44Z) | ARGV_ECHO upload OK; dispatch #1 OK; dispatch #2 5s later -> 30s timeout | Total console loss (ports + ping); user-confirmed PS5 crash; recovered same evening | RESOLVED — double-dispatch (R-ARGV-01); result file never read | b1/b2, f1, e4b |
| Crash 2 (20:43:17Z) | igf5 staged + ploader ARMED; user had NOT confirmed launch | PS5 crashed + restarted, pre-launch | UNLOCALIZED — 3 suspects (inline syscall shim, ploader arming, unrelated); syscall is leading hypothesis only | e1/e2, f2, f10 |
| Townfall crash (igf5 launch attempt) | igf5 (2 raw syscall sites, SYS_mprotect=74) launched in-game | ZERO records (write-through sink writes seq 1–3 first) + console AND game lost | igf5-specific failure CONFIRMED; syscall cause LEADING, never confirmed (user 20:49:22Z; igd1 control) | c5, f2, f11 |
| igf6 interruption (21:13Z) | Clean staging (stale JSONL renamed), launch; seq1–3 written (entry/startup/mapscan, 10,047 records) | Stopped AFTER mapscan, BEFORE first exec-span read; module-contained; game + console survived | NOT LOCALIZED (user amendment 21:17:12Z); exec-only enforcement is leading hypothesis | a6, c6, f3 |
| INITONCE /tmp (non-crash) | Composed without --report-path; payload logged to console /tmp | Result unreadable — FTP refuses /tmp; build superseded, not counted as evidence | Controller error, acknowledged | a7, f7, R-TMP-01 |
| v3 zero records (18:43Z) | v3 PRX staged | Zero records at first staging; lineage later succeeded (1,035 records) after diagnosis | Superseded by MAPPING_BREAKTHROUGH | c4/c5 (zero-records = stop, reconcile) |

## Appendix C — Source index

- `outputs/2026-10-04-zcode-controller/CONSOLE-LOG.md` (items 1–10)
- `outputs/2026-10-04-zcode-controller/CONTROLLER.md` (console policy, gates)
- Checkpoint entries (read-only): `zcode_console_unreachable`,
  `zcode_console_crash_2`, `zcode_igf5_diagnosis` (+user correction),
  `zcode_worker_igf5_diagnosis_rebuild`, `zcode_igf6_RESULTS`
  (+`accurate_recording`), full `zcode_*` timeline 15:29Z–21:39Z
- `outputs/2026-10-04-zcode-team/worker-checkpoint-reconcile/`
  (RECONCILIATION.md items 8/9/13, CURRENT-STATE.md "Console safety state")
- `outputs/2026-10-04-zcode-team/worker-risk-update/RISK-REGISTER-V2.md`
  (R-ARGV-01, R-PLD-01, R-FTP-01, R-FTP-02, R-EXEC-01, R-TMP-01, R-E2E-FR3,
  R-G1-02, R-G3-01) and REPORT.md
- `outputs/2026-10-04-zcode-team/worker-argv-probe/REPORT.md`
- `outputs/2026-10-04-zcode-team/worker-igf5-diagnosis-rebuild/REPORT.md`,
  `STAGE-PLAN-IGF6.md`
