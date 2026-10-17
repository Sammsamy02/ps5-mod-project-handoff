# REVIEW: capability #7 step 1 (Team A), independent reviewer

Verdict: **PASS-WITH-FIXES.** The tool is careful work and the tests are real. I found three
problems to fix before the first console use, all confirmed by running them on the Mac.
None of them can damage the base `.ucas` files or the mod files. Every claim here is
**Mac-only**, apart from the two citations marked "earlier console evidence".

## What I checked and re-ran (Mac-only)

- I ran `sh tests/run_tests.sh` on a copy of the folder in my scratchpad, so the team's logs were
  not overwritten. Result: `SUMMARY tests_run=33 failed=0 skipped=0 passed=33`, `GATE PASS`,
  `real replay: PASS items_checked=155059`, `RUN PASS` (2 min 51 s). The runner fails on zero
  tests or any skip, and the replay fails on `items_checked == 0`. My replay JSON matches the
  team's except for `record_sha256` (see N8).
- None of the read-only inputs changed: nothing under `pak-overlay/`, the 2026-10-01 captures
  or the Leon intake is newer than the run. The pinned builder
  `outputs/2026-10-08-l25-prep/pak-overlay/overlay_builder.py` hashes to `a72636fb…`, which
  matches the pin.
- The patch is a new stdlib-only Python tool plus data. **Nothing in the candidate runtime tree
  changes and no PRX is built.** So "does it apply / compile" only means: it runs (yes), and the
  runtime gates do not apply (correct).
- Hard rules: the code has no game or mod names (Leon paths reach the probe only as data taken
  from the mod's own directory index). No detours, no SDK material. The tool makes no console
  contact on its own: `--ftp` is LAN/loopback only and is used only by the integrator. Mod
  files are opened read-only. Recorded base `.ucas` names are refused for
  RETR/STOR/RNFR/RNTO/DELE. The real-run counts are printed. The ContainerHeader has two
  sources (the TOC's IoHash, and a package set equal to the export chunks). No console IP
  appears anywhere in the folder.
- The FTP command pattern (`LIST <path with spaces>`, RNFR/RNTO, the listing regex) is the same
  as the console-proven 2026-10-01 executor
  (`.superpowers/native-dependency-spike/owned-container-bundle-transaction-1/bundle_writer.py:52-66`,
  `iostore-sidecar-overlay-2/index_transaction.py` sha `6510b846…`).
- Extra test (mine, scratch only): a reply lost AFTER the change already happened, at each of
  the 10 change points. The team's test only fails BEFORE the change. Restore was byte-exact
  10/10 with no leftovers.
- The probe API exists in R11: `StaticFindObject` (candidate `src/LuaMod.cpp:1992`) and
  `LoopAsync` (`src/LuaMod.cpp:4178`). The long-path form is console-proven in launch 14
  (earlier console evidence:
  `evidence/launch-l26-R11-launch14-20261013/ue4ss.decoded.log:1359`,
  `[ST2] 70/168 StaticFindObject-long PASS valid=true`).
- The bright-outdoors cause is now backed by more than the team cites (earlier console
  evidence: the integrator's 2026-10-13 pull, re-hashed by me now). In
  `evidence/console-pulls-20261013-paks/`, the live `pakchunk0-ps5.pak` is `8bcf5eb1…`, exactly
  the 2026-10-02 config-trial `derived.pak`. Its `.bak` is `ee145cbc…`, exactly the original
  that paktx records. So the repair step will find a verified backup for the `.pak`.
  NOTES §2 still says "not re-hashed" and can be updated.

## Findings

### F1 (FIX-SOON): the gate and the repair never look inside subfolders of `paks/`
`run_gate` and `make_repair_plan` read only the top-level listing. Directory rows (`d`) are
skipped, or counted as "inert extra names". UE finds pak files recursively under `Paks/`
(general UE knowledge; not in the local source subset). That is why the usual `~mods/`
folder works.
**Confirmed on the Mac** (`review_extra2.py`, scratch): I copied the unchanged mod trio into
`paks/~mods/` and the install still went through. The output was
`gate: 11 recorded files match …; 1 inert extra names` and exit 0, `installed`. The repair did
not quarantine it either.
**Scenario:** the raw PC Leon trio (or any other pak mod) sits in `paks/~mods/`. The
texture-diagnosis card assumes it does, and the 2026-10-08 scan cannot rule it out. paktx
says the gate passed, the engine mounts the raw trio next to the derived index (duplicate
package ids, PC-cooked content), and the launch verdict means nothing. `PRISTINE` after
restore would also be false.
**Fix:** refuse on any unrecorded directory under `paks/`, or walk it recursively and apply
the mountable-file rule. In repair, quarantine mountable files found in subfolders. Until
then, add to the launch-card preconditions: "the step-1 LIST shows no subfolder under paks/".

### F2 (FIX-SOON): after the console has changed, a failure is reported as "refused (nothing changed)"
Inside `execute`, a `Refused` raised after the first change (phase-C `verify_hash_mismatch` or
`verify_size_mismatch`, a later `rename_source_identity`, `local_artifact_changed` on the
second upload) leaves `main` as exit **2** with `"result": "refused"`. An ftplib error during a
phase-C RETR leaves as an uncaught traceback (exit 1). LAUNCH-CARD.md says
`"result": "refused"` means "(nothing changed)", and APPLY tells the operator to run restore
only on exit 3.
**Confirmed on the Mac** (`review_extra.py`, scratch): I corrupted one byte of the derived
index while it was renamed into place. Result: `exit 2 result refused verify_hash_mismatch`,
then `verify` → `PARTIAL_OR_DRIFT`, so the console WAS changed.
**Fix:** in `execute`, once any change has been journaled, turn every later exception
(Refused or other) into `Stopped` (exit 3, "run restore"). Change the card row to: refused
means nothing changed only when the journal has no `pending`/`done` change.

### F3 (FIX-SOON): restore cannot get out of the F2 state
`restore_operations` raises `live_file_unknown_identity` when the live index matches neither
the original nor this plan's derived file (`paktx.py:1143`), even when the verified original
backup is right there. In the F2 case (an index that did not verify after the rename), the
console is left with a bad live `pakchunk0-ps5.utoc` that the tool refuses to roll back. The
only way out is to rename files by hand, outside the tool.
**Fix:** when `bstate == "original"`, rename the unknown live file to
`<name>.foreign-<nonce>.retired` (a rename, nothing deleted, the same as repair does), then
rename the backup back and verify. Add a test for it (corrupt after rename → restore →
byte-exact original).

### Notes (no change needed for a safe launch, but worth fixing in the docs)

- **N1:** `overlay_builder.parse_toc` accepts only containers with
  `compression_method_name_count == 0` (`overlay_builder.py` ~line 508), and `project_mod`
  writes no compression-name section. Leon and the Townfall bases qualify. Compressed IoStore
  mods (common on Nexus) are refused with `unsupported_compression_methods`. That fails closed,
  but it is a per-engine coverage limit and NOTES should say so.
- **N2:** APPLY step 2 says to check the `Oct 01 16:54` date on each fetched file. `global.utoc`
  and `global.ucas` are `Oct 01 16:40` in the 2026-10-08 scan (`storage/files.json`). An
  operator could wrongly stop there.
- **N3 (probe):** 30 cycles × 6 s is about 3 minutes, but the card tells the user to play about
  1 minute. So the card's FAIL line `[pk] T30 found=0/8` will usually never print. A better
  FAIL line: "no `[pk] FOUND` line by the last `Tn` printed after Leon was on screen". Two of
  the 8 preview paths (Knife, Pistol) may load only when a weapon is drawn. k ≥ 1 already
  allows for that.
- **N4:** restore without `--cleanup` leaves `<stem>_s1.ucas`. That is an unrecorded mountable
  file, so the next `install` gate refuses until cleanup (needs its own OK) or repair runs. The
  card's rollback section should say so. That matters because acceptance needs 2 clean
  launches.
- **N5:** this launch tests two things that have never run on the console: a derived index in
  a second container (`pakchunk10-ps5`), and a cross-container import
  (`0x5dd733019f47199a`). The `pakchunk10` target cannot be built on real data yet (its header
  is missing), so only the synthetic tests cover it. If the game crashes, the logs cannot say
  which of the two caused it. The card should name the fallback: restore, then a
  pakchunk0-only plan (the console-proven shape).
- **N6:** the original sha256 for `pakchunk1/10/11/12-ps5.pak` and `global.utoc` will come from
  one source only: today's file, trusted by size and date. No experiment touched them and no
  `.bak` exists for them, so this is acceptable, but the record's provenance should say
  "single source".
- **N7:** the repair also brings the original `pakchunk0-ps5.pak` back (darkness returns), and
  install-restore does not undo that. That is intended, but the card's rollback line should
  say the repair is kept.
- **N8:** `record_sha256` contains the absolute header path when the headers folder is outside
  the record's folder. My replay got `d8578526…` and the team's got `ed3c026a…`, with the same
  inputs. So plan shas change with the folder location. This is harmless for `--confirm`,
  which always uses the plan just built.

## Launch card

It is specific: preconditions, a separate user OK per action, what to look at in game, the
expected checkerboard marked "not a FAIL", rollback R11 bfd95db6. Two changes make it safe:
add the no-subfolder precondition (F1), and correct the meaning of "refused" (F2).
