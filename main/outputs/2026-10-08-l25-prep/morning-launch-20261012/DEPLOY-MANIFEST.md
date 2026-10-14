# Deploy manifest — morning Townfall launch, 2026-10-12

Nothing here has been uploaded. Every console step needs the user's explicit OK
for that step, with the game CLOSED. Run from the worktree root. The console
address comes from AGENTS.md (`$PS5` below); keep it out of shared reports.

## Final PRX

| Field | Value |
|---|---|
| Build | B4-shim-final-stack (snapshot b63ba460, build l26-20261012T040323Z) |
| Path | `outputs/2026-10-08-l26-frozen/B4-shim-final-stack-d044cf0b/modbridge_supplier_composition.prx` (read-only copy; the original is in `2026-10-08-l26-package-A/pass-1/` until the next build archives it) |
| sha256 | `d044cf0bf736b5f4399081eea764f59673d1d798a530572ab6897f8879c37599` |
| Size | 31,594,535 B, magic `4f153d1d` |
| Gates | fs-throw (pre + post compile), no-detour, build gates, packaging P0-P13, verify_build --manifest (24 markers) — all PASS |
| Fallbacks (kept) | B3 `40b8972b...`, B2b `74fab85a...`, B1 `b9cc5248...`, known-good `1e49188b...` (read-only copy in `outputs/2026-10-08-l26-frozen/`) |

Before upload, re-run `verify_build.sh <prx> --manifest` on the exact file.

## Files

| What | Where | Upload? |
|---|---|---|
| runtime PRX | above | yes (via prep_modded_launch.py, with decrypt-off backup + readback) |
| loadlist (30 rows, 1,772 B) | `morning-launch-20261012/upload/loadlist.txt` | yes |
| self-test (main + part2..12) | `morning-launch-20261012/upload/ue4ss/Mods/ueselftest/Scripts/` | yes, exact mirror |
| TownfallMiniMap v3.1 (196,743 B + enabled.txt) | already on the console, matches the original | no |
| other mods (spawner, CheatManager, ConsoleEnabler, InfiniteStamina, zzdiag, UEHelpers) | already on the console, unchanged | no |
| settings (1,947 B, cache OFF) | already on the console | no (prep re-asserts the same file) |
| writable folders `/data/modbridge/writable/<mod>/` | created by `deploy-precreate-writable.py` | yes (folders + marker file) |

## Morning read-only check (2026-10-12 11:40Z, console-proven, FTP reads only)

- PRX on console = known-good `1e49188b...` (31,562,327 B), decrypt-off magic `4f153d1d`: PASS (post-power-event readback).
- ploader.ini = pinned ENABLED state (440 B, `3eab9510...`); no ploader lock.
- 30 staged rows: 24 already match their pins, 6 self-test files will be uploaded
  (main, part2, part3, part4, part9, part11), 0 problems. Console loadlist lacks
  the 2 TownfallMiniMap rows; the staged loadlist adds them.
- ueselftest/Scripts on console = exactly the 12 staged names -> 0 stale, so step 3 is skipped.
- `/data/modbridge/writable/` does not exist yet (step 4 creates it).
- Evidence: `evidence/morning-readonly-20261012T114008Z/RECEIPT.json`.

Live fixes made before any upload (backups `*.pre-live-20261012`):
- this ftpsrv refuses NLST (`curl --list-only` rc 19): deploy_mirror and
  readonly_check now parse LIST. deploy_mirror also skips identical files and
  uploads loadlist.txt LAST, only when every other file passed.
- a missing dir answers curl rc 9 (not 78): deploy-precreate-writable treated it
  as "indeterminate" and would have created nothing (confirmed in an offline
  simulation). Now rc 9 = missing; dirs are created with the proven
  `MKD <absolute dir>` form level by level, each verified by LIST; evidence
  goes to `evidence/writable-<ts>/` (was /tmp). Simulation: 8 dirs + 7 markers
  created, re-run changes nothing.
- B4 fresh `verify_build --manifest`: PASS, 24/24 markers
  (`evidence/builds-20261012/B4-verify-build-fresh-20261012T114434Z.log`);
  prep dry-run with B4 exit 0 (`B4-prep-dryrun-20261012T114441Z.log`).

## Order (each line = one OK)

```
# 1  read-only check: listing + current PRX hash (decrypt-off) — no changes
# 2  self-test mirror + loadlist
python3 outputs/2026-10-08-l25-prep/morning-launch-20261012/deploy_mirror.py --console $PS5 --dry-run
python3 outputs/2026-10-08-l25-prep/morning-launch-20261012/deploy_mirror.py --console $PS5 --upload --list-stale
# 3  only if step 2 listed stale files (separate OK)
python3 outputs/2026-10-08-l25-prep/morning-launch-20261012/deploy_mirror.py --console $PS5 --delete-stale
# 4  writable folders
python3 outputs/2026-10-08-l25-prep/round5-staging/shim-fix/deploy-precreate-writable.py --console $PS5 --dry-run
python3 outputs/2026-10-08-l25-prep/round5-staging/shim-fix/deploy-precreate-writable.py --console $PS5
# 5  row-check every row against the exact pins
python3 outputs/2026-10-08-l25-prep/harvest/prelaunch_rowcheck.py --console-ip $PS5 --expect outputs/2026-10-08-l25-prep/morning-launch-20261012/rowcheck-pins.json
# 6  PRX: backup, upload, decrypt-off readback, settings, row-check, arm ploader, klog
python3 outputs/2026-10-08-l25-prep/harvest/prep_modded_launch.py --variant A --prx-path outputs/2026-10-08-l26-frozen/B4-shim-final-stack-d044cf0b/modbridge_supplier_composition.prx --dry-run
python3 outputs/2026-10-08-l25-prep/harvest/prep_modded_launch.py --variant A --prx-path outputs/2026-10-08-l26-frozen/B4-shim-final-stack-d044cf0b/modbridge_supplier_composition.prx
# 7  user launches once (LAUNCH-CARD.md)
```

Check the exact flag names of `prelaunch_rowcheck.py` with `--help` before step
5 (it was written by an earlier agent).
