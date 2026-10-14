# REPORT — worker-final-ini-layout

Worker: worker-final-ini-layout, 2026-10-04. Task: own THE definitive
return-session ploader.ini layout. Status: COMPLETE (proposal-only; no
console contact; no files of other workers touched).

## Delivered (in this directory)

| File | sha256 | What |
|---|---|---|
| `CLAIM.json` (first) | `4f101ede…` | ownership, pinned inputs, layout, verification |
| `FINAL-INI-LAYOUT.md` | `c724a36e…` | the definitive layout + ORDERING CONTRACT (§3) |
| `ploader.ini.canonical` | `bbc34c16621ad337632d0ff29a116b0aa6e86e45ad219e8ef7539b369a5a6fe1` (400 B) | the exact target bytes for `/data/plugins/ploader.ini` |
| `verify_final_ini_layout.py` | `56a8ec17…` | final-layout verifier (byte-exact + parser law + structured diagnosis) |
| `simulate_ploader_ini_final.py` | `8cf09984…` | extended ridealongs simulator (parser law embedded verbatim; original file untouched) |
| `CONSOLE-BATCH-CONSTANTS-BRIEF.md` | `a9607dae…` | constants the extension worker should adopt (brief, not an edit) |
| `REPORT.md` (last) | this file | — |
| `receipts/` | see below | simulator run1/run2, verifier selftest + canonical receipts |

## The definitive layout (headline)

8 entries, `[PPSA33286]`, in file order = injection order (pinned parser
L776-820): 1 xwp-armed write probe, 2 fc1pool validator probe, 3 supplier
composition, 4 qdrv1, 5 observer, 6 aligned_entry, 7 path_test (terminator
oracle), 8 ingame_facts = 0 (parser-inert, no slot). **7 of MAX_PLUGINS=10
registered; headroom 3.** Rationale per line in FINAL-INI-LAYOUT.md §2; both
probes precede the composition because their synchronous one-shot
`module_start`s must have verdicts flushed before the composition's
initializer passes its init (the consumer is
`worker-initializer-integration/staging/UnrealInitializer.cpp`
`SeedArming::kExecWritabilityVerdictGating`, currently `false` = "DESIGNED,
NOT ARMED" — this document pins the ordering half of its flip condition).

## Ordering contract (headline)

- Reordering consequences per case in §3.2: probe-after-composition = verdict
  absent at init = initializer's SHAPE A safe default — **acceptable degraded
  but forbidden** (today it is double-degraded: the gating constant is still
  false, so ordering is prospective until armed); composition-after-ridealongs
  = bounded windows burn pre-ladder = honest negatives + wasted launch;
  path_test-not-last weakens the freshness oracle; igf→2 and duplicates are
  hard hazards (parser does NOT dedupe paths).
- Staging steps do NOT tail-append: they run CONTENT-ANCHORED `ensure` ops
  (idempotent, insert after the canonical predecessor line, hard STOP on
  duplicates). PROVEN order-commutative: all 8! = 40320 permutations converge
  byte-for-byte to the canonical bytes (simulator variant
  `staging_permutations`).
- One sanctioned non-append op: the BOOTSTRAP reconciliation — two
  pre-existing lines (aligned_entry, path_test) must MOVE from their
  historical pre-composition position to entries 6-7, which no append can
  achieve; so the return session does Step-0 RETR + `.bak` + ONE whole-file
  STOR of the canonical template + RETR byte-exact diff. Existing
  console_batch anchors (qdrv1→composition, observer→qdrv1) are already
  canonical-position-correct.
- Post-launch gate becomes exactly **7** OK lines / zero FAIL (was 5).

## Verification receipts

- `receipts/simulate_final.run2.txt`: SIMULATION_PASSED, 13 variants, exit 0 —
  canonical 8-entry → 7 registered in exact order; igf never registered;
  igf=2 counterfactual → 8 (second-writer hazard shown); duplicate → x2
  registration (hazard shown); mode-3 rejected; mode-1 probes order-invariant;
  CRLF/comments/no-trailing-NL parse-tolerant; degraded orders parse but are
  flagged for the verifier; 40320-permutation + repeat-application byte-exact
  convergence.
- `receipts/verify_selftest.txt`: SELFTEST 9/9 — canonical ACCEPTed, all 8
  mutation classes (extra line, probe-after-composition, fc1 mode flip, igf
  mode flip, duplicate, missing trailing NL, empty, header-only) REJECTed.
- `receipts/verify_canonical.txt`: FINAL-INI-OK on `ploader.ini.canonical`.
- `receipts/simulate_final.run1.txt`: preserved raw — it caught MY wrong
  expectation (I first asserted the parser drops unknown lines; it REGISTERS
  them — that is exactly why the verifier, not the parser, rejects unknown
  lines; FINAL-INI-LAYOUT.md §3.2 documents this).
- Both probes' fself hashes re-verified on disk this session; SHT-strip
  readback constants recomputed from the bare-sdk ELFs (xwp-armed 67832 B /
  body `daea2ff4…`; fc1pool 67848 B / body `7e2dce27…`; run1==run2) and
  delivered in FINAL-INI-LAYOUT.md §4 for the stagers' upload law.

## Coordination notes

- `worker-observer-staging-extension` delivered (22:48) with the 6-entry law
  (271-check selftest green). The extension worker was NOT interrupted and its
  files were NOT touched; `CONSOLE-BATCH-CONSTANTS-BRIEF.md` gives the exact
  `XWP_ARMED` / `FC1POOL` / `FINAL_INI` dicts, the byte-exact end-state gate,
  the bootstrap reconciliation step, the 7-OK-line launch gate, and the
  new-evidence paths (`/data/modbridge-exec-write-probe.jsonl`,
  `/data/modbridge-fc1pool-facts.jsonl`). Adoption is a NEW bounded revision
  under its ownership chain.
- `worker-return-session-master-plan` step-3 gate "registered count exactly 5"
  is superseded to exactly 7 (noted in FINAL-INI-LAYOUT.md §0; their files not
  edited).
- `worker-game-session-ridealongs` files untouched; its parser law, SC step
  skeleton, SHT-strip channel law, and append-count verification discipline
  remain the law and are embedded/extended, not forked.

## Honesty boundaries

Host-side layout design and pinned-parser simulation only; zero console
contact; every on-console constant remains [TARGET] until Step-0 RETR and the
post-STOR RETR diff. Order guarantees are parser-source facts, not console
observations. Timing figures (12-18 s for 7 mode-2 injections, 8 s fc1 grace
inside the 25 s per-plugin cap, 120 s injector timeout) are derived from
pinned parser constants, not measurements. The initializer consumption path is
designed-but-not-armed (`kExecWritabilityVerdictGating = false`); nothing in
this layout arms it. No quality score is claimed; nothing here is engine
acceptance evidence.
