# REPORT — worker-game-session-ridealongs (2026-10-04)

Staging preparation only. NO console contact, no nested subagents, writes
confined to `outputs/2026-10-04-zcode-team/worker-game-session-ridealongs/`.
Deliverable order honored: CLAIM.json first, STAGE-PLAN-COMBINED.md,
VERIFICATION-RECEIPTS.json, this REPORT last.

## 1. What was done

1. **Artifact verification (task 1) — ALL PASSED.**
   - qdrv1 `modbridge_qh_driver.prx`: 26157 B, sha256 `52c32216...df70a4` —
     matches STAGE-PLAN-QDRV1 pin and build-receipt.json; top-level copy
     cmp-equal to the determinism/run1 staged copy. Re-ran the worker's own
     `check_qdrv_artifact.py` on BOTH determinism runs: rc=0
     LOCAL_PACKAGE_CHECKS_PASSED each (fself container law, init/fini 1/1,
     SDK 0x11000000, 11-row decoded import equality, forbidden-identity
     gate). Its bare-sdk sha equals the receipt's `inner_elf_sha256`.
   - observer `modbridge_runtime_aligned_observer.prx`: 30152151 B, sha256
     `1afa0997...11e30` — matches worker-observer-packaging CLAIM.json;
     cmp-equal to build-1. Re-ran `check_observer_artifact.py` on BOTH
     build-1 and build-2: rc=0 each (init/fini 232/2 == frozen lineage,
     191-row frozen-contract exact equality, combined object == frozen
     recipe, build2 byte-identical). Invocation note: its argv[1] is
     relative to the checker's own directory.
2. **ploader.ini reconciliation (task 2) — parser-proven.**
   - Pinned source facts: MAX_PLUGINS=10/title, MAX_TITLES=10, paths <=255;
     parse_mode accepts only 0/1/2; **mode 0 is skipped at parse and
     consumes no slot**; mode 2 = late (stable 1 s, settle 0.6 s, 25 s
     cap/plugin, 120 s overall); injection order == file order; **no path
     dedup** (duplicate lines inject twice).
   - Combined 6-entry layout verified by `simulate_ploader_ini.py`
     (SIMULATION_PASSED, 8 variants): 5 registered plugins (mode-0 line
     inert), comfortably within every limit.
   - Decisions: `ingame_facts = 0` STAYS (parser-inert; keeps qdrv1's jsonl
     single-writer — satisfies STAGE-PLAN-QDRV1 sec 1 without touching an
     existing byte); `path_test = 2` STAYS (one-shot freshness oracle,
     non-interfering; 5/10 registered with 6 total lines); BOTH new entries
     appended `= 2` AFTER the composition line (composition ladder starts
     first, ridealongs load last). Because the exact console bytes were
     never recorded host-side, the plan mandates a Step-0 RETR pre-image and
     an append-only edit (never recreate the file).
3. **Interaction audit (task 3) — verdict: NO interference.** Per-module
   I/O cited from their sources (see VERIFICATION-RECEIPTS.json):
   UE4SS.log has append-only writers (UE4SS; path_test 2 lines once;
   composition failure-paths only) and exactly one reader — the observer
   (stat + O_RDONLY pread <=512 B tail, shrink-safe, never writes, no
   lseek). `/data/modbridge-ingame-facts.jsonl` has exactly one active
   writer (qdrv1; igf6 is mode-0-skipped; composition provably never writes
   it). runtime-status.txt, the aligned-entry log, and the observer log are
   disjoint single-writer paths. One worker thread each (observer 30x2 s,
   qdrv1 30x4 s), nanosleep-cadence, no mutex imports on either surface;
   qdrv1's admission gate makes pre-engine start safe by design. ploader
   costs ~10-15 s for 6 sequential injections vs a 120 s budget; the
   pre-existing `all_ok` re-injection quirk is documented with mitigation
   (stop on any FAIL line; never relaunch an identical rejected layout).
4. **STAGE-PLAN-COMBINED.md (task 4).** Exact upload+readback for both PRXs
   under the pinned SHT-strip readback law with computed constants
   (qdrv1 readback 87112 B / body sha `71406d3c...`; observer 30143976 B /
   body sha `3f89a5b6...`), the append-only ini edit + verify procedure,
   rollback, the 5 expected evidence files with success signatures, and a
   machine-consumable combined-staging step table (SC-0..SC-8 + verdict
   ledger names) that extends worker-console-batch-runner's `stage-qdrv1`
   — that worker's files were NOT edited.

## 2. Extra verification beyond the ask

The readback verifier snippet published in the plan was itself validated
host-side: simulated ftpsrv output (bare prefix with e_shoff/e_shentsize/
e_shnum/e_shstrndx zeroed, per the pinned make_fself behavior) passes
READBACK-OK for both modules with body shas exactly equal to the pinned
constants; a single flipped payload byte correctly fails (rc=1,
body=MISMATCH). Simulation also surfaced and documented the duplicate-line
injection hazard (ploader has no dedup), now guarded by an explicit
count-check gate in SC-0/SC-5.

## 3. Honesty boundaries

No console contact was made; nothing on a console is claimed or changed.
The ini "expected content" is a shape reference only — the RETR pre-image
is authoritative. Rung verdicts, observer results, and composition behavior
all remain claims for the actual session; this worker only proves local
bytes, parser semantics, and non-interference from source evidence.
