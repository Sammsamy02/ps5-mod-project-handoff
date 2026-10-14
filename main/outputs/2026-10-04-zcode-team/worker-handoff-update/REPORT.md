# REPORT.md — worker-handoff-update, 2026-10-04

Task: replace the severely stale fork handoff
(`outputs/2026-10-04-zcode-controller/ZCODE-FORK-HANDOFF.md`, which still said "v3 PROBE
was IN FLIGHT at fork") with a complete rewrite reflecting the evening state. The
original file is read-only per task rules; the replacement was produced here. The original
was not modified.

## Deliverable

`ZCODE-FORK-HANDOFF-V2.md` (same directory) — the replacement fork handoff for any future
session continuation. It opens by declaring the V1 picture obsolete (v3 in-flight,
single-bias open, LOAD7 missing, fileio-prep pending, mutex host re-run — all resolved)
and is organized as: standing authorization -> one-paragraph state -> critical path ->
proven facts -> launch ritual -> integration queue -> console safety rules -> honest
limits -> honesty rules.

## What changed vs V1 (all sourced from checkpoint entries + worker deliverables)

- **Critical path**: rewritten around the two decisive deliveries. C2 alternative
  (`zcode_worker_c2_alt_correspondence`, 21:37:54Z): spec complete, 5 immutable
  file-backed candidates with runtime addresses, relocation-checked (1,663,596 r_offsets,
  zero in rodata/aux), explicit pass/fail criteria, one probe launch needed — this is the
  "in-flight" half. Composition-4 (`zcode_supplier_composition_4`, 21:39:34Z): composed,
  ack-enabled, harness-armed, 17/17 static, gaps `[]` (receipt verified on disk);
  promotion imminent with both in hand.
- **Proven facts**: single-bias CONFIRMED (base 0x400000), full v3 segment map with the
  LOAD7 exact span-start match at 0x953c000, GUA live-validated at 0x97ba110 (71,959
  objects, read+write, predicate bug identified as a checker bug and fixed), identity
  (5.6.1-0+++UE5 / 04bb4330…) and axes (SHIPPING 60 / STATS OFF 12) resolved, staged
  layout pair ParsePair-verified, and all three DLL families (InitOnce, Mutex, FileIO)
  target-proven via the 21:27Z mutex and 21:33Z fileio console passes with build ids,
  hashes and phase results cited.
- **Launch ritual**: kept the empirical core (game closed -> ploader arm -> records ~1 min
  after launch; ftpsrv FSELF auto-decrypt oracle) and added the day's three lessons: FTP
  caching (v3's 1,035 records invisible until the module's write flushed the file — a
  stale/missing readback is not proof of non-execution; R-FTP-02 hygiene summarized),
  ploader re-arm after crashes (no re-dispatch while unreconciled; read-only first
  contact; igd1 known-good path check; per-launch mode-2 entry re-send), and `/data/
  modbridge` existence (fileio stage-41 failure fixed by `FTP MKD /data/modbridge`).
- **Integration queue**: rebuilt around the master staging package — 9-file manifest
  (worker-console-staging-bundle, hashes recomputed on disk) rehearsed READY TO EXECUTE
  with zero corrections by worker-final-staging-rehearsal; Gate A0 STOR write-probe stays
  the first live command; the composition-4 §C contingency is flagged as now LIVE (the
  rehearsal's G3 caveat predates the 21:39Z composition-4 PRX delivery). Query-harness
  apply (40/40), rung-3 sequence (111/111 prepared, host probe 20/20 clean but never run
  in-game), DLL/installer/app/container/CRT follow-ons, and explicit closure of every V1
  queue item (v3 re-dispatch, fileio-prep, composition-2/3, mutex host validation).
- **Console safety rules**: distilled from worker-console-safety's standing procedure —
  gates, one-console-actor, 8-point pre-dispatch checklist, dispatch discipline (30s
  floor, timeout=failure, never blind replay), monitoring (expected-evidence table first,
  records-only truth, zero-records=stop), and the condensed 12-prohibition table.
- **Honest limits**: Tick/SCO unresolved (mapping-only; exec-span reads blocked; igf6
  mechanism explicitly NOT localized per user amendment), C2 scope limited to image-level
  correspondence, rung-3 reads pending, no staged launch ever performed (Gate A0
  unproven), nothing promoted (GUA = STRONG_LIVE_CANDIDATE_EVIDENCE), composition-4
  static-clean but never loaded, igf5/argv residuals open, gameplay remains a prototype.

## Method and bounds

10 tool calls total (well under the 30-call budget). Sources: the V1 handoff,
worker-checkpoint-reconcile/CURRENT-STATE.md, the eight named checkpoint entries plus the
full newest-entry tail (21:26Z–21:48Z) extracted from
`.superpowers/native-observations/transport-experiments-progress.json`, the console-safety
procedure, the master staging package, the final staging rehearsal result, and the
composition-4-fix PRX receipt (`gaps: []` verified). No network, no console, no nested
subagents; writes confined to this directory. CLAIM.json was written first, this report
last. All numbers (addresses, hashes, byte sizes, timestamps) are copied from the cited
records, not paraphrased from memory; where only a hash prefix exists in the checkpoint
(composition-4 PRX `de372302…`), it is recorded as a prefix with a pointer to the full
receipt.

## Residual caveats for the controller

- The staging rehearsal was assembled before composition-4's PRX landed; the V2 handoff
  resolves that with the manifest's own §C contingency, but the first staged launch must
  still re-verify every hash at execution time (the manifest mandates this).
- Whether composition-4's included ack units fully retire the composition-3-era
  "controller outstanding" debts (pinned-values patch, RuntimeAckConfig supplier) is
  flagged for verification in the queue, not asserted.
- The handoff deliberately does not advance any promotion state: promotion wording
  follows the checkpoint exactly ("imminent", pending staged launch + C2 evaluation).
