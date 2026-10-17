# Independent review: capability #8 (Team A, whole-chunk intake)

Reviewer: the one independent reviewer for this deliverable. All checks below are **Mac-only**. I made no
console contact and changed no deliverable file; this REVIEW.md is the only file I wrote here. I ran the tests on a
scratch copy of the folder, because `test_leon_real` rewrites `evidence/leon-intake/`.

## Verdict: PASS-WITH-FIXES

The Mac-side intake tool is correct and generic, and it was tested against real files. It changes nothing in the
candidate tree, needs no PRX build, and R11 `bfd95db6` stays the runtime. Both explanations of what the user saw hold
up. Before the one #8 launch, three things should be fixed: the zzintake probe's timing, its PASS line, and one
missing precondition on the launch card (F1-F3). The other items are smaller.

## Host tests (re-run by me)

- `bash tests/run_tests.sh` on a copy: **46 tests ran, 0 failing modules, PASS**. Wall time was 88 s.
- Per module: test_formats 17, test_intake_synthetic 13, test_diag_lua 3, test_config_and_gate 6, test_leon_real 7.
  0 skipped.
- The output matches the team's `tests/test-output.txt` line for line.
- The regenerated outputs are byte-identical:
  - `leon-base-fetch-plan.json` matches.
  - zzintake `main.lua` sha256 is `1da67e61…`, the same as theirs.
- The tests are real tests, not empty passes:
  - The synthetic suite makes 42 assertions, including refusal paths.
  - The real-file suite checks the Leon mod against the game's own TOCs. It checks 127 CityHash package ids against
    the cooker-written chunk ids, plus the IoHash of every chunk. This is a real second source for the parsers. A
    self-consistent fixture writer alone would not prove the parsers.

## Independent checks that held (Mac-only)

- **Mod and base files.**
  - The mod file hashes in the report match the local Leon trio: utoc `64503ced…`, ucas `1f10e60e…`.
  - The utoc sha equals the console copy the integrator pulled on 2026-10-13
    (`evidence/console-pulls-20261013-paks/SHA256SUMS`).
  - The mod files are unchanged after the run.
- **Base TOC.** The base `pakchunk0-ps5.utoc` used is the ORIGINAL one: 29,707,562 bytes, the same size as the
  console's `.original-*.bak`. It is not the rewritten 29,870,724-byte one. The fetch-plan offsets therefore point
  into the untouched `pakchunk0-ps5.ucas`, which is correct.
- **Material verdict.** I recounted it with my own script.
  - 13 root material packages carry D3D bytecode: DXBC, DXIL and SFI0 chunks. They are M_Leon_Body/Skin/Hair,
    2x M_Invisible and Simon_Slot0-7 (105,091 B each, 15 DXBC each).
  - None of them carries a PS5 token.
  - 43 replaced materials all chain to M_Invisible.
  - Leon's checker look is consistent with the report.
  - The older l25-prep `shader-fallback/fallback.md` says only "5 packages carry inline PC shader payloads". That
    count is wrong: it misses Simon_Slot0-7. Team A's count is the right one.
- **Bright outdoors.** I ran `config_disclosure.py` on the integrator's console-pulled
  `PS5Engine.ini.current` and `.original`. It reports 84 settings, with the look categories fog, exposure, shadows,
  brightness-colour and indirect-light, and `r.ShadowQuality` 1 instead of the game's 3. This is a second source,
  independent of the team's Nexus-ini run.
- **Generic code.** `grep -i "leon|simon|townfall|ps5|PPSA|192.168"` over `PATCH/pak-intake/*` finds 0 hits. The
  target format SF_PS5 is learned from the base ShaderArchive names.
- **No forbidden contact.** The tools use no network, no FTP and no subprocess. Nothing comes from an SDK.
- **No detours.** No hooks of any kind. zzintake only reads objects, through StaticFindObject, IsValid,
  GetFullName and `.Parent`.
- **Fail closed.** Compressed, encrypted, signed or multi-partition containers are refused, as is any unknown
  TOC, container-header or Zen version. If base headers are missing, the result is INCOMPLETE with a fetch plan.
  An ambiguous material parent gives UNKNOWN, which means INCOMPLETE. The gate requires the exact mod sha256, a
  non-zero item count and a recorded acceptance of the visual loss.
- **Probe reachability.** I checked statically that every one of the 18 probe objects can be reached through
  package imports from at least one replaced package. That shows they *can* load. It does not show they *do* load:
  see F2.

## Findings

### F1 FIX-SOON (before the launch): the probe stops 150 s after the script starts, not after you are in game
- **Where:** `PATCH/pak-intake/zzintake_template.lua` lines 43-49, and the LAUNCH-CARD "What you do in the game"
  section.
- **What happens:**
  - `LoopAsync(30000, ...)` with `left=5` probes at about 30, 60, 90, 120 and 150 s after UE4SS loads the mod,
    then stops itself. `test_diag_lua` asserts exactly 5 `SUM` lines.
  - Lua mods start at runtime start, before the menus. Boot, title screens, menu and loading the save can easily
    take longer than 150 s.
  - In that case every `SUM` line shows found < 18 and `f=0`, which the card reads as FAIL ("mount missed new
    packages"). One launch is wasted on a false FAIL.
- **Fix:** keep probing until all objects are found (or keep probing every 30 s with no limit, as zzdiag-v11 does
  at `LoopAsync(18000)`), and/or add a key-bind probe. Make the card's PASS read the **last** `SUM` line taken while
  you are in game.

### F2 FIX-SOON (before the launch): `found=18/18` is not a sound PASS line
- **Where:** `chunk_intake.diag_entries`, and the LAUNCH-CARD lines "Today's file expects found=18/18" and
  "FAIL: f=0".
- **What happens:**
  - The probe set includes replaced packages that the base game may never load during a normal play session:
    - P12/P13 `/Game/MetaHumans/Simon_Head/Face/Materials/Baked/MI_HeadSynthesized_Baked{,_LOD1}`
    - P17 `/Game/MetaHumans/Simon_Head/Face/Simon_Head_FaceMesh`
  - These are the non-"_5_6_Optimised" head. Each one can be reached only from itself, and I found no evidence
    that the game loads them.
  - If they never load, `f=0` is guaranteed, and the card calls it FAIL even though the mount worked.
- **Fix:**
  - Mark `f=0` as "not loaded (inconclusive)", not FAIL.
  - Define PASS on the mod's own new objects that Simon's in-use meshes reach (`M_Leon_*`, `MI_Leon_*`,
    `T_Leon_*`) plus at least one replaced object that is proven loaded.
  - Better still, choose probes generically: objects reached from the largest number of replaced packages first,
    and skip objects reached only from themselves.
- **Also:** `par=0:none` means the `.Parent` read failed or returned null. That is a runtime or property problem,
  not "the Mac predicted the parent wrong". The card should treat it as a separate case.

### F3 FIX-SOON (before the launch): the launch card does not require removing the loose PC Leon trio
- **Where:** LAUNCH-CARD "Before the launch". NOTES section 5.3 mentions the trio, but the card does not.
- **What happens:**
  - The integrator's 2026-10-13 LIST (`caps-team-work/CONSOLE-FACTS.md`) shows the raw
    `zzz_Leon_Townfall_2K_P.{pak,utoc,ucas}` sitting in `townfall/content/paks/`.
  - The card's "original game files" restore covers the utoc, the `_s1.ucas` and the pak, but not this mod trio.
  - If the trio stays and the engine mounts it directly, a PASS cannot be credited to #7's mount. A double mount of
    the same package ids is also possible.
- **Fix:** add an explicit precondition: the loose trio is removed or moved out of the paks folder (a console
  action, so the user's OK is needed), and a fresh LIST confirms it.

### F4 FIX-SOON: `run_tests.sh` passes when the real-file suites are skipped
- **Where:** `tests/run_tests.sh` lines 10-18.
- **What happens:**
  - `ran` counts skipped tests. `skipped_lines` is printed but never fails the run.
  - If the local Leon copy, the Engine.ini copy, `lua`/`luac` or the overlay_builder second source is missing, the
    run still prints "TOTAL tests ran: 46 ... PASS", with zero real-file checks behind it.
  - That breaks the rule "a gate that scans nothing is a failed gate".
- **Fix:** fail when any test is skipped, or require test_leon_real to report ran=7 with skipped=0.

### F5 NOTE: the APPLY.md paths do not resolve from any single directory
- **Where:** `PATCH/APPLY.md` steps 1, 4 and 5.
- **What happens:**
  - `cp caps-team-A/... outputs/2026-10-08-l25-prep/pak-intake/` needs both `caps-team-A/` and `outputs/` under one
    working directory.
  - `caps-team-A` lives in `/WORKSPACE/.codex/worktrees/translation-core/`, while `outputs/` lives in
    `.../translation-core/what/`. Step 5 uses a third base (`pak-intake/intake_gate.py`).
- **Fix:** use absolute paths.

### F6 NOTE: a corrupt fetched base chunk makes the result REFUSED (exit 2)
- **Where:** `chunk_intake._fetched_chunk` lines 201-203, and `test_corrupt_fetch_is_not_used`.
- **What happens:**
  - FTP reads made under load come back short; CLAUDE.md section 1 warns about this.
  - One truncated range read therefore puts a good mod into REFUSED. APPLY step 4 says "Exit 2 means refused. Do
    not install".
  - It is safe (fail closed), but it blames the mod for a transfer error.
- **Fix:** report it as INCOMPLETE ("re-fetch") and keep REFUSED for defects in the mod itself.

### F7 NOTE: "exit 3 expected after the fetch" is a prediction, not a guarantee
- **What happens:**
  - Once the chunk1/10/11/12 headers arrive, `plan_texture_samples` may pick different candidates, so a second
    fetch round is possible.
  - `parent_candidates` skips base imports of unknown class without planning a fetch (lines 576-578). A chain
    through a base material instance can therefore end UNKNOWN, which gives exit 4, with no new plan entry to say
    what is missing.
  - This cannot happen for Leon: all 67 chains end at mod-local roots. It is a gap in the generic logic.
- **Fix:** plan the fetch there instead of skipping, and say in APPLY that a second round may be needed.

### F8 NOTE: what the gate does not bind
- **Base inputs.** `intake_gate.py` checks only the mod sha256 values and the verdict. It does not check the base
  inputs (base utoc sha, ScriptObjects) the report was made with, so a report made before a game update still
  passes.
- **Acceptance record.** The acceptance is printed but never written to a file. Save the gate output into the
  evidence tree, or have the gate write a record.
- **Crashes and stale reports.** Errors outside `FormatError`, `OSError` and `struct.error` are not caught: for
  example `UnicodeDecodeError` from a bad FString, `KeyError` or `IndexError`. They exit 1 with no new report, and
  an older report at the same `--out` path stays in place.

### F9 NOTE: the NOTES are partly stale
- NOTES 1(b) says the 2026-10-08 listing "is not re-checked here".
- But the integrator's 2026-10-13 pull (`console-pulls-20261013-paks/FINDING-bright-outdoors.md`; exactly one pak
  entry differs, `PS5Engine.ini`) had already confirmed the cause before this deliverable was finished. So the
  "fresh LIST" in needs_from_integrator is already done.
- The claim can now be labelled: file-level, from console-pulled files (Mac analysis).

### F10 NOTE: smaller points
- `check_profile` refuses a mod whose compression block size differs from the game's. That is stricter than the
  loader needs, so it can refuse a good mod (fail closed).
- `config_disclosure` matches base values by key only, ignoring the section, and folds repeated keys together. On
  the merged console ini it shows `r.Streaming.PoolSize 1024 (game: 1024)`, although the merged file also sets 1536.
- Using the 2026-10-02 trial to argue that `MF_PhongToMetalRoughness` is non-fatal is evidence from an earlier
  session. Keep it as background, not as a verdict.
- Coverage limits the team already reports: Oodle-compressed PC mods are refused, and so are shared-shader-code mods
  (store hashes only, which gives UNKNOWN). These will limit #8's reach toward "~95% of an engine's mods" until a
  follow-up adds them.

## Hard-rule check

| Rule | Result |
|---|---|
| Generic (no game or mod names, no special cases) | OK (0 hits; per-mod data generated from the mod's files) |
| No detours, data-only | OK (no hooks at all; Lua only reads) |
| Fail closed | OK (F6 is conservative in the safe direction) |
| Two sources | OK: CityHash checked against 127 cooker ids; BLAKE3 against overlay_builder plus the official vector plus the TOC hashes; the target format from ShaderArchive names; the bright outdoors from the mod ini and the console-pulled ini |
| No SDK or platform-extension material | OK |
| No console contact | OK (needs are listed for the integrator) |
| Mod files untouched | OK (sha checked before and after the run) |
| Zero-count gates fail | Partly: the tools are OK; `run_tests.sh` has the skip gap (F4) |
| Claims labelled | OK; F9 asks for one label to be updated |

## What must happen before the #8 launch

1. Fix F1, F2 and F3. These are a Lua-only change plus the launch-card text. No rebuild.
2. Fix F4, so the PASS of `run_tests.sh` means something.
3. The preconditions already listed still apply: #7 passes first; base files are fetched (read-only); the intake is
   re-run and shows exit 3; the user records acceptance of the visual loss; zzintake is regenerated, deployed as an
   exact mirror, and the row-check is run. Each console action needs the user's own OK.
