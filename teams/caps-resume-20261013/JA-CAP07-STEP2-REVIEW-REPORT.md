# JA agent report — cap #7 step 2 independent review + kMaxPins hold (2026-10-13)

Scope honored: review + fixes confined to
`caps-team-work/cap07-step2/`; Mac-only; no builds, no console contact, no
original/mod/game file touched. Full verdict with file:line citations:
`outputs/cap07-step2/REVIEW-VERDICT.md`.

## Verdict

CONDITIONAL PASS for the Mac-only source stage. Conditions are integrator
actions only (composition incl. the newly staged profile/CHANGES.json,
whole-build gates + exact PRX build, no reuse of kMaxPins=8 lineage
artifacts, target-time ABI/baseline confirmations, pristine baseline + one
launch per LAUNCH-CARD). Nothing further is required from the source stage.

## Interrupted-review holds — all three resolved

1. kMaxPins 8→16 layout hold: REAL, but the concrete gap was the PIN FILE,
   not another header copy. Consumers enumerated and verified:
   (1) profile/units/CodeImageCache.hpp (recipe-patched candidate),
   (2) profile/CHANGES.json — pins both profile candidate hashes; consumed by
   build_variants G2 (`build_variants.py:285-296`) and l26_packaging_driver
   (`l26_packaging_driver.py:88-134`, lineage/CodeImageCache.hpp) — WAS
   MISSING, now the recipe's fifth exact file (derived sha 97d10855…);
   (3) l19-local-profile-r4/units original (immutable alias source, pin
   re-verified each stager run, never edited);
   (4) package lineage copies (regenerated from (1)+(2); historical ones
   stale). `tools/stage_current.py` now audits all of this and records the
   census in STAGE.json. Staged tree: `derived/ja-final`
   (tests/STAGE-JA-FINAL.log: 5 exact sources, 12 derived files).
2. Missing regressions: `tests/regressions2.cpp` adds 43 assertions —
   choose_order signed boundaries, engine-IsNumeric-parity numbered
   patch-bonus/overflow cases (parity re-verified against pinned
   IPlatformFilePak.cpp:5952-5974), failed/degenerate post-call snapshots,
   wrapper descent/refusal/cycle/unreadable-lower. Mutation red log:
   tests/REGRESSIONS2-RED.log (2/43 fail, exit 1).
3. Aggregate ABI: NOT a blocker. sret contract + 16-byte row layout are
   machine-evidenced (evidence/snapshot-windows.json: thunk 0x17831e0
   sret shape; body stores at 0x1783373/0x1783377), the adapter never
   compiles the genuine FMountedPakInfo type, and a wrong row model degrades
   to MountedUnverified — it cannot false-PASS because Mounted requires the
   engine-reported order to equal the computed effective order
   (RuntimePakMount.cpp:87-88).

## New defect found and fixed by this review

D2: the snapshot delegate met a weaker identity standard than the mount
delegate (receiver only, no member/adjustor). Fixed in
PATCH/NativePakMountAdapter.inl:54-58 (non-null [inst+0x20] member, zero
[inst+0x28] adjustor, bound raised to 0x30); fixture extended to 13
assertions. Exact snapshot member address is not derivable Mac-only and is
listed as a one-time target readback in the verdict conditions.

## Verified clean (selected)

Engine bonus-model parity incl. the production-shaped mixed "2K" part
(engine IsNumeric()=false → version 1; core matches — the interrupted
review's concern does not materialize); +0x18 count offset; runner
failure-propagation re-proven (exit 2 on seeded compiler error); gate
7 files/414 lines PASS; all four recipe original SHA pins still match
(no drift); namespace resolution of the patch anchors (RC::Unreal scope at
UnrealInitializer.cpp original :3184; LuaFNameBridge non-Lua ctor + to_fstring
exist at typescan-r4/include/LuaFNameBridge.hpp:407,533); no detours, no
names, no console contact anywhere in the stage.

## Final host evidence

tests/JA-REVIEW-GREEN.log — gate PASS; 23 + 11 + 43 + 13 C++ assertions;
4 + 4 + 2 Python checks; "PASS focused host suite; no target compilation or
console execution". Receipts refreshed (SOURCE-RECEIPT.json, 64 files;
REVIEW.md marker; NOTES.md/APPLY.md/current-source.patch synchronized).
`derived/current` and `derived/final` are superseded by `derived/ja-final`.
