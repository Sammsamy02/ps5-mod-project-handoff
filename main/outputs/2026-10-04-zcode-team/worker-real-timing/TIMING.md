# TIMING — real-run per-stage measurement (launch-10 runs 1-3)

Worker: worker-real-timing · 2026-10-05 · read-only; every number below is a
within-writer `t_ms` delta from
`outputs/2026-10-04-zcode-controller/launch10-evidence/run{1,3}-sdiag.jsonl`
plus the UE4SS `[t=...ms pid=...]` / `PATH_TEST` monotonic-ns stamps.
Provenance and honesty flags: `CLAIM.json` (T01-T09). Model being checked:
`worker-injection-latency/LATENCY-ANALYSIS.md` §4 ("~5-10 s + C_comp").

## 0. What the evidence actually is

- The on-console supplier-diag jsonl **accumulates across boots**:
  `run1-sdiag.jsonl` is byte-identical to the first 548 lines of
  `run3-sdiag.jsonl` (cmp-verified). `run3-sdiag.jsonl` therefore holds three
  boot epochs, one pid writer each, seq restarting at 1 per writer:
  **pid 317 = run1**, **pid 341 = run2** (UE4SS run2 log confirms), **pid 355 = run3**.
- All three pids are the same instrumented composition build (235 ctors, worker,
  buffered Lua probe). No cross-writer delta is used anywhere in this document.
- **Clock identity (validated 3/3 exactly):** sdiag `t_ms` and the UE4SS
  `[t=...ms]` stamps are the same monotonic ms clock — pid 317 refusal stamped
  60004266 in both; pid 341 60249877 in both; pid 355 60900581 in both. The
  `PATH_TEST` monotonic-ns stamps divide by 1e6 onto the same base.
- One writer per pid; refusal reasons differ per run:
  - run1 (317): `runtime_start_failed code=3` — "PS5Pad.ini must be ASCII text
    without control bytes" (pre-sanitized INI).
  - run2 (341): `mfa_activate_failed code=-8` — staged size file shorter than
    loadlist says (`ue4ss/PS5Pad.ini`); chain aborts before the VMMAP walk.
  - run3 (355): `runtime_start_failed code=3` — "PS5Pad input unavailable:
    phase=6, error=-2137915388" — **scePadOpen → 0x80920004
    PS5_PAD_ERROR_ALREADY_OPENED** (mission's "0x80A40004" corrected; phase 6 =
    `PadPhase::Open` per PS5PadSource.hpp:10-11; constant per pad_sdl_ps5.h:21).

## 1. Per-stage table — full startup chain (per-writer deltas only)

Canonical: **pid 355 (run3)**, absolute t0 = 60899781 ms. pid 317 (run1) shown
for reproducibility; pid 341 aborts at MFA (+704 ms) and skips stages 7-15.

| # | Stage (sdiag tags) | pid 355 t(+ms) | Δ pid 355 | pid 317 t(+ms) | Δ pid 317 |
|---|---|---|---|---|---|
| 1 | module_start_entered → entry dispatch (`entry_call_sp_mod16`) | 0 → 4 | 4 ms | 0 → 5 | 5 ms |
| 2 | import validation (187 imports, 0 missing) | 5 → 8 | 3 ms | 8 → 10 | 2 ms |
| 3 | **235 static constructors** (`constructor_begin/return`, `ctor-N`) | 12 → 691 | **679 ms** | 14 → 696 | **683 ms** |
| 4 | constructors complete + worker spawn/entry | 693 → 695 | 2 ms | 697 → 700 | 3 ms |
| 5 | runtime wiring + MFA activate (`mfa_activated_low/high`) | 699 → 707 | 8 ms | 702 → 709 | 7 ms |
| 6 | **VMMAP walk #1** (`bridge_vmentry_entries`=10028 … `bridge_exepath_sig`) | 712 → 723 | 11 ms | 717 → 729 (22761 entries) | 12 ms |
| 7 | stat checks (`stat_app0*`, `stat_ownmod*`, `stat_rootdir*`) | 728 → 735 | 7 ms | 734 → 741 | 7 ms |
| 8 | **lease registration** (`bridge_lease_reg_spans`=6, `bridge_lease_reg`, `bridge_spanpub_*`) | 738 → 746 | 8 ms | 744 → 752 | 8 ms |
| 9 | VMMAP walk #2 re-proof (same tags, second occurrence) | 751 → 762 | 11 ms | 760 → 771 | 11 ms |
| 10 | anchors (6 × `anchor_cov`/`anchor_match` + `anchor_c_either`) | 763 → 779 | 16 ms | 773 → 788 | 15 ms |
| 11 | cfg dirs (`bridge_cfg_gamedir/workdir`) | 781 → 782 | 2 ms | 790 → 791 | 2 ms |
| 12 | **lmf conversion** (`lmf_extent_*`, `lmf_anchor_count`=7, `lmf_intervals_found`=1) | 783 → 790 | 7 ms | 793 → 800 | 7 ms |
| 13 | runtime construction → refusal (`runtime_start_failed`=3; pad open ≈ +790-800, bounded) | 790 → 800 | 10 ms | 800 → 810 (INI parse) | 10 ms |
| 14 | **module_start_return** | **803** | — | **813** | — |
| — | *worker thread, after return:* sleep | 695 → 5731 | ~5.04 s sleep | 700 → 5736 | ~5.04 s |
| 15 | buffered Lua probe (88 lines) | 5732 → 5742 | 10 ms | 5737 → 5747 | 10 ms |
| — | last sdiag record | 5742 | — | 5747 | — |

pid 341 (run2): stages 1-5 identical shape; `mfa_activate_failed` (0xfffffff8)
at +704 ms, `module_start_return` at **+718 ms**; walk/lease/anchor/lmf absent
by design (fail-closed abort); worker tail identical (+5732 → +5744).

## 2. C_comp — the number the projection needed

| Definition (mission's two options) | pid 317 (run1) | pid 341 (run2) | pid 355 (run3) |
|---|---|---|---|
| **entered → module_start_return (ploader `pt_call`-blocking, game-thread occupancy)** | **813 ms** | 718 ms (early abort) | **803 ms** |
| entered → last sdiag record (includes post-return worker) | 5747 ms | 5744 ms | 5742 ms |

**C_comp = 0.80-0.81 s** (full-chain runs; 0.72 s on the MFA-aborted run).
Reproducible across boots, spread ±5 ms. Composition of C_comp: the 235-ctor
block is ~679-683 ms = **~84-85 %** (flat: max single-ctor gap 5 ms); every
measured "expensive" stage (VMMAP walk ×2, lease, anchors, lmf conversion) is
7-16 ms each. The extra ~4.93-5.03 s to the last record is the module's own
worker thread sleeping ~5.04 s and then running a ~10-15 ms buffered Lua probe
**after** `module_start_return` — it blocks neither ploader nor the hijacked
game thread.

## 3. Projection verdict (~5-10 s + C_comp, C_comp unknown)

- **C_comp resolved: 0.8 s, 37× under the 30 s `pt_call` cap.** Residual risk 1
  of the latency analysis (C_comp > 30 s → FAIL despite working) is closed.
  Updated band: ~5-10 s + 0.8 s ≈ **~6-11 s** of ploader-side serial wall clock.
- **User-visible wait:** the only game-thread-blocking mod cost measured is the
  0.80 s composition `module_start` (path_test's `module_start` writes one
  stamp line and is trivial per the pinned analysis). Readiness polling runs
  outside the process; attach stop windows are the only other stops (small,
  unmeasured). The user seeing the game load normally with 2 plugins is
  **consistent** with the measurement — including the fact that in all three
  boots the composition refused fail-closed and the game booted anyway.
- **Fraction, composition vs ploader overhead:** measured serial chain per boot
  = composition `module_start_entered` → path_test `module_start` =
  72.6 s / 71.4 s / 71.7 s. Composition occupancy: 0.80 s ≈ **1.1 %**; ploader
  overhead ≈ **99 %**. Counting the pinned 120 s post-FAIL idle tail (S03/S04),
  composition ≈ 0.42 % of ~193 s. Boot-to-boot spacing is consistent with that
  tail (run1 PATH_TEST +120 s → run2 t0 is +53 s later; run2 → run3 gap 459 s
  = tail + human restart; consistent, not proven — T09).
- **NEW measured fact the projection missed (T05):** the inter-plugin gap
  (composition refusal → path_test module_start, same clock, same boot) is
  **71.8 / 70.7 / 70.9 s** across the three boots — far above the 2-3 s modeled
  for path_test and above the pinned source's worst case (~25 s readiness S06 +
  ~7 s attach retries). It is measured end-to-end but NOT decomposed; per
  source it should not stop the game process (userspace polling), so it matches
  "loads normally", but it is now the dominant mod-side wall-clock term and the
  target for PATCH ploader-03 (step log) instrumentation.
- Pre-`module_start` costs (composition readiness, attach, 30 MB kernel load)
  happen before the first sdiag stamp and remain unmeasured; the 5-10 s band
  for them is neither confirmed nor refuted — only made irrelevant to
  game-thread blocking.

## 4. Injection timing vs game boot — pad-defer retry window (T06-T08)

- **Live fact:** pad open is attempted during composed-runtime construction at
  ≈ **+790-800 ms** into `module_start` (bounded: `lmf_intervals_found` +790 →
  `runtime_start_failed` +800, pid 355) and returns **0x80920004
  ALREADY_OPENED** (phase 6 = `PadPhase::Open`). Single-shot: runtime-status
  written once at the refusal (t=60900572 = +791 ms), no retry in any run.
- **How early is injection?** No process-spawn stamp exists in the evidence, so
  bounds only: ploader polls pids at 50 ms (S03/S07), readiness minimum ~1.6 s
  (S08) up to 25 s if settling (S06) → `module_start` begins within roughly the
  first **~2-28 s** of process life when ploader is already waiting. The walked
  module space is small (10,028-22,761 VM entries), consistent with early boot.
- **Inference (flagged):** the game's pad stack was already holding an open
  handle at that earliest practical moment — the game opens its pad within the
  first seconds of boot, **before** injection, not after. The pad-defer fix's
  window is therefore **not** "until the pad service is up"; it is "until the
  game's exclusive handle is free", and when/if the game releases it is
  unmeasured (no successful open exists anywhere in this evidence). A fixed
  boot-relative delay is unjustified; the fix should retry event-driven
  (re-attempt `activate()` per poll/tick or on Disconnected→Active
  transitions; each attempt is bounded and cheap).
- Lower bound for any deferred retry: the same process was still executing
  injected `module_start`s **≥71-73 s** after injection (PATH_TEST stamps), so
  a deferred retry lands while the game is still up.

## 5. Reproduction

Extract with: `python3` over `run3-sdiag.jsonl`, group by `pid`, take
`t_ms - t_ms(seq where tag=="module_start_entered")`; key rows are
`module_start_return` and `buffered_lua_result`. Cross-check refusals against
the three `[t=...ms pid=...]` lines in the UE4SS logs (must match exactly) and
PATH_TEST `monotonic-ns/1e6`. Input hashes are pinned in `CLAIM.json`.
