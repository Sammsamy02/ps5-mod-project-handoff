# EVIDENCE-NOTES.md — source census for OVERLAY-DESIGN.md

Worker: worker-file-overlay · 2026-10-06. Every path below was read by this
worker during this task. No console contact; no files outside
`outputs/2026-10-04-zcode-team/worker-file-overlay/` were modified.

## 1. Worktree documents read

| path | what it established |
|---|---|
| `outputs/ZCODE-HANDOFF-2026-10-04/START_HERE.txt` | product goal, authorization frame, current priority |
| `outputs/ZCODE-HANDOFF-2026-10-04/RUNTIME_MAP.txt` | UE 5.6 static ID, engine-binding state, native ABI rules, layout contract |
| `outputs/2026-10-04-zcode-team/worker-boot-protocol/BOOT-PROTOCOL.md` + `probe/*` | console topology: pldmgr/autoload (incl. shadowmountplus), `/mnt/sandbox/PPSA33286_000` + `pfsmnt`, boot identity, ploader.ini contents, `/data` listing (homebrew/ exists) |
| `outputs/2026-10-04-zcode-team/worker-ploader-imports/PLOADER-IMPORT-ANALYSIS.md` | injection mechanism (ptrace + sceKernelLoadStartModule), kernel-side import binding, libSceFios2.prx readiness gate, NID law |
| `outputs/2026-10-04-zcode-team/worker-activation-accessibility/ACCESSIBILITY-ANALYSIS.md` | exact opens of the mod root; roles; all-or-nothing activation; error visibility |
| `outputs/2026-10-04-zcode-team/worker-f-r2-fix/composition/vendored/modforge_activation.h` | the /data readability-window contract; preload limits (8 MiB/file, 64 MiB total); listed-not-preloaded roles; status-file fd trick |
| `outputs/2026-10-04-zcode-team/worker-minimal-path-test/REPORT.md` | PATH_TEST proof of in-module_start /data read + /data/plugins write; `read` import precedent |
| `outputs/2026-10-04-zcode-team/worker-e2e-staging-final/STAGED-E2E-COMMANDS.md` | register→prepare→install→ack→remove verbs; journal/rollback; loadlist = commit truth |
| `outputs/2026-10-04-zcode-team/worker-configured-roots-design/REPORT.md` | `/app0` configured-root status; "directories only" consumer census |
| `outputs/2026-10-04-zcode-team/worker-preentry-review/PREENTRY-REVIEW.md` | W-1..W7 injection stages; readiness scan detail |
| `outputs/2026-10-01-townfall-mod-trial.md` | Leon trio staging; engine acceptance pending; cleanup discipline |
| `outputs/2026-10-01-file-override-selection.md` | `choose_file_override` / `FileOverrideTable`; "game file-open interception surface" named as open work |
| `outputs/2026-10-02-native-downloaded-iostore-preparation.md` | IoStore prep pipeline + PS5 ELF build; format constraints (v8 None, unencrypted) |
| `outputs/2026-09-30-unreal-container-survey.md` | PAK v11 + IoStore TOC v3 formats; unencrypted indexes; content split (assets in IoStore, loose resources in PAK) |
| `outputs/2026-10-06-claude-cloud/MAC-SESSION-BRIEF.md` | current engine-binding blockers (B1–B3); the phase-2 dependency reality |
| `include/modbridge/core.hpp` (ps5-mod-bridge) | FileOverrideTable public API exists in-tree |

## 2. Spike/evidence trees read

| path | what it established |
|---|---|
| `.superpowers/native-dependency-spike/iostore-toc-overlay-c-1/{README.md,plan.md}` | pure-C v8 None TOC overlay merger; admission rules; 31-assertion control |
| `.superpowers/native-observations/console-return-2026-10-01.md` | dump layout: `townfall/`, `engine/`, `sce_sys/`, eboot.bin, manifest |
| `.superpowers/native-observations/transport-progress-before-native-default-detour-2026-10-02.json` | **the prior projection**: `pakchunk9999-ModBridge_<uuid>_P.{pak,utoc,ucas}` staged into the dump paks dir; `engine_mount_or_asset_origin_proved:false`; `original_game_files_overwritten:false`; cleanup booleans; `pakchunk0-ps5.utoc.original-<uuid>.bak` (TOC-merge route exercised) |
| `.superpowers/native-dependency-spike/ploader-cleanup-candidate-1/original/main.c` | MAX_PLUGINS=10/title; readiness scan module list |
| ps5-payload-sdk `target/include/ps5/` | community SDK has **no** fios headers → fios NIDs need console-side census (W3) |

## 3. External sources consulted (2026-10-06, links in REPORT.md)

- Epic "How to Create a Patch" (pak `_P` priority) + buckminsterfullerene02 Pak Patching guide (pak priority mechanics).
- PSXHAX/consolemods practice threads: UE console mods = containers into the decrypted dump's `content/paks/` or fpkg repack.
- UE4SS upstream `assets/UE4SS-settings.ini` (fetched): `[Overrides]` = mod-folder paths only; **no** upstream loose-file/file-API override system exists to port.
- PSDevWiki PS5 Libraries page (system module table; no fios overlay documentation — hence W3 is a console-side census, not a docs lookup).
