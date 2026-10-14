# APPLY: capability #8 whole-chunk intake (Mac-side tool and a Lua-only diagnostic)

## What this patch changes

- Nothing in the candidate runtime tree. There is no C++ change, no PRX build, and R11 `bfd95db6` is reused.
- It adds new Mac-side files only. These are tools that run at install time and never ship to the console:

| File | Role |
|---|---|
| `pak-intake/ue_formats.py` | Bounded readers for the UE5 IoStore and Zen formats: TOC v8, container header v5, the Zen package header for UE 5.6, and ScriptObjects. Also CityHash64 (package ids) and BLAKE3 (IoHash). Anything outside the admitted profile fails closed. |
| `pak-intake/chunk_intake.py` | The #8 gate. It checks the format profile, identity, the import closure (package level and export level) and the replacement export check. It discloses visual loss for materials and textures, writes the exact base fetch plan, and generates the zzintake probe. |
| `pak-intake/intake_gate.py` | The pre-install gate the #7 install step calls. It checks the report against the exact mod files about to be installed, needs a non-zero item count, and needs a recorded acceptance of any visual loss. |
| `pak-intake/config_disclosure.py` | A plain-language disclosure for `.ini` config mods: which cvars change fog, exposure, shadows and so on, compared with the game's own ini. |
| `pak-intake/zzintake_template.lua` | The generic Lua probe template. It names no game and no mod. The data table is generated from the mod's own files. |

## Integration steps

All paths below are absolute. Set these two names once in the shell first:

```sh
CAP=/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap08
L25=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep
```

1. Copy the files:

   ```sh
   mkdir -p "$L25/pak-intake"
   cp "$CAP"/PATCH/pak-intake/* "$L25/pak-intake/"
   ```

2. Run the host gate (Mac): `bash "$CAP/tests/run_tests.sh"`.
   - It must print `== TOTAL tests ran: 59, failing modules: 0, skipped: 0, modules with zero tests: 0` and
     then `PASS`.
   - It fails on any failing test, on any skipped test (missing real files, lua/luac or a second source),
     and on zero tests in any module. `test_leon_real` takes about 90 s.

3. Fetch the base inputs (integrator, read-only FTP range reads, with the game closed). Use
   `$CAP/evidence/leon-intake/leon-base-fetch-plan.json`.
   - For each chunk, read every `physical_ranges` entry `{offset, length}` from
     `/data/homebrew/<game>/townfall/content/paks/<ucas>`.
   - Concatenate the ranges and save the result as `<fetched-dir>/<save_as>`.
   - Priority 1 and 2: 21 chunks, 3.68 MB. These are needed for the closure, the material parents and the
     texture samples.
   - Priority 3: 68 chunks, 141 MB. These are the base versions of the replaced packages, needed to check
     that the replacement keeps their exports. Fetch them second.
   - The intake re-checks each file against the base TOC IoHash. A bad file (for example a short read) is
     reported as `INCOMPLETE ... re-fetch it` and is planned again; it is never used.

4. Re-run the intake with the fetched files:

   ```sh
   source "$CAP/tests/leon_args.sh"
   EV="$L25/evidence/cap08-leon-intake"; mkdir -p "$EV"
   python3 "$L25/pak-intake/chunk_intake.py" "${LEON_ARGS[@]}" \
     --base-fetched <fetched-dir> --out "$EV/leon-intake-report.json" \
     --fetch-plan "$EV/leon-base-fetch-plan.json" --emit-diag "$EV/diag"
   ```

   - Expected: exit 3 `ADMITTED-WITH-VISUAL-LOSS`.
   - Exit 4 means inputs are still missing. Read the `INCOMPLETE:` lines. The new fetch plan says exactly
     what is left. **A second fetch round can be needed**: once the first headers arrive, the tool can see
     further base parents or texture samples it could not name before. Fetch the new plan and re-run until
     the exit is no longer 4.
   - Exit 2 means refused (a defect in the mod itself). Do not install.
   - The tool deletes any older report and plan at those paths before it runs, so a crash never leaves a
     stale report in place.

5. Disclosure: show the user the `VISUAL LOSS` lines in plain words. If they accept, run

   ```sh
   N=/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations
   python3 "$L25/pak-intake/intake_gate.py" --report "$EV/leon-intake-report.json" \
     --mod-utoc $N/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc \
     --mod-ucas $N/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.ucas \
     $(for c in 0 1 10 11 12; do echo --base-utoc $N/townfall-main-utoc-2026-10-01/pakchunk$c-ps5.utoc; done) \
     --script-objects-utoc $N/townfall-manifest-capture-2026-10-01/global.utoc \
     --accept-visual-loss "<user, date>" --record "$EV/intake-gate-record.json"
   ```

   - It must print `PASS` and `record written:`. The #7 install step must not install a pak mod without
     this PASS.
   - The gate checks the mod files, AND that the report was made with exactly these base `.utoc` files and
     this ScriptObjects file (by sha256). A report made before a game update fails.
   - The base `.utoc` copies must be the game's ORIGINAL ones (the 29,707,562-byte pakchunk0 utoc, not the
     rewritten one). See `../PRISTINE-PAKS.md`.

6. Diagnostic (Lua only, so no rebuild): deploy `$EV/diag/zzintake/` as `ue4ss/Mods/zzintake/`.
   - It holds `Scripts/main.lua` and `enabled.txt`. Use the file generated in step 4, not an older one.
   - Deploy it as an exact mirror of the local folder, add its loadlist rows, and run the row-check.
   - It is our own diagnostic folder. No mod file is touched.

7. Launch per `$CAP/LAUNCH-CARD.md`.

## Gates that apply

- `run_tests.sh` (above) must PASS: 59 tests, 0 failing, 0 skipped.
- `intake_gate.py` must PASS before the pak install, and its record is kept in the evidence tree.
- Not applicable: `fs_throw_gate.py`, `no_detour_gate.py` and `verify_build.sh`. No runtime source or PRX
  changes. The R11 PRX decrypt-off readback (full sha and magic 4f153d1d) still runs as usual before the
  launch.
- The loadlist row-check runs after the zzintake deploy, as for any Lua deploy.

## Wiring into #7 (owner of the mount step)

Before #7 writes or arms anything for a pak mod:

1. Call `intake_gate.py` (with the base files and `--record`).
2. Refuse the install on a non-zero exit.

The gate reads only the report, the two mod files and the given base files.
