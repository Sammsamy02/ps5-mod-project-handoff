# INVENTORY — retained third-party native DLL mods (worker-dll-mod-retention, 2026-10-04)

Workspace: `/WORKSPACE/.codex/worktrees/translation-core/what`
Question: does the workspace already retain ANY third-party native (PE) DLL mod
beyond our own dwmapi/UE4SS toolchain artifacts?

## RESULT: NONE. Zero third-party native DLL mods are retained.

The only retained PE DLLs are our own toolchain (UE4SS distribution + project
builds). The complete retained mod corpus (worker-6's 8 zips) is asset/logic
container mods with no native PE entries.

## Searches performed (exact terms, negative results recorded)

1. `find . -path ./.git -prune -o -type f -iname '*.dll' -print` (whole worktree,
   credentials excluded)
   - Positive hits, ALL first-party — no third-party DLL retained:
     - `.superpowers/native-dependency-spike/windows-image-original-inputs-1/dwmapi.dll` (our UE4SS proxy, 279552 B)
     - `.superpowers/native-dependency-spike/windows-image-original-inputs-1/UE4SS.dll` (our runtime, 20572160 B)
     - `.superpowers/native-dependency-spike/windows-worker-contract-review-1/*/worker.dll` (our builds)
     - `.superpowers/native-dependency-spike/ue4ss-native-prx-link-1/{tool,reader-checker}/SharpProspero*.dll` — verified project-built (same dir contains `build.py`, `manifest.json`, `source/`, `REPORT.md`), not third-party
     - `outputs/ps5-mod-bridge/build/fixtures/{fake,managed,native}.dll` (our fixtures)
2. `find . -iname '*.zip' -o -iname '*.7z' -o -iname '*.rar'` (credentials excluded)
   - Retained mod archives beyond toolchain: exactly the worker-6 corpus of 8
     (5 in `.superpowers/native-observations/nexus-real-mods-2026-10-01/`, 3 intake
     dirs). Everything else is our source/spike/fixture zips.
3. `unzip -l` PE-entry scan (`.dll|.asi|.so|.prx|.exe|.sys`) of all 8 corpus zips:

   | Archive | Native PE entries |
   |---|---|
   | nexus-real-mods-2026-10-01/infinite-stamina-v1.zip | none |
   | nexus-real-mods-2026-10-01/leon-2k-v1.1.zip | none |
   | nexus-real-mods-2026-10-01/minimap-v3.1.zip | none |
   | nexus-real-mods-2026-10-01/mod-menu-v6.zip | none |
   | nexus-real-mods-2026-10-01/ue4ss-v1.zip | `dwmapi.dll` + `ue4ss/UE4SS.dll` (our toolchain distribution) |
   | item-spawner-real-mod-intake-1/original.zip | none |
   | movement-speed-real-mod-intake-2026-10-01/movement-speed-2x.zip | none |
   | performance-pak-real-mod-intake-1/original.zip | none |

4. `find work -type d -iname '*download*' -o -iname '*store*'`
   - Negative: no download_store directory exists under `work/` (only
     `nexus-test.crt/key` credentials — untouched per rules, `goal-state.md`,
     `nexus-types-pinned.ts`). The download owner's sources live at
     `outputs/ps5-mod-bridge/native-host/download/` (code, no retained artifacts).

## Conclusion

The services backlog was ranked purely from first-party imports (dwmapi proxy +
UE4SS runtime). Before broadening the backlog, one representative third-party
DLL mod must actually be acquired and censused (see ACQUISITION-PATH.md); the
inspection tool for that is ready and baseline-tested (see
`inspect_dll_census.py`, `baseline-dll-census.json`).
