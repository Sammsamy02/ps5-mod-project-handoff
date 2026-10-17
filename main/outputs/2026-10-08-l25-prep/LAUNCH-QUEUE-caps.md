# Console launch queue: capabilities 1-12 (integrator, 2026-10-13)

One launch per capability, in this order. Every console step needs the user's OK for that step.
Mac work is done ahead of time, so each session is only the console steps plus the launch.

## Done on the console (2026-10-13)

| Launch | Capability | Runtime | Verdict | Report |
|---|---|---|---|---|
| A | #7 pak mods (Leon via paktx) | R11 | PASS: `[pk] T1 found=8/8`, fog back | `evidence/runtime/cap07-launch-20261013/REPORT-cap07-launch-A.md` |
| B | #8 intake (Leon) | R11 | PASS: `[zi] SUM ... state=PASS`, checker as predicted | `evidence/runtime/cap08-launch-20261013/REPORT-cap08-launch-B.md` |
| C | #4 early loading | R13c `ab2642f3` | PASS for #4, but the build lacked B7 (`mode=longjmp`) | `evidence/runtime/cap04-launch-20261013/REPORT-cap04-launch-C.md` |
| D+E | #2 shadow line + #5 crash capture (combined, user's choice) | R14 `37e034a4` | PASS both: `[AutoProfile] shadow ... verdict=MAT`; crashcap names `modbridge_cap05_crashtest_fault` + fault VA 0xc08; B7 back (`mode=cxx-throw`) | `evidence/runtime/cap0205-launch-20261013/REPORT-cap02-cap05-launch-DE.md` |
| F | #9 C++ mods | R15c `7e681540` | PASS: C++ mod loaded/started, hook fired, F9 speed 2.00/1.00; self-test 161/168 | `evidence/runtime/cap09-launch-20261013/REPORT-cap09-launch-F.md` |
| G | #11 C++ mod windows | R15e `53a64553` | PASS: window shown on the right (scale 2.0), 76/76 ImGui calls, button 2x speed; blurry at 4K (follow-up) | `evidence/runtime/cap11-launch5-20261013/REPORT-cap11-launch-G.md` |
| H | #12 Leon full look | R15e `53a64553` | PASS with gaps: Leon's textures show (30 PS5-layout textures, 10 R<->B); glasses checker (invisible route not yet in), some squares on clothes (to classify), hair checker (refused); self-test 161/168 -> R15e new rollback | `evidence/runtime/cap12-launch-20261013/REPORT-cap12-launch-H.md` |

Leon stays installed (undo: `paktx restore --plan evidence/pristine-paks-20261013T144738Z/leon/plan.json`).
Rollback runtime stays R11 `bfd95db6` until R14 passes.

## Next

- **STATE 2026-10-14 ~01:50Z (user away):** next build = **R17b** = R16d + #11 follow-ups + #10 + #7 step 2 game-paks
  source. Gates + smoke compile PASS on the live tree (`evidence/builds-20261012/pregate-R17b-*/SUMMARY.md`).
  **Blocked on one user OK**: the stale #10 `package_helper` pin
  (`evidence/builds-20261012/R17-combined-cap10-cap11fu/PACKAGING-STOP-DECISION.md`). Launch card:
  `LAUNCH-CARD-R17b-combined.md`.
- **Combined launch (R17b):** #12 round 6 (installed on the PS5), #11 sharp window, #10 C# probe, #7 empty `~mods`
  listing check (`round17-staging/deploy_cap07s3.sh empty`), K1 keyboard probe, zzpathprobe.
- **Then #7 Leon launch (no rebuild):** paktx restore #12 -> `deploy_cap07s3.sh leon` -> launch.
- **Then Stellar Blade core blind test.** Readiness map DONE (`evidence/sb-readiness-20261014/READINESS.md`, Mac-only):
  today's runtime is tied to Townfall (compiled title ID, Townfall memory windows, ~11 compiled per-game headers) and
  would stop safely at start-up on Stellar Blade. 8 generic blockers (B1-B8), ~12-20 agent-days; no per-game build
  needed. Team S1 DONE, STAGED (`../sb-team-S1`: PROFILE-FORMAT.md, freestanding loader, s1.patch 8 files; B5 pin split,
  B6 5.6 offsets -> profile, B7 engine version Detect; Townfall equivalence 88/88; loader tests 17/17; gates PASS).
  Not applicable alone: needs S2's start-up inputs (paused) + the header->loader rewiring and bridge install wiring
  (APPLY.md). Reviewer focus later: B6 relaxes the staged-vs-baked offset compare to presence. Follow-ups S4/S5. And
  Team S2 (B1 title detection, B2 image anchoring, B8 readable executable): **stopped twice by safety checks
  (B8 planning; then Townfall executable analysis) and paused for the user's decision; not retried.** Generator team S3 DONE
  (`../sb-team-S3`, B4): emit_profile.py writes the S1 format; Townfall two-source check PASS (39/39 scalars, 73/73
  layout offsets; GMalloc base slot 0x930add8 + vtable 0x7ec26e0 from a real second channel). Gaps fail closed: pak
  mount set, delete-listener ops, thread-name accessor (features off), 4 vtable slots, exe_path (B8 paused);
  FProperty.InitializeValueInternal 0x158 vs 0x150 unresolved (needs a second source). Branch suites 330 OK. Read-only console facts: Stellar Blade dump
  `/data/homebrew/PPSA13197-app0` (v01.014.001, UE4, project `sb`, eboot fake-signed `5414f5ee` = plaintext
  segments, so no upload is ever needed). Its executable is NOT downloaded yet (SB prep waits for the round to end).
  S1 (should): "already mounted ~mods" check before the game-paks source mounts.
- **K2 keyboard runtime: STAGED, not applied** (`caps-team-I/caps-team-work/keyboard-mouse/k2/`, APPLY.md + LAUNCH-CARD.md).
  49 host tests / 5537 checks PASS (ASan/UBSan, TSan), gates PASS, 0 new imports (object-level check), one review
  SHIP-WITH-FIXES with 0 blockers (fixes applied, not re-reviewed). Base = live LuaMod.cpp with cap07s3 + UE4SSProgram.cpp
  b6a68e3f. Apply only after K1 (zzkbprobe, rides on the R17b launch) shows the engine sees the USB keyboard; then one
  build + launch K2. Risk to watch: controller lookup in the marshal hook (heavily throttled). Two scratch tree copies
  are left in k2/gates/{pipeline/mirror-final,ratchet/tree-final} (agent's rm was refused): user to remove; never commit.
- **Parallel Mac work:** sound/video base facts
  (`evidence/capH-av-fetch-20261014T011626Z/SUMMARY.md`; needs one real PC sound mod + one PC movie mod);
  Team J saves/assets done (`caps-team-J/caps-team-work/saves-assets/`: BP LogicMods blocked generically by
  IterateGameDirectories on PS5 + pak mounting; saves need a C++ loader).

## Later launches (staged, not built)

- **#11 / #9:** DONE (launches F, G).
  #11 `ba467508`, 0 new imports. The runtime patch matches R13c (25/25). The runtime build waits until after the
  #5 launch. Order note: #11 needs #9's loader.
- **#12:** picker redesigned and run on real data. It now refuses see-through and special materials; 418 game
  materials were checked.
  - Default: no change (fails closed).
  - With 2 user decisions ("accept a partial look", "treat Leon's skin/clothes as solid"), 10 of 11 parts use the
    game's own skin material with Leon's colour and bump textures. The hair stays checkered.
  - Team A is adding the pak-tool step that installs those re-pointed files.
- **#7 step 2** (mount without changing files): the fixer stopped (safety check). Review BLOCK B1-B3 and
  F1-F5 are open.
- **#1/#3 kernel and #10 C# host:** stopped by safety filters. Nothing was delivered.

## User decisions (answered 2026-10-13)

1. Restore the original `pakchunk0-ps5.pak`: done.
2. #8: the checker on Leon is accepted for test #8 only. #12 must fix it.
3. #12: the goal is the FULL look of the mod (every part, hair included), not a partial look. Method is the
   integrator's call.
4. #9 before #11.
5. #7 step 2 and #10 are being built by GPT (Codex); #1/#3 by ZCode. After #9, #11 and #12: integrate GPT's #7 step 2 and #10, then ZCode's #1/#3 last (kernel: explicit user OK + 3 passes across restarts). Each goes through the same rules: one independent review, gates, one build, one launch.
