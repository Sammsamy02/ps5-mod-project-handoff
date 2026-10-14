# Townfall mod trial

Townfall reaches a playable 3D scene on the user's PS5. An original-only extra PAK and a temporary render-control PAK were transferred as new files, with exact readback. The original-only trial opened and played normally. The user could not distinguish the render-control effect; **engine mounting and mod acceptance remain unproved**.

The render-control file was visible through the running game's app0 filesystem and matched its prepared bytes. Filesystem visibility does not establish that Unreal loaded the container or applied its configuration. Both temporary experiments are now removed; fresh readback confirms their owned final and temporary names are absent. Original game files were not overwritten.

The cleanup experiment identified an actual FTP interoperability issue: ftpsrv0.21.1 returned `226 File deleted`, which Python's standard delete handler rejected. Recovery verified absence without repeating the delete. A separate experimental revision accepts the measured response while preserving ownership checks, failure propagation and mandatory post-delete absence verification; its focused local checks pass. That revision has not been exercised on the console.

The user completed Nexus sign-in in the official browser. These requested packages have now been downloaded into private development quarantine:

| Package | Selected version | Actual contents / current boundary |
| --- | --- | --- |
| Leon replacement | 2K v1.1 | Unchanged PAK/UTOC/UCAS bundle staged for a controlled trial; all three files match exactly after transfer. Engine acceptance is awaiting the user's visible check. |
| UE4SS for Townfall | v1 | Windows runtime package. A working PS5 runtime port remains missing. |
| Townfall Mod Menu | v6 | Lua entry point, about 128 KB; requires Unreal reflection, UMG widgets, input and game-thread services. |
| Minimap | v3.1 | Lua entry point, about 197 KB; requires the same shared engine services and mod-local settings paths. |
| Infinite Stamina | v1 | Its downloaded script initializes a local enable flag to false and never assigns it again; unchanged installation would leave its stamina action inactive. |

Only the Leon bundle has been staged. Its IoStore v8 containers are unencrypted and use uncompressed blocks; 68 asset paths and package IDs match the installed target's indexes. This establishes candidate overrides, not platform cooking compatibility or engine loading. The generic streaming transaction published UCAS, PAK, then UTOC under fresh owned names, verified every temporary and final file by complete SHA-256 readback, and confirmed all temporary names absent. It wrote 185,497,319 bytes without changing the downloaded assets or overwriting original files. Its source has no game or mod branches; target paths and source identities come from an external profile.

The menu, minimap, stamina and Windows UE4SS files remain uninstalled and unexecuted. Browser sign-in, downloads and computer-driven trial staging are development observations, not a finished native PS5 account/download/install flow. The current application automates download and analysis; preparation backends and installation are still being connected.

The shared archive inventory now recognizes container groups automatically by normalized parent directory and exact filename stem, preserving member names, complete sizes and hashes. Thirty focused cases pass, including wrappers, separate bundles sharing a basename, missing/ambiguous partners and late archive failure. The actual Leon ZIP produces one unchanged trio candidate whose identities match independent extraction; the rebuilt production analysis CLI emits the identical report. Large fully hashed members now have a separate byte-inventory completeness fact; limited structural inspection still blocks preparation and installation. Candidate recognition is not container or platform validation.

The installer design remains shared by format and engine, with no AI or manual mod edits during installation. Game paths and build settings belong in profiles. The verified deterministic native PAK writer is committed as a reusable preparation primitive; packing bytes alone does not translate platform-specific cooked assets or provide UE4SS's engine services. A [native IoStore reader](2026-10-01-native-iostore-reader.md) now inspects shared container metadata. Native materialization, preparation and runtime installation remain unfinished. The unchanged Leon trial's visible result is pending.

The same production analysis entry point has now inventoried all five downloaded ZIPs without changing their bytes. A separate [automatic package-header walk](2026-10-01-native-package-header-reader.md) composed the shared native readers across all 127 asset packages in the unchanged Leon bundle, with no per-asset selection. All 127 headers had one structurally coherent candidate under the implemented schemas; both independent reference headers matched. This local development result establishes header inspection, not platform conversion, completed automatic engine-profile discovery or an in-game mod result.
