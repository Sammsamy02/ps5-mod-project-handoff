# REPORT — worker-ploader-70s

Worker: worker-ploader-70s · 2026-10-05 · read-only decomposition + proposal-only
patch. No console writes, no builds, no engine files touched. Scratch: /tmp only.

## Mission verdict in one paragraph

The 70.7-71.8s inter-plugin gap (4/4 boots, measured: 71.83 / 70.71 / 70.87 /
70.96s) is **not** a failed-plugin cleanup cost. At source level the failure path
is byte-identical to the success path — a refused `module_start` still returns
through `pt_call`'s SIGTRAP success branch (shellcode `INT3` fires on return,
main.c:93-96, 280-289), then the SAME mprotect → munmap → notify(string only) →
restore_creds → detach sequence runs; there is no cleanup timeout, no cleanup
retry, and no retry of the failed plugin (known-list one-shot, main.c:755-764;
loop continues, main.c:819). The dominant term is **path_test re-paying the
per-plugin readiness wait** (main.c:784) whose "25s" budget is a TICK budget —
500 ticks × 50ms with no clock check (main.c:518-559) — and whose ticks each also
pay ~264 kernel backdoor calls (main.c:531, 538 → 492-514): a full mid-boot burn
is ~64s wall, matching all four boots with a single fitted per-call cost
K ≈ 0.3ms. The mission's "~32s source worst case" was the sleep-only bound of the
SAME single path (25 + 6.9 + ~1), not a distinct failure-path flow; the latency
worker's "~2-3s" was its fast case.

## Evidence work performed

- Re-verified the pinned source sha256 `2b521cd7…` (851 lines) this session.
- Re-derived run4 from raw evidence rather than trusting the mission text: pid 370
  refusal `[t=62842697ms]`, PATH_TEST 62913653732826ns → gap 70.957s; sdiag shows
  refusal +798ms, `module_start_return` +802ms → C_comp(run4)=802ms, same shape as
  runs 1-3. Clock identity per worker-real-timing's 3/3 exact validation reused.
- Confirmed the live ini law: 2-plugin config `1216aab6…`, composition=2 first,
  path_test=2 second/last, both mode 2 (LAUNCH-DAY-10 pre-state; staging never
  writes the ini) — rules out extra ini entries between the two plugins.
- Enumerated all 8 waits between the two `pt_call`s with source lines and
  sleep-only vs compute-inclusive worst cases (GAP-ANALYSIS.md §2).
- Signature-tested 5 hypotheses; H1 (readiness full-burn + tick compute) is the
  only fit; H5 (mission's cleanup-timeout hypothesis) refuted at source; H2/H3/H4
  rejected with reasons and a cheap discriminator (composition FAIL toast `dt`,
  recorded durably by PATCH 03's steplog).

## Deliverables

- `CLAIM.json` — pinned facts F01-F11, boot table, hypotheses H1-H5 with verdicts,
  honesty flags (K is fitted, not measured; gap internal split is inference until
  patch 03 lands; readiness1 unmeasured).
- `GAP-ANALYSIS.md` — decomposition + signature match, all source line refs.
- `PROPOSAL.md` — bounded fix: keep PATCH 02 (hoist-readiness, the actual gap fix,
  ~63-70s saving) + new 5th patch `05-wallclock-deadline.diff` (makes max_ms a true
  wall deadline; caps the single remaining burn at ~25.3s; zero fast-path change;
  applies standalone and after 01-04 at zero rejects). Ini-order mitigation
  (path_test first) evaluated and REJECTED: masks the metric, saves ~0 real
  seconds, breaks the documented PATH_TEST terminator guard.
- `PATCHES/05-wallclock-deadline.diff` — the diff (sha256
  `9c0ec7e1…`; post-series tree sha256 `d59da3ec…`).

## Key numbers

| Config | Gap | Saving |
|---|---|---|
| Today (measured) | 70.7-71.8s | — |
| 02 only | 0.8-7.9s | ~63-70s |
| 05 only | ≤ ~32.7s | ~38-41s (settling) |
| 02+05 (recommended, with 03 steplog to verify) | ≤ ~8s | ≥ ~100s serial worst-case |

## Honesty limits

The gap is measured; its internal split is source-derived inference — no
ploader-side timestamps exist in any log, and K is a fitted parameter, not a
datum. PATCH 03 (steplog) on the next necessary run converts the inference into
measurement; acceptance criteria are written in PROPOSAL.md §4 such that a
refuted H1 still leaves 05 harmless and points at the real sink. Nothing here
claims the fix is applied or verified on console — proposal-only, per scope.
