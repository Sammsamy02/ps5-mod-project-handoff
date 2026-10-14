# FIXES: capability #7 step 1 (Team A), after the independent review

All Mac-only. No console contact, no candidate-tree or mod-file change, no PRX built.
Tool: `PATCH/tools/paktx.py` (sha256 after fixes: `174df9dfde7bb96cc6077c641b5f5c7a9af92e13f67d81e7d787463d633ee31b`).
Shared runbook added: `../PRISTINE-PAKS.md`.

## Findings → result

| # | Finding | Result | What changed | Test evidence |
|---|---|---|---|---|
| F1 | gate and repair never look in subfolders of `paks/` | **fixed** | New `walk_tree`: recursive listing of the whole `paks/` tree (depth limit 8, size limit, refuses links, refuses an unreadable subfolder). `run_gate` refuses any unrecorded `.utoc/.ucas/.pak/.sig` at any depth. `make_repair_plan` quarantines them keeping their subfolder path, outside `paks/`, and checks each moved file at its new place (`present` op). New read-only `check` command prints the recursive listing and runs the gate. | `T11Subfolders` (5 tests): the trio in `~mods/deeper/` → install refused `unrecorded_container_files_present`, nothing changed; inert subfolder passes, a link is refused; repair moves `~mods/` trio to `paktx-quarantine-*/~mods/`, no file deleted, `check` → PRISTINE; the full PRISTINE-PAKS sequence over FTP with spaces in the path (MKD, cross-folder RNFR/RNTO, no DELE, no base `.ucas` touched). Against the OLD tool, `test_gate_refuses_mod_in_subfolder` and `test_repair_quarantines_subfolder_files_keeping_structure` FAIL (scratch run). |
| F2 | after a change, a failure is reported as "refused (nothing changed)" | **fixed** | `execute` now does every pre-check before the journal's `pending` entry, and backends count started mutations (`attempted`). Once any change was started, every later error (refusal, failed verify, lost connection, ftplib error) becomes `Stopped` → exit 3, `"result": "stopped"`, `"console_changed": "maybe"`, with the recovery to run. Before any change: exit 2, `"console_changed": false` (also for unexpected I/O errors, which used to be a traceback). `main` double-checks: any opened backend with a started change → exit 3. | `T12AfterChange`: the reviewer's case (derived index corrupted right after its rename) → exit 3 `stopped`, `verify_hash_mismatch`, `verify` = PARTIAL_OR_DRIFT; first-upload local change → `Refused`, 0 changes; second-upload local change → `Stopped`, journal ends `stopped_after_change`; RETR error in the verify phase → `Stopped`. |
| F3 | restore cannot get out of the F2 state | **fixed** | `restore_operations`: when the live index matches neither original nor derived AND the verified original backup exists, it renames the live file aside to `<name>.foreign-<nonce>.retired` (kept, never deleted), then renames the backup back and verifies it by sha256. Without a verified backup it still refuses and keeps everything. | Same F2 test continues: `restore --dry-run` shows "renamed aside", `restore --cleanup --allow-delete` → `PRISTINE`, every original byte-exact, the only extra file is the kept `.foreign-*.retired`, `check` → PRISTINE. `test_unknown_live_without_backup_is_preserved`: refuses, file untouched. Reviewer's extra test made permanent: `test_lost_reply_after_each_change_restores_exactly` (reply lost AFTER each of the 10 changes → byte-exact, no leftovers). |
| N1 | compressed IoStore mods are refused | **documented** (deferred: coverage work, not a safety bug) | NOTES §7 item 7. | – |
| N2 | wrong expected date for `global.*` | **fixed** | APPLY step 2: `Oct 01 16:54` for `pakchunk*`, `Oct 01 16:40` for `global.utoc/.ucas` (2026-10-08 scan rows). | – |
| N3 | probe FAIL line could never print in 1 minute | **fixed** | Card: play ~3 minutes; FAIL = "no `[pk] FOUND` by the last `[pk] Tn` printed while Leon was on screen"; knife/pistol may need a drawn weapon, one FOUND is enough. APPLY step 6 states 6 s × 30 rounds. | – |
| N4 | restore without cleanup leaves `_s1.ucas`, next install refuses | **fixed (docs)** | APPLY step 8 and the card's Undo say so and give both ways out (`--cleanup --allow-delete` with its own OK, or `repair --only` = move, no delete). | `T03…without_cleanup` already showed the leftovers; the gate refusal is the F1-tested rule. |
| N5 | crash would not say which new thing caused it; no fallback named | **fixed** | New `install --only-into STEM` (diagnostic): one container only, packages owned elsewhere listed as `NOT INSTALLED`, separate plan sha. APPLY "Fallback" section and the card name it. | `T13OnlyInto` (plan, cycle byte-exact, refusal for a non-destination). Real Leon replay: `--only-into pakchunk0-ps5` plans exactly the pakchunk0 build (`24884a02…` index, `6546d73f…` sidecar), 66+59 packages, 2 pakchunk10 packages listed as not installed, and works without the missing pakchunk10 header. |
| N6 | some originals are single-source | **fixed (docs)** | APPLY step 3 passes a `provenance.json` with `two_sources` / `single_source`. NOTES §7 item 8. | – |
| N7 | repair is kept after install-restore | **fixed (docs)** | APPLY step 8 and the card's Undo say the cleaned folder stays cleaned. | – |
| N8 | record sha depends on the header folder location | **fixed** | `make_record` stores the header path relative to the record's folder, also with `..`. The replay passes `relative_to`. | `T14RecordShaStable`: two different folder locations → same record sha, header still loads. Fails on the OLD tool. |

## Extra safety added while fixing (cheap, fail-closed)

- `repair` and `check` refuse a PARTIAL record (`record_is_partial`). Otherwise a partial
  record would quarantine real game files.
- `repair` never moves a top-level file that shares a stem with a recorded container
  (`unrecorded_file_of_recorded_container`), nor a file named like a base `.ucas`.
- `repair --only a,b,c`: one console step per OK (PRISTINE-PAKS). An unknown name is
  refused. A step skips the final gate, which `check` then runs.
- FTP `MKD` is allowed only outside `paks/` (the quarantine).

## Test runs (Mac-only)

- Full suite, real tree: `SUMMARY tests_run=45 failed=0 skipped=0 passed=45`, `GATE PASS`, `real replay: PASS 155059`, `RUN PASS tests_run=45 replay_items_checked=155059`, exit 0 (7 min 15 s; logs `tests/output/host-tests.log`, `tests/output/real-leon-replay.json`). Before the fixes: 33 tests.
- Runner fails correctly (scratch copies under the session scratchpad
  `a7a-teamA-planted/`; the planted lines existed only there, and the copies were removed after the runs):
  - planted failing test → `SUMMARY tests_run=46 failed=1 …`, `GATE FAIL`, `RUN FAIL (tests_run=46 failed=1 skipped=0 rc=1; …)`, exit 1
  - zero tests collected → `RUN FAIL (tests_run=0 failed=0 skipped=0 rc=1; replay rc=0 PASS 155059)`, exit 1
  - replay with `items_checked = 0` → `real replay: FAIL 0`, `RUN FAIL (tests_run=45 failed=0 skipped=0 rc=0; replay rc=1 FAIL 0)`, exit 1
- The 12 new tests pass on the fixed tool. Run against the ORIGINAL tool (scratch), 8 of
  them fail or error (F1 ×5, F2/F3 ×2, N8 ×1). So they really test the findings.

## Not changed

- Nothing in the candidate runtime tree, no PRX, no mod files, no other team folders.
- No console contact. Not console-proven yet: FTP `MKD` and rename into another folder
  (used by repair's quarantine). PRISTINE-PAKS.md says what to do if the server refuses them.

## Integrator follow-up (2026-10-13, Mac-only)

- **Bug found on the real record:** the full Leon plan refused with `mount_point_mismatch`. pakchunk10-ps5 is
  mounted at `../../../Townfall/Content/Townfall/Characters/Simon/`, but the projected mod part kept the mod's
  `../../../` mount point.
- **Fix:** `project_mod(..., target_dir=)` re-roots the projected directory onto the deeper mount point. Every
  virtual path stays byte-identical, and this is checked per file. A whole mod at a different mount point is now
  projected too. It refuses if the target mount is not below the mod's mount, if a file lies outside it, or if a
  root name is set.
- **Test:** `real_leon_replay.py` step 6 builds the complete two-container plan from the integrator's read-only
  pull of `pakchunk10-ps5.ucas`. Checks: all base paths kept, no unknown paths, and 2 mod-served paths in
  pakchunk10.
  - Suite: 45/45 PASS.
  - Replay: 155,061 items (155,059 + 2 re-rooted).
- **Real dry run:** plan `56783199…`, 2 targets. pakchunk0: 66 replaced + 59 new. pakchunk10: 2 replaced. All
  15/15 base paths are kept in pakchunk10.
- **Mirror bug avoided:** non-game files in the mirror folder (README, sizes.json) were recorded as game files.
  Keep the mirror holding only game files. (Possible tool hardening later: refuse non-container names in `record`.)
