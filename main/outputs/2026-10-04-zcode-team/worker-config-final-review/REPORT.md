# REPORT.md — worker-config-final-review, 2026-10-04

Role: final config review before first UE4SS runtime start in Silent Hill
Townfall (PS5, UE 5.6.1 SHIPPING, 71959 GUA objects, `5.6.1-0+++UE5`,
build_id `04bb4330a7d71dcefe1cc2c4d19769d5`). Host-only review of staged
config bytes against the composed runtime source. No console contact, no
nested subagents, no edits outside this worker directory, no staged-root or
executable-source modifications. CLAIM.json was written first;
CONFIG-VERDICT.md carries the full analysis; this REPORT is the last artifact.

## Result: GO for all 8 staged files — zero must-fix edits

One conditional contingency (pad provider not ready ⇒ remove the PS5Pad row +
file and re-stage loadlist) and two expectation caveats (pad bridge is a
hard start dependency; USB keyboard cannot trigger UE4SS keybinds) are
documented in CONFIG-VERDICT.md §4/§6.

## What was done

1. **Byte recovery (Task 1).** Located and hashed all 8 staged files in
   `worker-loadlist-final/final-package/`; every size matches the
   console-verified root inventory and the `loadlist.txt` rows. Layout INIs
   are byte-identical to `worker-emit-final-pair/emission-final/`; settings
   INI byte-identical across three more worker copies; main.lua identical
   across four. Full sha256 table in CONFIG-VERDICT.md §1. Flagged the
   obsolete `worker-itemspawner-bundle/bundle` set (221 B settings, 326 B
   loadlist) as not-to-stage.
2. **Settings review (Task 2).** Traced the retained parser
   (`port/ue4ss/src/SettingsManager.cpp`, PROSPERO branch) — the staged INI's
   cited lines (60-68, 112-113) match it exactly. All 9 staged keys are
   parsed; nothing we set is ignored. Established that the zeroed
   Console/GuiConsole/CrashDump values plus `RenderMode=ExternalThread` are
   hard requirements of `validate_native_services` (UP.cpp:736-762) and
   `create_simple_console` (UP.cpp:888-890), and that hot-reload keys must
   stay absent (defaults false; true throws). No log-verbosity key exists in
   the parser — nothing missing there.
3. **ModsFolderPath decision (Task 2).** The build REFUSES non-empty
   `ModsFolderPath`/`ControllingModsTxt` under native activation
   (SettingsManager.cpp:112-114), and `setup_mod_directory_path`
   early-returns with a nominal `ue4ss/Mods` (UP.cpp:851-856). Mods are
   discovered from the loader-built `NativeModSource` snapshot keys
   (`ue4ss/Mods/<Name>/Scripts/main.lua`, NMS.cpp:98-126) and enabled by
   `enabled.txt` presence (NMS.cpp:178-179); `start_mods` uses
   `startup_mod_names()` (UP.cpp:1962-1969). Conclusion: the activation root
   is reachable via the mod source (no path override needed or possible).
4. **Layout review (Task 3).** Staged pair == ParsePair-verified promoted
   pair (worker-emit-final-pair receipts; exit 0, 17 members, `vtables: []`,
   identity round-trip, negative controls 2/3). FUObjectArray 10 keys match
   layout A and the recorded GUA template/live invariants (35477/35476,
   OpenForDisregardForGC 0, 2162688=33×65536, 2/33 chunks, total 0xB8);
   TUObjectArray 7 keys match (total 0x20). All keys accepted by
   NativeLayoutSchema.inc; total-size/width fit enforced and satisfied
   (NLB.cpp:105-116). Promotion scope rung 1→2 with C1–C5 PASS
   (`zcode_PROMOTION_SIGNED`); header identity matches C3/C4 axes. Found ZERO
   entries contradicted by validated evidence; header-only VTableLayout.ini
   is by design (no promoted vtable facts; file must exist and does).
5. **Pad/keybind review (Task 4).** PS5Pad.ini's 6 bindings map 1:1 onto
   main.lua's exactly 6 `RegisterKeyBind` calls (main.lua:694-729); all
   button names are in the parser set (PS5PadConfig.cpp:41-46), all key
   names resolve in `string_to_key` (KeyDef.cpp:22,44-47,117); `User=-1`
   valid; no chord conflicts. Determined the composed PROSPERO runtime has
   no keyboard input provider, so the user's USB keyboard cannot fire these
   binds — pad only. Confirmed staged PS5Pad.ini makes runtime start
   hard-dependent on pad provider activation (NativePadInput.cpp:13-25).
6. **Ancillary files.** `enabled.txt` 0 B is canonical (presence-based);
   `UEHelpers.lua` reachable via the native shared-require path
   (LuaMod.cpp:867-874) and pcall-guarded in main.lua; `loadlist.txt` rows
   byte-verified and rehearsed 46 PASS / 0 FAIL against the real activation
   implementation (`worker-final-staging-rehearsal/REPORT.md`).

## Key findings (essentials)

- GO per file: settings INI, both layout INIs, PS5Pad.ini, main.lua,
  enabled.txt, UEHelpers.lua, loadlist.txt — none require edits.
- ModsFolderPath is ignored+refused by design; mod reachability = snapshot
  keys under the verified activation root; host rehearsal already proved the
  8-file set activates.
- Console/GUI/crashdump zeros and `RenderMode=ExternalThread` are startup
  requirements, not preferences — future edits flipping any of them will
  hard-refuse native start.
- PS5Pad.ini staged ⇒ first launch includes pad-provider activation risk
  (start throws with phase/error if unavailable). Contingency replacement
  documented: delete `file	config	ue4ss/PS5Pad.ini	2960` (loadlist line 9)
  plus the staged file, re-stage loadlist (expected 405 B, row sum 32930 B).
- USB keyboard: game receives it, UE4SS does not (no keyboard source on the
  native input path).
- Harness-log "35897 B" preload figure superseded by the rehearsal's own
  corrected row-sum 35890 B (independently recomputed here).

## Honest limits

- No console or hardware contact: this review cannot prove the pad provider
  activates on real hardware, only that the config path is correct and the
  failure mode is loud (throw, not silent).
- Layout tail keys (UObjectCreateListeners 0x68 … bShouldRecycleObjectIndices
  0xB4) are covered by the promoted pair (C5) and schema/fit checks; the
  directly validated GUA words cover the first five FUObjectArray offsets and
  all TUObjectArray offsets. No contradiction was found anywhere in the
  progress JSON; code-anchor Tick/SCO confirmation remains pending upstream
  and does not gate these files.
- Review-only: CONFIG-VERDICT.md contingency edits were NOT applied to any
  staged material.

## Artifacts

- `CLAIM.json` (first)
- `CONFIG-VERDICT.md` (full field-by-field analysis, citations, verdict table)
- `REPORT.md` (this file, last)
