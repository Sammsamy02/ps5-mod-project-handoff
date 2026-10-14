# REPORT — worker-patch-conflict-retry (2026-10-04)

Task: verify ALL pending controller-owned diffs apply cleanly TOGETHER against the production
tree `outputs/ps5-mod-bridge/` (retry after infrastructure failure). No production files were
modified; all work on a byte-identical scratch copy inside this worker's output area.

## Result: 9 diffs, 8 apply clean as-is, 1 has a single-hunk context conflict with a verified resolution

- 7 diffs from `worker-fileio-prep/PATCHES/` (01–07)
- 1 diff from `worker-installer-auto-setup/` (fix2-build.py.diff)
- 1 diff from `worker-createmutexa/` (mb_win_mutex_a_integration.diff, touches 2 files)
- Plus `worker-fileio-prep/PATCHES/new-files/` (6 new files, no collisions)

## Apply matrix (as measured, dry-run then real apply in scratch)

| Diff | Target (base for -p1) | Clean? |
|---|---|---|
| 01 build.py (11 hunks) | `outputs/ps5-mod-bridge` CWD | NO as-is: 10/11 hunks, hunk 10 rejected. YES with `-F3` |
| 02 owned_runtime.c (11 hunks) | `outputs/ps5-mod-bridge` CWD | CLEAN |
| 03 owned_runtime.h (1) | `outputs/ps5-mod-bridge` CWD | CLEAN |
| 04 result_report.c (1) | `outputs/ps5-mod-bridge` CWD | CLEAN |
| 05 test_entry_candidate.c (1) | `outputs/ps5-mod-bridge` CWD | CLEAN |
| 06 test_worker_candidate_failures.c (1) | `outputs/ps5-mod-bridge` CWD | CLEAN |
| 07 tests/worker-fixture.c (3) | `outputs/ps5-mod-bridge` CWD | CLEAN |
| fix2-build.py (2) | repo root CWD → vendor copy `outputs/2026-10-04-zcode-team/worker-fileio-apply/vendor/windows-candidate/build.py` | CLEAN (literal target) |
| mb_win_mutex_a_integration (3) | repo root CWD → `outputs/ps5-mod-bridge/windows-services/mb_win_mutex_abi.c` + `.h` | CLEAN |

Key path fact: the 7 fileio diffs use paths relative to the production tree root
(`windows-candidate/…` → apply from `outputs/ps5-mod-bridge`); fix2 and the mutex diff use
repo-root-relative paths → apply from repo root. Mixing these up yields "No file to patch".

## Conflict detail (diff 01 hunk 10) and resolution

- Hunk context expects `run('native-'+source.stem,[cc,*nflags,…],env)` directly above the
  `expected={…}` line. Production drifted: an earlier unrelated fix inserted two ms_abi /
  FreeBSD9-frontend comment lines plus `flags=bflags if source.name=='owned_runtime.c' else
  nflags`, and changed `*nflags` → `*flags` in that run() line. The hunk's changed line itself
  (`expected={…,'mb_win_mutex_abi':6}`) is unchanged and unique in the file.
- Resolution: `patch -p1 -F3` for diff 01 only (or hand-append `,'mb_win_fileio_abi':20` to the
  expected dict). Verified the fuzzed result matches the vendor staging copy semantically
  (`'mb_win_fileio_abi':20` present in vendor).

## fix2 note (important for the controller)

fix2 is NOT yet in the production tree: pristine production `windows-candidate/build.py` still
has `--report-path` default `/tmp/modbridge-owned-dll-candidate.jsonl`; only the vendor staging
copy carries the `/data/` default. fix2's diff paths address the vendor copy. Verified:

- Applies clean to the literal vendor target.
- Translated to `windows-candidate/build.py`, applies clean BEFORE or AFTER diff 01, and
  `production + 01(-F3) + fix2(translated) == vendor build.py` byte-for-byte (final diff shows
  exactly fix2's delta: `import sys`, `/tmp/`→`/data/` default with help-text note, `/tmp/`
  stderr warning). So either apply the translated diff to production or copy vendor build.py
  over production after step 7 — equivalent; do not do both.

## Cross-diff interference

None beyond the diff-01/fix2 same-file case (disjoint hunks, order-independent, verified both
orders). mutex diff files (`mb_win_mutex_abi.{c,h}`) are disjoint from all fileio targets and
new files. `new-files/` adds 4 files under `windows-services/` and 2 under
`windows-candidate/tests/` — none pre-exist in production.

## Post-apply sanity checks (scratch only)

- `python3 -m py_compile` OK on: patched production build.py, vendor build.py (+fix2),
  fix2-translated production variant.
- MutexA present in patched `mb_win_mutex_abi.c` (2 hits) and `.h` (1 hit).
- Patched line counts: build.py 281, owned_runtime.c 483, owned_runtime.h 45, result_report.c 84,
  test_entry_candidate.c 78, test_worker_candidate_failures.c 124, tests/worker-fixture.c 228,
  mb_win_fileio.c 844, mb_win_fileio.h 176, mb_win_fileio_abi.c 78, mb_win_fileio_abi.h 32,
  mb_win_mutex_abi.c 85, mb_win_mutex_abi.h 41.

## Artifacts (all under this worker dir)

- `CLAIM.json` — written first
- `PATCH-ORDER.md` — exact apply sequence + resolution notes
- `scratch/outputs/ps5-mod-bridge/` — production copy with all 9 diffs + new-files applied
  (final combined state; build.py here has 01–07 only, no fix2)
- `scratch/outputs/2026-10-04-zcode-team/worker-fileio-apply/vendor/…` — vendor copy with fix2
  applied (literal target)
- `scratch/fix2-prod-order-test/` — fix2-translated order tests (before/after 01)
- `dryrun-01.log`, `apply-01.log` — raw patch evidence for the conflicting diff

Production tree untouched. Reads only; writes confined to
`outputs/2026-10-04-zcode-team/worker-patch-conflict-retry/`.
