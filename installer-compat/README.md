# ps5-installer-compat

**Mac-only / host-tested.** Every tool in this project was written and tested
on a Mac. Nothing here contacts a PS5 console: no FTP, no payloads, no game
launches. All paths are parameters; all mod analysis is static (files are
read, never executed).

## What this is

This project is a set of Mac command-line tools that prepare PC-made game
mods for a PS5 mod runtime. The tools inspect a downloaded mod archive and
report what kind of mod it is, statically read the mod's Lua code and grade
whether the APIs it calls exist in the PS5 runtime's API catalog, install
the mod into a local folder that mirrors the console's expected layout
(and verify every installed byte), and uninstall it again with rollback.
The compatibility verdicts are only as good as the catalog they run
against: the runtime owner (GPT) fills in a per-API console-test status,
and until that field is filled the checker honestly answers "unknown".

## Quick start

All commands run from the project root with `python3` (3.9+, stdlib only).

### Check compatibility (static scan of Lua sources)

```bash
python3 compat/checker.py <mod.lua | mod-dir> compat/catalog-real-merged.json --json report.json
```

- `catalog-real-merged.json` is built from the two real catalogs
  (`catalog/globals.json` + `catalog/types.json`) by `python3 compat/merge_catalogs.py`.
- **Current state:** every real-catalog entry has an empty `ps5_runtime_status`,
  so real mods grade `unknown` ("runtime status not filled"). See
  `compat/townfall-item-spawner-grade.md` for a real example.
- `compat/sample_catalog_TEST_FIXTURE.json` has made-up statuses and is for
  testing the grader's other outcomes (`compatible`, `partly-compatible`,
  `unsupported`).

### Install a mod

Write a metadata JSON file, then run the installer against local target dirs:

```json
{
  "name": "Townfall Item Spawner",
  "version": "1.2.0",
  "source_url": "https://www.nexusmods.com/townfall/mods/42",
  "file_list": ["MyMod/Scripts/main.lua"]
}
```

```bash
python3 installer/installer.py install \
  --archive mod.zip --metadata mod.json --title PPSA33286 \
  --roots ./sandbox/roots --plugins ./sandbox/plugins --state ./sandbox/state
```

Installs into `<roots>/<TITLE>/ue4ss/Mods/<ModName>/`, writes the UE4SS
enable row + marker, `loadlist.txt`, and `ploader.ini`, and verifies every
installed file's sha256 against the archive. Exit codes: 0 ok, 1 io,
2 bad request (unsafe archive / bad metadata), 3 verification failure,
4 rollback incomplete. Any failure rolls back to the pre-install state.

### Verify / recover

```bash
python3 installer/installer.py verify --title PPSA33286 --mod "Townfall Item Spawner" \
  --roots ./sandbox/roots --plugins ./sandbox/plugins --state ./sandbox/state
python3 installer/installer.py recover \
  --roots ./sandbox/roots --plugins ./sandbox/plugins --state ./sandbox/state
```

`verify` re-checks installed files + config rows against the manifest.
`recover` completes or rolls back any transaction interrupted by a crash.

### Uninstall a mod

No CLI; use the Python API (`installer/uninstall.py`). Its record format
(`install_state.json` + manifest with relative paths) differs from the
installer's manifest — the official adapter is `installer/adapter.py`
(`InstallPipeline.install_mod()` → `verify_mod()`/`check_mod()` →
`uninstall_mod()`), which reconciles the record formats and gates installs
on conflicts and the compat grade. Details: `docs/HOWTO-MOD-MANAGER.md`.

### Classify an archive

```bash
python3 installer/detector.py mod.zip            # JSON classification to stdout
```

Labels ue4ss-lua / ue4ss-cpp / pak-iostore / bp-logicmod / native-code /
config-only / mixed / unknown (9 enum values incl. `ue4ss-runtime`, the
"this is a UE4SS distribution, not a mod" flag), plus requirements parsed
from bundled readme/metadata text.

## Project structure

```
ps5-installer-compat/
├── installer/          install/verify/recover CLI, archive detector, uninstall,
│                      conflict checks, zip-safety + case-sensitivity tests
├── compat/             static Lua compatibility checker, catalog merge tool,
│                      merged real catalog, sample mod + fixture catalog,
│                      Townfall Item Spawner grade report
├── catalog/            the real UE4SS Lua API catalogs (globals.json 106 entries,
│                      types.json 42 types) + build scripts
├── conformance/        static + live conformance Lua suites, log diff tool
│                      (diff_results.py), host-test mock harness
├── docs/               CONTRACT-PROPOSAL, COVERAGE-MAP, REQUIREMENTS-TRACE,
│                      COUNTS, CLEAN-RUN-REPORT, audience how-tos
├── tests/              frozen independent tester harness (src-tester/) with
│                      saved adversarial + integration test results
├── test-results/       raw output of the clean-copy test run
├── REVIEW-PACKAGE.md   top-level review summary of the whole package
└── REVIEW-PACKAGE.v1.md  earlier historical version (frozen)
```

Every module also keeps its authoring-time original in a frozen `src-*`
subdirectory (e.g. `installer/src-installer-core/`) for provenance; those
copies are not the working code and some still hard-code their original
absolute staging paths.

## How to run all tests

```bash
cd installer && python3 -m unittest test_installer test_config_outputs test_zip_safety \
                                   test_case_sensitivity test_detector test_uninstall \
                                   test_conflicts test_adapter test_adapter_edge
cd ../compat && python3 -m unittest test_checker
cd ../tests/src-tester && python3 framework.py suite_installer.py suite_detector.py \
                                       suite_uninstall.py suite_compat.py integration_tests.py
```

Status as of 2026-10-07 (fresh single-agent sequential clean run on this Mac,
`test-results/raw-test-output.txt`):

- `compat`: **63 tests, all pass**.
- `installer` side (9 test files, one combined run): **214 tests, all pass**
  (55 installer + 11 config-outputs + 21 zip-safety + 23 case-sensitivity +
  52 detector + 11 uninstall + 9 conflicts + 19 adapter + 13 adapter-edge).
  `test_case_sensitivity` creates and detaches a real disk image
  via `hdiutil` and skips when unavailable.
- Ported independent tester suites (`tests/src-tester/`): **88/88 PASS**
  (suite_installer 27, suite_detector 19, suite_uninstall 19, suite_compat 20,
  integration 3). Combined total: **365/365 PASS / 0 FAIL / 0 ERROR**.
- Conformance host tests (mock UE4SS environment, no game; needs a host
  Lua interpreter — `brew install lua`, the repo no longer bundles one):
  `cd conformance/src-conformance-static && lua hosttest_mock_env.lua conformance_static.lua`.
  Mac-only / host-tested.

## Important docs

- `docs/CONTRACT-PROPOSAL.md` — proposed interface contract between runtime
  (GPT), mod manager, and this installer/compat layer. Proposal only.
- `docs/COVERAGE-MAP.md` — which catalog APIs the conformance scripts call,
  and why the rest are not called.
- `docs/REQUIREMENTS-TRACE.md` — every spec item traced to code, tests, and
  result, with per-item caveats.
- `docs/COUNTS.md` — the one authoritative count sheet for catalog/coverage
  numbers.
- `docs/CLEAN-RUN-REPORT.md` — clean-copy verification run and known
  `/tmp/sideproject` path-reference cleanup.
- `docs/HOWTO-GPT.md` — runtime owner guide (catalog status fill-in,
  conformance runs, diffing PC vs PS5).
- `docs/HOWTO-MOD-MANAGER.md` — mod manager integration guide (APIs,
  schemas, badges).

## Known limits (honest, current)

1. Real-catalog `ps5_runtime_status` is unfilled (106/106 globals, 476/476
   type rows) — compatibility grading against the real catalog is not yet
   meaningful; grades come back `unknown` by design.
2. `InstallPipeline` (`installer/adapter.py`) is the supported entry point:
   it converts installer manifests to the uninstaller's record format and
   gates installs on conflicts and the compat grade. Calling the raw
   `Installer.install()` / `Uninstaller` APIs directly skips those gates
   (residual of tester findings I-1/I-2, fixed at the pipeline level).
3. No locking: concurrent installs into the same root are safe only in the
   sense that failures roll back; run installs serially.
4. The conformance Lua suites have passed only under host-side mock
   environments, never against a real UE4SS/game session.
