# cap05 crash capture - integration steps (for the integrator)

All of this is Mac-side until step 7. Every console action in steps 7-10 needs
the user's explicit OK for that action (CLAUDE.md section 1).

## What is in this patch

| Piece | Kind | Ships to PS5? |
|---|---|---|
| `LuaMod.cpp.cap05.diff` (66 added lines; stage script `apply_cap05.py` + `cap05_crashtest_snippets.py`) | runtime change: Lua global `ModBridgeCrashTest(kind, token)`; does nothing unless token = `CRASH-THE-GAME-NOW` | yes (inside the PRX) |
| `zzcrashtest/Scripts/main.lua` + `zzcrashtest/enabled.txt` | our own diagnostic Lua mod; crashes the game once, ~150 s after start | yes, for the cap05 launch only |
| `../tools/final_symbols.py`, `../tools/elf64.py` | Mac-only: exact, byte-verified function addresses for a built PRX | no |
| `../tools/crashcap.py`, `../tools/post_harvest_crashcap.sh` | Mac-only: crash report from the klog tap + harvested logs | no |
| `run_labelled_build.sh.cap05.diff` | build script: new step 8b runs final_symbols.py after packaging (NON-FATAL: a failure is a WARN, never a lost build) | no |

No new import, no new `#include`, no filesystem call, no detour, no game/mod name.

## Steps

1. **Tools in place** (new files, Mac-only):
   ```
   mkdir -p outputs/2026-10-08-l25-prep/harvest/crashcap
   cp caps-team-C/caps-team-work/cap05/tools/{elf64.py,final_symbols.py,crashcap.py,post_harvest_crashcap.sh} \
      outputs/2026-10-08-l25-prep/harvest/crashcap/
   ```
   `crashcap.py` reads `harvest/decode_session_log.py` (absolute default path; `--decoder ''` = built-in rule).

2. **Runtime patch** into the candidate tree that the cap05 build will use
   (the next clean base after Townfall step 1; R11 = `bfd95db6` is the rollback):
   - If `candidate/src/LuaMod.cpp` sha256 is still `96e8ff4d5eb8c2a98ec37f1d54282687e169fca8dfac816b46f0299e1c38999e`:
     `cd outputs/2026-10-08-l26-runtime/typescan-r4 && patch -p1 --dry-run < .../cap05/PATCH/LuaMod.cpp.cap05.diff` then without `--dry-run`.
   - Otherwise (file moved on, e.g. B7): `python3 -I -B .../cap05/PATCH/apply_cap05.py --base candidate/src/LuaMod.cpp --out-file <staging>/LuaMod.cpp --out-diff <staging>/LuaMod.cpp.diff`
     (anchor based; refuses unless both anchors occur exactly once; inverse-checked), then copy the staged file in.
   - Do NOT bundle cap05 into the B7 launch (or any other capability's launch): one launch per capability, and a
     deliberate crash would hide the other launch's results.
   - Add the usual CHANGES.json row-sync entry (`cap05_row_sync`, new candidate sha256).

3. **Gates before compile** (each prints its count; all must PASS):
   - `fs_throw_gate.py` (expect 0 NEW; the patch adds 0 filesystem calls - Mac check: 54 call sites before and after).
   - `no_detour_gate.py --root <candidate> --patch .../LuaMod.cpp.cap05.diff` (my run on a patched copy: PASS, 48 sources + 1 patch scanned, 0 detour sites).
   - `syntax_check_tree.py --unit luamod=src/LuaMod.cpp` (my run on a patched copy: exit 0, 0 errors; a deliberate typo in a copy gave 1 error, so the check really reads the new code).
   - include-graph gate (no new `#include` lines).

4. **Build script**: `patch -p1` the `run_labelled_build.sh.cap05.diff` (from the repo root) - adds step
   `8b/9 final symbols`. It writes `outputs/2026-10-08-l26-package-A/modbridge_supplier_composition.prx.final-symbols.json`
   and a copy in the build's evidence folder. The step is **non-fatal**: if the final layout is not reproduced
   byte for byte (or the output file already exists), it prints `8b/9 final symbols: WARN: ...`, writes
   `FINAL_SYMBOLS=WARN: ...` to `<build evidence>/FINAL-SYMBOLS-STATUS`, and the build carries on to step 9
   (RESULT=PASS stays possible; the label is not spent). Success writes `FINAL_SYMBOLS=PASS`. A WARN only means
   crash reports for that build would be unsymbolized; fix the tool and re-run the hand command below - no rebuild.
   Check `FINAL-SYMBOLS-STATUS` = PASS before the cap05 launch.
   If you prefer not to touch the script, run the same command by hand after the build:
   ```
   python3 -I -B harvest/crashcap/final_symbols.py --object <PKG_A>/pass-1/build/combined-supplier.o \
     --bare <PKG_A>/pass-1/bare-original/modbridge_supplier_composition.prx \
     --packaged <PKG_A>/pass-1/modbridge_supplier_composition.prx --out <PKG_A>/modbridge_supplier_composition.prx.final-symbols.json
   ```
   Expect one line: `final_symbols: PASS sections_verified=<~78000> ... packaged=<first 8 of the new PRX sha>`.

5. **One build** (BUILD POLICY: everything for this launch, gates, build once). Record the sha in the build ledger.
   Then `verify_build.sh --manifest` for that exact PRX. Then ONE independent reviewer agent on `LuaMod.cpp.cap05.diff` (shipping runtime code).

6. **Host tests** (already run, output in `../tests/out/TEST-RUN.txt`): `bash caps-team-C/caps-team-work/cap05/tests/run_tests.sh`.
   The tests find R11 by its PRX sha (`bfd95db6...`): the packaged PRX from `outputs/2026-10-08-l26-frozen/R11-watch-world-uenum-bfd95db6/`,
   the object + bare PRX from whichever of package-A or `archive-r6/*/` holds that sha; they fail loudly if none does.

7. **Deploy** (each with the user's OK): PRX upload + decrypt-off readback (full sha256 + magic 4f153d1d);
   `ue4ss/Mods/zzcrashtest/Scripts/main.lua` (1969 bytes, sha256 `bdc9d411...`) and `ue4ss/Mods/zzcrashtest/enabled.txt` (0 bytes)
   as an exact mirror; two loadlist rows
   `file	lua_mod	ue4ss/Mods/zzcrashtest/Scripts/main.lua	1969` and `file	lua_mod	ue4ss/Mods/zzcrashtest/enabled.txt	0`;
   then the automatic loadlist row-check.

8. **Launch** per `../LAUNCH-CARD.md` (klog tap must be running before the game starts).

9. **After the launch** (user confirms the game is closed first): normal harvest + copy into the launch evidence folder, then
   ```
   bash harvest/crashcap/post_harvest_crashcap.sh <evidence-dir> <PKG_A>/modbridge_supplier_composition.prx.final-symbols.json \
     outputs/2026-10-04-zcode-controller/eboot-backup/eboot-original.bin \
     --expect-crash --expect-rip-symbol modbridge_cap05_crashtest_fault --expect-fault-va 0xc08
   ```
   Exit 0 + two `EXPECT PASS` lines = capability PASS. The report is `<evidence-dir>/CRASH-REPORT.md`.

10. **Clean up** (with OK): remove the two zzcrashtest loadlist rows and the folder (exact-mirror rule), row-check.
    The runtime function may stay in later builds (inert without the token) - user's call. Rollback = R11 `bfd95db6`.

## Also recommended (not required for the launch)

- Use `crashcap.py` (block count) instead of harvest.py's "copyin/nonsleeping" text hits as the crash verdict:
  that kernel note is printed INSIDE most crash dumps, and harvest.py does not read the dump itself.
- Stop using the sidecar's `map_reconstructed` rows: they name the wrong function for 18 of 18 real frames
  (see `../evidence/sidecar-map-reconstructed-check.json`). `phasea-tools/symbolize_crash.py` also misses
  every "abort is called" block (0 of 1 in launch 3, 1 of 8 in the l23 klog) - crashcap replaces it.
