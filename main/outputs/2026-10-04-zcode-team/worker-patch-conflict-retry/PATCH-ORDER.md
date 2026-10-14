# PATCH-ORDER — controller apply sequence for pending diffs (verified 2026-10-04)

Scope: all pending controller-owned diffs vs production tree `outputs/ps5-mod-bridge/`.
All paths below are relative to repo root `/WORKSPACE/.codex/worktrees/translation-core/what`.
Verified by applying every diff in this exact order to a byte-identical scratch copy of the
production tree under `worker-patch-conflict-retry/scratch/`.

## Apply sequence (use exactly this order)

| # | Diff | Apply from CWD | Command | Result |
|---|------|----------------|---------|--------|
| 1 | `outputs/2026-10-04-zcode-team/worker-fileio-prep/PATCHES/01-windows-candidate_build.py.diff` | `outputs/ps5-mod-bridge` | `patch -p1 -F3 < <diff>` | CLEAN with -F3 (1 hunk needs resolution, see below) |
| 2 | `PATCHES/02-windows-candidate_owned_runtime.c.diff` | `outputs/ps5-mod-bridge` | `patch -p1 < <diff>` | CLEAN (11 hunks) |
| 3 | `PATCHES/03-windows-candidate_owned_runtime.h.diff` | `outputs/ps5-mod-bridge` | `patch -p1 < <diff>` | CLEAN (1 hunk) |
| 4 | `PATCHES/04-windows-candidate_result_report.c.diff` | `outputs/ps5-mod-bridge` | `patch -p1 < <diff>` | CLEAN (1 hunk) |
| 5 | `PATCHES/05-windows-candidate_test_entry_candidate.c.diff` | `outputs/ps5-mod-bridge` | `patch -p1 < <diff>` | CLEAN (1 hunk) |
| 6 | `PATCHES/06-windows-candidate_test_worker_candidate_failures.c.diff` | `outputs/ps5-mod-bridge` | `patch -p1 < <diff>` | CLEAN (1 hunk) |
| 7 | `PATCHES/07-windows-candidate_tests_worker-fixture.c.diff` | `outputs/ps5-mod-bridge` | `patch -p1 < <diff>` | CLEAN (3 hunks) |
| 7b | `PATCHES/new-files/` | repo root | copy tree into `outputs/ps5-mod-bridge/` | 6 new files, zero collisions |
| 8 | `outputs/2026-10-04-zcode-team/worker-installer-auto-setup/fix2-build.py.diff` | repo root | `patch -p1 < <diff>` (literal target = vendor copy) | CLEAN (2 hunks) — see note B |
| 9 | `outputs/2026-10-04-zcode-team/worker-createmutexa/mb_win_mutex_a_integration.diff` | repo root | `patch -p1 < <diff>` | CLEAN (3 hunks: mb_win_mutex_abi.c ×2, .h ×1) |

Steps 2–7 are order-independent among themselves (disjoint files). Step 8 targets
`outputs/2026-10-04-zcode-team/worker-fileio-apply/vendor/windows-candidate/build.py`, not the
production tree — see note B. Step 9 touches only `windows-services/mb_win_mutex_abi.{c,h}`;
no textual overlap with any other pending diff.

## Conflict found and resolution (diff 01, hunk 10 of 11)

Symptom: with default fuzz, `1 out of 11 hunks failed` on `windows-candidate/build.py`.

Conflicting hunk (saved at `scratch/`-verified reject):

```
@@ -216,7 +231,7 @@
    for source in [*native_c,EXEC/'pe_ps5_memory.c',HERE/'sdk_driver.c']:
     obj=out/(source.stem+'.o');dep=out/(source.stem+'.d');objects.append(obj);deps.append(dep)
     run('native-'+source.stem,[cc,*nflags,'-MD','-MF',dep,'-c',source,'-o',obj],env)   <-- context drifted
-   expected={...,'mb_win_init_once_abi':2,'mb_win_mutex_abi':6}
+   expected={...,'mb_win_init_once_abi':2,'mb_win_mutex_abi':6,'mb_win_fileio_abi':20}
```

Root cause: production `build.py` diverged from diff 01's base via an unrelated earlier fix
(FreeBSD9 frontend / ms_abi per-source flags). Production now has, immediately above the target
line: two comment lines (`# owned_runtime.c contains the ms_abi combined_close_handle dispatch…`,
`# default sie-ps5 target rejects ms_abi, compile with the FreeBSD9 frontend`), a new
`flags=bflags if source.name=='owned_runtime.c' else nflags` line, and the run() line uses
`*flags` instead of `*nflags`. Only the hunk's PRE-CONTEXT drifted; the changed line itself
(`expected={…}`) is byte-identical and unique in the file (1 occurrence).

Resolution (verified): apply diff 01 with `patch -p1 -F3` (fuzz 3). Result is semantically
identical to what worker-fileio-apply actually staged in the vendor copy (vendor contains
`'mb_win_fileio_abi':20` in the same dict). Alternative: hand-apply the single changed line
(append `,'mb_win_fileio_abi':20` before the closing `}` of the `expected=` dict at production
line 222). Do NOT regenerate or skip the hunk.

## Note B — fix2-build.py.diff dual target

fix2's literal paths address the vendor staging copy
`outputs/2026-10-04-zcode-team/worker-fileio-apply/vendor/windows-candidate/build.py`
(already has fileio 01 applied; pre-fix2 state had `/tmp/` default). Verified:

- Against the literal vendor target: applies CLEAN (2 hunks: `import sys` + `--report-path`
  default `/tmp/`→`/data/` + `/tmp/` stderr warning).
- The production tree's `windows-candidate/build.py` still has the `/tmp/` default, i.e. fix2 is
  NOT yet in production. To land it there, translate the paths to
  `windows-candidate/build.py` and apply AFTER step 1: verified CLEAN in both orders
  (before or after 01), and

  `production build.py + [01 with -F3] + [fix2 translated] == vendor build.py` byte-for-byte
  (only fix2's 3-line delta remains) — no drift, no hidden divergence.

Controller should choose ONE of: (a) apply fix2 translated to production as step 8, or
(b) copy the vendor build.py over production after step 7 (equivalent result). Do not do both.

## Cross-diff conflict summary

- No two pending diffs modify the same production file except diff 01 and fix2 (both touch a
  `build.py`), and those overlap regions are disjoint (argparse lines ~9 and ~32–38 vs 01's
  hunks); verified clean in either relative order.
- mutex diff vs fileio diffs: disjoint files (`mb_win_mutex_abi.*` vs `mb_win_fileio.*`,
  `windows-candidate/*`). Clean.
- `new-files/` (mb_win_fileio.c/.h, mb_win_fileio_abi.c/.h, tests/worker-fileio.h,
  tests/worker-fileio-kernel.def): none exist in production — no collision.

## Post-apply sanity (in scratch)

`python3 -m py_compile` passes on: patched production `build.py` (01–07), vendor `build.py`
(+fix2), and the fix2-translated production variant. MutexA symbols present in patched
`mb_win_mutex_abi.c` (2) and `.h` (1). All patched file line counts recorded in REPORT.md.
