# APPLY — capability #7 step 1 (automated "Leon" index method)

All of this is Mac-side tooling. **Nothing in the runtime candidate tree changes,
no PRX is built**, the R11 runtime (PRX bfd95db6) stays as it is.

## What is staged

| File | What | Integration |
|---|---|---|
| `tools/paktx.py` | the one-command tool (record / check / repair / install / verify / restore / cycle / probe). Stdlib only. Loads `overlay_builder.py` by **pinned sha256 a72636fb…** (exec from verified bytes, no `__pycache__`). Calls the pinned repak (sha 57b321cc…) only for classic `.pak` rebuilds. | New file. Copy to `outputs/2026-10-08-l25-prep/pak-overlay/paktx.py` (next to `overlay_builder.py`), or leave it here and pass `--builder <path to overlay_builder.py>`. Do not edit `overlay_builder.py`: the pin would refuse. |
| `records/townfall-PPSA33286-02.003.000.PARTIAL.record.json` | game-build record built from the 2026-10-01 console captures (record_sha256 `33616d2f…`). **PARTIAL**: it lacks 6 files (see step 2). `repair` and `check` refuse a partial record (`record_is_partial`), and the install gate refuses too. | Reference / starting point. Rebuild with step 3. |
| `records/headers/pakchunk0-ps5.container-header.bin` | exact copy of `.superpowers/native-observations/townfall-original-container-header-1/container-header.bin` (sha `d7b6f7ca…`), tied to the captured index by its IoHash. | Keep next to the record (the record points to it by a path relative to the record's folder). |

Paths below: `T=outputs/2026-10-08-l25-prep/pak-overlay/paktx.py`, `ROOT` = the game's
paks folder `/data/homebrew/Silent Hill Townfall PPSA33286/townfall/content/paks`,
`FTP=<console LAN address>:2121` (never written into any report).

## What the tool's exit codes mean

| Exit | JSON `result` | Meaning | What to do |
|---|---|---|---|
| 0 | `installed` / `repaired` / `repair-step-done` / `restored` / `checked` / `verified` / `dry-run` | done | next step |
| 2 | `refused`, `"console_changed": false` | **nothing on the console changed** (no change was started) | read `code`, fix the cause, tell the user |
| 3 | `stopped`, `"console_changed": "maybe"` | a change was started, so the console MAY have changed. Every error after the first started change lands here (failed verify, refusal, lost connection). | run the recovery the `detail` names: `restore … --dry-run` (install) or `repair … --dry-run` (repair), show it to the user, then run it with their OK |

## Gates that must pass

1. Host tests: `sh tests/run_tests.sh` → `SUMMARY tests_run=45 failed=0 skipped=0`,
   `GATE PASS`, `real replay: PASS 155059`, `RUN PASS tests_run=45 replay_items_checked=155059`
   (output saved: `tests/output/host-tests.log`, `tests/output/real-leon-replay.json`).
   The runner fails on zero tests, any skip, any failure, or zero replay items.
2. One independent reviewer for `paktx.py` before its first console use (done: `REVIEW.md`;
   fixes: `FIXES.md`).
3. At run time the tool gates itself: the `gate` step walks the WHOLE paks folder tree
   (subfolders such as `~mods/` included) and prints
   `gate: N recorded files match (…); E entries walked in F subfolders; K inert extra names`.
   It refuses on zero files, any size/sha mismatch, any link, or any unrecorded
   `.utoc/.ucas/.pak/.sig` at any depth.
4. Runtime gates (`fs_throw_gate.py`, `no_detour_gate.py`, `verify_build.sh`) do
   not apply: no runtime source or PRX changes. If you deploy the optional probe,
   it is Lua-only: fast Lua deploy plus the usual loadlist row-check.

## Steps (each console action needs the user's OK for that action)

0. Ask the user to confirm the game is closed. Keep FTP decrypt OFF (`SITE SELF` off).
   None of these are executables, but keep the rule.
1. **Read only (game closed):** recursive LIST of `ROOT` (the top folder and every
   subfolder), saved to the evidence tree. Compare with the 2026-10-08 scan
   (`outputs/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/harvest-20261008T174239Z-25929d/storage/files.json`)
   and with `../CONSOLE-FACTS.md` (2026-10-13 LIST).
   Expected leftovers: a 29,870,724 B `pakchunk0-ps5.utoc` (old Leon index),
   two `pakchunk0-ps5.utoc.original-*.bak`, `pakchunk0-ps5_s1.ucas`, the
   `zzz_Leon_Townfall_2K_P.{utoc,ucas,pak}` trio (top level in both scans; check
   subfolders such as `~mods/` too), a 297,483,249 B `pakchunk0-ps5.pak`
   (the Engine.ini experiment) and `pakchunk0-ps5.pak.original-9a914e38-….bak`.
2. **Read only:** RETR these into a fresh mirror folder (in the evidence tree):
   `pakchunk1-ps5.pak`, `pakchunk10-ps5.pak`, `pakchunk11-ps5.pak`, `pakchunk12-ps5.pak`,
   `global.utoc`, and **`pakchunk10-ps5.ucas` (113,396,720 B)**: Leon owns 2 packages in
   that container and its ContainerHeader is only inside this `.ucas`. Check that each
   file's size and date match both the 2026-10-01 capture listings and the 2026-10-08
   scan, so they are the untouched originals. Expected dates: **`Oct 01 16:54`** for the
   `pakchunk*` files, **`Oct 01 16:40`** for `global.utoc` / `global.ucas`.
3. **Build the complete mirror + record (Mac):** the mirror holds the ORIGINAL files.
   Symlink the 2026-10-01 captures for `pakchunk0-ps5.utoc`
   (`.superpowers/native-observations/townfall-main-utoc-2026-10-01/`, sha `35de2752…`),
   `pakchunk1/10/11/12-ps5.utoc` (same folder) and `pakchunk0-ps5.pak`
   (`townfall-main-pak-full-2026-10-01/`, sha `ee145cbc…`). **Never use the live
   `pakchunk0-ps5.utoc/.pak`: they are modified.** Add the step-2 files. Write
   `sizes.json` with the sizes of the `.ucas` files you did NOT mirror (from the listing:
   pakchunk0 128240377200, pakchunk1 62955632, pakchunk11 43697344, pakchunk12 183707072,
   global 3412992). Write `provenance.json` that says where each sha came from, and
   which are **single source** (review N6):
   ```json
   {"label": "built Mac-only from read-only console captures",
    "two_sources": {"pakchunk0-ps5.utoc": "2026-10-01 capture + .original-*.bak re-hashed by repair",
                    "pakchunk0-ps5.pak": "2026-10-01 capture + .original-9a914e38 .bak (console pull 2026-10-13)"},
    "single_source": ["pakchunk1-ps5.pak", "pakchunk10-ps5.pak", "pakchunk11-ps5.pak",
                      "pakchunk12-ps5.pak", "global.utoc",
                      "note: today's file, trusted by size + date only; no experiment touched them, no .bak exists"]}
   ```
   Then:
   ```
   python3 $T record --mirror MIRROR --sizes sizes.json --headers records/headers \
     --provenance provenance.json --title-id PPSA33286 --version 02.003.000 \
     --paks-dir "$ROOT" --out records/townfall.record.json
   ```
   Expect `record: 17 files, 6 containers` (6 .utoc, 6 .ucas, 5 .pak), and in the record a
   `header` for both `pakchunk0-ps5` and `pakchunk10-ps5` (`iohash_matches_toc: true`).
4. **Bring the paks folder back to the original game: follow `../PRISTINE-PAKS.md`.**
   It uses `repair --only …` in three console steps (one OK each), then the read-only
   `check`. Nothing is deleted. The step ends when `check` prints `"verdict": "PRISTINE"`.
5. **Install Leon (console uploads and renames):**
   ```
   python3 $T install --record records/townfall.record.json --mirror MIRROR \
     --mod <folder with the unchanged Leon 2K v1.1 files> --work W/leon --dry-run
   ```
   Expected: 2 targets, `pakchunk0-ps5` (66 replaced + 59 new packages) and
   `pakchunk10-ps5` (2 replaced). README and the empty companion `.pak` are listed as
   ignored. Show the printed list to the user. **After the user's OK:** add
   `--ftp $FTP --confirm <sha12> --game-closed`. Sequence: gate, then reserved names free,
   then 4 uploads each re-read and sha-verified, then per container: sidecar into place,
   original index renamed to `.original-<id>.bak`, derived index into place, then a full
   re-read of each live index and backup. **On exit 3 (stopped), run step 8 at once.**
6. *(optional, recommended: makes the PASS console-provable)* probe:
   `python3 $T probe --plan W/leon/plan.json --mod <Leon folder> --out zzpakprobe/`
   writes a 1 KB Lua `main.lua` (8 object paths that exist ONLY in the mod). Deploy it as
   a Lua-only diagnostic folder (exact mirror + loadlist row-check, size-exact row), or
   as this launch's `zzdiag` content. It scans every 6 s for 30 rounds (~3 min).
7. `python3 $T verify --plan W/leon/plan.json --ftp $FTP` → `"verdict": "INSTALLED"`.
   Run the loadlist row-check, then launch (user OK).
8. **Restore (rollback, byte-exact):** game closed, user OK:
   `python3 $T restore --plan W/leon/plan.json --ftp $FTP --confirm <sha12> --game-closed`.
   It puts the original indexes back by rename and re-reads them (sha). If a live index
   matches neither the original nor our derived file (e.g. it did not verify), restore
   renames it aside to `<name>.foreign-<id>.retired` (kept) and still puts the verified
   original back. `verify` then says `PRISTINE_WITH_INERT_SIDECAR`.
   **Without cleanup, `pakchunk0-ps5_s1.ucas` and `pakchunk10-ps5_s1.ucas` stay in the
   paks folder, and the next `install` gate refuses** (`unrecorded_container_files_present`)
   until they go: either `restore … --cleanup --allow-delete` (a deletion: its own OK;
   deletes only files this plan owns, after a re-hash), or `repair --only <those names>`
   (moves them out, no deletion). Then `verify` says `PRISTINE`.
   The step-4 repair (original `pakchunk0-ps5.pak`, darkness back) is **kept** by restore.

Run `restore --dry-run` at any time to see what it would do. It decides from the live
file identities, so it also works after an interrupted install.

## Fallback if the game crashes with Leon installed (review N5)

This launch is the first console run of two new things at once: a derived index in a
second container (`pakchunk10-ps5`) and a cross-container import. If the game crashes
and the logs cannot say which one caused it: restore (step 8), then the console-proven
shape, one container only:
```
python3 $T install … --only-into pakchunk0-ps5 --dry-run
```
It installs the 125 packages of `pakchunk0-ps5` and prints
`NOT INSTALLED (--only-into pakchunk0-ps5): 2 packages … owned by pakchunk10-ps5`
(the 2 "Deluxe" pieces keep the game's own look). That is a separate launch with its own OK.
