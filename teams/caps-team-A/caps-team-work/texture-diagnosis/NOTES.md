# NOTES: texture diagnosis (Team A)

Everything here is Mac-only unless a log line is cited. No console contact,
no builds, no edits outside this folder. Large scratch data (retoc
conversions) is deleted after each test run.

## Inputs (read-only; sha256 checked unchanged by tests/run_tests.sh)
| Input | sha256 (prefix) |
|---|---|
| Leon 2K `.utoc` / `.ucas` (Nexus zip, extracted 2026-10-01) | `64503ced…` / `1f10e60e…` |
| Leon 4K `.utoc` / `.ucas` | `cfc9504d…` / `488837b4…` |
| Game `pakchunk0-ps5.utoc` (+ 1/10/11/12) | `35de2752…` |
| Game pakchunk0 ContainerHeader (2,272,324 B) | `d7b6f7ca…` |
| Game `global.utoc` / `global.ucas` (ScriptObjects, 52,042 entries) | `f4206416…` / `8dfeee26…` |
| Game `pakchunk0-ps5.pak` listing (repak) | file names only |
| retoc v0.1.5 binary (independent reader, used only to cross-check) | pinned in its own receipt |

## Tools (all generic, stdlib, fail closed)
- `tools/iostore_lib.py`: TOC v8, uncompressed chunk reads, Zen package header (UE5.6 layout: export payloads laid out in
  export-bundle order, ending in the package tag), container header v5, ScriptObjects. Paths are normalised by mount point.
  (The pakchunk10 sub-mount had hidden 2 overlaps. This fixes that.)
- `tools/texture_diag.py`: the gate/report. Classes of material, texture platform data (self-validating mip walk), PC/PS5
  shader tokens, store shader hashes, the non-Leon blast radius, a linear-size test with the mod as positive control.
  Material verdicts come from `material_verdict()`: OK when the mod's own store entry has shader-map hashes or the export
  carries the game's own platform tokens; instances resolve their mod parents recursively. Every row has
  `will_draw_checker`. The blast radius compares PackageIds (no path rewriting, no game name).
- `tools/material_plan.py` option B rows carry `static_permutation_check`: an instance with its own permutation (own
  shader tokens or own store hashes) becomes `B_BLOCKED_OWN_STATIC_PERMUTATION`. Per candidate master it also counts the
  game's own packages that import it and how many carry their own store hashes (Mac-only, general UE knowledge).
  Limit: static parameters are inferred from the cooked permutation evidence, not read by property name (no schema).
- `tools/material_plan.py`: #8 options A/B/C, with base masters taken from the game's own mesh→material imports.
- `tools/zen_import_rewrite.py`: derived-copy import rewrite with full re-verification.
- `tools/base_fetch_plan.py`: exact UCAS ranges + BLAKE3 IoHash verification of fetched bytes.
- `tools/base_texture_layout_stats.py`: base vs mod texture size classes and the VT model fit.
- `tools/cityhash64.py`, `tools/blake3_min.py`: hash implementations validated against the files' own stored values.

## Verified format facts (each one reproduced on real data)
- FName hash = CityHash64(lower(name)): 17,138/17,138 stored hashes.
- PackageId = CityHash64(UTF-16LE(lower(package name))): 127/127.
- PublicExportHash = CityHash64(UTF-16LE(lower(export name))): 127/127. The two `M_Invisible` packages at different paths
  share one hash, so the hash depends on the name, not the path.
- Store entry ImportedPackages == ids of the package's own ImportedPackageNames, same order: 127/127.
- IoHash = first 20 bytes of BLAKE3: 114/114 chunks ≤140 KB, plus the 2 public vectors.
- My reader agrees with retoc to-legacy on 127/127 packages: export byte totals and class/export names.

## Re-checks of earlier work (treated as unverified)
- GM `shader-fallback/` (2026-10-08). Confirmed: PC tokens in `M_Invisible`, 0 store hashes in the mod. Corrected:
  13 (not 5) PC-only base materials (`Simon_Slot0..7` were missed by name filtering), and 67 (not 59) material
  packages. Superseded: "Leon's own materials appeared acceptable" (an unanchored, lost screenshot; the user now
  reports Leon is checkered). Rule issue: its hide candidate `M_FluxSurfaceInvisible` comes from a hand-written
  candidate pool. The data-derived alternative is `MetaHumans/Common/Materials/M_Hide`, which the base face mesh imports.
- overlay_builder `closest-match` (agent HI): repoints the store entry only. It cannot resolve, for the reasons above.
  The fail-closed guard is staged.
- ChatGPT cap08/cap12 prep: consistent with these findings: no type 8/9 shader chunks in Leon, script classes were
  unresolved there and are resolved here. Not relied on.
- The SUSPECT files `pak-overlay/leon-replay-drift.json` and `leon-substitutions-fixture.json` are regenerated
  byte for byte by the unmodified `test_overlay_builder.py` (relocated run in `PATCH/run-orig/`, sha256 `63ceec0e…`,
  `597af39a…`). That explains their unhashed modification. Still not used as evidence. The patched suite writes
  neither file next to itself any more.

## Console-proven facts used (existing logs only)
- `evidence/launch-l26-R11-launch14-20261013/ue4ss.decoded.log` line 794: `[R3] LoadMap detour registrations deferred … InitGameState/BeginPlay/EndPlay/… stay unregistered for this process lifetime`
- same file line 941: `[R3] LoadMap deferred arm refused (pre id 0, post id 0): the LoadMap detour stays off for this process lifetime`
- same file lines 824-904: `[l22] Lua mod started:` ×7 (CheatManagerEnabler, ConsoleEnabler, SpeedHack, ItemSpawner, MiniMap, ueselftest, zzdiag)
- User observations (reports, not logs): launch 13 "Outside is very bright and some textures look wrong"; launch 14
  "Bright outdoor light and wrong textures show up in every launch". The user's 2026-10-12 message: checkered grey/white Leon,
  bright outside, monster clearly visible.

## Open questions
1. Which Leon variant is installed, and is there any other pak in `~mods`? (FTP LIST, DIAGNOSIS fetch #1)
2. Is the outdoors bright with Leon removed? (LAUNCH-CARD A/B) Was it bright in launch 11, when no Lua mods ran?
3. Base master parameter names, and how M_Hide works (fetch #4-#6). This gates #8 option B and the hide variant.
4. PS5 non-VT texture header fields: the ~64 KiB inline tail and opt data (fetch #2). Do PC-layout textures display on PS5?
   Answered by the first #8 launch.
5. Which mesh slots actually use `Simon_Slot0..7` vs `MI_Leon_*`. Needs the SkeletalMesh property schema, which is not on the Mac.
6. Not pinned locally: the engine's exact fallback path (the local Epic source subset has no material code). sRGB/LODGroup
   property names (indices only).

## Scale
26 own host tests + 32 patched overlay tests = 58 pass. 32 count categories, every count > 0 (`tests/test-output.txt`).
`tests/run_tests.sh` fails on any failure, skip, zero-test suite or zero count; proven by a planted run
(`tests/planted-failure-proof.txt`). Fixes for the review are listed in `FIXES.md`.
