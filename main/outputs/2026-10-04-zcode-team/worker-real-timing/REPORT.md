# REPORT — worker-real-timing

**Mission:** turn the latency worker's launch-10 projection ("~5-10 s +
C_comp, C_comp unknown") into measured numbers from the real launch-10 runs.
**Status:** complete; read-only; deliverables `CLAIM.json` (T01-T09 + honesty
flags) and `TIMING.md` (tables) alongside this file.

## Inputs actually used

`outputs/2026-10-04-zcode-controller/launch10-evidence/` — run1/run3 sdiag
jsonl (per-record `t_ms`), run1/run2/run3 UE4SS logs (`[t=...ms pid=...]`,
`PATH_TEST` monotonic-ns), run3 runtime-status, loadlist, sanitized INI.
Model: `worker-injection-latency/LATENCY-ANALYSIS.md` §4 (S## keys reused).

## Headline findings

1. **C_comp is measured: 0.80-0.81 s** (module_start_entered →
   module_start_return: 813 ms run1/pid 317, 803 ms run3/pid 355; 718 ms on the
   MFA-aborted run2/pid 341). 37× under the 30 s `pt_call` cap — the
   projection's C_comp risk is closed. ~84-85 % of it is the flat 235-ctor
   block (679-683 ms); every "expensive" stage (two VMMAP walks 11-12 ms each,
   lease reg 8 ms, anchors 16 ms, lmf conversion 7 ms) is single-digit to
   low-teens ms.
2. **Earliest→latest sdiag record per pid is 5.74 s, but that is NOT C_comp:**
   ~4.93-5.03 s of it is the module's own worker thread running after
   `module_start_return` (sleep ~5.04 s + 10-15 ms buffered Lua probe, 88
   lines). It blocks neither ploader nor the hijacked game thread — the
   worker-offload design is visible working in the real data.
3. **Projection verdict:** ~6-11 s ploader-side serial wall clock with C_comp
   measured; the only game-thread-blocking mod cost is the 0.80 s
   `module_start`. Composition occupancy vs ploader overhead in the measured
   per-boot serial chain (~71-73 s from composition inject to path_test
   module_start): **~1.1 % vs ~99 %** (0.42 % if the pinned 120 s post-FAIL
   idle tail is counted). The user's "game loads normally with 2 plugins" is
   consistent — including with all three boots refusing fail-closed.
4. **New fact the projection missed:** the composition-refusal → path_test
   inter-plugin gap is **70.7-71.8 s** in all three boots (same monotonic clock
   as sdiag, validated 3/3 exact matches), far above the 2-3 s modeled and the
   pinned source's ~32 s worst case. Measured end-to-end, not decomposed —
   PATCH ploader-03 step log is the instrument for that. Mostly
   game-nonblocking per source, but now the dominant mod-side wall-clock term.
5. **Pad timing fact (corrected):** the run3 failure is `scePadOpen` (phase 6 =
   `PadPhase::Open`) returning **0x80920004 = PS5_PAD_ERROR_ALREADY_OPENED**
   (mission shorthand said 0x80A40004 — the evidence says 0x80920004,
   pad_sdl_ps5.h:21). The open fires ≈ +790-800 ms into `module_start`, i.e. at
   the earliest practical injection moment (~2-28 s of process life from pinned
   ploader bounds). **ALREADY_OPENED means the game's pad stack had already
   opened the pad before we could** — the pad service is UP, not down. Pad-defer
   retry-window inference (flagged as inference): the window is not "until the
   pad service is up" but "until the game's exclusive handle is free", which is
   unmeasured (no successful open anywhere in evidence); use event-driven
   retries (per tick/poll or Disconnected→Active), not a fixed boot delay. The
   process demonstrably still runs injected modules ≥71-73 s after injection.

## Structural discoveries worth keeping

- `run1-sdiag.jsonl` is byte-identical to run3's first 548 lines: the on-console
  diag file accumulates across boots; run3's file = run1+run2+run3 epochs
  (pids 317/341/355, seq restarting per writer). All analysis is per-writer.
- sdiag `t_ms`, UE4SS `[t=...ms]`, and `PATH_TEST` monotonic-ns are one clock —
  cross-writer correlation is legitimate (3 exact refusals matches) and is what
  made the inter-plugin gap measurable at all.
- Each boot's run1→run2 (+53 s after the modeled 120 s tail) and run2→run3
  (+459 s) spacing is consistent with the pinned ploader idle-tail model.

## Not done / limits

- No process-spawn timestamps in evidence: boot-relative numbers are bounds.
- Pre-module_start costs (readiness, attach, 30 MB kernel load) are invisible
  to sdiag and remain unmeasured.
- The 71 s gap decomposition and the game's handle-release time need new
  instrumentation (ploader-03; a deferred pad probe), not more mining here.
