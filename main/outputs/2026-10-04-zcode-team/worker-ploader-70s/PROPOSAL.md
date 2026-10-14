# PROPOSAL — bounded fix for the ~71s inter-plugin gap

Worker: worker-ploader-70s · 2026-10-05 · **PROPOSAL-ONLY.** Nothing here is built,
staged, or applied to the console. The controller owns all console changes and
ploader builds. Base tree and series: pinned ploader `main.c` (sha256 `2b521cd7…`)
+ `worker-injection-latency/PATCHES/01..04`.

---

## 0. What the gap actually is (drives the fix)

Measured (4/4 boots): 70.7-71.8s between composition's refusal and path_test's
PATH_TEST stamp. Decomposition (GAP-ANALYSIS.md): the failed-plugin cleanup is
INNOCENT — failure path ≡ success path + one toast string; no cleanup timeout, no
retry exists at source. The dominant term is **path_test re-paying the per-plugin
readiness wait** (main.c:784) whose "25s" budget is a TICK budget (500 × 50ms, no
clock check, main.c:518-559) and whose ticks each also pay ~264 kernel calls —
a full mid-boot burn is ~64s wall, not 25s.

Therefore the mission's requested "short-circuit failed plugins' cleanup" diff
targets a term worth ~0.3s. The bounded fix is instead:

1. **Keep PATCH 02 (hoist-readiness) — it is THE gap fix.** With readiness paid once
   per title before the loop, path_test pays only attach + inject.
2. **New PATCH 05 (wall-clock deadline)** — makes `max_ms` mean what the code says
   it means (a time budget), capping the ONE remaining readiness burn at ~25s wall
   and permanently removing the unbounded-compute hazard for any future ini.

## 1. New PATCH 05 — `PATCHES/05-wallclock-deadline.diff`

- Adds a forward declaration of `now_ms()` (defined later at main.c:570; `g_t0` is
  set in `main()` before any `wait_stable` call, so it is valid there).
- Captures `deadline_ms = now_ms() + max_ms` at `wait_stable` entry; breaks out of
  the tick loop when exceeded, falling through to the EXISTING "try anyway if still
  alive" exit (main.c:557-558). No new exit status, no semantic change: both the
  fast path (20 stable ticks + settle, unreachable when settling) and the
  try-anyway path behave exactly as before.

Behavioral delta, honestly stated:

| Scenario | Before | After |
|---|---|---|
| Process stable at injection (fast path) | 20 ticks + 600ms settle ≈ 2-4s wall | identical (deadline ≥ 25s never fires) |
| Module space settling (mid-boot, the measured case) | **~64s wall** (500 ticks × (50ms sleep + ~264 kernel calls)) | **≤ ~25.3s wall** (deadline), try-anyway exit unchanged |
| Process dies mid-wait | return 0 immediately | identical (kill(0) check unchanged) |

Risk: minimal and reversible — 3 inserted lines + a comment block + a forward
declaration; no timeout values changed, no `pt_call` 30s cap touched, no
`wait_stable` stability-gate semantics touched (both explicitly preserved by the
latency worker's "deliberately NOT proposed" note, respected here too).

Verification performed locally (same standard as the 01-04 series): applies with
`patch -p1` at zero offsets/rejects BOTH standalone against the pinned original AND
in series after 01-04 (final tree sha256
`d59da3ecc1a5a5f0befc28c7c563a9a0a8c6e35d8771813947e3b70f05630bee`; diff sha256
`9c0ec7e1d7700ae17c0b109a9eb9d66ccfb0cf8c2bef2a4da5cde5fd5d9ab7d2`). Full compile
requires PS5 SDK headers, unavailable on this host.

## 2. Estimated saved seconds

| Configuration | Inter-plugin gap | Mod-side serial startup (readiness + comp module_start + gap) | Saving |
|---|---|---|---|
| Today (measured) | **70.7-71.8s** | implied ~135-143s (readiness1 unmeasured, H1-inferred ~65s + 0.8s comp + gap) | — |
| 02 only | 0.8-7.9s (cleanup 0.3-0.5 + attach 0.3-6.9 + inject-to-stamp 0.2-0.5) | ~67-75s | **~63-70s** of MEASURED gap per invocation |
| 05 only | ≤ ~32.7s (readiness re-payment capped at ~25.3s) | ~93-101s | ~38-41s when settling; 0 when stable |
| **02 + 05 (recommended)** | **≤ ~8s** | **≤ ~34s worst; ~5-8s if the game is already stable at readiness** | **≥ ~100s worst-case; bounded forever** |

Honesty: the 63-70s "02 only" saving is anchored to the measured gap; the total
-column numbers for "Today" depend on the H1 inference for readiness1 and are
labeled as such. The 05 column's worst case is a source bound (25.3s + fixed terms),
not a measurement.

## 3. Ini-order mitigation — REJECTED (honest evaluation, as requested)

Moving `path_test` FIRST in ploader.ini would:

- **Mask, not fix:** PATH_TEST would stamp BEFORE composition's refusal, so the
  measured "gap" goes negative/vanishes while BOTH readiness burns are still paid —
  total serial wall time is unchanged (~0s saved).
- **Break the terminator guard:** LAUNCH-DAY-10 explicitly uses "PATH_TEST stamps
  prove full ini traversal (path_test is the TERMINATOR, after composition
  module_start returned)". Reordering destroys that property for future runs.
- **Lose comparability:** runs 1-4 (and any historical PATH_TEST pairs) become
  incomparable.

Verdict: reject as a mitigation. It is acceptable ONLY as a one-off diagnostic
shuffle, and 03's steplog measures the same thing without touching the ini.

## 4. Recommended action for the controller (bounded)

1. Adopt `05-wallclock-deadline.diff` into the proposal series after 04
   (proposal-only today, same as 01-04).
2. On the next NECESSARY console run, stage ploader built with **03 + 02 + 05**
   (03 first in the series so the steplog measures the very boot the fix lands on;
   ini unchanged, `1216aab6…`).
3. Acceptance for this fix, read off ploader.log (steplog), no new evidence class:
   - `injecting 2 plugin(s)` → composition `FAIL (<dt>ms)`: dt ≈ 1-2s confirms H1
     and simultaneously REFUTES H2 (step-loop pathology would show dt ≈ 70s);
   - time from `FAIL` line to `OK (<dt>ms): modbridge_path_test.prx` ≤ ~9s
     (attach worst 6.9s + cleanup + inject);
   - total `injecting` → last OK ≤ ~34s worst (single hoisted readiness ≤ ~25.3s
     wall + comp 0.8s + gap ≤ 8s), vs ≥ 135s implied today.
4. If instead the steplog shows the burn somewhere OTHER than readiness (H1
   refuted), 05 is still harmless (no fast-path change) and the steplog identifies
   the real sink for a follow-up bounded patch. No replay of this analysis needed.

Explicit non-goals (unchanged from the latency worker's note): lowering pt_call's
30s cap; trimming wait_stable's 1000ms stability gate + 600ms settle; any change to
composition/path_test PRX behavior; any ini rewrite.
