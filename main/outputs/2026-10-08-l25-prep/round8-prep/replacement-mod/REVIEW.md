# Review of replacement-mod track (adversarial)

Verdict in plain words: the research is honest about its limits (no proof any mod works on the current version), and the hashes, the InfiniteStamina facts and the patch dates check out. But the top recommendation (ModMenu v6) is described as milder than it is, and several evidence citations point at the wrong lines. Refuted on those concrete points, not on the overall "no mod is proven" conclusion.

## Checked and confirmed (Mac-only unless stated)
- All Downloads zip/rar sha256 values in section 9 match (re-hashed). ModMenu main.lua sha 2ff1fa53..., 3,198 lines. InfiniteStamina main.lua sha 7667fed7... matches the deployed spool copy and the Mac copy; `InfiniteStaminaActive` is set only at line 11 (never true).
- PS5 eboot capture: capture.json sha 3b6cfad4... equals the BuildProfile image (D:28, D:34). Capture date 2026-10-02 (remote path /data/homebrew/.../decrypted/eboot.bin).
- LuaMod.cpp registration lines (1869, 1920, 1758, 2516, 2203, 3865, 6077, 6835, 6364, 3834, 3813, 2994, 5946, 3124, 3151, 2918) are all correct. API call counts for ModMenu match grep.
- D:807/D:818 (native ClientRestart hook), D:839, D:922 (LoadMap refused), D:995 (StaticConstructObject) are right.
- The Click (opened by me): 1.4.0 on 8 Oct 2026, PS5+Steam+Epic, menu v1.5.154194, no stamina/speed/health/item changes listed. Matches section 3.
- PS5Pad.ini: exactly 9 free chord slots listed. Matches.

## Problems

1. **Blueprint (refused) hook list for ModMenu is incomplete (major).** Section 5 names only the 5 gait hooks (main.lua:2587-2606). The real Blueprint-path RegisterHook attempts are about 10: gait x5 (:2602, `BP_TownfallPlayerGaitComponent_C:SetPace/ChangePace/OnWorldMarkupPaceChanged/UseAimingPace/EndAimingPace`), `BP_TestAIAgent_C:SpawnGroundMotionlessCollider` (:1163-1171), `BP_MotionlessTrigger_C:OnBoxBeginOverlap`, `BP_TheMotionless_C:BndEvt__...` (:1163 helper), `BTT_WidowAttackDecision_C:ReceiveExecuteAI` (:1234), and `WBP_SimonOS_C:Construct` (:1933). The reviewer asked for a full refused-hook list for the fix queue; this track leaves out 5+ ModMenu entries. Native engine hooks also omitted by name: `ExponentialHeightFogComponent:SetFogDensity/SetSecondFogDensity/SetVolumetricFogExtinctionScale/SetVolumetricFog` (:2112-2143, with `Value:set()` argument override, never exercised on console) and `TownfallPlayerCharacter:IsCheatHiddenFromAIPerception` (:1250, return-value override).
2. **Contradicts the task's own preference (major).** The task says "Prefer mods that need no RegisterHook". The #1 pick has 11 RegisterHook calls and 3 LoadMap hook calls. The write-up admits this, but ranks it first anyway, and the plain verdict says "changes values (God Mode, movement speed, healing)" without saying a first-run on B4 or earlier would re-trigger the lock leak. (Section 5 does say it, but the verdict does not.)
3. **"Unverified pcall" item is answerable and was left open (minor/major).** Section 10 item 5 says lines 1234 and 1250 may be unguarded. Reading the file: 1234 is inside `pcall(function() return RegisterHook(...) end)` (:1230-1235) and 1250 is inside `pcall` (:1248-1252); 1163, 1539, 1559, 1933, 2038, 2120, 2143, 2602, 2734 are all in pcall too. API-PARITY-TABLE.md:371 says ModMenu has "UNGUARDED RegisterHook sites (:1234/:1250/:1539+)". That table entry is wrong for the v6 file now on disk, and the report did not reconcile the two. (Under the pre-B5 lock bug a pcall'd refusal still leaks the lock, so the guard does not remove the risk.)
4. **Key collision with the spawner not mentioned (major for a combined test).** ModMenu binds UP_ARROW, DOWN_ARROW, RETURN (:3178-3189); TownfallItemSpawner binds the same keys (spawner main.lua:698-722), and PS5Pad.ini serves them from the same pinned rows (UP/DOWN/CROSS). Every d-pad Up/Down/Cross press fires both mods' callbacks. CROSS is also a game action. The report only says Backspace needs a new row.
5. **Wrong or weak line citations.**
   - "FindFirstOf/FindAllOf/StaticFindObject console-proven (D:24-25)": D:24-25 are `[ST] wcslen/wstring-rt` lines, not Find* checks. The B4 report cites the same lines; the real `[ST]` Find* rows were not located by this track (grep of the decoded log for FindFirstOf/FindAllOf/StaticFindObject returns nothing; only the report's summary count backs it).
   - "`require("UEHelpers")` console-proven (D:815-816)": those are `[LUA-FN] Tilde / F10` FName decodes. (Inherited from the B4 report, but copied without checking.)
   - "minimap tick ran (D:994)": D:994 is an `[LFB-M] fname-rev` line. The Mac-only inference from MM:4267 may be right, but API-PARITY-TABLE.md:377-378 lists LoopInGameThreadWithDelay as unexercised, which the report does not acknowledge.
   - "`FText` :7705, `FName` :7696": those lines in LuaMod.cpp are `{`/`}`. The globals are set at :7967 and :7976.
   - "D:2410 area" for ExecuteInGameThread queueing: D:2410 is `keybind callback entry key=118`. Queue lines are D:809-810, 817, 915-916; drain lines D:923, 935.
   - "D:869, D:873, D:887: object and key-array writes": 869/873 are the CheatManager construct/enable prints and 887 is a read-back print; no line proves a key-array write.
6. **Uncited claims.** "295 downloads, the 2nd most of any Townfall file" and "Infinite Ammo has 43 downloads" come from VGTimes pages I could not tie to a quoted line; "Stamina half likely inert like InfiniteStamina" for SpeedHackMod has no source (labelled "likely", acceptable). The "9 key" ModMenu Nexus date ("30 Sep") comes from the zip filename only, not a page.
7. **Performance not assessed (minor).** Acceptance requires "no stutter". ModMenu runs a 100 ms game-thread tick (:2429-2431) that calls `II_Update` every tick; with features on it adds 12 FindAllOf sites. The last full self-test run put FindFirstOf/FindAllOf at 78 ms SLOW (B4 report line 78). Not discussed for the recommendation.
8. **Option B incomplete.** The only non-ModMenu option needs a download, and its Lua API list is unknown, so the task's "list the Lua APIs each candidate calls and check them" is done for one candidate of three.

## Rule check
- Writes: only the round8-prep/replacement-mod folder (CANDIDATES.md, README.md) plus scratchpad extraction. No violation found. No console contact evidence, no mod edits, no hashes disagree.
- "Never claim a mod works without a cited source": complied with; the verdict says nothing is proven.

## Net
Fix list before relying on it: (a) complete the ModMenu refused-hook table (item 1); (b) mention the Up/Down/Cross collision with the spawner (item 4); (c) correct the citations in item 5; (d) put the hook count and the pre-B5 lock-leak warning in the plain verdict; (e) record that all ModMenu hook sites are in pcall and correct the parity table.
