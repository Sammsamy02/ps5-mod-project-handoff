# GAP-ANALYSIS — the 70.7-71.8s inter-plugin gap, decomposed

Worker: worker-ploader-70s · 2026-10-05 · read-only analysis; proposal-only patch
(`PATCHES/05-wallclock-deadline.diff`). Every number's provenance is in `CLAIM.json`
(F## = pinned source facts, boot table = measured). Source basis: pinned ploader
`main.c` (sha256 `2b521cd7…`, re-verified this session), all `main.c:L` into that file.

---

## 0. Headline answers (mission questions)

**Q1 — Does a FAILED module_start trigger a different cleanup path? NO — refuted at
source level.** The refusal is just the SIGN of the returned module handle. The
injected shellcode ends in `INT3` after `call r15` (main.c:93-96), so the SIGTRAP
fires when `module_start` has RETURNED, success or failure; `pt_call` takes its
success branch (main.c:285-289) and `inject_prx`/`run_plugins` then run the
BYTE-IDENTICAL sequence for OK and FAIL: mprotect de-exec (448) → pt_munmap (452) →
notify (812 vs 814 — string only) → restore_creds (816) → pt_detach (817). There is
**no cleanup timeout, no cleanup retry, and no retry of the failed plugin** anywhere
in the file: each pid is processed exactly once per invocation (known-list,
755-764), and a FAIL does not even break the loop (819; only death-while-waiting
breaks, 787). The mission's primary hypothesis (failed-plugin cleanup timeout) is
dead at source level. H5, CLAIM.json.

**Q2 — Which path was the ~32s worst case modeled for? THE SAME (only) path.**
There is no distinct failure-path flow (Q1). The ~32s figure is the SLEEP-ONLY
bound of the per-plugin flow both plugins pay:
readiness tick budget 25s (mode 2, main.c:779/522) + attach retries ≤6.9s (F09) +
cleanup ≤0.5s + inject-to-stamp ≤0.5s = **32.9s**. The latency worker's "~2-3s" was
the same flow's fast case (readiness fast path 1.6s sleep-only + typical attach).
What NEITHER figure modeled:

- **F06 — `max_ms` bounds the TICK COUNT, not wall time.** `wait_stable` (518-559)
  loops `max_ticks = 25000/50 = 500` times with NO clock check; the only guaranteed
  cost per tick is `usleep(50ms)`. "25s readiness" is a tick budget, not a deadline.
- **F07 — each tick also pays ~264 kernel calls** (`count_modules` at 531 = 128 ×
  `kernel_dynlib_mapbase_addr`, plus `deps_ready` at 538 = 3×(handle+mapbase) +
  `pt_resolve` + `count_modules`' 128 again). For a healthy game process nothing
  short-circuits. 500 ticks = **132,000 kernel calls**; at K ≈ 0.3 ms/call that is
  ~40s of compute ON TOP of 25s of sleep.
- **F05 — the readiness wait is charged PER PLUGIN** (main.c:784, inside the loop):
  path_test re-pays the whole thing ~0.8s after composition already passed it.

**Q3 — What composes to ~70s? H1 (primary, matched):** path_test's readiness
re-payment runs its FULL 500-tick burn — mid-boot the module count never holds 20
consecutive stable ticks (stable_req=20, main.c:521) — so the wall clock is
`500 × (0.05s sleep + 264·K compute) ≈ 64.6s` at K ≈ 0.30ms, plus cleanup 0.3-0.5s +
attach 0.3-6.9s + inject-to-stamp 0.2-0.5s = **65.5-72.2s**, matching all four boots
(70.7-71.8s) with a single stable parameter. The determinism (±1.12s over 4 boots)
is the signature of a fixed tick cap and firmware-stable per-call cost — not of any
timeout-driven path (timeouts would quantize to 1s/2s/30s steps and stack).

---

## 1. The measured gap (4 boots, same-clock per-writer stamps)

Clock identity (sdiag `t_ms` ≡ UE4SS `[t=…ms]` ≡ PATH_TEST `monotonic-ns/1e6`)
was validated 3/3 exact by worker-real-timing; run4 re-derived from raw evidence,
not trusted from the mission text.

| boot | pid | refusal `[t=ms]` | PATH_TEST (ms) | gap |
|---|---|---|---|---|
| run1 | 317 | 60004266 | 60076100.108 | **71.83s** |
| run2 | 341 | 60249877 | 60320585.374 | **70.71s** |
| run3 | 355 | 60900581 | 60971455.045 | **70.87s** |
| run4 | 370 | 62842697 | 62913653.733 | **70.96s** |

Mean 71.09s, spread 1.12s. Run4 corroboration from its own sdiag: pid 370
`module_start_entered` 62841898 (+0), `runtime_start_failed` 62842696 (+798ms),
`module_start_return` 62842700 (**+802ms = C_comp(run4)**), refusal lands ~4ms
before the return — so the gap is, to within 4ms, "from composition's pt_call
return to path_test's module_start entry". Config law: 2-plugin ini `1216aab6…`,
supplier_composition=2 FIRST, path_test=2 SECOND/LAST, both mode 2.

## 2. Complete enumeration of the waits (source-derived)

Every wait between finishing composition's `pt_call` (t_r) and starting path_test's
`module_start` (t_p), in execution order:

| # | Wait | Source | Sleep-only worst | With compute (K=0.30ms) |
|---|---|---|---|---|
| 1 | pt_call epilogue (getregs+setregs, both branches restore) | main.c:280-289 | ~10ms | ~10ms |
| 2 | de-exec mprotect | main.c:448 | ~ms | ~ms |
| 3 | pt_munmap = pt_syscall step loop (2-4 steps × ≤1s waits) | 452 → 256-258 → 237-246 | ~0.02-0.15s | ~0.02-0.15s |
| 4 | notify FAIL + restore_creds + pt_detach + bookkeeping | 806-819, 321-346, 192-194 | ~0.05-0.3s | ~0.05-0.3s |
| 5 | **wait_for_needs for path_test** (charged per plugin) | **784 → 518-559** | fast 1.6s / full-burn **25.0s** | fast ~3.2s / full-burn **~64.6s** |
| 6 | pt_attach | 790 → 176-190 | ≤6.9s (3 attempts) | same |
| 7 | save_creds + jb_pid | 796-804, 294-376 | ~ms | ~ms |
| 8 | inject_prx(path_test) up to the PATH_TEST stamp (pt_mmap, 2×PT_IO, mprotect, PT_CONTINUE, 68KB load, module_start entry) | 807 → 423-454 | ~0.2-0.5s | ~0.2-0.5s |
| — | pt_call's 30s cap for path_test's own module_start | 280 | AFTER the stamp — not in the gap | — |

Failure-vs-success of plugin 1 changes NOTHING in rows 1-8 (Q1). Sum of sleep-only
worsts ≈ **32.9s** — the "~32s" — already exceeded by row 5 alone once compute is
counted. Note row 5's full-burn wall time is NOT bounded by `max_ms` (F06): the
only structural cap is 500 iterations.

## 3. Signature match (which path's timing fits ~70.9s?)

| Hypothesis | Predicted gap | Verdict |
|---|---|---|
| **H1** path_test readiness full 500-tick burn + tick compute (F05/F06/F07) | 500×(0.05+264K) + 0.3-0.5 + 0.3-6.9 + 0.2-0.5 = **65.5-72.2s** at K≈0.30-0.33ms | **MATCH** — fits all 4 boots with one stable fitted parameter; deterministic because the tick cap is fixed and K is firmware-stable; run1's +0.9s is attach/jitter-class |
| H2 pt_syscall step-loop pathology (F10 walking thousands of instructions) | needs ~3500 clean steps through a 2-3 instruction munmap stub | REJECT (residual): no mechanism; would also inflate composition's own inject dt. Cheap discriminator next run: composition FAIL toast `dt` ≈ 1-2s under H1 vs ≈ 70s under H2; patch 03's steplog records it durably |
| H3 pt_call's 30s waitpid timeout fired on composition (lost SIGTRAP) | 30 + fast-readiness ≈ 40.6s, or 30 + full-burn ≥ 88s | REJECT: `module_start_return` stamped +802ms in 4/4 boots (the SIGTRAP demonstrably fired); neither total is 70.9s |
| H4 second ploader invocation / extra ini entries paying readiness between the two plugins | 2× readiness + more | REJECT: ini law is the 2-plugin `1216aab6` config (staging never writes the ini); ploader is one-shot per launch |
| H5 failed-plugin cleanup timeout (mission's primary hypothesis) | — | REFUTED AT SOURCE (F03/F04): no such timeout or extra wait exists; failure path ≡ success path + one toast string |

**Decomposition verdict:** gap ≈ 0.3-0.5s cleanup (rows 1-4) + **~63-70s path_test
readiness re-payment, full-burn with compute (H1)** + 0.3-6.9s attach + 0.2-0.5s
inject-to-stamp. The composition's failure is INNOCENT: had it returned OK, the
identical readiness burn would sit between the OK toast and path_test. The
composition's refusal stamp merely makes the burn measurable.

**Corollary (flagged inference):** by the same mechanism, composition's OWN
readiness (before `module_start_entered`, invisible to sdiag) most likely burned
similarly — which extends the implied injection age beyond TIMING T07's "~2-28s"
bound, a bound that had silently assumed sleep-only readiness.

## 4. Why the evidence cannot yet split rows 5/6 internally

No ploader-side timestamps exist in any log (toasts are ephemeral and uncollected);
the split inside the gap is source-derived inference with K as the single fitted
parameter. PATCH 03 (steplog) is exactly the instrument that turns this section
from inference into measurement: `injecting 2 plugin(s)` → per-plugin
`OK/FAIL (<dt>ms)` lines bound rows 1-4 vs rows 5-8 per plugin. Apply 03 (and 05)
on the next NECESSARY run; no dedicated diagnostic launch is warranted by this
analysis alone.
