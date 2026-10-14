# REPORT — worker-session-audit (re-dispatch, 2026-10-04)

Worker: worker-session-audit (re-dispatch with correct absolute workspace after the
capacity-trial 1302 death; prior instance left no output). Workspace:
`/WORKSPACE/.codex/worktrees/translation-core/what`. Read-only audit; writes
confined to this directory. No nested subagents, no network. Tool budget used: 14/25.

## Method

1. Read `outputs/2026-10-04-zcode-controller/ZCODE-FORK-HANDOFF.md` in full.
2. Enumerated every `zcode_*` entry in
   `.superpowers/native-observations/transport-experiments-progress.json` (517,183 B),
   with chronological ordering, and dumped the entries the handoff cites plus all
   post-close fork-session entries (capacity trial, span-arbitration, parser, crt,
   zip-bridge-console-link, vaddr-crosscheck, roster, takeover, v1/v2 results).
3. Inventoried all 53 dirs under `outputs/2026-10-04-zcode-team/` for CLAIM/REPORT
   presence and read the ambiguous CLAIMs.
4. Recomputed hashes (`shasum -a 256`) for the staged v2 PRX, the v1/v2 result copies,
   and the post-fork v3 PRX; read `outputs/CURRENT-PS5-TEST/` (CURRENT-BUILD.md,
   README.md) and the controller `console/` receipts.

## Result summary (details + exact corrections in findings.json; verdict in VERDICT.md)

### 1) Handoff vs cited checkpoint entries

Verified accurate: bias-hypothesis correction (single-load-bias reopened/co-official,
LOAD7 arbitration requirement), proven segment facts (LOAD0=0x400000, Tick 0x33ac8c0,
SCO 0x890aa0, GUA delta 0x27e110, gap, 0x9633000 caveat; 0x953c000 arithmetic correct),
v1/v2 in-game facts + launch ritual, v3 amendment content as far as its two citations
go, ps5io-rdwr (ELF 1197880 B confirmed on disk), zip-to-mfb 75/75, mapping-recovery
55-span headline.

Mismatches flagged:
- M-1 (high): seat "max 10" vs `user_decision_final` 12 incl. controller (18:12Z).
- M-2 (high): in-flight roster line still lists mapping-recovery / zip-to-mfb /
  ps5io-rdwr as re-dispatch candidates though all three are recorded DELIVERED
  (zip-to-mfb explicitly "NOT orphaned").
- M-3 (high): v3 brief cites only v2-design + real-input + vaddr relay; it predates
  `zcode_worker_span_arbitration` (kve_structsize=144 stride, full-count cap, P1–P6,
  57/52/5 validation void, 1243-reader-slots/10017-real-records) and the ADOPTED
  ingame-log-parser. Handoff-alone v3 rebuild would use the wrong stride/cap.
- M-4 (medium): "console link of the bridge TU pending" superseded by
  `zcode_worker_zip_bridge_console_link` (payload elf cfa96ab5…, 116 imports, 75/75).
- M-5 (medium): "each has CLAIM.json + REPORT.md" false for 13/53 dirs.
- M-6 (low): v3 has since been BUILT post-fork (52205 B, sha256 1dfbf3d5…,
  determinism run1==run2) but has no checkpoint entry, no STAGE-PLAN-V3/REPORT, and
  is not staged/launched.

### 2) Checkpoint self-consistency

- C-1 (high): `zcode_worker_mapping_recovery` superseded by span-arbitration
  corrections with NO amendment marker; its "counters bound the recovery exactly
  (50/48/2 of 57/52/5)" and "High family … not trusted" claims are voided/arbitrated.
  Fix: add a `controller_correction_2026_10_04` field pointing at
  `zcode_worker_span_arbitration.corrections_to_recovery` (pattern already used on
  vaddr-semantics).
- C-2 (medium): `zcode_worker_8_app_backend` still IN_FLIGHT (15:56Z) while the close
  says status-unknown and the fork redo is documented only in its dir CLAIM.json.
- C-3 (medium): v2 status "56_RECORDS" vs 63 lines on disk and parser acceptance
  "63/63 repaired" — reconcile and restate.
- C-4/C-5 (low): roster (budget 10, "keep 9/10") and `zcode_ue4ss_critical_path`
  (S1_PROBE_DISPATCHED) are stale snapshots superseded the same day, unmarked.
- C-6 (medium): six trial-wave dirs (composition-prep, e2e-console-staging, f-r2-fix,
  launch-automator, uninstall-fault-expansion, fname-fc1-prep) sit in the team tree
  with no checkpoint entry and no partial/blocked marker.
- C-7 (info): vaddr-crosscheck re-queue still owes a delivery entry.

### 3) Team-dir inventory

53 dirs: 40 complete CLAIM.json+REPORT.md. 13 incomplete — 7 legitimately marked
(worker-8 redo, worker-9/composition-review/initonce-review reviewer convention,
crt-startup-design and ingame-log-parser CLAIM.md-with-adoption, fileio-prep
session-close partial), 6 unmarked (listed under C-6). Blocked-wrong-tree deliveries
now adopted into the authoritative tree: **worker-ingame-log-parser** and
**worker-crt-startup-design** (both adoption notes present in their entries).
**worker-vaddr-crosscheck** (blocked) is correctly absent; re-queue open.

### 4) Console artifact states

- Staged ingame probe hash: MATCH —
  `worker-ingame-probe-v2/build-764f957dc0a6/modbridge_ingame_facts.prx`
  sha256 94c10408055c6e6f4d403d1c66f4a8da399fdbae0ef0eb945e1faad16c23b09d, 30701 B,
  equal to `zcode_ingame_v1_results.v2_staged`.
- Result copies: MATCH — v1 `ac59d843…`, v2 `6538d368…` equal their
  `result_copy_sha256` values.
- CURRENT-PS5-TEST: CONSISTENT — CURRENT=INITONCE_DLL_TEST.elf (1a6b37b7) passed,
  matching `zcode_worker_4_integration` target pass; NEXT_DLL_THREADS (6d295f56) and
  TEST_THIS_DLL (f72faea5) history matches `zcode_takeover_2026_10_04`. One stale
  doc: `README.md` in that dir still presents TEST_THIS_DLL.elf as current — update
  before the next user-facing launch instruction.
- Post-fork v3 PRX exists but is unrecorded and unstaged (see M-6).

## Honest limits

- I did not re-run builds, parsers, or any worker tooling; all "match" claims are
  hash/size/text recomputation against recorded values.
- The 56-vs-63 v2 discrepancy is flagged, not adjudicated — deciding whether 56 counts
  experiment records vs 63 envelopes requires reading the v2 jsonl semantics, which I
  deliberately left to the controller with the exact reconciliation instruction.
- Timezone derivation (local = UTC−5) was inferred from file mtimes vs entry utcs and
  used only for consistency color, never as load-bearing evidence.
- Entry bodies I did not dump (e.g. full `zcode_worker_3_gamethread`, container
  details) were out of scope for the four named audit targets.

Deliverables in this directory: `CLAIM.json` (this engagement record), `findings.json`
(11 verified-correct items, 6 mismatches M-1..M-6, 7 consistency items C-1..C-7, full
dir inventory, console artifact results — each with exact correction text),
`VERDICT.md` (one-page verdict), and this `REPORT.md`.
