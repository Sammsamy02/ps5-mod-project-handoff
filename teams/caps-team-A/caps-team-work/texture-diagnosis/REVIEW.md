# Independent review: Team A texture diagnosis (#7/#8)

Reviewer: one independent agent, 2026-10-12. Everything in this review was checked on the Mac only. I made no console contact.

**Verdict: PASS-WITH-FIXES.** The staged patch and tools are correct and can be integrated now (they are Mac-side only).
**Do not run the launch card as written.** One line in it would make the A/B result wrong (finding 1).

## What I re-ran (Mac-only)
- `tests/run_tests.sh` → `RESULT: PASS`. Own tests: 19/19 (hashes 3, reader 4, diagnosis+plan 7, rewrite 3, fetch 2).
  Patched overlay suite: 32/32. 22 count categories, 83,241 items, every count > 0. All 10 read-only inputs were unchanged.
  The output matches `tests/test-output.txt` line for line (only the timings differ).
  Note: the run regenerates `tests/out/` and `PATCH/run-new/leon-replay-drift.json` inside the team folder, by design.
- I applied the diff to copies of the real l25-prep files, using the pre-image sha `a72636fb…` / `965e49b8…` (matches the live files).
  It applies cleanly, and the result is byte-identical to `PATCH/new/` (`c668c9f0…` / `de20f1b2…`).
- SUSPECT files: the live `leon-replay-drift.json` (`63ceec0e…`) and `leon-substitutions-fixture.json` (`597af39a…`)
  match the regenerated copies. The team's claim holds. They are still not used as evidence.
- I checked the hard rules. There is no network code in `tools/` or `tests/`, no runtime or C++ change, no detour,
  and no SDK material. Mod inputs are only read (sha unchanged).
- Leon checker diagnosis (H1): the independent checks agree. The mod has 0 store shader-map hashes, 0 game-platform tokens,
  0 shader-library chunks, and inline DXBC/PCD3D in all 13 base materials. The claim is sound (Mac-only).

## Findings

### 1. BLOCK (launch card): the "Leon off" folder is still inside `paks/`, so the engine would probably still load Leon
`LAUNCH-CARD.md` step 2 moves the files to "a backup folder next to it, for example `~mods-off/`", which is still inside
`content/paks/`. Unreal finds pak files by searching the whole `Paks` folder, subfolders included
(`FPakPlatformFile::FindPakFilesInDirectory` → `IterateDirectoryRecursively`). That is also why `~mods/` works at all.
This is general UE knowledge (Mac-only). I cannot cite a line for it, because the local Epic subset has no PakFile module.
**Scenario:** the files go to `paks/~mods-off/`, the engine mounts `zzz_Leon_*_P` anyway, and the outdoors is still bright.
The card would then call that PASS-B ("Leon is not the cause"). That is the wrong conclusion, and the launch is wasted.
**Fix:** move the 3 files to a folder outside `content/paks/` (for example `…/townfall/leon-off/`). After the move,
LIST the whole `paks/` tree to confirm no `zzz_Leon` file is left anywhere under it. Also add an in-game check: "the player
looks like the normal game character (no Leon, no checker)". If Leon or the checker is still visible, the result is
**INVALID** (not PASS-B). Then roll back.

### 2. FIX-SOON: the material gate can never judge a material as OK
`tools/texture_diag.py:311-316`: a `Material` export always gets `NO_PS5_SHADER_MAP` or `NO_SHADER_MAP_AT_ALL`.
The check never looks at the package's store `shader_map_hashes` or at the game's platform tokens. No test has a positive case.
**Scenario:** someone installs a mod cooked correctly for PS5, and the gate is wired into the mod manager as APPLY §3 suggests.
The user is told every material "will draw as checker". This does not change the Leon diagnosis, because those facts
are checked separately. It must be fixed, with a positive-control test, before the gate is wired in.

### 3. FIX-SOON: a game-specific name in a tool meant for the mod manager
`tools/texture_diag.py:335` hard-codes `"townfall/content/"`. **Scenario:** on any other game, `base_dependents_outside_mod`
is computed wrong (every dependent counts as outside the mod). Use the data-derived root, as `material_plan.py:49` already does.

### 4. FIX-SOON: the #8 reparent plan (option B) ignores static-permutation shader maps
A material instance with static switch or static parameter overrides has its own shader map.
The derived MI copy is a new package, so its store `shader_map_hashes` stay empty. **Scenario:** a Leon MI with static
overrides is reparented to `M_MRO_Cloth_NonVT`. It still has no PS5 shader for its permutation and still draws the checker.
The #8 launch spends a launch without fixing it. Add a check (static overrides in the MI payload, and whether the game's
own MIs on that master carry their own store hashes) to `material_plan.py` before any option-B launch.
(Mac-only, general UE knowledge.)

### 5. FIX-SOON: after the patch, nothing tests a successful closest-match through `build_overlay`
Both former success tests now expect refusals. The admit path is only tested by calling `require_package_import_rewritten`
directly. I checked how admission works: `retoc pack-raw` keeps the **old** parent in the derived container's store entry
(my scratch run printed `store entry has OLD parent: True has NEW parent: False`). So `build_overlay` admits only a
package whose header names the new parent while its store entry still names the old one, and the repoint then fixes the
store entry. **Scenario:** a future derived-container writer rebuilds store entries from the headers. Then
`substitution_import_not_found` refuses first. That is fail-closed, but APPLY §3 ("the guard passes…") would not work as
written. Add one end-to-end `build_overlay` test using the rewritten Leon container, and write down this ordering in APPLY.

### 6. FIX-SOON: the APPLY gate rewrites a SUSPECT file in l25-prep
APPLY §1 says to run `python3 test_overlay_builder.py` "in that folder". Test line 757 rewrites
`pak-overlay/leon-replay-drift.json` there. The bytes come out identical (verified), but the gate still touches a SUSPECT file.
Make the relocated run the only gate, or point line 757 to an output folder in the same diff.

### NOTES
- The guard's name-batch parser (`zen_imported_package_ids`) ignores NumStringBytes and any UTF-16 alignment. A mis-parse
  can only cause a false refusal (admitting needs an exact 64-bit id match), so it stays fail-closed.
  A bad UTF-16 name raises `UnicodeDecodeError` instead of `Refused`.
- The diff does not create `cityhash64.py`. If someone applies the diff without the `cp` step, `overlay_builder.py` fails
  at import for every action, `hide` included. APPLY lists the steps in the right order. Consider adding the file as a
  new-file hunk in the diff.
- The claim "the runtime has no engine detours" cites launch-14 `ue4ss.decoded.log` lines 794 and 941. Those lines
  (verified) show only that the LoadMap detour stayed off. The rest rests on the build-time no-detour gate (Mac-only).
  The wording should say that.
- The log markers on the launch card exist in launch 14: `[l22] Lua mod started` ×7 (lines 824-904) and
  `[WORLD-EV] route live` (line 1011).
- The launch card should also say: keep the console offline, and do the usual decrypt-off PRX readback and row-check.
  Scheduling: this diagnostic launch is not in the current order (B6p/B7 first). The user decides when it runs.
- In a passing run, `FAIL: zero items checked in: game_shader_platform_files` will alarm a non-programmer.
  Prefix it with "(expected, negative test)".
- `needs_from_integrator` reads are all read-only partial RETRs, and each fetched range is checked against its IoHash.
  The local base `.utoc` is from 2026-10-01. If the game was updated since, verification reports `IOHASH_MISMATCH`
  (fail-closed).
- The fixtures in `tests/test_base_fetch_plan.py` name game assets. That is fine in a test.
