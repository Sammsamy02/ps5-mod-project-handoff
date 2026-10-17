# TEST-RESULTS.md — worker-e2e-transcript checker validation (2026-10-04)

Tool: `check_transcript.py` (host-side, Python 3 stdlib, no network, no console access).
Raw machine outputs: `test-results-rehearsal.json`, `test-results-rehearsal-strict.json`,
`test-results-corrupted.json`. Fixture: `corrupted-transcript.jsonl`.

## Test 1 — known-good rehearsal transcript (PASS)

Input: `outputs/2026-10-04-zcode-team/worker-e2e-rehearsal/rehearsal/e2e-status.jsonl`
(the `rehearsal/run/state/e2e-status.jsonl` copy is byte-identical, `cmp` verified).

Command:

```
python3 check_transcript.py <rehearsal>/e2e-status.jsonl \
  --expect-lines 44 --expect-begins 21 --expect-ends 20 --expect-status 3 \
  --expect-unmatched-begins 1 --expect-failed-ends 0 \
  --json test-results-rehearsal.json
```

Result: **exit 0, PASS**. Ledger matches the task brief and
`worker-e2e-rehearsal/READINESS.md` §S7 exactly: 44 lines, 21 begins, 20 ends,
20 paired, 3 status, 0 failed ends, 1 unmatched begin.

The single unmatched begin is identified precisely:

```
unmatched begin: line=14 cmd=install stage=stage-3-install title='PPSA33286'
[note] line 14 expected-unmatched-begin: begin install/stage-3-install
       (title='PPSA33286') never closed — within the expected unmatched-begin
       allowance (e.g. a refusal probe)
```

This is the known S2a install refusal probe (F-R2 dangling begin). The checker
additionally emits 3 `concurrent-overlap` notes (lines 25/37/41: later
`install` begins open while the dangling refusal-probe begin is still open) —
informational only, never errors, so a known-good transcript with a deliberate
refusal probe still gates clean.

## Test 1b — same transcript, strict gate (correctly FAILS)

Command: same input, no `--expect-unmatched-begins` allowance (default 0).

Result: **exit 1, FAIL** with exactly 1 error:
`unmatched-begin at line 14 (install/stage-3-install)`. This proves the PASS in
Test 1 comes from the explicit, documented allowance — not from the checker
ignoring unmatched begins.

## Test 2 — synthetic corrupted transcript (correctly FAILS, all 11 defect classes)

Input: `corrupted-transcript.jsonl` (21 lines, hand-built). Command:

```
python3 check_transcript.py corrupted-transcript.jsonl \
  --require-terminal-status stage-3-install:ok \
  --forbid-status stage-6-removal --json test-results-corrupted.json
```

Result: **exit 1, FAIL**, counts `lines=21 begins=7 ends=8 status=4 paired=5
failed-ends=1 unmatched-begins=2`, 14 errors. Every defect class the tool is
required to detect was exercised directly and caught:

| Defect class | Fixture line(s) | Finding kind |
|---|---|---|
| unmatched begin | 18, 21 | `unmatched-begin` |
| unmatched end (stray end, nothing open) | 1 | `unmatched-end` |
| duplicate end (extra end after a matched pair) | 6 | `duplicate-end` |
| duplicate end (id-carrying end for an already-closed stage+job) | 14 | `duplicate-end` |
| out-of-order status (status after its pair closed) | 11 | `out-of-order-status` |
| orphan status (no open/closed pair of that stage) | 7, 17 | `orphan-status` |
| missing terminal status (stage-3-install pairs lack `status:ok`) | pairs ending 5, 16 | `missing-terminal-status` |
| forbidden status (stage-6-removal must emit none) | 17 | `forbidden-status` |
| failed end (nonzero exit) | 16 | `failed-end` |
| malformed JSON line | 19 | `malformed-json` |
| unknown event type | 20 | `unknown-event` |

The anonymous-vs-identified `duplicate-end` distinction: a plain end with an
empty open stack is `duplicate-end` when the previous event was also an end
(line 6) and `unmatched-end` otherwise (line 1); an end carrying
`stage`/`cmd` that matches only an already-closed pair is always
`duplicate-end` (line 14).

## Verdict

- Known-good transcript gates PASS with exact ledger match and correct
  refusal-probe identification.
- Strict gate fails on the same transcript when the deliberate dangling begin
  is not explicitly allowed — no silent tolerance.
- Corrupted transcript gates FAIL with every injected defect detected, none
  missed, no false-positive error findings on valid lines.

Checker is fit for use as the G2/G3-style transcript gate for the E2E console
run (see REPORT.md for the recommended console-run invocation).
