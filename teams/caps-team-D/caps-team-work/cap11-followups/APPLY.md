# APPLY: cap11 follow-ups (integrator only)

Team D never touched the live tree, a build or the console. Everything below is for the integrator.

## What is in PATCH/

- `post/` full post-images, at their live relative paths under `typescan-r4/`
- `base/` the live pre-images they were made from (read-only copies), `diffs/*.diff` unified diffs (`ALL.diff` = all four)
- `BASES.json` live path, base sha256, post sha256 for each file; `apply_followups.py` (check / apply)

| File (under `outputs/2026-10-08-l26-runtime/typescan-r4/`) | base sha256 (live = R15e) | post sha256 |
|---|---|---|
| `include/Platform/NativeGuiHost.hpp` | `ca9021097d3603716a9d17e885e0823cd0b8e022331dc9ae9402d0f6c8a88df8` | `7ee5124f28c6aa0922a66d7c0bf439da15e5ac134a187c8d8a955ca6d1b31ce6` |
| `include/Platform/NativeModHostCore.hpp` | `6b521ddb8e2e7443c64a82b357cbc4c201e64501d05d1a266b90d35f5b01b629` | `691e36e0fb8b73dde50715357d110f2d1f299e138ab6ee5d1938802c4dff8464` |
| `candidate/src/Mod/NativeGuiEngineBackend.inc` | `9ade839484aecefb2ceb1e635d1d17fa49fcf7de17430a2b536dd65f57867065` | `8716719204da03dc7d614286fe1627c00d6f5e29835df29a3abc10e44d69d617` |
| `candidate/src/Mod/NativeModEngineServices.inc` | `ca928339172c439ff0a7a0a73a570daf53c8b45d2bbff4fd83d3f90745bc84ed` | `07279af60bc7faa89fda91e76f376cfd094bb64c255394b0ffced2be0f4b7fe8` |

Not changed (relied on): `ImGuiCanvasBatcher.hpp` (its existing `BatchOptions::scale_x/scale_y` does the unit-to-pixel
scaling), `ImGuiAtlasRuns.hpp`, `mbm_imconfig.h`, `NativeModAbi.h`, `HookAdmission.hpp`, `CppMod.cpp` (`fcc5e79d`, so its
CHANGES "files" row stays), `NativeGuiImGuiUnity.inc`. No new file, no new unit, no new include line.

## Steps

1. Check (read-only): `python3 -I /WORKSPACE/.codex/worktrees/translation-core/caps-team-D/caps-team-work/cap11-followups/PATCH/apply_followups.py`
   - must print `apply_followups mode=check items_checked=4 problems=0` and `(base)` for every file. If a file says
     `MOVED`, stop: the live file changed after staging; the patch must be re-staged on the new base.
2. Apply: the same command with `--apply`. It refuses unless all four files are still at their base sha, then:
   - backs up each file as `<file>.pre-cap11-fu-20261013` and `CHANGES.json` as `CHANGES.json.pre-cap11-fu-20261013`;
   - copies the four post-images;
   - sets these four `added_header_hashes` values in `typescan-r4/CHANGES.json` (keys = the full live paths above):
     - `.../include/Platform/NativeGuiHost.hpp` → `7ee5124f28c6aa0922a66d7c0bf439da15e5ac134a187c8d8a955ca6d1b31ce6`
     - `.../include/Platform/NativeModHostCore.hpp` → `691e36e0fb8b73dde50715357d110f2d1f299e138ab6ee5d1938802c4dff8464`
     - `.../candidate/src/Mod/NativeGuiEngineBackend.inc` → `8716719204da03dc7d614286fe1627c00d6f5e29835df29a3abc10e44d69d617`
     - `.../candidate/src/Mod/NativeModEngineServices.inc` → `07279af60bc7faa89fda91e76f376cfd094bb64c255394b0ffced2be0f4b7fe8`
   - adds a `cap11_fu_row_sync` note (date, content, the four shas). The file keeps its format (indent 2, no trailing newline).
   - Manual alternative: copy the four `post/` files over the live ones and edit only those four values.
3. Gates before the build (your usual invocations):
   - `fs_throw_gate.py`: the four files have 0 std::filesystem sites (tests/output/gates/fs_throw_gate_changed.log).
   - `no_detour_gate.py`: PASS on a patched copy (tests/output/gates/no_detour_gate.log).
   - `field_gate.py`: expect `49 distinct engine fields read: 48 proven, 1 not in this game's layout data` and PASS,
     the same list as the live tree (tests/output/gates/fields-*.txt are identical).
4. ONE build, then `verify_build.sh --manifest` for the exact PRX, row-check, as always. Mac evidence that the build
   should pass: `tests/output/target/target_compile.log` (cppmod with R15e's own command: 0 errors,
   **0 new undefined symbols**, 0 duplicate definitions vs R15e `runtime-l26-A.o` 84dfccb3).
5. Optional after apply: `tests/run.sh live` then compiles the (now patched) live headers: expect 13/13 PASS.

## Launch card hints (#11 re-launch on Townfall, 4K TV)

Expected log lines (this session's log, decoded with decode_session_log.py):
- `[GUI] surfaces built panel=1100x864 rt=2200x1728 viewport=3840x2160 scale=2.000000 units=1920x1080 pos=772,108`
- `[GUI] font baked size=20 density=2.000000 atlas=512x512`
- `[GUI] atlas route=png-import bytes=...`, `[GUI] atlas uploaded runs=...`
- #9 probe: `[NPROBE] game-thread tick n=1800` about every 30 s at 60 fps (before: about every second).

Must NOT appear: `[GUI] imgui assert`, `[GUI] refused`, `[GUI] viewport scale unavailable`,
`[GUI] sharp-font bake ... skipped`, `[GUI] surfaces could not be built`.

User check: open the window (same chord as launch G): same size and place as before, text and lines crisp; the
2x-speed button still works; close the window: movement and camera work at once.

Known, accepted: one retired (collapsed, empty) widget stays in the viewport per resolution/DPI change until the next
map change; the window render target (~15-30 MB at 4K) stays allocated after the first open until the next map
change, as before (now 4x larger).

Rollback: the last clean build (R15e `53a64553`), or restore the four `*.pre-cap11-fu-20261013` files and CHANGES.json.
