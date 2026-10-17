# VERDICT — worker-session-audit (2026-10-04, re-dispatch)

Scope: read-only drift audit of `outputs/2026-10-04-zcode-controller/ZCODE-FORK-HANDOFF.md`
against `.superpowers/native-observations/transport-experiments-progress.json` (`zcode_*`
entries), the team-dir inventory, and the console artifact states. No files outside
`outputs/2026-10-04-zcode-team/worker-session-audit/` were written.

**Verdict: HANDOFF SUBSTANTIALLY ACCURATE — 6 drift items require exact corrections
before the next v3 re-dispatch or any new controller acts on it. No fabricated
evidence found; every hash I could recompute matched.**

## What checks out (evidence-verified)

- Bias-hypothesis correction: handoff faithfully relays
  `zcode_worker_vaddr_semantics.controller_correction_2026_10_04` — single-load-bias
  REOPENED and co-official; 0x97ba110 == 0x93ba110+0x400000 single-bias-consistent;
  LOAD7 correspondence evidence required for arbitration. The prior "single global
  bias DISPROVEN" wording is correctly downgraded in the handoff.
- PROVEN SEGMENT FACTS match the vaddr-semantics verdict byte-for-byte in substance
  (LOAD0=0x400000, Tick=0x33ac8c0, SCO=0x890aa0, GUA delta 0x27e110, gap, L7/L8
  0x9633000 caveat). Handoff arithmetic 0x97ba110−0x27e110=0x953c000 re-verified.
- v1/v2 in-game facts, launch ritual, v3 amendment content (candidate-discovery-only,
  multi-anchor promotion, comma-correct JSONL, total/retained/truncated, 7 verbatim
  imports, igf3- build id) all match their cited entries.
- ps5io-rdwr: ELF exists at the stated path, exactly 1197880 bytes; F1/123-import/0-missing
  claim matches its entry. zip-to-mfb 75/75 F-R1 claim matches.
- mapping-recovery headline (2 trusted + 53 plausible = 55, dense gap) matches.
- Console artifacts MATCH: staged v2 PRX sha256 94c10408…30701 B ==
  `v2_staged.prx_sha256/bytes`; ingame-v1/v2 jsonl copies match
  `result_copy_sha256` exactly; CURRENT-PS5-TEST chain (TEST_THIS_DLL →
  NEXT_DLL_THREADS → INITONCE_DLL_TEST current/passed) is consistent with
  `zcode_takeover_2026_10_04` and `zcode_worker_4_integration`.

## Drift to fix (exact corrections)

1. **Seat (M-1, high):** handoff says "max 10 open requests"; the later
   `zcode_flash_capacity_trial_1.user_decision_final` (18:12Z) sets **12 total
   INCLUDING the controller**, paced refill. Update the handoff seat line; amend
   `zcode_worker_roster` (budget still 10).
2. **Stale in-flight roster (M-2, high):** handoff still lists mapping-recovery,
   zip-to-mfb, ps5io-rdwr as "in flight at fork / re-dispatch if orphaned" although
   the same file and the checkpoint mark all three DELIVERED (zip-to-mfb explicitly
   "NOT orphaned"). True orphans: ingame-probe-v3 (since built), fileio-prep,
   worker-8 redo, mutex host rebuild.
3. **v3 brief misses span-arbitration + parser (M-3, high):** the handoff v3 section
   predates `zcode_worker_span_arbitration` (18:24Z). Its `v3_relay` (kve_structsize=144
   stride, full-count cap, P1–P6) and `corrections_to_recovery` (1024-kept was
   IG_SPAN_MAX; 1243 reader slots vs 10017 real records; 57/52/5 validation VOID) must
   be added, plus `zcode_worker_ingame_log_parser` (ADOPTED standard collection tool).
   Building v3 from the handoff alone would reuse the wrong stride/cap.
4. **Zip console link superseded (M-4, medium):** "console link of the bridge TU
   pending" is false since `zcode_worker_zip_bridge_console_link` (18:15Z): payload
   elf cfa96ab5…, 681632 B, 116 imports, missing=[]. Remaining: staging+ritual,
   stage-1 native download merge, on-console ack, F-R2/v1 bounds.
5. **Blanket deliverable claim (M-5, medium):** "each has CLAIM.json + REPORT.md" is
   false for 13 of 53 dirs (reviewer dirs use VERDICT.md; 6 trial-wave dirs are
   partial/unmarked — see findings.json). Reword and mark.
6. **v3 built but unrecorded (M-6 + C-1..C-6):** post-fork v3 PRX exists (52205 B,
   sha256 1dfbf3d5…, determinism run1==run2) with no checkpoint entry and no staging.
   Checkpoint also needs: amendment marker on `zcode_worker_mapping_recovery` pointing
   at span-arbitration corrections (C-1), worker-8 stale IN_FLIGHT (C-2), 56-vs-63 v2
   record-count reconciliation (C-3), roster/critical-path staleness (C-4/C-5), and
   markers for the six unmarked blocked/partial dirs (C-6).

## Team-dir inventory (53 dirs)

40 complete (CLAIM.json+REPORT.md). 13 incomplete, of which 7 are legitimately marked
(reviewer convention, adopted, or session-close partial) and 6 are unmarked
(composition-prep, e2e-console-staging, f-r2-fix, launch-automator,
uninstall-fault-expansion, fname-fc1-prep). Adopted copies from wrong-tree blocked
deliveries: worker-ingame-log-parser and worker-crt-startup-design — both carry
explicit adoption notes in their checkpoint entries. worker-vaddr-crosscheck (blocked)
is correctly absent from the authoritative tree; its re-queue still owes a delivery.

## Console artifacts

All recomputed hashes match the checkpoint. One stale local doc:
`outputs/CURRENT-PS5-TEST/README.md` still tells the user to run TEST_THIS_DLL.elf as
"current"; CURRENT-BUILD.md (INITONCE_DLL_TEST.elf) is authoritative. Low-risk, but
update or delete the stale paragraph before the next user-facing launch instruction.

Honesty note: this audit recomputed hashes and re-derived arithmetic; it did not
re-run any worker tooling. Full per-item evidence and exact correction text:
findings.json and REPORT.md in this directory.
