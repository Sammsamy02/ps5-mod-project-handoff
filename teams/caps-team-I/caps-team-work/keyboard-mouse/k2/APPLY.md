# K2 (keyboard and mouse, engine keys): integrator apply steps

Team I2, 2026-10-14. Everything here is **Mac-only**. Nothing was run on the console, nothing in the live runtime
tree, frozen builds, build folders, mod files or harvest tools was changed. No PRX was built or packaged (the
target check compiles two objects into `tests/target-check/` only, no link).

## What the patch does (one paragraph)

On the game thread (the R3 marshal drain, the pump that runs on PS5), at most every 8 ms and only while at least one
key with an engine name is bound, the runtime asks the
local PlayerController's own reflected `IsInputKeyDown(FKey)` about the keys mods bound (plus the 6 modifier keys,
only on a frame with a key edge) and turns up->down changes into normal UE4SS key events. The handler gets
`Merged{ the PS5Pad.ini source (only when the composition made one), engine keys }`. Every engine fact (function,
parameter layout, bool byte/mask, `KeyName` offset, FName size) comes from reflection at run time; any failed check
switches the engine-keys half off for good and the pad chords are untouched.

## Files

| Path (relative to `outputs/2026-10-08-l26-runtime/typescan-r4/`) | Base sha256 | New sha256 |
|---|---|---|
| `candidate/src/UE4SSProgram.cpp` | `b6a68e3f945b76f8ffef7caa5652b299022dd775b82f6a17823c8b288e4a451a` (live) | `aedf2ed4a331fbf03ad89718d91c5c57b18c092d9bead3c7f4a23ddeb6761800` |
| `candidate/src/LuaMod.cpp` | `5f09ec1ee024cf7d8cc60ceac860bdf11711af415bd6a9517e0da49739eeb33a` (live = R17b, cap07s3 already in) | `f5b5b86c512e179816662dd704710fd61508f73c7b6740c7924f340e9635f9d2` |
| `candidate/src/KbmEngineKeysRuntime.inc` | new file | `8daf496ce8a4730d70cfca63df223937719d7b4c9aee3504b4dbc14819624d49` |
| `include/Platform/KbmEngineKeys.hpp` | new file | `2caaf977cd33fce773992cc541a3b208c1b2d46002c77b16edd9b560ec4fbe46` |

`k2.patch` sha256 `ee6804be7cdfc0f50bbc69d8c74a66143d9a02569340b883d1c791d070061a51` (= `diff -ruN base new`, apply with `-p1` inside `typescan-r4/`).
Checksums: `BASE-SHA256SUMS`, `NEW-SHA256SUMS` (paths relative to `typescan-r4/`).

Changes in the two live files (everything else is in the two new files):
- `UE4SSProgram.cpp`: (1) include block after `#include <UnrealDef.hpp>`: `#include "KbmEngineKeysRuntime.inc"`
  under `#if defined(__PROSPERO__) && defined(HAS_INPUT)` (no-op `KbmGameThreadFrame` otherwise);
  (2) `validate_native_services`: `Platform::KbmComposeInput(m_native_context.input_source);` right before the
  existing `if (m_native_context.input_source)`; (3) `shutdown_native_program`: `Platform::KbmRetire();` right after
  the successful `m_input_handler.shutdown()`.
- `LuaMod.cpp`: one declaration next to the `[CAP09] NativeModsGameThreadFrame` one, and the call
  `RC::Platform::KbmGameThreadFrame();` right after `NativeModsGameThreadFrame(0)` in `funcptr_marshal_hook` (the R3
  marshal drain, the pump that runs on PS5) and right after `NativeModsGameThreadFrame(1)` in `VmtEngineTickDrain`
  (parity with #9 / cap07, review item 3; the sampler's 8 ms gate keeps it once per frame when both run). The hunks
  are far from the cap07s3 hunk; the patch applies to the live file as it is now.

**Where the composition lives (deviation from DESIGN.md section 5, on purpose).** DESIGN.md names
`NativePadInput.cpp` / `ConfigureRetainedPS5PadInput`. That unit (`native-pad-input`) is a RETAINED unit (its
object is reused from run-l19-2, its source is outside the l26 tree), and the include-graph gate would refuse the
new headers there. `UE4SSProgram::validate_native_services` is the single consumer of the composed source, is
already a fresh unit with a CHANGES row, and runs right after `ConfigureRetainedPS5PadInput` (ue4ss_backend.cpp),
so the result is identical: `Merged{pad only if PS5Pad.ini made one, engine keys}`. `NativePadInput.cpp` and
`PS5Pad.ini` are unchanged (no MFA size re-pin).

## Steps

```
K2=/WORKSPACE/.codex/worktrees/translation-core/caps-team-I/caps-team-work/keyboard-mouse/k2
T=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4
cd "$T"
shasum -a 256 -c "$K2/BASE-SHA256SUMS"                     # both live files must say OK
cp -p candidate/src/UE4SSProgram.cpp UE4SSProgram.cpp.pre-k2-20261014
cp -p candidate/src/LuaMod.cpp       LuaMod.cpp.pre-k2-20261014
cp -p CHANGES.json                   CHANGES.json.pre-k2-20261014
patch -p1 --dry-run < "$K2/k2.patch" && patch -p1 < "$K2/k2.patch"
shasum -a 256 -c "$K2/NEW-SHA256SUMS"                      # all four must say OK
python3 -I "$K2/apply_changes_rows.py" "$T/CHANGES.json"   # rows below; refuses if any base pin differs
```

### CHANGES.json rows (done by `apply_changes_rows.py`; it asserts every old value first)

- `files[]` row `candidate/src/UE4SSProgram.cpp`: `candidate_sha256` `b6a68e3f…` -> `aedf2ed4a331fbf03ad89718d91c5c57b18c092d9bead3c7f4a23ddeb6761800`.
- `files[]` row `candidate/src/LuaMod.cpp`: `candidate_sha256` `5f09ec1e…` -> `f5b5b86c512e179816662dd704710fd61508f73c7b6740c7924f340e9635f9d2`.
- `added_headers` += `…/typescan-r4/include/Platform/KbmEngineKeys.hpp`, `…/typescan-r4/candidate/src/KbmEngineKeysRuntime.inc`
  (42 -> 44); `added_header_hashes` += the two new shas (42 -> 44).
- new key `k2_kbm_row_sync` (date, by, content, files, backups).
- Nothing else: no `build_variants.py` change (no new unit; the `.inc` is compiled inside `program`, the pattern of
  `VmtTickPumpCore.inc` / `NativeModEngineServices.inc`), no new include dir (`typescan-r4/include`,
  `include/Platform` and `candidate/src` are already in the include dirs), no new compile flag, no settings change,
  no `PS5Pad.ini` change. Fresh units: `program` and `luamod` (both already carry `files[]` rows).

### Markers

Add the `K2` set from `MARKERS.txt` to `harvest/add_build_markers.py` `SETS` (rename the set to the build label
prefix), then `python3 -I add_build_markers.py <set>`, so `verify_build.sh` proves the new code is in the PRX.

## Expected gate results (run_labelled_build.sh step 1), measured on a mirror of the live tree with the patch applied

| Gate | Live now | Expected after apply |
|---|---|---|
| fs_throw_gate pre-compile (`--files-from compiled-files-1e49188b.txt --root candidate --baseline …1e49188b`) | 3570 files, 69 sites, 49 known, 0 NEW, PASS | 3571 files (+ the `.inc`), 69 sites, 49 known, **0 NEW**, PASS |
| no_detour_gate (default: candidate tree) | 56 sources, 0 sites, PASS | **57** sources, 0 sites, PASS |
| extra roots (`typescan-r4/include` + `cap10/native`): no_detour / fs_throw | 40 / 40 files, 13 sites (all error_code), PASS | **41 / 41** files, 13 sites, 0 violations, PASS |
| field_gate (`--root candidate`) | 49 files, 49 distinct fields (48 proven + 1 accepted), PASS | **50** files, the **same 49** fields, 0 unproven, PASS |
| layout_binder_gate | unaffected (no declared field added) | unchanged |
| build_variants include-graph (compile_unit) | — | no refused path (tests/target-check: 2679 deps checked, 0 refused) |

Evidence: `gates/` (`pipeline/live-*.txt`, `pipeline/mirror-new-*.txt`, `fs_throw_gate-ratchet-new.txt`,
`no_detour_gate-{base,new}.txt`, `field_gate-{base,new}.txt` and the field lists). Note: the folders
`gates/pipeline/mirror-final/` and `gates/ratchet/tree-final/` are scratch copies of the runtime tree used for those
runs. A safety check refused my clean-up (`rm -rf` on them), so they are still there: delete them by hand, never
commit them.

## No new import (inspection + object check)

- New code calls only: libc++ inline templates, `std::chrono::steady_clock::now` and `std::snprintf` (both already
  used by the runtime; `clock_gettime` / `snprintf` are already in the import list), and runtime-internal functions
  (reflection accessors, `FindAllOf`, `FName` constructor, `UObject::ProcessEvent`, `Output::send`, `Handler`,
  `ModifierKeys`).
- `tests/target-check/TARGET-CHECK.txt` (exact l26-A compile argv, objects only): the new `program` object adds
  33 undefined symbols over the base object, **all 33 defined in the latest composed `runtime-l26-A.o`**; the new
  `luamod` object adds 1 (`RC::Platform::KbmGameThreadFrame()`), defined by the new `program` object. 0 symbols
  outside the link, so the PRX import set cannot grow. After the build, `EXACT-IMPORT-IDENTITIES.json` must show
  the same count as the R17b build.
- No new compiler warning (program 6 = 6, luamod 13 = 13).

## Behaviour changes to know

0. Cost: nothing is asked of the engine while no key with an engine name is bound. The controller search
   (`FindAllOf("PlayerController")`, the same lookup the #11 GUI uses from this pump) runs only while keys are bound
   and no controller is cached: at once, then after 0.5, 1, 2, 4, 8, 8 ... s (reset after a find); a search that took
   4 ms or more waits the full 8 s. The cached controller is checked through its object-array slot only (the object
   is never read after the engine may have freed it). A slow patch raises the sample divisor (2, 4, 8); 60 sampled
   frames in a row under budget halve it again.

1. With a `PS5Pad.ini`: the handler's source is `Merged` (name "Merged", not "PS5Pad"; nothing on PS5 reads the
   name). Pad availability, activation, deactivation, retry and terminate-on-failed-deactivate are unchanged
   (host test `controller_events_identical_with_and_without_keyboard_route`).
2. **Without a `PS5Pad.ini`** (a game with no pad profile yet): before K2 the PS5 handler had no source and
   `RegisterKeyBind` threw ("no active input source"); now `Merged{engine keys}` is attached (deferred, like the
   held pad) and keyboard binds work. Host test `compose_without_pad_profile_gives_keyboard_binds`.
3. Evidence note: keyboard key presses now also produce `pad_key_edge` diagnostic tags (the handler tags every
   arriving key event, budget 60 per boot). In K2 and later reports those tags are not proof of controller input.
4. Off switch: none at run time beyond fail-closed (the engine-keys half switches itself off on any failed check;
   with no keyboard plugged in the engine answers "up" and nothing happens). A user-facing ini switch would need a
   `SettingsManager` change and is not in this patch; the rollback build is the off switch.

## Tests and review

- Host: `tests/run.sh` -> `tests/HOST-RUN.txt` (table gate 730 items; 49 tests / 5537 checks under ASan+UBSan,
  including 18 reader reflection-check cases on a fake reflection model; TSan two-thread stress 10000/10000; the
  zzkbbind Lua test 9 checks). The run fails on zero tests or zero checks.
- Target check: `python3 -I tests/target_check.py` -> `tests/target-check/TARGET-CHECK.txt`.
- One independent review: `REVIEW.md` (SHIP-WITH-FIXES, 0 blockers; items 1-3 and 5 fixed, 4 partly, 8 added tests).

## Deploy for launch K2 (see LAUNCH-CARD.md)

- The new PRX through the normal verify_build / manifest / decrypt-off readback rules.
- Test mod `../probe/zzkbbind/` as an exact mirror `ue4ss/Mods/zzkbbind/`, rows:
  `file	lua_mod	ue4ss/Mods/zzkbbind/Scripts/main.lua	570` and `file	lua_mod	ue4ss/Mods/zzkbbind/enabled.txt	0`
  (main.lua sha256 `45604aa20090ba22ad61392b105b9092ff12136491a4c1c17a2eeb71f02d4551`), then the row-check.

## Rollback

The last clean build. To undo the source: copy back the two `.pre-k2-20261014` files and `CHANGES.json.pre-k2-20261014`,
and delete the two new files.
