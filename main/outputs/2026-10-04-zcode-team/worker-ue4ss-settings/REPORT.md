# REPORT — worker-ue4ss-settings

Workspace: /WORKSPACE/.codex/worktrees/translation-core/what. Date 2026-10-04. No console
contact, no network, no nested subagents. Calls used: well under the 50-call bound.

## Result summary

1. **Consumption path read from the retained composed runtime** (native-coherent-runtime-1/source,
   the composition-3 input tree): settings deserialize (SettingsManager.cpp:48-81, key
   `ue4ss/UE4SS-settings.ini` from native_mod_source, missing/NUL throws),
   load_unreal_offsets_from_file (UE4SSProgram.cpp:896-906, keys ue4ss/MemberVariableLayout.ini +
   ue4ss/VTableLayout.ini from mod_source, pair mandatory), NativeModSource logical-key surface
   (NativeModSource.cpp:10-136 + activated_mod_source.cpp:18-46: exactly ue4ss/* files with role
   lua_mod/config/script), optional keys (Mods/mods.txt, shared require paths, UE4SS_Signatures).
   m_working_directory on PS5 = parent of NativeProgramContext.module_file_path (absolute,
   existing, normalized; NativeProgramContext.hpp:85-101; UE4SSProgram.cpp:749-785), i.e. the PRX
   directory; config never reads the disk there, but UE4SS.log is written there.
2. **Settings surface documented in BUNDLE-SPEC/FIRST-RUN-CONFIG is incomplete and, as bytes,
   would abort startup.** The retained validate_native_services (UE4SSProgram.cpp:719-746) throws
   when CrashDump.EnableDumping / Debug.SimpleConsoleEnabled / Debug.DebugConsoleEnabled /
   Debug.DebugConsoleVisible are true or RenderMode != ExternalThread — and the code defaults for
   the first four are TRUE (SettingsManager.hpp:87-89, 99). worker-activation-bundle's generated
   221-B ini (ConsoleEnabled=1, no CrashDump/GuiConsole keys; sha256 f6d06cada43d43d0…) parses
   fine but trips the crash-dumping, simple-console and debug-GUI throws before any mod work.
   worker-itemspawner-dryrun inherits the same content. Neither doc mentions the gates.
3. **Canonical first-run UE4SS-settings.ini produced and [VERIFIED]** (staged/ue4ss/
   UE4SS-settings.ini, 9 keys): parses through the VENDORED settings-port IniParser/ParserBase
   with PS5 char16_t semantics; all five native gates pass; all ten expected values assert
   (host-test/PARSE-TEST-OUTPUT.txt: "all gates + expected values OK").
4. **Measured parser findings**: a section containing only comments wedges the parser (hard
   syntax error at the next section header) — hence no empty [Overrides] section; inline `;`
   comments corrupt values (get_bool falls back to default); `False` casing, empty values,
   full-line comments and blank lines are safe; get_list("Overrides") on an absent section is safe
   (SettingsManager.cpp:86). Doc snippets in BUNDLE-SPEC/FIRST-RUN-CONFIG carry inline comments —
   do not paste them as bytes.
5. **InputSource fact**: General.InputSource defaults to "Default" and is never parsed from the
   ini; with HAS_INPUT composed in, an injected available input source (retained PS5 pad backend,
   ue4ss_backend.cpp:25-26) is a hard prerequisite, else validate_native_services:739-741 throws.
6. Deliverables: WORKING-DIRECTORY.md (PS5 dir + full logical-key surface + staged tree),
   SETTINGS-CONTENT.md (exact ini, per-key sources), STAGING-COMMANDS.md (controller-run FTP/
   installer commands with hashes; STOR gate honesty; layout pair explicitly blocked),
   staged/ (4 real files), host-test/ (harness, probes, transcript).

## Limits (honest)

- Verification is host-side parse + gate replay only: it proves the ini bytes clear
  SettingsManager + validate_native_services as written; it does NOT run the PS5 runtime, does
  not prove engine init, mod start, or any visual/audio behavior. No >8-style claims.
- The two layout keys remain BLOCKED-ON-LAYOUT-DATA (worker-2): without them the run ends inside
  setup_unreal() before Lua. No placeholder was staged or endorsed.
- Route A's register/prepare/install argv grammar is copied from the proven e2e runbook; the
  actual $SVC_CMD mechanism is controller-resolved (STEP-BY-STEP G1), not by this worker.
- m_working_directory writability (UE4SS.log next to the PRX) is flagged, not solved: it is a
  caller/composition decision outside settings scope.
