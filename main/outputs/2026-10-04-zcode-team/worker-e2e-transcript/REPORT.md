# REPORT.md — worker-e2e-transcript (2026-10-04-zcode-team)

## Deliverable

`check_transcript.py` — a host-side transcript-pairing checker for modbridge
E2E status logs (`e2e-status.jsonl`). Python 3 stdlib only; no network, no
console access, no engine execution. Exit 0 = gate PASS, exit 1 = gate FAIL.
Everything written this session lives in
`outputs/2026-10-04-zcode-team/worker-e2e-transcript/` (CLAIM.json first,
REPORT.md last; all other files read).

## Transcript contract (proven from sources, not assumed)

- `worker-e2e-rehearsal/owned/e2e_status.c` (`e2e_status_event`, lines 92-101)
  and `worker-e2e-console-dry/E2E-CMD-SCHEMA.md` §7:
  - `{"event":"begin","cmd":…,"title":…,"stage":…}`
  - `{"event":"end","exit":<int>}` — carries **no** stage/job identifiers
  - `{"event":"status","stage":…,"outcome":…,"detail":…}`
  - stage mapping: records→`stage-0-baseline`, download/register-mod→
    `stage-1-download`, prepare/prepare-archive→`stage-2-prepare`,
    install→`stage-3-install`, simulate-runtime→`stage-4-5-runtime-ack`.
- Because `end` is anonymous, begin/end pairing is strict last-open-first-
  closed (stack) discipline — exactly how the installer serializes commands.
  If an `end` does carry optional `stage`/`cmd` ids (defensive richer
  variant), it is matched to the innermost open begin with those ids; an
  id-matched end over an already-closed pair is a duplicate end.
- Status pairing is by stage: a status must fall inside the open begin..end
  window of its own stage. `worker-e2e-console-dry/SUCCESS-CRITERIA.md`
  defines the terminal-status rule (stage 3 requires
  `status stage-3-install outcome=ok`) and the no-status rule (stage 6
  removals emit none), plus the happy-path ledger "33 new e2e-status lines,
  0 unmatched, 0 failed ends".

## Checks performed

1. **Unmatched begins** — begin still open at EOF (`unmatched-begin`), with
   `--expect-unmatched-begins N` allowance for deliberate refusal probes
   (allowed ones downgrade to `expected-unmatched-begin` notes; extras are
   errors).
2. **Unmatched ends** — end with no open begin (`unmatched-end`).
3. **Duplicate ends** — extra anonymous end after the stack is drained
   (`duplicate-end`), or an id-carrying end matching only an already-closed
   stage+job (`duplicate-end`).
4. **Out-of-order events** — status after its stage's pair closed
   (`out-of-order-status`), status with no open/closed pair of its stage
   (`orphan-status`), id-matched close while inner begins remain open
   (`interleaved-close` warning), plus malformed JSON / unknown event types.
5. **Missing terminal status** — `--require-terminal-status
   STAGE[:OUTCOME[:DETAIL]]` requires every pair of that stage to contain a
   matching status inside its window.
6. **Forbidden statuses** — `--forbid-status STAGE` (stage-6 removals).
7. **Ledger expectations** — `--expect-lines/--expect-begins/--expect-ends/
   --expect-status`, and nonzero exits beyond `--expect-failed-ends`.
8. Concurrent same stage+job overlap while an earlier identical begin is
   still open is a **note** (`concurrent-overlap`), never an error — the
   rehearsal's deliberate dangling refusal probe legitimately causes this.

Structured reports via `--json` (counts, findings, full pair list with
statuses); human summary + every finding on stdout.

## Test results (details in TEST-RESULTS.md)

- **Known-good rehearsal transcript**: PASS (exit 0) under
  `--expect-lines 44 --expect-begins 21 --expect-ends 20 --expect-status 3
  --expect-unmatched-begins 1 --expect-failed-ends 0`. Ledger matches the
  brief and `worker-e2e-rehearsal/READINESS.md` §S7 exactly
  (44/21/20/0-failed/1-unmatched). The unmatched begin is pinpointed:
  line 14, `cmd=install stage=stage-3-install title='PPSA33286'` — the S2a
  install refusal probe. The `run/state/e2e-status.jsonl` copy is
  byte-identical (`cmp`).
- **Strict gate on the same file** (no allowance): FAIL (exit 1) with exactly
  one error on that refusal-probe begin — proves the PASS is from a
  documented allowance, not silent tolerance.
- **Synthetic corrupted transcript** (`corrupted-transcript.jsonl`): FAIL
  (exit 1), 14 errors, with all 11 defect classes caught directly: unmatched
  begin (x2), plain unmatched end, duplicate ends (both anonymous and
  id-matched), out-of-order status, orphan statuses, missing terminal status
  (x2), forbidden status, failed end (exit 2), malformed JSON, unknown event.
  No false-positive errors on valid lines.

## Recommended console-run gate (for the E2E console run controller)

```
python3 check_transcript.py <collected>/MODBRIDGE_INST_E2E-e2e-status.jsonl \
  --expect-lines 33 --expect-unmatched-begins 0 --expect-failed-ends 0 \
  --require-terminal-status stage-3-install:ok:movspeed,mm-skin,mm-lua \
  --forbid-status stage-6-removal --json transcript-gate.json
```

The `--expect-lines 33 / 0 unmatched / 0 failed` figures are verbatim from
`SUCCESS-CRITERIA.md`; the terminal-status detail string is that document's
verbatim stage-3 requirement. Exact `--expect-begins/--expect-ends/
--expect-status` values are intentionally not guessed here — the 33-line
ledger is not itemized per event type in the source docs, so the controller
should pin those three flags once its ledger is itemized (the tool accepts
them as-is). For run-scoped checking, point the tool at the run's delta
transcript; expectations apply to the whole file given.

## Bounds and honesty

- ≤40 tool calls; no nested subagents; no network; no console (PS5) access;
  writes confined to `outputs/2026-10-04-zcode-team/worker-e2e-transcript/`.
- The checker validates transcript event discipline only. It does not execute
  the E2E, judge records.txt semantics, or constitute runtime acceptance; a
  PASS here means "every begin has a matched end, statuses are ordered and
  complete per the given expectations" — one input to the G2/G3-style gate
  described in `worker-e2e-console-dry/SUCCESS-CRITERIA.md`, not a run
  verdict by itself.
- Known limitation: pairing of anonymous `end` lines is inherently positional
  (the format's `end` carries no ids), so if the installer ever emitted two
  interleaved anonymous windows out of nesting order, a wrong-but-plausible
  LIFO pairing could mask a misordering. No such emission exists in the
  proven format (commands are strictly serialized), and id-carrying ends are
  supported defensively should the format ever gain them.

## Files

| File | Purpose |
|---|---|
| `CLAIM.json` | session claim (written first) |
| `check_transcript.py` | the checker (task 2) |
| `corrupted-transcript.jsonl` | synthetic corrupted fixture (task 3) |
| `test-results-rehearsal.json` | raw checker JSON, rehearsal (PASS) |
| `test-results-rehearsal-strict.json` | raw checker JSON, strict gate (FAIL as intended) |
| `test-results-corrupted.json` | raw checker JSON, corrupted (FAIL, 14 errors) |
| `TEST-RESULTS.md` | consolidated test evidence |
| `REPORT.md` | this report (written last) |
