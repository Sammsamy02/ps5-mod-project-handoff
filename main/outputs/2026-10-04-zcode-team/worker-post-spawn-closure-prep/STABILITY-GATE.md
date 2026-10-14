# STABILITY-GATE — post-spawn closure criteria: does the session stay playable with the runtime resident?

Worker: worker-post-spawn-closure-prep, 2026-10-04. Read-only synthesis + spec. PROPOSAL:
every criterion below is a contract for a future console session; nothing here has been
console-executed. Companion to `ACCEPTANCE-RECORD.md` (what to archive) and
`RESTORE-PATH.md` (how to end the session). Root `R = /WORKSPACE/.codex/worktrees/translation-core/what`;
`W = R/outputs/2026-10-04-zcode-team`; `FTP = ftp://ps5-host-not-configured.invalid:2121` (anonymous).

Position in the ladder: the acceptance goal ends at **visible menu (CP5) + successful
in-game spawn (CP6 / rung 5)** — runbook G4/G5: the user's eyes decide, log lines never
substitute. The stability gate is the **next** rung the project owes after that moment. It
never gates the acceptance goal, never advances a CP or rung, and never modifies the
console: it is observed, bounded, and honest.

---

## 0. Why a stability gate exists here (the fail-open frame)

- The runtime is **fail-open BY DESIGN**: `module_start` returns 0 on any
  wiring/activation failure → kernel continues → the game runs unmodded (rollback
  runbook §3; probe build REPORT §2 "fail-open — a deliberate, documented delta").
- That design has been exercised on **eight real launches (L1–L8)**, root-causing seven
  distinct blockers (CURRENT-STATE-DIGEST item 3): every failure was loud, attributed, and
  the game/console survived each one. Fail-open is **proven**; what is NOT yet proven is
  the complementary claim: *with the runtime resident and functional — post-spawn — the
  session stays playable for a bounded residence period, and any crash during that period
  leaves an attributable record.*
- The gate therefore asks a different question than the CP ladder. CP0–CP6 ask "did each
  hop work?"; SG asks "does the whole thing keep working, and when it stops, can we say
  why?"

## 1. Preconditions

1. CP0–CP6 passed in THIS launch window (fresh session evidence; `INCONCLUSIVE-STALE`
   cells from a prior launch can never precondition the gate).
2. The user has a loaded save and is in-game; the menu has opened; at least one visible
   spawn confirmed (eyes + `Spawned …` line).
3. The `game-evidence` flow has written its receipt for this window (that receipt is the
   anchor artifact the SG record extends, not replaces).
4. All standing law carries over unchanged: dispatch exactly once, ≥30 s gaps, timeout =
   crash signal, gate mismatch = STOP+preserve+reconcile, no re-rating unchanged evidence,
   bounded FTP reads (fresh connection, `--connect-timeout 10 --max-time 60
   --max-filesize 32M`, retries at ~60 s, never hammer).

## 2. The criteria (SG-A … SG-E)

Verdict vocabulary: each criterion is **PASS**, **FAIL**, or **NOT-ATTEMPTED** (with the
reason). `NOT-ATTEMPTED` is honest and keeps the overall verdict `SG-PARTIAL`; it is never
silently dropped.

### SG-A — time-resident (PROPOSED threshold: 30 min continuous post-spawn residence)

- With a loaded save and ≥1 confirmed spawn, the game remains playable with the runtime
  resident for **≥30 minutes** of wall-clock time. Milestone checks at **T+5 / T+15 /
  T+30** (T = first confirmed spawn):
  - Host-side, read-only, bounded: fresh-connection RETR of the sink sizes/mtimes —
    `S` (`/data/modbridge/roots/PPSA33286/runtime-status.txt`), `L`
    (`/data/plugins/UE4SS.log`), `D1` (`/data/townfall_runtime_aligned_entry.log`), and
    the observer log `/data/townfall_runtime_aligned_observer.log`. Expect: S still ends
    `mods-started` (no `failed`/`stopped` line), L and D1 mtimes advancing or at least
    non-stale, no entry failure lines newer than the launch.
  - USER eyes at each milestone: "is the game still running normally?" (playable, no
    frozen frame, no crash report on screen). Eyes outrank files (G4/G5 law).
- **Honest bounds:** the 30-minute figure is a host-side observation budget chosen as
  ~6× the ~5-min boot budget and long enough to cover a real play slice — it is a
  PROPOSAL, not a console-side timer. No console deadline may be extended or bypassed;
  the controller only watches. If the user needs to stop earlier, the record is
  `NOT-ATTEMPTED(residence ended at T+<n> by user)` — not a FAIL.
- A console crash report, frozen frame, or S `failed` line inside the window = SG-A FAIL
  → §4 crash procedure immediately.

### SG-B — repeated spawn attempts (anchored on EVIDENCE-SPEC rung-5 repeatability)

- In the same session: **N ≥ 5 successful CROSS spawns total** (PROPOSAL, anchored to
  EVIDENCE-SPEC §5.2/§5.4 item 5), **mixing rows and at least one different item after
  navigation**, each with its `Spawned <Item> x<N> ID=<id>` line AND a visible pickup.
- Keep the rung-5 negative controls alive in the same window: CROSS with menu CLOSED →
  nothing (menuOpen gate); OPTIONS toggle close/reopen → silent close, clean reopen
  (T4/T5 shapes); pad stays paired (no phantom events after any disconnect).
- Spawn is retry-safe by construction (guards + pcall; smoke phase 3/4), so a `Failed at
  <stage>` on a LATER attempt after earlier successes is an honest negative for that
  attempt — record it, distinct revision ID for any fix, **never blind-retry, never
  re-rate unchanged evidence**. It does not by itself fail SG-B if an earlier N≥2 was
  clean; it DOES fail SG-B if failures cluster (e.g. every attempt after the 4th) —
  clustering is the leak/degradation signature SG exists to catch.
- Archive the operator count table per press: `press # → item id → wall time →
  visible? → log line present?` (feeds ACCEPTANCE-RECORD Track A rung-5 count table).

### SG-C — save/load cycles (EXPLORATORY: the mod was never expected to survive a map change)

Honest framing first: the runbook records "a second load after a map change is not
expected" (CP4 note) — i.e. the mod's re-initialization across save/load is **unverified
territory**, and a dead menu after reload is a *plausible by-design* outcome, not
evidence of instability. SG-C is therefore a probe with three named outcomes, not a
pass/fail bar:

- **Procedure (user-performed; controller observes):** from in-game post-spawn → save →
  quit to title (or load a different save) → load a save → in-game → press OPTIONS once.
- **(c1) Mod re-initializes cleanly:** fresh load banner appears again, or the menu opens
  and scans without error → strongest stability evidence; record the exact L lines.
- **(c2) Menu dead after reload, game stable:** game plays fine, OPTIONS produces the
  known no-reaction shape → stability HOLDS, mod needs a new revision for reload support
  (distinct ID; the §4.3d ladder applies before claiming the dispatch chain broke).
- **(c3) Game crash / hang during save/load with the runtime resident:** SG-C FAIL and
  SG-A FAIL → §4 crash procedure; disable-before-next-launch per rollback §4.
- At most **2 cycles** in one session (PROPOSAL bound; each cycle = one new observation,
  individually recorded; no re-rating).

### SG-D — growth-record corroboration (what the observer can and cannot prove)

- **What the observer (entry 5, `modbridge_runtime_aligned_observer.prx`, sha
  `1afa0997…`) provides:** ONE window per launch, 30 polls × 2 s (~60 s) from its own
  `module_start`, recording UE4SS.log growth (`ue4ss_log_baseline` → `ue4ss_log_grew`
  byte-deltas, bounded 512-byte sanitized tail lines), mod-root presence
  (`mod_root_present`), and summary counters (`observer_polls_completed`,
  `observer_growth_events`, `observer_final_size`) in
  `/data/townfall_runtime_aligned_observer.log` (D2). Read-only guarantee: it never
  writes, truncates, or renames anything it watches.
- **Use as evidence:** the boot-window growth records prove the log channel was alive and
  growing immediately after injection, and `PATH_TEST:`/`ACTIVATION_TEST:` terminator
  lines (entry 7) prove full 7/7 list traversal for THIS launch. Together they anchor the
  session as "runtime alive from boot" — the baseline every SG-A milestone compares
  against. Fetch D2 once per milestone window (it is append-only; late fetches still
  return the whole window).
- **Honest limits (do not over-claim):** the observer's window closes ~60 s after boot;
  it does NOT observe post-spawn residence. Post-spawn growth evidence comes from the
  host-side size/mtimes snapshots in SG-A plus the session's own mod lines. A longer or
  delayed observer window would require a **NEW observer revision** (new build ID, new
  gate run, re-pinned ini bytes) — never a silent re-parameterization of the frozen PRX.
- Observer absence is NOT instability: windows close with honest negatives by design
  (`stat_failed` / summary counters at zero) — record, never chase.

### SG-E — clean exit

- Session ends by the USER quitting the game normally (never by controller action).
- Afterwards, collect ONCE: S final state (expect terminal `stopped`; a missing terminal
  line means the process died — record exactly that, never fabricate), D1
  `runtime_stop_*` tags, final sink sizes. F10 (`stopped` + nonzero
  `runtime_stop_result`) is an honest stop-path failure: record, triage offline.
- Game returns to home without a console crash report → SG-E PASS.

### Overall verdict

| Verdict | Meaning |
|---|---|
| `SG-PASS` | SG-A, SG-B, SG-E PASS; SG-D corroborates; SG-C at least documented (c1 or c2). |
| `SG-PARTIAL` | Some criteria `NOT-ATTEMPTED` (reasons recorded); no FAIL. A legitimate terminal state — record and archive it. |
| `SG-FAIL` | Any crash/hang/instability with the runtime resident → §4 procedure; RESTORE-PATH disable/teardown decision follows. |

The SG verdict is a **precondition of the leave-installed option** (RESTORE-PATH §B):
leave-installed may follow `SG-PASS` only; without a stability pass, the standing
end-state is disabled (`= 0`) or teardown.

## 3. Residence evidence summary table (fill per milestone)

| Milestone | S last state / mtime | L size Δ | D1 new tags | D2 growth summary | Eyes: playable? | Notes |
|---|---|---|---|---|---|---|
| T+0 (spawn confirmed) | | | | | | |
| T+5 | | | | | | |
| T+15 | | | | | | |
| T+30 | | | | | | |
| Exit | | | | | | |

## 4. Crash-report procedure (aligned-entry log + the probes' persist-before-mutate discipline)

### 4.1 The moment a crash is suspected (game frozen/crashed, or console unreachable)

1. **STOP all console contact immediately.** A dispatch timeout IS the crash signal —
   never resend, never re-dispatch on ambiguity (crash-1 precedent: the argv
   double-dispatch 5 s apart killed the console; igf5 precedent: an inline-syscall probe
   crashed console+game). Do not touch payloads while the game is in a bad state.
2. **User action first:** close Townfall from home (do not rely on the mod to unfreeze;
   the tick detour lives in the game process only and dies with it). If the console is
   wedged: full restart (full boot, not rest mode) — rollback §5 ritual; nobody
   dispatches until the user says it is back.
3. **Console-side crash report:** the PS5's own crash report is the console's evidence
   for cause attribution — the user preserves/photographs/reports it; it outranks our log
   reads (outcome #6 precedent: "Console crash report is the evidence").
4. **One bounded evidence collection, ONCE** (severity-A style, rollback §3; plain files
   so the byte-exact law applies): fresh connection per file, into a timestamped local
   dir `comp-sg-crash-<TAG>/` — `runtime-status.txt`, `UE4SS.log`, the aligned-entry log
   D1, the observer log D2, both probe verdict sinks
   (`/data/modbridge-exec-write-probe.jsonl`, `/data/modbridge-fc1pool-facts.jsonl`),
   plus the slot readback sha (law-aware: never whole-file sha vs upload for fself).
   Hash everything; `shasum -a 256` the dir. Never hammer: if FTP is dead, ONE polite
   retry at ~60 s, then wait for the user.
5. **Read the aligned-entry log as the crash record:** D1
   (`/data/townfall_runtime_aligned_entry.log`) is append-only with per-launch counters
   (`module_start_entered` count = launches attempted, oracle O1) and typed, stamped
   failure tags (r4 entry stamps failures with timestamp+pid; `runtime_start_failed`
   values decode per CP1). The LAST D1 tags before the silence name the stage the
   runtime believed it was in. Cross-read S (per-session truth) → D1 → L in that order.
6. **Probe sinks survive mid-ladder death by design:** the FINAL-INI-LAYOUT ordering puts
   the xwp and fc1pool verdict sinks on disk BEFORE the composition ladder starts —
   "if the ladder dies the title, fc1's verdict is already flushed." Their last records
   bound WHEN the crash happened relative to the ladder.
7. **Recovery ritual then applies** (rollback §5): single reachability probe
   `GET http://ps5-host-not-configured.invalid:8084/list_payloads` after full boot; FTP plain-file sanity
   (ploader.ini sha vs last recorded; slot ELF magic); staged /data intact vs recorded
   shas (power loss has preserved /data twice-proven; a mismatch = possible mid-STOR
   crash → treat the slot as unverified, RESTORE-PATH §A before the next launch).
8. **If the crash suspect is the mod:** disable BEFORE any next launch — the §4.4
   single-byte flip (`= 2` → `= 0` on the composition line, fresh `.bak`, byte-exact
   readback). Never relaunch an identical layout hoping for difference.
9. **Log the incident** in CONSOLE-LOG style: timeline, suspect, evidence read, actions.
   Each crash earns its own permanent do-not rule (crash-1 → dispatch law; igf5 →
   syscall ban); a stability-window crash must ADD an entry, not bend an existing one.

### 4.2 The persist-before-mutate discipline (from the probes' map-persist pattern)

The xwp exec-writability probe's per-attempt sequence is the project's crash-capture
canon (probe build REPORT §2):

```
row record → fresh VMMAP scan → vmmap_persist (PRE-STATE, flushed) → mutation
→ attempt record (rc+errno) → gated test → restore → post_restore re-proof
```

Rules any future stability-window probe or iteration MUST inherit (and the controller
MUST mirror at session level):

1. **Persist the pre-state BEFORE the risky op**, synchronously, via `sceKernelWrite`
   before the next syscall — a crash then leaves the intent + pre-state on disk.
2. **Restore and re-prove after** (`restore` + `post_restore` records); a failed restore
   is recorded as an anomaly, never assumed.
3. **Hard budget caps** (xwp: 8 attempts), re-entry answers 0, single-shot synchronous,
   no threads, no unload; `module_start` returns 0 on EVERY path (fail-open).
4. **Session-level analogue (controller-side):** before each risky console op inside the
   stability window, the intent is already in the run dir (`events.log` + the flow's
   `receipt.json`: what is about to happen, the expected readback identity, the abort
   law). An unlogged risky op is prohibited — after a crash, the run dir + D1 + S must
   reconstruct the exact point of death (the "aligned-entry" property: host log, entry
   log, and status file tell ONE aligned story).
5. **New-evidence law unchanged:** a post-crash observation window requires a NEW launch;
   every observed failure gets a distinct revision ID; never re-rate unchanged evidence.

## 5. What the stability gate is NOT

- It never advances CP0–CP7 or rungs 1–5; rung evidence laws (EVIDENCE-SPEC) still apply
  to any file the window produces.
- It never claims "AAA quality", completion, or parity — an `SG-PASS` says exactly:
  *one bounded residence window on one launch stayed playable, with this evidence.*
- Absence of rung/driver evidence inside the window (`UNVERIFIED_DRIVER`,
  `CANNOT_ASSERT`, closed observer windows) is honest unevidence, not failure.
- The gate's verdicts are recorded in the session's `game-evidence` run dir as
  `stability-gate-verdict.md` (PROPOSED name; see ACCEPTANCE-RECORD §5), produced by the
  controller from the milestones table — it certifies nothing on its own.
