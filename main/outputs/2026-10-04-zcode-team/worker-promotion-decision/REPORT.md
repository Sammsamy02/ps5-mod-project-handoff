# REPORT — worker-promotion-decision (2026-10-04)

CLAIM.json was written first; this REPORT.md is the last artifact. All writes
confined to `outputs/2026-10-04-zcode-team/worker-promotion-decision/`. No
network, no console, no nested subagents, no code, no tests. ~20 tool calls
used (bound: 40). Existing files elsewhere read-only.

## Deliverable

`PROMOTION-DECISION.md` — the formal promotion decision document the
controller will sign when the code anchors confirm. It contains:

1. **Evidence table (section 1)** — all seven checks with exact source
   citations, grounded in the primary records rather than secondhand
   summaries:
   - GUA structural validation — PASS (igf4 live capture seq 6–7 + corrected
     predicate host replay [B] + staged-pair-live consumption).
   - Single-bias mapping — CONFIRMED (`zcode_v3_MAPPING_BREAKTHROUGH`
     checkpoint, status `SINGLE_BIAS_CONFIRMED_LOAD7_PROVEN_GUA_ADDRESS_
     ESTABLISHED`, plus the static L0/L7 derivation).
   - Axes — RESOLVED (SHIPPING margin 60, STATS OFF margin 12, host-measured
     on the real retained image by the ported patternsleuth cost model).
   - Identity — RESOLVED (`5.6.1-0+++UE5` / `04bb4330a7d71dcefe1cc2c4d19769d5`,
     byte-verified against the retained image; two controller ratifications
     explicitly reserved: reconstruction convention, GNU build-id convention).
   - Code anchors — PENDING igf6 (igf4 safe refusal `covered_not_readable`
     documented; igf5 built but failed with zero records; expected-16
     `554889e5...` triple-sourced; vtable reads noted as corroborating only,
     never a substitute).
   - Original predicate refutation — DOCUMENTED, corrected evaluation kept
     SEPARATE (igf4 seq 9 raw record stands; GC-order root cause; vendored
     original/fixed trees; RCode retained with corrected meaning).
   - igf5 failure — DOCUMENTED, cause under diagnosis (zero records,
     crash-correlated; SYS_mprotect-74 residual risk named as the leading
     hypothesis, not established; no replay of the failed candidate).
2. **Promotion criteria (section 2)** — C1–C5, encoding the user's
   discipline: only completed checks promote; C2 requires igf6 byte_match=1
   for both anchors or a controller-named concrete alternative; scope
   capped at the FUObjectArray/TUObjectArray subset (rung 1→2); rung 3+
   requires the RUNG3-REQUIRED-READS ladder (R1–R5, R6 strengthening-only;
   UFunction permanently refused; FName pool excluded as data).
3. **Meaning boundaries (section 3)** — promotion means the pair is real
   Townfall layout data, stageable on the console (after C5 re-emission with
   real identity); it does NOT mean engine integration, game-thread
   delivery, or Item Spawner working; per-rung acceptance ladder table.
4. **Sign-off block (section 4)** — fillable formal block: decision, UTC
   timestamp, per-criterion checkboxes with citation slots, identity
   ratification initials, igf5-failure acknowledgment, conditions/reservations,
   signature line. Signature deliberately deferred until C2 settles.

## Key grounding decisions (why the citations are what they are)

- The PASS for the GUA validation is stated with its honest boundary: the
  corrected-predicate Validated is a host-side replay of the igf4-captured
  live bytes (header bytes real; chunk scaffolding labeled reconstruction);
  the in-game corrected replay (igf5 seq 6) was never observed. The document
  does not upgrade that boundary silently.
- C2's alternative-path wording (controller names and accepts a concrete
  alternative) mirrors the task brief and worker-staged-pair-live §7's
  "corroborate but never substitute" rule for R3 vtable reads.
- Identity fields are handled as staging provenance (never runtime-compared
  per the pinned publish gate), which is what makes them promotable now
  despite the worker-staged-pair-live GAP markers those values supersede.
- Axes are marked RESOLVED with their evidence class stated plainly
  (host measurement of the real game image; console-side in-probe record
  transfers to the igf rebuild), and the runtime publish gate's fail-closed
  disagreement refusal is cited as the backstop.

## Not claimed

No promotion was executed, no signature forged, no console action taken, no
code written, no criterion pre-certified. The pair remains evidence until the
controller signs. Nothing here claims completion, parity, or quality scores.

## Bounds accounting

About 20 of 40 allowed tool calls: 3 writes (CLAIM.json first,
PROMOTION-DECISION.md, this REPORT last), the rest reads/greps of primary
evidence (igf4 anchor-validation records, v3 checkpoint ledger entry,
predicate-gc-fix / staged-pair-live / vaddr-semantics / anchor-validation /
axes-detector / layout-identity / code-anchor-reads / integration-checklist
reports, igf5-diagnosis CLAIM).
