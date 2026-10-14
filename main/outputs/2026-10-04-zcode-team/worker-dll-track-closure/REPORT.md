# REPORT — worker-dll-track-closure (2026-10-04)

Read-only synthesis. ~27 tool calls of 40 used. NO console contact, no
upload/dispatch, no target/PS5 execution, no nested subagents, no network;
writes only under `outputs/2026-10-04-zcode-team/worker-dll-track-closure/`.
No source, vendor, plan, or tool file was modified; the only newly generated
evidence is five on-disk sha256 re-computations.

Status: **DLL_TRACK_CLOSURE_DOCS_COMPLETE** — the track's closure package is
written; the track itself closes when the pending dll8 console run records
its verdict per the already-prepped oracle.

## Deliverables (in write order)

1. `CLAIM.json` — scope, read-only inputs, verified claims (incl. all
   re-hashes), non-goals; written first.
2. `DLL-TRACK-STATE.md` — the track state document:
   - §1 per-family provenance table (8 families × host status × console
     status × console build/receipt), substrate separated from families;
   - §2 candidate ELFs with sha256 + bytes + status (6/7/8-family combined
     + standalone SEH64), the 55-symbol import-set invariant;
   - §3 plans/ledgers table (6-family executed; 7-family frozen fallback;
     8-family preferred; batch tool with dll8 in session order);
   - §4 the prepped 8-family expected ledger (summary + oracle pointers,
     incl. the memory.last 114688/163840 correction provenance);
   - §5 what dll8 adds over the 6-family PASS: the 2 new families' first
     console proof (Static TLS dir-9 route; SEH64 dir-3 route) + the
     combined admission of the 3-way merged composition (bind/retire order,
     dual-owner stage-55/56 window, union census 90, union win64cc 1+7);
   - §6 successor queue — honest, nothing console-blocking: the SEH64
     PS5-trampoline pre-execution review item (quoted from the owning
     worker: trampoline register contracts host-verified only, "target-route
     review before any execution"), family host-only caveats, the
     conditional 7fam fallback, post-PASS bookkeeping, MSVCP140 as adjacent;
   - §7 UE4SS acceptance independence, both directions, five concrete facts
     (rung oracles, mechanism/artifact/state separation, separate build
     lineages, order-not-dependency, motivation-link-not-acceptance-link);
   - §8 provenance discipline.
3. `TRACK-COMPLETE.md` — one-page done-criteria: 8 numbered criteria with
   met/not-met state, dll8 outcome branches (PASS / SEH64-carve-out /
   not-done), binding honesty clauses, and a done-statement template.
4. `REPORT.md` — this file, last.

## Key verified facts (this session)

- Re-hashed on disk, EXACT vs receipts: 6-family `133c5e9e` =
  `96296c1c…07e81c` (300456 B); 7-family `e72f5318` = `83ee2ae9…47cae`
  (334656 B); 8-family `c3aba79f` = `79281191…804fb` (336552 B);
  standalone SEH64 `5f7e67b3` = `9d74b27b…bbcc3` (222408 B); CRT console
  lineage `53808597` = `a6208aea…0379f`.
- dll8 IS in the batch tool's session order: `DEFAULT_DLL="dll8"`,
  order `dll8 → e2e → stage-qdrv1 → game-evidence`, `--dll 7` fallback,
  exactly one DLL stage per session (worker-batch-runner-8fam-patch; 162+31
  green checks). Nothing has been dispatched.
- The 8fam expected ledger was already prepped (EXPECTED-DELTA-8FAM +
  CONSOLE-STAGE-PLAN-8FAM pass-oracle + the batch runner's
  `verdict_8fam_records`); this package summarizes and points at it rather
  than duplicating it.
- UE4SS independence: no EVIDENCE-SPEC rung references any DLL artifact;
  dll8 is payload-only game-CLOSED while UE4SS acceptance is a
  running-game PRX mechanism; separate lineages and staged paths; the batch
  order is scheduling, not a gate.

## Honest limits

- Nothing here is console evidence; the 8-family candidate remains
  `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`. Whether the track closes
  as PASS, as the documented 7-family carve-out, or stays open is decided
  only by the user-authorized dll8 session.
- Console status values for families 5–6 (module enum, VM) come from the
  combined-run ledger, not individual payloads — stated as such in the
  table.
- The successor queue's "nothing console-blocking" judgment covers the
  project's current console milestones (E2E, ridealongs, game-evidence,
  composition launches); it does not certify any future feature work.
- EXPECTED-DELTA-8FAM.md's memory.last omission is reported as found
  (correction lives in the plan and the batch oracle); the delta doc was
  not edited — out of scope, provenance preserved.

## Key paths

- This package: `outputs/2026-10-04-zcode-team/worker-dll-track-closure/`
  (CLAIM.json, DLL-TRACK-STATE.md, TRACK-COMPLETE.md, REPORT.md)
- Preferred plan: `outputs/2026-10-04-zcode-team/worker-8fam-console-prep/CONSOLE-STAGE-PLAN-8FAM.md`
- Candidate: `outputs/2026-10-04-zcode-team/worker-dll-8fam-combined/out-sdk-composition/c3aba79f968a492b849e5ece2f009e8e/owned-dll-candidate.elf`
- Batch tool: `outputs/2026-10-04-zcode-team/worker-console-batch-runner/console_batch.py` (+ USER-SESSION-GUIDE.md)
- UE4SS acceptance contract: `outputs/2026-10-04-zcode-team/worker-acceptance-evidence/EVIDENCE-SPEC.md`
