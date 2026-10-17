# REPORT — worker-verdict-first-real

Date: 2026-10-05 · Local only; no console contact; evidence.sh executed unmodified in
`--from-dir` host mode, once per run, exit 0 each.

## What was done

1. Verified all four launch10-prep baseline pins (size + sha256) before classifying, and
   verified the append-only contract: the first 7591 B of every run log sha-matches the
   baseline head; per-run windows are strictly incremental (+141/+620/+926 B). Zero
   classifier warnings on any run.
2. Constructed per-run fixture dirs with documented placement (`fixtures/run{1,2,3}/`):
   logs as `UE4SS.log`; run1/run3 sdiag renamed `runN-sdiag.jsonl` →
   `modbridge-supplier-diag.jsonl` (the name `--from-dir` expects — G5); run3 status as
   `runtime-status.txt`. Run2 has no sdiag/status capture on the console side.
3. Ran the real classifier once per run against `--baseline-dir launch10-prep`, preserving
   full output, one-page summary, stderr, and `--json-tail` per run
   (`classifier-outputs/`).
4. Classified each run against ORACLE.md (incl. the merged final-build supplement) and
   verified the controller's run-3 MINIMAL-ACCEPT assertion clause-by-clause.
5. Wrote `LAUNCH10-RESULTS.md` (results record for controller promotion) and `CLAIM.json`
   (machine-readable). Raw findings unchanged; nothing fixed in evidence.sh (real-data
   gaps are reported per task; nine of them, G1-G9).

## Verdicts (headline)

- **run1 (pid 317)** — PS5Pad ASCII refusal, **stage E2 START (code=3), AFTER conversion
  passed**: its own sdiag carries the exact predicted geometry (extent [0x400000,0xc0602c8),
  anchor_count=7, intervals=1, spans=6/6, cfg 1/1). Entry returned 0; game survived.
  E4 `activated` not captured (no status fetch for run1) → conversion pass is
  diag-proven; rung claim reserved.
- **run2 (pid 341)** — loadlist size refusal (`mfa_activate_failed code=-8`), oracle B7.
  **The runtime STARTED — recorded**: module_start_entered, all 235 ctor pairs returned 0,
  probe_lifecycle_result=0 (the B4 dead-rebuild did NOT fire), runtime_wiring_begin
  present, module_start_return=0, buffered-Lua harness green; no bridge tags (nothing
  downstream of MFA ran, exactly per B7). Fix: loadlist-sizefixed.txt.
- **run3 (pid 355)** — pad phase-6 refusal (`PS5Pad input unavailable: phase=6,
  error=-2137915388` = 0x80920004), stage E2 START (code=3), AFTER conversion passed with
  the EXACT predicted values. Status session 355 fresh, `state 1 failed 1 <same detail>`.

## Run-3 declaration: controller's MINIMAL-ACCEPT assertion — CONFIRMED

All oracle clauses verified with lines: D1+D2 (exact extent), D3 (7), D4 (1 — the
predicted coalesced value, not 6), D5 (1/1), no D6 (refusal fan-out tags entirely
absent), NO-CRASH (module_start_return=0, PATH_TEST stamped after the refusal, one boot
carried all three runs). The one soft point is E4: the literal `state 1 activated 0` line
was truncated by the entry's terminal write — the oracle's own documented two-openers
behavior — and activation is proven by the diag pair (mfa_activated_low=36564/high=0)
plus START-stage reachability. CONFIRMED with that caveat recorded; launch-11 should
fetch status before a terminal write can truncate. The predicted pair
(anchor_count=7 next to intervals_found=1, spans=6) is surfaced by the classifier's
SDIAGSUM verbatim and confirmed TWICE (run1 and run3 independent boots).

## Next run (pad-deferred): pinned expectation

Phases B-D replay run3; then `runtime_start_ok` + a surviving `activated` line; Phase F
is new territory (armed marker, slot 0, pool v2 published-span format, GMalloc V1+V2,
slot 2 behavioral, slot 4); pinned terminal at `Finding GameEngine::LoadMap`: W1 Error
line + W2 `guard_off_no_rx_verdict` status failure (fold-0, no read, zero syscalls, game
survives) → verdict GOOD per oracle S1.2. A real walk read there = FAIL/REGRESSION
(unguarded or folded-ON unit shipped).

## Gold: real-data classifier gaps (reported, not fixed)

G1 no E2/B7 verdict names (classifier says FAILED_code3/FAILED_code-8 where the oracle
says EXPECTED-REFUSAL ≥ MINIMAL-ACCEPT / B7 — its raw verdict understates the ladder);
G2 cumulative sdiag with no baseline cut/per-pid split (stale pid_first, cross-session
"single-writer" deltas, last-write-wins values safe only by luck of equal values); G3
JSON tail has no sdiag-derived fields (JSON-only consumers see "no staged markers" on the
MINIMAL-ACCEPT run); G4 module_start_return → S11_STOP mislabels the post-refusal return
as a stop; G5 fixture-name coupling; G6 cumulative log windows mix prior-run lines;
G7 run1 vmentry=22761 > pinned 16384 with walk_stop=0 (capacity/counter reconciliation
question); G8 oracle C7 nuance (exepath source=0 resolves via ownmod, no refusal); G9
spanpub env_low/high is a 64-bit split and spanpub_cap=1 saturated at 16 on both boots.

## Honest bounds

Evidence is real console data supplied by the controller; classification is local and
reproducible from the preserved fixtures/outputs. Nothing beyond MINIMAL-ACCEPT is
claimed; no Phase F seed line exists in any window; the classifier was not modified; no
retries, no replay, no staging writes.
