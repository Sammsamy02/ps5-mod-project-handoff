# ZCODE FORK HANDOFF — 2026-10-04 (session continuation; subagents now GLM-5.3-Flash)

You are the new controller for this PS5 mod-manager project. Authoritative workspace:
/WORKSPACE/.codex/worktrees/translation-core/what
Read STATE from .superpowers/native-observations/transport-experiments-progress.json — every
component this team produced is a `zcode_*` entry with hashes and evidence. Controller files:
outputs/2026-10-04-zcode-controller/ (CONTROLLER.md, CONSOLE-LOG.md, console/ receipts).
Worker deliverables: outputs/2026-10-04-zcode-team/<slug>/ (each has CLAIM.json + REPORT.md).

## Standing authorization (user-granted)
- Console (ps5-host-not-configured.invalid: pldmgr HTTP 8084, FTP 2121): upload/run project tests, collect logs,
  reversible mod ops; game-file changes need backups + restoration path. Never repeat an
  already-passed diagnostic without changed implementation or a specific open question.
- Seat: max 12 open requests total INCLUDING the controller (user decision 2026-10-04 18:12Z). Deep budgets: 100-150 calls for Flash workers. Workers now run Flash (user set).
  Refill completed slots; no nested subagents; honest host-vs-target evidence always.

## CRITICAL PATH STATE (UE4SS acceptance ladder: game-thread callback -> object query ->
## reflected access -> unchanged Item Spawner -> visible spawn via real input)
- In-game probes v1 and v2 BOTH ran inside Townfall (records: console/ingame-v{1,2}-facts-*.jsonl).
  Module loading + all imports proven; module quiesces inert.
- PROVEN SEGMENT FACTS (worker-vaddr-semantics, byte-cited): survey values are ELF p_vaddrs;
  mapped_start(LOAD0)=0x400000 (EXEC span == LOAD0 memsz byte-exact) => runtime
  Tick=0x33ac8c0, StaticConstructObject=0x890aa0 (validated in-span). Single-load-bias hypothesis OPEN (controller correction: 0x97ba110 == 0x93ba110+0x400000 is single-bias-consistent; LOAD7 correspondence evidence required for arbitration). GUA 0x93ba110 lives in LOAD7 BSS => runtime = mapped_start(L7)+0x27e110;
  LOAD7 mapping not yet found; remaining LOADs plausibly in gap [0x787c000,0x8018c000).
  L7/L8 share page 0x9633000 (caveat in SEGMENT-CORRESPONDENCE.md).
- v3 PROBE was IN FLIGHT at fork (may be orphaned — re-dispatch if no delivery arrives):
  build from outputs/2026-10-04-zcode-team/worker-ingame-probe-v2/ with the amendments in
  checkpoint entries zcode_ingame_v2_results.v3_design + zcode_worker_real_input.v3_amendments_recorded
  + the vaddr relay (this file's PROVEN SEGMENT FACTS). Amendments: writer/stride fixes with
  total/retained/truncated counts; comma-correct JSONL; GUA predicate = CANDIDATE DISCOVERY ONLY
  (preserve all matches, explicit ambiguity, promotion requires independent multi-anchor
  correspondence incl. Tick/SCO prologue byte-checks at 0x33ac8c0/0x890aa0 + segment extent
  consistency); primary target = find mapped_start(L7) in the gap, probe L7+0x27e110;
  both translation hypotheses recorded; 7 verbatim imports only; igf3- build id; same
  log path/module name (replaces console file, no ini change).
- LAUNCH RITUAL (empirically nailed): game fully CLOSED -> ploader armed (user clicks
  ploader.elf in Payload Manager, or GET /loadpayload:/data/pldmgr/payloads/ploader/ploader.elf)
  -> user launches Townfall -> records appear ~1 MINUTE AFTER LAUNCH at injection completion
  (ploader shows "injecting 2 plugins", then OK/FAIL ~1min in). Freshness oracle:
  /data/townfall_runtime_aligned_entry.log mtime. ftpsrv auto-decrypts FSELF on RETR
  (decryption-oracle verification; full-hash equality impossible for fself).

## NEW (post-fork-write): worker-mapping-recovery DELIVERED
- 55 recovered spans (2 trusted incl. both known images; 53 plausible) as outputs/2026-10-04-zcode-team/worker-mapping-recovery/vmmap-v3-records.jsonl (drop-in cross-validation for v3).
- The gap [0x787c000,0x8018c000) is DENSE with hidden/corrupted data segments; falsifiable prediction: L7 covers ~0x97ba110 with a W span (GUA at mapped ~0x97ba110 => L7 base ~0x953c000 + check against SEGMENT-CORRESPONDENCE.md extents).
- High family >36-bit VAs flagged for arbitration.

## SESSION-CLOSE ADDITIONS
- worker-ps5io-rdwr DELIVERED at close: F1 closed (RDWR=SDK O_RDWR 0x0002), native download ELF LINKED (worker-ps5io-rdwr/native-link/modbridge-installer.download.elf, 1197880 B / 123 imports / 0 missing); only signed-in owner session + one-job-per-process remain for console stage 1.
- Agents stopped at user fork: v3 (MID-BUILD, re-dispatch with amendments), fileio-prep (mid-task, re-dispatch), mutex host build (killed mid-driver, re-run with watchdog). worker-8 status unknown (check its dir first). zip-to-mfb DELIVERED after close: F-R1 closed, archives install E2E on host 75/75 (console link of the bridge TU pending) — no re-dispatch needed.

## Integration queue (controller-owned)
1. On v3 delivery: verify determinism, FTP-stage (same filename), one launch, analyze vs the
   segment table; if a candidate promotes through multi-anchor correspondence -> first
   validated layout facts -> assemble staged pair via worker-layout-minimum assembler ->
   query-harness rung-1->2.
2. Recompose UE runtime with worker-supplier-ingame-update patches (VMMAP-primary; +1 extra
   unit: base-scan TU) via build_runtime.py --extra-unit x4 --extend-undefined-baseline
   (mechanism reviewed ALL_CLEAR; attestation mandatory; provider hashes pinned).
3. Mutex: fixes applied (fixture token retirement + combined_close_handle dispatch); host
   validation was re-running under watchdog at fork — check
   .superpowers/native-dependency-spike/windows-mutex-candidate-1/ latest receipt; then
   compose + target run (non-blocking for game path).
4. Apply worker-fileio-prep diffs to the production builder when delivered; same for
   worker-zip-to-mfb (archive->bundle bridge) and worker-ps5io-rdwr (F1 + native link prep).
5. Console E2E staged run (0/2a/2b/3/4-5/6 per worker-e2e-rehearsal READINESS.md GO conditions).
6. package_prx.py --link for the supplier-composed PRX when builds serialize.

## Fork worker status: mapping-recovery/zip-to-mfb/ps5io-rdwr ALL DELIVERED. v3 PRX BUILT
## (igf3-5e5b6fcf83cd, determinism verified, corrections applied; see checkpoint zcode_v3_prx_interim).
## fileio-prep READY_TO_APPLY. Post-fork completions: F-R2, zip-bridge-console-link, composition-2,
## span-arbitration, session-audit, CRT design (adopted+census filled).

## Honesty rules in force
Candidate != validated. No guessed offsets/widths (width table gates). FName construction is
its own gap (P2). Predicate hits never auto-establish identity/base/layout. Unperformed runs
are never reported as passed. Raw provenance preserved; corrections recorded as amendments.
