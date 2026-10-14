# REPORT — worker-settings-bundle-merge

Workspace: /WORKSPACE/.codex/worktrees/translation-core/what. Date 2026-10-04.
No console contact, no network, no nested subagents. Writes confined to this worker dir.
Calls used: well under the 50-call bound.

## Result summary

1. **Merged activation package produced at `staged-package/`** — one complete, ready-to-stage
   tree (6 files), each source verified before inclusion:
   - `ue4ss/UE4SS-settings.ini` — the CORRECTED canonical settings from
     worker-ue4ss-settings/staged (779 B). The prior bundle's 221-B ini (`f6d06cad…`,
     `ConsoleEnabled=1`) is intentionally excluded: it trips the crash-dump/simple-console/
     debug-GUI throws in validate_native_services (UE4SSProgram.cpp:719-746) and would abort
     startup before any mod work.
   - `ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua`, `enabled.txt`,
     `ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` — byte-identical copies of the
     worker-itemspawner-bundle bundle files; sha256/size equal every `bundle.json` pin
     (`immutable_mod_bytes` preserved; cmp-verified).
   - `ue4ss/PS5Pad.ini` — byte-identical copy of worker-real-input-pad's staged profile
     (2960 B).
   - `loadlist.txt` — merged manifest (deltas in §3).
   - `MemberVariableLayout.ini` + `VTableLayout.ini`: **deliberately NOT package bytes** —
     dependency noted. The task's "worker-emit-final-pair" output does not exist under that
     name; the actual staged-pair output is
     `worker-staged-pair-live/emission-live/` (also `worker-staged-pair-emission/emission-synthetic/`,
     synthetic, never shippable). The live pair is REAL predicate data but carries
     gap-marked EngineRevision/BuildId and its own receipt rule: "NEVER stage or ship this
     pair until the identity gaps close". Adding loadlist rows for files not staged aborts
     activation, so the merged loadlist correctly omits them until a gap-closed pair lands
     (exact append-on-landing procedure in STAGING-COMMANDS.md §1).

2. **Settings gate verification PASSED**: the merged settings file fed through the VENDORED
   settings-port IniParser (PS5 char16_t semantics) + replayed SettingsManager::deserialize
   reads + validate_native_services gates → `PASS (all gates + expected values OK)`, exit 0
   (worker-ue4ss-settings host-test harness, unmodified). Evidence: `host-verification.txt` §1.

3. **Bundle byte-identity verification PASSED**: all three bundle files cmp-identical to
   their pinned sources; PS5Pad.ini and (pre-edit) loadlist identical to their sources.
   Evidence: `host-verification.txt` §2.

4. **Merge defect found and fixed — loadlist was stale against the corrected settings.**
   The frozen bundle loadlist pins `ue4ss/UE4SS-settings.ini` at 221 B; the corrected file
   is 779 B. Running the bundle worker's compiled host test against the naive merge proved
   the runtime abort: `mfa_activate` fails with `MFA_E_SIZE` ("file is longer than the load
   list says"). Shipped the package's byte-identical bundle loadlist would have guaranteed
   an activation abort. The size rule is load-bearing (INSTALL-PACKAGE.md:47-49).

5. **Merged loadlist** (360 B, `9eef4dbb…`): two deltas vs the frozen bundle manifest
   (original `cbe30136…` preserved in STAGING-COMMANDS.md §2 for provenance):
   - settings row size `221` → `779`;
   - added `file config ue4ss/PS5Pad.ini 2960` — the pad config is read from the activated
     mod source (NativePadInput.cpp:10-26), so it must be preloaded via a loadlist row;
     unlisted, it would never reach the runtime (and per the settings worker's InputSource
     finding, a missing injected input source can throw at validate_native_services:739-741).

6. **Merged package re-verified through the vendored activation path**
   (worker-itemspawner-bundle `test_activation`, unmodified binary): `mfa_activate` OK,
   title/engine/framework gates pass, **all five files preload with exact-size verification**,
   main.lua/enabled.txt/UEHelpers.lua byte-identical to intake/distro pins, mod discovery
   intact (enabled.txt marker, shared/ skipped, no mods.txt), pair absence asserted,
   negatives intact (MFA_E_TITLE / MFA_E_SIZE / MFA_E_NO_LOADLIST).
   **4 expected FAILs — all frozen-bundle cross-worker pins this merge deliberately
   supersedes**: "exact four file rows" (now 5), "four files in activation" (now 5),
   "preloaded bytes == 31602" (now 35120), and "settings byte-identical to the 221-B build"
   (the correction IS the merge). Full transcript: `host-verification.txt` §3.
   Per AWAITING-STAGED-PAIR.md step 5, a changed bundle must re-pass mfa_activate before it
   is installable — T2's activation gate passes.

7. **Deliverables**: `staged-package/` (6 files), `STAGING-COMMANDS.md` (gate-0 write-probe,
   per-file STOR + RETR readback hash checks, loadlist LAST, layout-pair dependency
   procedure), `VERIFY-CHECKLIST.md` (exists/size-vs-loadlist, sha256 identity, settings
   parse spot-checks, manifest sanity, runtime-status.txt oracle incl. the honest
   setup_unreal-stop expectation until the pair lands), `host-verification.txt` (transcript).

## Limits (honest)

- Host-side verification only: vendored parser/gate replay + vendored mfa_activate preload.
  Nothing ran on the PS5; no engine init, no Lua execution, no runtime-status observed.
- The package as staged will NOT reach `mods-started`: without the layout pair the run
  stops inside setup_unreal() before Lua (UE4SSProgram.cpp:899-904). That pair is a hard
  external dependency, gap-marked and not stageable today; no placeholder staged or endorsed.
- The 4 host-test FAILs are asserted frozen-bundle contracts, superseded by this merge with
  evidence; they are documented, not silenced. No test binary was modified.
- FTP STOR write to the console is not evidenced in workspace records; STAGING-COMMANDS.md
  keeps the bounded write-probe gate before any upload.
