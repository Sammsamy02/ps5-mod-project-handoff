# RISK-REGISTER-V2.md — worker-risk-update (team 2026-10-04, evening refresh)

Update of `worker-risk-e2e/RISK-REGISTER.md` (26 rows: 9 mandated + 17
supplementary) against today's late-session outcomes. Basis: per-row evidence
re-read from the owning workers' reports delivered after the register was
compiled, plus `worker-checkpoint-reconcile/CURRENT-STATE.md` (~21:30Z
snapshot) and `worker-checkpoint-reconcile/RECONCILIATION.md`.

Status vocabulary:
- **RESOLVED** — the risky event happened and the outcome is evidenced good,
  or the attribution question was answered.
- **RESOLVED-HOST-SIDE** — the fix/merge is built, tested, and re-pinned in
  the workspace; only the console-side application remains (controller step).
- **ADVANCED** — new evidence today; risk still open, likelihood/mitigation
  updated.
- **OPEN-UNCHANGED** — no new evidence; row stands as written.
- **SUPERSEDED** — closed without its original question being answered;
  superseding evidence named.

Four NEW risks discovered today are appended in section C (R-EXEC-01,
R-FTP-02, R-COMP4-01, R-TMP-01). Likelihood/impact remain qualitative
engineering judgments. Evidence citations are worker dirs under
`outputs/2026-10-04-zcode-team/` unless a checkpoint key is named.

---

## A. Mandated risks — updated

### R-RW-01 — Rosetta unkillable-wedge amplifier — OPEN-UNCHANGED
- Today: no new wedge/abort event. Positive discipline evidence:
  `worker-composition-4` hit a delivered-script gate and STOPPED with
  documentation instead of forcing or killing anything; mutex lineage passed
  build 6 under watchdog bounds (`worker-mutex`, checkpoint
  `zcode_worker_mutex` 19:09Z).
- Residual: unchanged; keep watchdog-bounded reproduction and the
  serialize-heavy-processes rule for every future host-driver run
  (mutex-sdk-retry and fileio-apply SDK runs each serialized correctly).

### R-DRV-01 — driver unbounded cleanup retry — ADVANCED (downgraded likelihood)
- Today: the previously-failing composed paths went green end-to-end:
  mutex host controls PASS after 6 builds (`zcode_worker_mutex`); full mutex
  SDK composition 81/81 commands exit 0 with console candidate produced
  (`worker-mutex-sdk-retry/REPORT.md`: prior `ms_abi` failure at command 34
  fixed). fileio+mutex joint host validation PASSED first run and full SDK
  composition with `--fileio` PASSED (`worker-fileio-apply/REPORT.md`).
- Residual: the `sdk_driver.c` retry-loop hardening itself is still not the
  load-bearing protection — today's passes exercised the happy teardown
  path. A NEW permanently failing cleanup stage would still spin to the
  watchdog. Keep: bound-the-retry proposal, fixture token-retirement
  invariant in-suite. Likelihood medium→low-medium; impact unchanged.

### R-E2E-FR3 — console link not bit-reproducible — OPEN-UNCHANGED (practice adopted)
- Today: the register's size+import-count identity rule was adopted in the
  successor manifests: `worker-d1-chain-relink/STAGING-MANIFEST-REPIN.md`
  re-pins per F-R3, and `worker-console-staging-bundle/MASTER-STAGING-PACKAGE.md`
  pins sha256+bytes for all 9 staged files with per-file RETR readback.
- Residual: standing channel property; every future relink must re-pin
  deliberately with a receipt. Compounding hazard R-FTP-02 (below) added.

### R-PAD-01 — pad provider never ran on hardware — ADVANCED (still unrun)
- Today: PS5Pad profile staged package verified and the hardware test is
  declared ready (`CURRENT-STATE.md`; `worker-real-input-pad`).
  `hardware_runtime_validated` is still false by construction.
- Residual: first real ScePad open/read still happens at the first console
  input test; run it bounded, records-on-failure, no silent fallback.

### R-ARGV-01 — argv probe crashed the console — RESOLVED (attribution)
- Today: the crash is attributed to the argv double-dispatch (dispatch #1
  OK, dispatch #2 30s timeout; temporally linked to console unavailability —
  `worker-argv-probe/REPORT.md`, RECONCILIATION items 8/13). The console
  recovered the same evening and served igf4/igf5/igf6. Checkpoint
  `zcode_console_unreachable` gets `resolved: true`.
- Residual: ARGV_ECHO stays for later attribution only; G1 is served by the
  config-file-driven entry design (`worker-e2e-console-dry/E2E-CMD-SCHEMA.md`)
  — see R-G1-02.

### R-V3-01 — v3 in-game probe stopped after VMMAP — SUPERSEDED (resolved in effect)
- Today: `zcode_v3_MAPPING_BREAKTHROUGH` (19:15Z, 1035 v3 records) answered
  what the DIAG ladder was for; the igf4/igf5/igf6 lineage ran to completion
  and unblocked layout promotion (GUA live-validated, staged pair complete).
  The three surviving v3 hypotheses were never individually confirmed and
  no longer need to be.
- Residual: none on the critical path. Historical entry
  `zcode_v3_final` ("ZERO_RECORDS_CAUSE_UNCONFIRMED") should carry the
  superseding pointer (RECONCILIATION item 22).

### R-FTP-01 — FTP readback artifact / size law — ADVANCED (law operationalized)
- Today: RETR-auto-decrypt behavior and the inner-ELF expectation are now
  codified in the execution docs (INTEGRATION-CHECKLIST STEP 8.4:
  first bytes `7f 45 4c 46`, receipt-hash equality; STAGING packages make
  readback verification a mandatory step; freshness via log mtime + stale
  JSONL rename in `worker-igf5-diagnosis-rebuild/STAGE-PLAN-IGF6.md`).
- Residual: law still binding on every staging receipt; new sibling risk
  R-FTP-02 (FTP caching) added today — see section C.

### R-PLD-01 — ploader cannot see module_start's return — OPEN-UNCHANGED (re-confirmed)
- Today: discipline re-validated from both directions: igf6 "module loaded,
  entry/startup clean" was judged ONLY from the record stream, and the igf5
  zero-record outcome proved records are the only truth (write-through
  sink writes seq 1–3 before anything else can run —
  `worker-igf5-diagnosis-rebuild/REPORT.md` Part 1).
- Residual: never claim "started" from ploader OK; record-stream acceptance
  on every future launch.

### R-D1-01 — D1 journal-order fix propagation — RESOLVED-HOST-SIDE
- Today: `worker-d1-chain-relink/REPORT.md` built the canonical 51-tuple
  merged chain (4 uf tuples D1-FIRST, then 2 r2 tuples) via two recorded
  splices, rebuilt the console payload ELF (host+console flavors), ran the
  regressions, and re-pinned the staging manifest
  (`STAGING-MANIFEST-REPIN.md`). This was the register's #1 refill item.
- Residual: the merged payload still needs to be STORed to the console and
  exercised (controller staging step, E2E install/uninstall/clean-state
  stages). Until that run, `/data/modbridge` console state remains pre-D1 —
  avoid long-lived console state before the relinked payload stages.

---

## B. Supplementary risks — updated

### R-R2C-01 — F-R2/uf merge-order conflict — RESOLVED-HOST-SIDE
- Today: the canonical 51-tuple merge order (uf BEFORE r2, exactly as the
  register prescribed) was implemented and regression-tested by
  `worker-d1-chain-relink` (see R-D1-01). fr2-a/fr2-b checks ride in the
  merged-chain regression.
- Residual: keep the order fixed in every future chain edit.

### R-G1-02 — argv forwarding unproven — ADVANCED (mitigation designed)
- Today: `worker-e2e-console-dry` delivered G1 config-file-driven entry
  (FTP-STOR'd `/data/modbridge/intake/e2e-cmd.json`, bare dispatch,
  E2E-CMD-SCHEMA.md), so stage commands no longer depend on argv forwarding.
- Residual: forwarding itself remains 50/50 until a bounded probe runs;
  ARGV_ECHO re-dispatch stays barred until reconciled (now attributed —
  R-ARGV-01 — so a single deliberate re-probe is designable, not yet owed).

### R-G3-01 — FTP STOR into console dirs unproven — ADVANCED
- Today: `worker-console-staging-bundle/MASTER-STAGING-PACKAGE.md` specifies
  Gate A0 write-probe → pre-image backups → mkdir → STOR ×9 → per-file RETR
  readback. The probe is designed and ordered first; execution is pending.
- Residual: open until the Gate A0 probe actually runs on console.

### R-DL-01 — console download stage host-linked — ADVANCED (unchanged on console)
- Today: `worker-e2e-final` bound the download stage to the exclusive
  curl-owner v1 interface (`mb_curl_runtime_start_v1` /
  `mb_curl_download_v1` / `mb_curl_runtime_close_v1`) — host-side only; the
  owner-placed-zip probe-A path remains the first-E2E route.
- Residual: unchanged on console until the v1-bound payload stages and runs.

### R-ACK-01 — real runtime ack absent — ADVANCED (blocked one level up)
- Today: composition-3 built with ack bundles 01/02 consumed
  (4/4 hashes verified; `worker-composition-3`); all 10 replacement/new-unit
  sources hash-attested including `runtime_ack_supplier.{cpp,hpp}`
  (`worker-composition-4/REPORT.md` §1). The initializer unit compiled clean
  with the 5 QueryHarness/ack symbols (`run-1/unreal-unrealinitializer.o`).
  But composition-4 FAILED at the script gate (R-COMP4-01) — no composed
  object with the ack units exists yet.
- Residual: running=1 stays SIMULATED on console until recomposition + a
  launch proves the five module_start emissions.

### R-GT-01 — game-thread delivery gate — ADVANCED (still not delivered)
- Today: composition-4's compiled initializer proves the S2-arm wiring
  landed in source (ArmLayoutGate placement, disarm-on-throw wrappers,
  persistent `EngineTickPreCallback` registration —
  `worker-composition-4/REPORT.md` §1/§3). Not composed, not staged, not run.
- Residual: `ExecuteInGameThread` seam delivery still gates rung-5; path
  runs through R-COMP4-01 then the acceptance ladder.

### R-IDENT-01 — identity/retention facts — MOSTLY RESOLVED (residuals named)
- Today RESOLVED: axes resolved from the retained image + marker model —
  engine 5.6.1-0+++UE5, build_id `04bb4330…`, SHIPPING (margin 60), STATS
  OFF (margin 12) (`worker-layout-identity`, `worker-axes-detector`); GUA
  live-validated (layout A, `0x97ba110`, 71,959 objects, read+write); eboot
  base observed `0x400000` (rt−profile vaddr delta, consistent across igf4/
  igf6 — `worker-c2-alt-correspondence/CLAIM.json` inputs); FINAL staged
  pair carries all-real identity (`worker-emit-final-pair`).
- Residual (open): DT_DEBUG population, RTLD_NOLOAD retainability,
  exec-module-handle signature, module-list size unit, loader-vs-kernel
  handle domain equality, fw 13.60 profile values, and the Tick/SCO
  criterion (explicitly NOT substituted by the C2 alternative — c2-alt
  CLAIM constraints). text_base is now anchored at 0x400000 observed base.

### R-LAYOUT-01 — layout predicates synthetic-only — MOSTLY RESOLVED
- Today: GUA predicate validated LIVE and the GC-order bug fixed and
  re-validated (`worker-predicate-gc-fix`); FINAL staged pair ParsePair-
  verified, 17 members, stageable today; first live-data pair earlier
  (`worker-staged-pair-live`). FName P2 has a single lead (runtime
  `0x63d590`, 9 call sites materialized) and the FC-1 runtime probe
  delivered pending controller verification (`worker-fname-runtime-probe`).
- Residual: rung-3+ predicates (UStruct/UClass/FField/FProperty beyond the
  gated subset) unvalidated in-process; UFunction still has no validated
  predicate by discipline; ACCEPTED.md rows only after the harness
  milestone (INTEGRATION-CHECKLIST STEP 14).

### R-LIBC-01 — console libc dirent + fsync durability — OPEN-UNCHANGED
- No new evidence; answered only by E2E stage-3 rollback probes. Staging
  plan ready (`worker-e2e-console-dry`); keep recovery fail-loud pinned.

### R-PRX-01 — PRX route static-only — RESOLVED (for the igf lineage)
- Today: the PRX route is proven in-game end-to-end: v3 PRX interim built
  deterministic and staged (`zcode_v3_prx_interim` sha `1dfbf3d5…`), and the
  igf4/igf5/igf6 PRXs loaded in-game with records (igf6 `ad9bda1fc50a`,
  24,173 B, two-run determinism EQUAL — `worker-igf5-diagnosis-rebuild`).
  Module-load evidence exists; "loads and records" ≠ behavior success.
- Residual: composition-4's packaged candidate remains pending
  (R-COMP4-01); static checks stay gates, never load claims.

### R-CRT-01 — CRT census gap — OPEN-UNCHANGED
- No census delivery; DESIGN.md 2.2 still PENDING. DLL track advanced
  around it (mutex/fileio SDK candidates green host-side), so it stays
  queued, not E2E-critical.

### R-LOG-01 — in-game log parser unvalidated — ADVANCED (validation now possible)
- Today: the missing prerequisite now exists — real console JSONL logs
  (igf4/igf5/igf6) are in the workspace/records; the parser's rules can now
  be tested against real bytes. No evidence it has been run yet.
- Residual: run `python3 -m unittest discover -s tests` against the real
  logs before adopting as standard collector (see REFILL-QUEUE item Q4).

### R-SER-01 — joint fileio+mutex full-green receipt — RESOLVED
- Today: `worker-fileio-apply/REPORT.md`: all 7 fileio diffs applied to the
  mutex-integrated tree; host validation with the full family stack PASSED
  first run (`OWNED_CANDIDATE_HOST_CONTROLS_PASSED`); full SDK composition
  with `--fileio` PASSED (7.2 s, `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_
  EXECUTED`). The serialization stall is gone.
- Residual: console target execution not attempted, not claimed — that is
  the mutex+fileio staged-session design task (REFILL-QUEUE Q10).

### R-BOUND-01 — bridge v1 bounds — OPEN-UNCHANGED
- Honest refusal behavior unchanged; streaming variant still post-E2E,
  commission only on corpus demand.

### R-CONTC-01 — container coverage gaps — ADVANCED (one gap closed)
- Today: pak index delta SETTLED — saved pak authoritative count 4699
  (byte-exact), 4544 belongs to the original on-disc pak, never cross-apply
  (`worker-pak-index`); complete 144-byte utoc header spec + v2 rules
  (`worker-container-headers`).
- Residual: Zen TOC signing, Oodle presence in the two complete pairs,
  encrypted ZIP/PAK fixtures, RAR/bsa/ba2/7z — still unexplored; do not
  guess byte semantics.

### R-ACT-01 — actions layer host transport — ADVANCED (honesty improved)
- Today: `zcode_worker_8_redo` DELIVERED_61_OF_61_REAL_BINARY_TESTS —
  records store (TSV, exact transitions), actions delegating to the
  composed CLI, honest capability JSON (running only from ack), jobs
  synchronous, cancel no-op for terminal jobs. Console payload binding
  remains the named controller integration step.
- Residual: `installation_supported:false` stands until a console E2E
  proves otherwise. Audit-provenance caveat: the records/UI audit applied
  to an export copy; authoritative worktree files need re-verification
  (REFILL-QUEUE Q11).

### R-MUT-01 — CreateMutexA census slot unbound — OPEN-UNCHANGED
- The mutex family is now SDK-composed (`worker-mutex-sdk-retry`), but the
  observed UE4SS import is still `CreateMutexA` and the CP_ACP policy
  decision is still owed. Shim proposal queued (REFILL-QUEUE Q8).

---

## C. NEW risks discovered today

### R-EXEC-01 — exec-only page enforcement blocks C2 code anchors (igf6 platform finding)
- **area**: promotion criterion C2 / exec-span reads from a ploader module
- **risk**: igf6 ran clean through entry/startup/mapscan (10,047 records,
  8,685 readable spans) but was interrupted before the FIRST exec-span byte
  read — the module died at the exact novel operation; console and game
  survived (module-contained). Leading hypothesis: exec-only page
  enforcement on this firmware/kernel — i.e. mapped-and-"readable"-flagged
  exec pages fault on data read. This contradicts the igf6 build-time
  safety rationale ("x86-64 NX gates EXECUTE, not READ"). User-amended:
  the mechanism is NOT localized; per user directive there must be NO broad
  investigation and NO repeated text-read attempts.
- **evidence**: `worker-c2-alt-correspondence/CLAIM.json` inputs ("4th
  record (first exec-span read) never arrived — module crash");
  `CURRENT-STATE.md` igf6 bullet; checkpoint `zcode_igf6_RESULTS.
  accurate_recording` (21:17:12Z); `worker-igf5-diagnosis-rebuild/REPORT.md`
  Part 2 safety rationale (the assumption now in doubt).
- **likelihood**: occurred once; mechanism unlocalized
- **impact**: high — C2 (code anchors) is blocked as designed; promotion
  framework waits on the alternative
- **mitigation**: pursue the data-segment correspondence alternative
  (in flight, `worker-c2-alt-correspondence`: file bytes of retained
  `decrypted-eboot.bin` vs live DATA-segment bytes, no exec-span reads,
  exclusions for relocation-dependent/runtime-modified/self-copied bytes,
  readable mappings verified in-invocation); keep Tick/SCO criterion
  explicitly unresolved, never silently substituted; any future probe that
  must touch exec pages needs a NEW falsifiable design and user
  coordination.

### R-FTP-02 — FTP caching can invalidate the readback oracle (task-named)
- **area**: console staging channel (ftsrv RETR verification path)
- **risk**: every staging package (MASTER-STAGING-PACKAGE, INTEGRATION-
  CHECKLIST STEP 8) uses per-file FTP RETR readback as the staging oracle.
  If any layer between RETR and the console disk caches, a readback can
  serve STALE bytes: a failed/absent STOR "verifies" against the previous
  file's content (false PASS), or a good staging shows a hash mismatch
  (false NO-GO inviting re-pin-to-match — the exact hazard R-E2E-FR3
  warns against). Distinct from R-FTP-01 (segment-covered-prefix law,
  which is deterministic and understood); this one is nondeterministic
  staleness.
- **evidence**: task-named by the controller brief today; workspace anchors:
  RETR-readback-as-oracle in `worker-console-staging-bundle/
  MASTER-STAGING-PACKAGE.md` and INTEGRATION-CHECKLIST STEP 8.4; freshness
  rituals already exist because stale-serving bit this session
  (STAGE-PLAN-IGF6 "rename stale JSONL"; MASTER-STAGING-PACKAGE "freshness
  reset"); igf5 diagnosis showed console-side caches can lose/hold data
  unsynced (page-cache loss on kernel panic — worker-igf5-diagnosis-
  rebuild Part 1). No caching mechanism has been demonstrated — this row
  exists so the oracle never silently depends on one.
- **likelihood**: open (mechanism unverified; consequences observed once as
  staleness symptoms)
- **impact**: medium-high on staging honesty (oracle trust), low on safety
- **mitigation**: after STOR, verify readback size + sha256 against THIS
  attempt's receipts; treat any mismatch as channel ambiguity, never
  re-pin to match a readback; keep mtime/freshness oracle primary for
  launch evidence; prefer unique per-attempt destination names (suffix or
  new path) for high-stakes payloads so a cache cannot alias generations;
  if a mismatch repeats, probe caching explicitly before staging anything
  else.

### R-COMP4-01 — composition-4 bootstrap-replacement architecture defect
- **area**: runtime recomposition (`build_runtime_composition4.py`
  `--replace-runtime-unit` pin walk / bootstrap units)
- **risk**: composition-4 FAILED at a delivered-script gate: the fresh-unit
  dependency pin walk rejects the staging initializer because the
  replace-receipt map `rr` is consulted only AFTER the loop (script line
  78 vs 83–89), and `--replace-runtime-unit` sources can never pass unless
  they live under the spike tree's `W/src|include` globs — while the
  delivered design deliberately stages them under
  `worker-query-s2-arm/staging/` to keep the spike tree clean. Net:
  no composition-4 runtime object exists; packaging and every downstream
  console step are blocked; the harness + S2 arm + ack supplier units are
  compiled and attested but uncomposed. Follow-on diagnosis
  (`worker-composition-4-fix/CLAIM.json`, in flight): bootstrap units
  (`native-runtime-owner`, `ue4ss-backend`, …) live in
  `base['bootstrap_units']` and need a parallel `--replace-bootstrap-unit`
  path with link-input substitution — the architecture gap is wider than
  the one-line hoist.
- **evidence**: `worker-composition-4/REPORT.md` (full traceback, root
  cause, salvaged run-1 unit `1a8bed2a…` with the 5 QueryHarness symbols,
  no-retry rationale, required-reconciliation spec); script hash-attested
  `7a83cb38…`; run-1 preserved failure artifact (`out.mkdir(exist_ok=
  False)` blocks blind replay).
- **likelihood**: certain until fixed (deterministic script gate)
- **impact**: high — blocks the entire compose→package→stage→arm→query
  ladder (R-ACK-01, R-GT-01, INTEGRATION-CHECKLIST STEPS 6–14)
- **mitigation**: `worker-composition-4-fix` owns the fix (new script copy,
  new hash, `--replace-bootstrap-unit`, fresh run dir run-2). After it
  lands: independent receipt review (hash attestation, undefined-delta
  must equal `[sceKernelGetModuleInfo, clock_gettime, getpid]`, PRX
  packaging + 20/20 static checks) before any staging decision. Never
  bypass the pin gate; never mutate frozen inputs to satisfy a glob.

### R-TMP-01 — /tmp as report/probe sink is a recurring failure pattern
- **area**: worker evidence hygiene (host /tmp; console-side sinks)
- **risk**: workers keep placing outputs/probe artifacts under `/tmp`,
  which is volatile and outside the write-scoped receipts tree. Today it
  recurred: `worker-fname-fc1-prep/REPORT.md` records a /tmp survey-reprobe
  output deleted within the same task ("Unblocking is a …" recovery) and
  re-states the older INITONCE lesson verbatim: "sink must be /data".
  Historical occurrences: argv-probe and the emission-runbook negative
  control also stage through `/tmp` paths. Every /tmp artifact is a
  receipt that can vanish before reconciliation, and console-side /tmp
  sinks silently lose evidence across sessions.
- **evidence**: `worker-fname-fc1-prep/REPORT.md` lines ~56 and ~71;
  `/tmp` occurrences in `worker-argv-probe/REPORT.md`+`PROBE-PLAN.md`,
  `worker-staged-pair-emission/EMISSION-RUNBOOK.md` (`/tmp/nc.ini`),
  fileio selftest READMEs.
- **likelihood**: certain recurrence (observed in at least three lineages)
- **impact**: low-medium per incident (lost/repro-gaps in evidence),
  compounding across a session
- **mitigation**: standing rule — evidence sinks are owned `outputs/…`
  dirs on host and `/data/…` on console, never `/tmp`; /tmp allowed only
  for byte-shreddable intermediates re-derivable from receipts; new task
  briefs carry the rule; cheap lint: grep new REPORTs for `/tmp` sink
  claims at review time.

---

## Reading order for the next refill dispatch (supersedes V1 order)

1. R-COMP4-01 — composition-4-fix terminal review, then compose→package
   (unblocks the whole arm/query ladder).
2. R-D1-01 + R-R2C-01 — console staging of the relinked 51-tuple payload
   (host-side done; controller STOR per STAGING-MANIFEST-REPIN +
   MASTER-STAGING-PACKAGE Gate A0).
3. R-EXEC-01 — C2 alternative terminal review when
   `worker-c2-alt-correspondence` lands; keep Tick/SCO unresolved.
4. R-ARGV-01/R-G1-02 + R-G3-01 — E2E entry + intake gates at the console
   session (config-file entry, Gate A0 probe).
5. R-SER-01 successor — mutex+fileio staged console session design.
6. R-LOG-01 — real-log parser validation; R-PAD-01 — pad hardware test.
7. R-IDENT-01 residuals + R-LAYOUT-01 rung-3 (post-harness-milestone).
8. Queued: R-CRT-01, R-MUT-01, R-BOUND-01, R-CONTC-01, R-ACT-01.

Standing channel risks that still bound ALL console work: R-E2E-FR3 (sha
identity), R-FTP-01 (readback law), R-FTP-02 (caching), R-PLD-01 (record-
stream truth), R-RW-01 (host wedge amplifier).
