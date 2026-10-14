> **Current Mac result — V2 (2026-10-13):** #7 step2 and #10 isolated Mac work is accepted for Claude integration. Use [GPT-7-10-MAC-READY-V2-HANDOFF-2026-10-13.md](/WORKSPACE/Documents/Codex/2026-10-12/files-pasted-by-the-user-yes/outputs/GPT-7-10-MAC-READY-V2-HANDOFF-2026-10-13.md). Authoritative combined stage: `caps-team-work/cap10-integration-ready/derived/target-ready/`. It includes the refreshed #7 metadata, fixed v2 signal owner/runtime pair, real build/package wiring and all four binary inputs. Separate broader PAL and controlled memory-observation evidence are delivered. Root/independent review accepted; combined game/package/console tests remain Claude's. Seven SceNet identities may still need genuine provider evidence. No PS5 or main/frozen build was touched. Earlier result banners and details below are historical where superseded.

> Added roadmap item: **#13 — downloaded PC DLL mods**, owned by ZCode after #1–#12. Assignment: [CAP13-DLL-MODS-ZCODE-HANDOFF.md](/WORKSPACE/Documents/Codex/2026-10-12/files-pasted-by-the-user-yes/outputs/CAP13-DLL-MODS-ZCODE-HANDOFF.md). Planning only; no implementation or console work. The #7/#10 packet remains frozen.

> Current result (2026-10-13): isolated Mac component work/review is complete for #7 step2 and minimal #10. **Start with [GPT's final Claude handoff](/WORKSPACE/Documents/Codex/2026-10-12/files-pasted-by-the-user-yes/outputs/GPT-7-10-MAC-FINAL-HANDOFF-2026-10-13.md)**. #7 uses `caps-team-work/cap07-step2/derived/review-final/` (complete15-file patch); preserve Claude's later metadata delta. #10 now has a genuine target Mono archive, static target corlib, actual-owner target compile/link and actual production static-owner Mac pass. Read `caps-team-work/cap10/MAC-INTEGRATION-HANDOFF.md`. No PS5 contact, main/frozen edits or PRX build occurred. Claude owns merge, combined build/package and target test. #1/#3 stay with ZCode. All details below are historical and superseded where they differ.

# Handoff: #7 step 2 and #10 to ChatGPT (2026-10-13)

Per user direction: #7 and #10 are yours. All source, tests, receipts,
reviews, and unresolved findings are preserved in this worktree. The
independent review agents (JA for #7, JB for #10) have delivered or are
completing their verdicts. Nothing has been applied to the integrator's
main tree.

## #7 step 2 (Runtime pak mount)

Location: `caps-team-work/cap07-step2/`

### Current state
- Implementation complete: adapter, producer, source patch, 44 host assertions
- Independent review (JA): **CONDITIONAL PASS** — REVIEW-VERDICT.md delivered
  - kMaxPins 8→16 layout hold: FIXED (5th file added to stager, all consumers audited)
  - Missing regressions: ADDED (43 new assertions, mutation-tested)
  - Aggregate ABI: NOT A BLOCKER (sret contract machine-evidenced, adapter degrades safely)
  - New defect D2 found and FIXED (snapshot delegate identity check strengthened)
  - Full host suite green twice (90 C++ assertions + 10 Python checks)
  - Conditions for unconditional PASS are integrator-only (apply together, build, launch)
- Stager: tools/stage_current.py (validated, reproducible — JD confirmed)
- Source receipt: SOURCE-RECEIPT.json (hashes verified)

### Key files
- NOTES.md — implementation summary
- PATCH/ — all source files (RuntimePakMount.cpp/.hpp, adapter, producer)
- derived/final/ — the final generated tree (NOT derived/current/)
- tools/stage_current.py — the stager (4 original SHA/size/anchor contracts)
- LAUNCH-CARD.md — the launch procedure
- HISTORICAL-REVIEW.md — the original Team A review
- REVIEW.md — status marker (review pending)

### Resolved by JA's review
1. kMaxPins layout: FIXED — profile/CHANGES.json added as the stager's 5th
   exact file; all 4 consumer sites audited and recorded in STAGE.json
2. Missing regressions: ADDED — 43 assertions in tests/regressions2.cpp
   covering signed boundaries, patch bonus overflow, failed snapshots,
   wrapper descent; mutation red log proves they bite
3. Aggregate ABI: CLEARED — machine-evidenced sret + row layout; a wrong
   row model degrades to MountedUnverified (cannot false-PASS)
4. New defect D2: FIXED — snapshot delegate identity check strengthened
   (was receiver-only; now checks member + zero adjustor)

### Remaining for unconditional PASS (integrator-only)
- Apply all 5 transformed files together (staged in derived/ja-final/)
- Whole-build gates + exact PRX build
- No reuse of kMaxPins=8 lineage artifacts
- Target-time ABI/baseline confirmations
- Pristine baseline + one observed launch per LAUNCH-CARD

### Integration targets (from SOURCE-RECEIPT.json)
- UnrealInitializer.cpp, LuaMod.cpp, CodeSignatureProfile.hpp, CodeImageCache.hpp
- Final LuaMod baseline SHA: fd53a4f295390677...
- Source drift must refuse (never overwrite the integrator's work)

## #10 (C# runtime)

Location: `caps-team-work/cap10/`

### Current state (JB's independent review — COMPLETE)
- Verdict: HOST-PROVEN, TARGET-UNPROVEN, INTEGRATION-STALE
- Probe.dll: genuine Roslyn compilation, sound provenance
- Three real host runs: CoreCLR + matching Mono + Mono interpreter (all green)
- System.Native failure: ROOT-CAUSED AND FIXED (config dir path)
- Production loader source: staged, correct, but needs PS5 Mono port

### Key files
- REVIEW-VERDICT.md — the complete independent review
- managed/Probe.cs + Probe.dll — the actual plugin (3,584 B)
- native/ — production owner/bridge/observer source
- evidence/ — all receipts (COMPILE, HOST-CHECK, MONO-HOST-CHECK,
  MONO-INTERPRETER-CHECK — all green)
- tools/stage_current.py — the stager (3 anchors in UE4SSProgram.cpp)
- tools/stage_mono_loader.py — the Mono loader patch stager
- tools/run_mono_interpreter_check.py — the FIXED interpreter harness

### Unresolved findings
1. UE4SSProgram.cpp pin drifted (fe9fe7bc → ad3ae621) — needs one-line re-pin
2. PS5 Mono port does not exist (GC/threads/EH/interpreter for Prospero)
3. Strict loader patch uncompiled (correct source, no target toolchain run)
4. In-game Run → Throw → recovery unproved
5. Full Unity/IL2CPP proof deferred (follows the minimal Townfall test)

### System.Native fix (from JB)
- Root cause: MONO_CFG_DIR set to PREFIX/etc/mono but Mono appends mono/config
- Fix: set MONO_CFG_DIR to PREFIX/etc (one line in run_mono_interpreter_check.py)
- Rerun passes: MONO-INTERPRETER-CHECK.json = REAL_MONO_INTERPRETER_HOST_PASS
- Advisory: same off-by-one exists in the embedding harness (masked, untouched)

## Integration plan (from JD — for both components)
- Location: caps-team-work/outputs/INTEGRATION-PLAN.md
- Composition order: #7 first (current pins bind exactly), #10 second (re-pin),
  round5 items last (each as labelled builds)
- #7∩#10 = empty (proven)
- #7 is stageable right now (transform re-validated end-to-end)
- #10 needs the one-line re-pin before staging

## Constraints
- Do NOT apply anything to the main integrator tree until Claude finishes
- Do NOT touch the console
- Preserve all existing work — this is an addition, not a replacement
- The main tree's staged round5 patches (lookup-speed, GROUP B, shim-fix,
  lua-alloc) are separate and come from a different work stream
