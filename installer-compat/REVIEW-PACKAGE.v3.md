# REVIEW-PACKAGE.md — Installer + Compatibility Layer (side project) — FINAL

> Mac-only / host-tested throughout. No console contact at any point (no FTP,
> no payloads, no launches; the sole network fetch was the public UE4SS
> v3.0.1 release zip downloaded Mac-only to `/tmp` as the detector's
> layout reference — public URL + sha256 + full listing pinned in
> `docs/UE4SS-RELEASE-LISTING.txt`). Work confined to this project directory
> plus two documented read-only exceptions: `git show` of the mod-manager
> worktree to stage the TownfallItemSpawner mod source, and read-only
> inspection of the UE4SS source trees for catalog attribution (both recorded
> in `compat/townfall-item-spawner-grade.md` and `compat/real-catalog-run.md`).
>
> **This is the FINAL rewrite.** Earlier versions are preserved frozen:
> `REVIEW-PACKAGE.v2.md` (the round-4 final — already reconciled to the
> 267 definitive run, with earlier totals retained only as labeled history;
> kept for provenance, do not cite its numbers) and `REVIEW-PACKAGE.v1.md`
> (the first package). Round-4 change log: `docs/CHANGES-ROUND4.md`; round-4
> content pushed to the private remote `github.com/Sammsamy02/ps5.git`,
> branch `side/installer-compat`, latest commit
> `69c88ede930168e6070f8fdca16f9c23eca0c850` (15:35, superseding the first
> round-4 push `390ef65f…` of 15:27; this rewrite and the definitive test
> log are newer than both).
>
> **One set of numbers everywhere:** every count below comes from
> `docs/COUNTS.md` (the authoritative sheet, recounted for catalog v4).
> **One test run:** every test result below comes from the definitive final
> clean run `test-results/raw-test-output.txt` (15:29, 267/267, re-verified
> live on the current tree). Pending or stale items are labeled as such,
> never papered over.

## 1. WHAT WAS BUILT

The INSTALLER + COMPATIBILITY layer between GPT's PS5 runtime and the mod
manager, per the side-project spec. Six modules plus test/review/audit
infrastructure:

- **Catalog** (`catalog/`): machine-readable UE4SS Lua API surface —
  `globals.json` (**106 global entries**: 98 functions + 6 Rev-2 conformance
  additions + the v4 `Key` (165-constant) and `ModifierKey` (3-active)
  PC-only input TYPE tables) + `types.json` (**42 types / 456 method entries
  / 20 metamethods**). Both files are versioned wrappers
  (`catalog_version: 2026-10-07-v4`, `citation_tree`); entries carry **real
  upstream UE4SS 3.0.1 file:line citations** (`upstream_reference` /
  `upstream_implementation`, re-derived by `catalog/update_upstream_refs.py`)
  with port-tree citations kept as the secondary reference;
  `ps5_runtime_status` is empty everywhere awaiting GPT's console fill-in.
- **Conformance** (`conformance/`): `conformance_static.lua` (353 calls,
  read-only, game-state-independent) + `conformance_live.lua` (**Rev 2**:
  86 CALL rows + 6 prerequisite rows — TArray battery rewritten to the
  port's real members, fictional `Loop` table replaced with the real
  scheduling globals, `UClass:GetFunction` replaced with
  `ForEachFunction`/`GetSuperStruct`/`IsChildOf`/`GetCDO`, every cataloged
  hooking global now has a gated no-op call) + `diff_results.py`
  (MATCH/MISMATCH/MISSING/EXTRA log differ) + host-test mock environment and
  bundled Lua 5.4.7.
- **Installer** (`installer/`): transactional journal-first install engine
  (crash-safe, sha256 verify) writing **all four runtime config outputs** —
  loadlist.txt block, `ue4ss/Mods/mods.txt` enabled row (`<Name> : 1`),
  `ue4ss/Mods/<Name>/enabled.txt` marker, ploader.ini line — plus the
  mod-type detector (**9 types incl. `ue4ss-runtime`**, with real
  UE4SS 3.0.1 release-layout rules), journalled uninstall/rollback, conflict
  detection (path/keybind/hook/deps), and case-conflict detection for the
  PS5's case-sensitive disk.
- **Integration adapter** (`installer/adapter.py`, rounds 3–4): the
  production `InstallPipeline` — one entry point wiring detector → compat
  checker → conflicts → installer → uninstaller, a unified
  `install_state/<TITLE>.json` registry feeding both uninstall and
  conflicts, and manifest reconciliation (closes tester findings #22/#23).
- **Compat checker** (`compat/`): static Lua scanner + 4-grade ladder,
  blank-runtime-status bucketing, cataloged-stdlib-override handling, and a
  dedicated **PC-ONLY INPUT category** (`Key.*`/`ModifierKey.*` keyboard
  dependencies graded as a PS5 difference, not "unknown"); catalog merge
  tool; real-mod grading of TownfallItemSpawner.
- **Tests/review/audit** (`tests/`, `docs/`, `test-results/`): frozen
  independent adversarial tester harness (88 tests, 65-fixture corpus,
  pure-Python Lua 5.3 validator), coverage map, requirements trace,
  clean-run verification, merged contract proposal v1.1, audience how-tos,
  pre-push security scan (verdict CLEAN).

## 2. FILE LIST

**217 files.** Sizes in bytes. Frozen provenance originals under `src-*/`
are byte-copies of the module-authoring state (the working copies above them
have since diverged — Section 8). Reproduce this listing with
`find . -type f -not -path './.git/*' -exec stat -f '%z %N' {} \; | sort -k2`.

```
.gitignore                                 816  ignores __pycache__/*.pyc/.DS_Store/*.dmg
README.md                                 8070  project readme (NOTE: pre-dates catalog v3/v4 —
                                                says "98 globals" and quotes a mid-round test
                                                state; see Section 9 "stale sibling docs")
REVIEW-PACKAGE.md                       (this file)
REVIEW-PACKAGE.v1.md                     9301  first package (frozen)
REVIEW-PACKAGE.v2.md                    57332  round-4 final (frozen; earlier totals
                                                appear in it only as labeled history)
catalog/
  globals.json                          123343  106 entries, wrapper {catalog_version:
                                                2026-10-07-v4, citation_tree, entries}
  types.json                            184605  42 types / 456 methods / 20 metamethods (v4)
  SUMMARY.md                             10600  summary (98-build provenance + v4 update note)
  update_upstream_refs.py                13476  re-derives upstream 3.0.1 citations (host tool)
  backup-before-upstream-refs-2026-10-07/ globals.json 82001, types.json 131416
  src-catalog-globals/  (build_catalog.py 31776, globals.json 74646, SUMMARY.md 9865 — frozen)
  src-catalog-types/    (build_types.py 36828, types.json 128208, SUMMARY.md 11425 — frozen,
                         FNameUserdata CORRECTION)
compat/
  checker.py                             48743  static Lua scanner + grader (blank-status,
                                                stdlib-override, PC-ONLY INPUT categories)
  test_checker.py                        30752  55 tests (4 Townfall pins + 8 TestPcOnlyInput)
  merge_catalogs.py                       8633  globals+types -> catalog-real-merged.json
  catalog-real-merged.json              315438  599 merged entries (v4 rebuild; 599/599 blank)
  sample_mod.lua                          4500  test mod (~25 APIs)
  sample_catalog_TEST_FIXTURE.json        6157  INVENTED-status test fixture (never ship badges)
  sample-mod-real-catalog-report.json    34018  checker report (pre-v4 run, sample mod)
  townfall-item-spawner-grade.json       68966  THE required grade artifact (v4, full scan)
  townfall-item-spawner-grade.md          9630  human summary (partly-compatible)
  townfall-item-spawner-mod/Scripts/main.lua 21415  staged mod source (read-only copy)
  real-catalog-run.md                    11815  attribution audit + real-catalog checker runs
  README.md                                522  pointer to docs/CONTRACT-PROPOSAL.md §4
  src-compat-checker/   (frozen: checker.py 41155, test_checker.py 17231,
                         CONTRACT-PROPOSAL.md 7876, demo-report.json 20637,
                         sample_catalog.json 4583, sample_mod.lua 4500)
conformance/
  conformance_static.lua                 37578  353-call static suite (unchanged this round)
  conformance_live.lua                   52368  Rev-2 live suite (86 CALL + 6 PREQ rows)
  diff_results.py                        10496  log differ
  README.md                              11247  static-suite spec + host-test results
  sample_a_vs_sample_b.diff.json        162993  diff artifact (copy at top level)
  src-conformance-live/  (conformance_live.lua 52368 = Rev 2, README.md 13209 = Rev-2 spec)
  src-conformance-static/ (conformance_static.lua 37578, diff_results.py 10496,
      hosttest_mock_env.lua 14010, README.md 11247,
      sample_a_vs_sample_b.diff.json 162993, samples/: sample_a.log 22857,
      sample_b.log 22784, sample_b_full.log 22868,
      sample_a_vs_sample_b.diff.json 162919, sample_a_vs_sample_b_full.diff.json 162902)
      — the bundled `bin/lua` interpreter binary (298384 B, Lua 5.4.7 arm64)
      was removed from the repo 2026-10-07 (`git rm`); host-side Lua is now
      `brew install lua` (Mac-only / host-tested: mock variants re-verified
      with brew Lua 5.5.1 after the removal — same 353-call totals)
docs/
  CONTRACT-PROPOSAL.md                  16430  merged v1.1 (+keybind-mapping asks §6.4/§8.8)
  COVERAGE-MAP.md                      129554  580 coverage rows (v3-era snapshot)
  coverage.json                        193846  machine companion (580 rows)
  REQUIREMENTS-TRACE.md                 13979  15 spec rows (rows 2/5/15 superseded — §7)
  COUNTS.md                             11620  THE authoritative count sheet (v3+v4 recounts)
  CLEAN-RUN-REPORT.md                   12842  clean-run verification + proposal merge + audit
  ANSWERS.md                            13220  the three open questions, answered (+ Q4/Q5 for GPT)
  HOWTO-GPT.md                           7536  runtime-owner guide (pre-v3 counts — §9)
  HOWTO-MOD-MANAGER.md                   8521  mod-manager integration guide
  SECURITY-SCAN.md                       7733  pre-push audit: CLEAN
  UE4SS-RELEASE-LISTING.txt              3630  official UE4SS_v3.0.1.zip listing (URL+sha256)
  CHANGES-ROUND4.md                      6057  round-4 change log (pushes + 242→267 arithmetic)
installer/
  installer.py                          82794  transactional install engine (+mods.txt/enabled
                                                flags, case-conflict wiring)
  adapter.py                            50189  InstallPipeline: production integration adapter
                                                closing #22/#23 (+ CLI install/check/uninstall/
                                                verify)
  detector.py                           42546  9-type classifier (+UE4SS release layouts,
                                                Nexus /games/ URL fix)
  uninstall.py                          26164  journalled uninstall + rollback (+markers)
  conflicts.py                          17884  path/keybind/hook/dependency conflicts
  test_installer.py                     51786  55 tests
  test_config_outputs.py                22413  11 tests (four config outputs + round trip)
  test_detector.py                      36356  50 tests
  test_uninstall.py                     13583  11 tests
  test_conflicts.py                      8734  9 tests
  test_case_sensitivity.py              20790  23 tests (case conflicts + APFS volume)
  test_zip_safety.py                    21597  21 tests (duplicates/bombs/UE4SS-as-mod)
  test_adapter.py                       26175  19 tests (InstallPipeline lifecycle, closes
                                                #22/#23)
  test_adapter_edge.py                  25875  13 tests (independent edge-case verifier)
  src-installer-core/  (installer.py 55747, test_installer.py 40720 — frozen)
  src-mod-types/       (detector.py 32992, test_detector.py 17044 — frozen)
  src-uninstall/       (uninstall.py 22514, conflicts.py 17884, test_uninstall.py 13345,
                        test_conflicts.py 8496 — frozen)
tests/
  REVIEW.md                             18263  tester report, verbatim (paths localized)
  results/*.json                         14 files, 227–3504 bytes each: tester final-run JSONs
      (suite_installer 27, suite_detector 19 [updated 14:50 after the Nexus fix; frozen
      18+1 original in src-tester/results], suite_uninstall 19, suite_compat 20,
      integration_tests 3, 8 static scans)
  src-tester/  (frozen harness: framework.py 16282, lua_syntax.py 22970, corpus.py 11370,
      suite_installer.py 24087, suite_detector.py 11011, suite_uninstall.py 16824,
      suite_compat.py 10228, integration_tests.py 12982, adapters.py 4462,
      review_static.py 4622, poll.py 2232, module_state.json 735, REVIEW.md 18160,
      results/ [the 14:08 JSONs — detector 18+1 preserved],
      runs/ [mock_a.log 22857, mock_b.log 22868, hostile_mock.lua 1496, synchk.lua 103,
      mock_a_vs_mock_b.diff.json 162863],
      corpus/ [65 adversarial fixtures — detector/: 16 zips incl. many_files.zip 1128894,
      _payload.zip 137, lua_is_pe.zip 250, obfuscated_loader.zip 196 …;
      installer/: 20 zips incl. bomb.zip 108872, many_files.zip 1128894, traversal.zip 376,
      symlink.zip 507, nested.zip.layer19 2457 … + configs/: 16 hostile JSONs incl.
      very_long_name.json 10011; lua/: 19 hostile files incl. huge_line.lua 40012,
      deep_nesting.lua 1806, syntax_err_late.lua 3525 …; uninstall/: 12 manifests incl.
      huge_list.json 58915, circular_a/b.json 28/28 …])
test-results/
  raw-test-output.txt                   29007  THE definitive final clean run (Section 4)
```

## 3. HOW TO RUN EVERY TEST

All Mac-only, stdlib-only (Python 3.9+). Working directory shown per command.

```bash
# 3.1 ALL installer-side suites in one command — 212 tests (the exact
#     definitive-run command, raw-test-output.txt Section 1)
cd /WORKSPACE/Projects/ps5-installer-compat/installer
python3 -m unittest test_installer test_config_outputs test_zip_safety \
                       test_case_sensitivity test_detector test_uninstall \
                       test_conflicts test_adapter test_adapter_edge

# 3.2 Compatibility checker — 55 tests (includes the Townfall grade pins)
cd /WORKSPACE/Projects/ps5-installer-compat/compat
python3 -m unittest test_checker

# 3.3 Conformance static suite under the host mock (variants A and B;
#     needs a host Lua interpreter — `brew install lua`; the repo no longer
#     bundles one. Mac-only / host-tested: re-verified with brew Lua 5.5.1
#     after the binary removal — same 353-call totals as recorded)
cd /WORKSPACE/Projects/ps5-installer-compat/conformance/src-conformance-static
lua hosttest_mock_env.lua conformance_static.lua
MOCK_VARIANT=b lua hosttest_mock_env.lua conformance_static.lua

# 3.4 Conformance diff tool on the committed sample logs
cd /WORKSPACE/Projects/ps5-installer-compat/conformance/src-conformance-static
python3 diff_results.py samples/sample_a.log samples/sample_b.log --verbose

# 3.5 Rebuild the merged real catalog, then the checker runs against it
cd /WORKSPACE/Projects/ps5-installer-compat
python3 compat/merge_catalogs.py
python3 compat/checker.py compat/sample_mod.lua compat/catalog-real-merged.json \
        --json /tmp/sample-mod-report.json
python3 compat/checker.py compat/townfall-item-spawner-mod compat/catalog-real-merged.json
# ^ re-runs the required TownfallItemSpawner grade (Section 5)

# 3.6 The full pipeline through the integration adapter (one entry point)
cd /WORKSPACE/Projects/ps5-installer-compat/installer
python3 adapter.py --help        # install | check | uninstall | verify
python3 adapter.py check --archive mod.zip --metadata mod.json \
        --title PPSA33286 --game-root ./sandbox/roots/PPSA33286 \
        --plugins ./sandbox/plugins --state ./sandbox/state

# 3.7 Tester's frozen adversarial suites (NOT runnable as-is from here —
#     the frozen harness hard-codes its original /tmp/sideproject staging
#     paths; port the path constants at the top of tests/src-tester/
#     framework.py first. Authoritative saved results: tests/results/*.json)
cd /WORKSPACE/Projects/ps5-installer-compat/tests/src-tester   # caveat applies
python3 framework.py suite_installer.py suite_detector.py \
        suite_uninstall.py suite_compat.py integration_tests.py
```

## 4. FULL TEST RESULTS (from the LATEST clean run ONLY)

**Source: `test-results/raw-test-output.txt`** — the definitive final clean
run (2026-10-07 15:29): one single-agent sequential pass from a fresh `cp -R`
of the entire tree (216/216 files) to `/tmp/final-run-v2/`, stdout+stderr
unfiltered (ResourceWarnings/UserWarnings preserved), re-verified live on
the current tree (installer-side 212/212, checker 55/55). It supersedes
every earlier total (196 → 242 → 250-projected → 267; history in
`docs/COUNTS.md` §5 and `docs/CHANGES-ROUND4.md`). **No other run's numbers
appear as current anywhere in this package.**

### 4.1 Unit suites — 267 / 267 PASS, 0 FAIL, 0 ERROR

| suite | tests | result |
|---|---|---|
| installer/test_installer.py | 55 | OK |
| installer/test_config_outputs.py | 11 | OK |
| installer/test_zip_safety.py | 21 | OK |
| installer/test_case_sensitivity.py | 23 | OK |
| installer/test_detector.py | 50 | OK |
| installer/test_uninstall.py | 11 | OK |
| installer/test_conflicts.py | 9 | OK |
| installer/test_adapter.py | 19 | OK |
| installer/test_adapter_edge.py | 13 | OK |
| compat/test_checker.py | 55 | OK |
| **TOTAL** | **267** | **267 PASS / 0 FAIL** |

Cross-check embedded in the run: the combined installer invocation reports
`Ran 212 tests … OK` (55+11+21+23+50+11+9+19+13 = 212), checker
`Ran 55 tests … OK`. The case-sensitivity volume test attached a real
case-sensitive APFS image (hdiutil) and re-ran a full installer-side suite
on it — passed, not skipped. `test_installer` emits cosmetic
`ResourceWarning: unclosed file` notices; preserved verbatim in the raw log.

### 4.2 Conformance host/mock results (same clean run, raw §3–§4)

| run | result |
|---|---|
| conformance_static, mock variant A | 353 calls, **353 ok / 0 fail** |
| conformance_static, mock variant B (deliberate value changes) | 353 calls, 352 ok / 1 fail (the designed fail) |
| diff_results.py: sample_a vs sample_b | **347 MATCH / 4 MISMATCH / 2 MISSING / 1 EXTRA** (354 keys), differences found (by design) |
| conformance_live (Rev 2), fresh full-battery mock (all APIs stubbed, Pawn+PC present) | **86 CALL rows / 6 PREQ rows: 82 PASS / 1 FAIL / 3 SKIP** — the FAIL is the intentional `string.toFName` absence probe; the SKIPs are LoopAsync (uncancelizable by design) + two TArray read-only exclusions. Verified by the Rev-2 pass with the repo's Lua parser (`tests/src-tester/lua_syntax.py`) + `luac -p`; the scratch harness was removed after verification (evidence: `conformance/src-conformance-live/README.md`, "Host verification") |
| conformance_live, degraded mock (constructors absent, no Pawn/PC) — Rev-1-era figures, retained for history | 27 PASS / 7 FAIL / 18 SKIP — the 7 FAILs are exactly the absent-API probes |
| hostile missing-global probes (tester, 17 globals deleted one at a time) — frozen tester evidence | **17/17 survived** — all 353 static calls always emitted, zero crashes |

**Mock caveat (required honesty statement):** pass under mock only proves the
scripts run against our own stand-ins, not against the real runtime —
`ps5_runtime_status` is blank in 106/106 globals and 476/476 type rows
(599/599 merged), so no catalog entry has been proven against a real PS5
UE4SS runtime (Section 6). Neither conformance script has run in a real game
session.

### 4.3 Fresh real-catalog grade (same clean run, raw §5)

| run | grade |
|---|---|
| TownfallItemSpawner vs catalog-real-merged.json (599 entries) | **partly-compatible — PC-only input dependency** (Section 5) |
| sample_mod.lua vs the real catalog | `unknown - runtime status not filled` (pre-v4 saved report `compat/sample-mod-real-catalog-report.json`; the sample mod uses no `Key.*` constants, so v4 does not change its outcome) |

### 4.4 Frozen historical evidence — NOT part of the 267

The independent tester's suites (`tests/src-tester/`) cannot run from this
tree (hard-coded `/tmp/sideproject` paths, Section 3.7). Their saved results:
originally **87 pass / 1 fail** across 88 tests (the fail:
`det.requirements_adversarial_text`, the Nexus `/games/<title>/` URL gap).
That behavior was fixed in round 3 (finding #6); the saved working-copy
result `tests/results/suite_detector.json` (updated 14:50) records 19/19
PASS, and the frozen original with the FAIL is preserved at
`tests/src-tester/results/suite_detector.json`. The behavior is pinned green
in the current tree by
`test_detector.RequirementsTests::test_nexus_url_with_games_path_prefix`
(passing in the 267).

## 5. TOWNSFALL ITEMSPAWNER GRADE (the required test result)

Artifacts: `compat/townfall-item-spawner-grade.json` (verdict + provenance +
FULL checker scan), `compat/townfall-item-spawner-grade.md` (summary),
`compat/townfall-item-spawner-mod/Scripts/main.lua` (staged source).

```
GRADE:   partly-compatible
REASON:  partly-compatible - PC-only input dependency: keyboard keybinds
         (Key.DOWN_ARROW, Key.F7, Key.LEFT_ARROW ... +3 more) need
         runtime/config controller mapping on PS5
         (PS5 has no keyboard; see PS5Pad.ini)
```

- **What was graded:** the real mod "TownfallItemSpawner v1.0" (F7 menu item
  spawner), source read-only `git show`n from the mod-manager worktree branch
  `origin/claude/townfall-item-spawner-hyj9eg` @ `2e1ef8be…`; 25 of 26 branch
  copies byte-identical (sha256 `f586043a…b6559658`, 746 lines); the one
  divergent copy is the intentionally corrupted `root-badsize` host-test
  fixture (excluded). `Mods/shared/UEHelpers` ships outside the mod dir and
  is out of scope.
- **Catalog used:** `compat/catalog-real-merged.json` rebuilt against v4 —
  **599 entries** (106 globals passthrough − 3 fused string-ctors = 103,
  + 42 types + 454 method entries; arithmetic in `docs/COUNTS.md` §3),
  **599/599 blank `ps5_runtime_status`** (the `Key.*` classification comes
  from the entries' `pc_only_input: true` flag, not a filled status).
- **Fresh numbers (definitive run, raw §5):** total 66 calls | 32
  catalog-matched | **0 unknown** | 34 unresolved (game-side Blueprint calls
  — never counted against the grade) | 85 lua-builtins. **PC-ONLY INPUT —
  the six `Key.*` keybind constants, all central** (top-level
  `RegisterKeyBind` args): `Key.F7` main.lua:694 (menu toggle),
  `Key.UP_ARROW`/`Key.DOWN_ARROW` :698/:703 (navigate), `Key.RETURN` :708
  (confirm/spawn), `Key.LEFT_ARROW`/`Key.RIGHT_ARROW` :713/:722 (category
  cycle). **9 cataloged-but-blank APIs** (RegisterKeyBind x6 [central, the
  consumer of these constants], ExecuteInGameThread x6 [central],
  StaticFindObject x4 [central], FText x2 [central], StaticConstructObject
  x2, FName x2, FindFirstOf x2, LoadAsset x1, print x1). **Not in catalog:
  none** — the six constants were the previous "unknown" entries.
- **How the grade evolved (documented, not hidden):** first grade
  `unknown - runtime status not filled` (all statuses blank + 6 `Key.*`
  "not in catalog"). The stdlib-override fix surfaced the 9th blank API
  (`print`); catalog v4 cataloged `Key`/`ModifierKey`; the checker's new
  PC-ONLY INPUT category turns the six constants into a **graded PS5
  difference** (the constants exist and are readable — pure Windows VK
  integers, `needs_live_game: false` — but the PS5 has no keyboard, so
  every keybind needs runtime/config controller mapping; current approach:
  PS5Pad.ini chords, e.g. L3+R3 = F7). A confirmed difference outranks
  unverifiability — hence `partly-compatible`. PC-only input alone never
  forces `unsupported` (reserved for a central *missing* API); the 9
  blank-status APIs remain neither passed nor failed.
- **The automated test (ANSWERS.md Q3):**
  `compat/test_checker.py::TestTownfallItemSpawnerGrade::test_townfall_grade_partly_compatible_pc_only_input`
  — THE required test: loads the real staged mod + the real merged catalog,
  runs `checker.scan()`, asserts grade `partly-compatible` and the
  PC-only-input reason. Companions: `test_townfall_inputs_exist`,
  `test_townfall_blank_status_core_apis`,
  `test_townfall_six_key_constants_are_pc_only_input` (six constants in the
  `pc_only_input` bucket, all central; `unknown` empty), plus 8
  `TestPcOnlyInput` unit tests for the category itself. All pass in the 267.
  The pins intentionally go red if the catalog/checker changes without
  re-grading — that is their job.
- **To grade for real:** (1) runtime/config side — controller mapping for
  the six keybinds (open question 9, Section 10); (2) catalog side — fill
  `ps5_runtime_status` for at least the 9 blank APIs; then re-run §3.5.

## 6. CONFORMANCE COVERAGE

Source: `docs/coverage.json` + `docs/COVERAGE-MAP.md` (the LATEST coverage
snapshot, 2026-10-07 15:07–15:11) — every catalog entry mapped to the
conformance script that calls it, or the reason it is not called.

| scope | rows | invoked | presence-only | any CALL row | not covered |
|---|---|---|---|---|---|
| globals (v3 snapshot: 98 + 6 Rev-2) | 104 | 68 | 13 | 81 | 23 |
| type methods (456) + metamethods (20) | 476 | 80 | 0 | 80 | 396 |
| **total (v3 snapshot)** | **580** | **148 (25.5%)** | **13** | **161 (27.8%)** | **419** |

Two conventions to read it with (both defined in `docs/COUNTS.md` rows 6,
12–14):

- **The snapshot is v3-era (580 rows).** Catalog v4 added the
  `Key`/`ModifierKey` TYPE containers afterwards, so the coverage convention
  total is now **582 API rows** — the two new containers have no coverage
  rows yet (they are type containers, not function/method rows). Recounting
  the map against v4 is a documented pending item.
- **Call-count reconciliation:** static 76 literal sites loop-expand to 353
  CALL rows; live Rev 2 emits 86 CALL rows (+6 PREQ); total 439 — matching
  every claim in this package.

**The mock caveat (verbatim from the coverage map):** *"Pass under mock only
proves the scripts run against our own stand-ins, not against the real
runtime."* Mock agreement is evidence the scripts are syntactically valid,
crash-safe and log-format-correct; it is NOT evidence of runtime conformance.

**What Rev 2 fixed here (was a named mock blind spot):** the old live TArray
battery matched no catalog TArray member — Rev 2 rewrites it to the port's
real members (`GetArrayNum`, `#a` (`__len`), in-bounds `a[1]`, `GetArrayMax`,
`IsValid`, `GetArrayAddress`, immediate-stop `ForEach`, discovered read-only
via `ForEachProperty`); `UClass:GetFunction` (never a catalog method — does
not exist in this port) is replaced with real members; the fictional `Loop`
table is replaced with the real scheduling globals (self-owned
register/cancel cycles); every cataloged hooking global now has a gated
no-op call. Mocks still cannot catch this defect class — the fix came from
source reading, not a console. Remaining flagged gaps: `string.toFName`
(labeled absence probe), FString/FText `__tostring`, FString `__concat`;
`LoopAsync` intentionally not called (no cancellation API — SKIP-noted).

## 7. REQUIREMENTS TRACEABILITY

Full per-row detail (implementation file → test file → test names → result):
`docs/REQUIREMENTS-TRACE.md`. **Its rows 2, 5 and 15 were written before
this round's fixes landed and are superseded by the table below** (the trace
file itself still carries the older wording — see Section 9).

| # | Spec item | Result (current tree, definitive run) |
|---|---|---|
| 1 | Input: archive + Nexus metadata + title ID | **PASS** — 55-test installer suite; tester 27/27 |
| 2 | Output: install layout (loadlist, **mods list, enabled flags**, ploader.ini) | **PASS (round 3; was PARTIAL)** — all four outputs written on install and restored byte-for-byte on uninstall: loadlist block, `mods.txt` row `<Name> : 1`, empty `enabled.txt` marker (unless the archive ships one), ploader.ini line. Pinned by `installer/test_config_outputs.py` (11 tests: four outputs, round trip, crash-rollback of all four, archive-shipped marker, case-variant `Enabled.txt` rejection, mods.txt editor) |
| 3 | NEVER-EDIT rule: byte-for-byte + manifest + verify | **PASS** — sha256 verify, CRLF preserved, foreign rows preserved, overrides-not-settings rule |
| 4 | Mod-type detection (Lua / C++ DLL / pak-IoStore / LogicMods / other) | **PASS** — 9 types incl. `ue4ss-runtime`, with REAL UE4SS 3.0.1 release-layout detection (flat `UE4SS.dll`+`dwmapi.dll` root and nested `ue4ss/` installed-style layouts, exact default-mods list; reference `docs/UE4SS-RELEASE-LISTING.txt`; +3 detector / +1 installer tests). Open note: `.lua` content never binary-sniffed (finding #7) |
| 5 | Requirements: UE4SS version + other mods | **PASS (round 3; was open defect)** — the Nexus `/games/<title>/` URL gap is FIXED (`NEXUS_ID_RES` handles the prefix, locale prefixes, hyphenated slugs); pinned by `test_nexus_url_with_games_path_prefix`; the tester's saved detector result updated to 19/19 PASS (frozen original preserved) |
| 6 | Uninstall + rollback | **PASS** — 11/11 + tester 19/19; the round trip now ships WITHOUT the tester's bridge: `installer/adapter.py` `InstallPipeline` reconciles installer manifests → uninstaller records (finding #22 closed), pinned by 19 + 13 adapter tests |
| 7 | Conflict detection | **PASS** — 9/9 module tests; NOW WIRED into the install path by `InstallPipeline._preflight` (`conflicts.check_install`, adapter.py:648): PATH_CONFLICT/VERSION_MISMATCH blockers refuse before any write; keybind/hook conflicts warn and proceed (finding #23 closed at pipeline level — the raw `Installer.install()` path itself still does not gate; integrators go through the pipeline). Level-N comment bug still open (finding #10) |
| 8 | Archives: zip supported, 7z/rar report needed | **PASS** — magic sniff, conversion guidance, tar.gz/xz/bz2 also supported |
| 9 | Install/verify/uninstall round trips | **PASS** — crash injection + recovery verified; end-to-end through one entry point (`test_adapter` lifecycle + `test_adapter_edge` full round trip) |
| 10 | Tampered file detected | **PASS** — verify exit 3; uninstall confirm flow; adapter tamper test |
| 11 | Each mod type classified correctly | **PASS** — 50 detector tests, all types |
| 12 | Two mods writing the same path | **PASS** — conflicts module AND the pipeline gate (adapter conflict-blocking + shared-ploader-line edge tests) |
| 13 | Corrupt archive | **PASS** — truncated/empty/not-a-zip/encrypted/bomb all rejected (tester corpus + zip-safety suite) |
| 14 | Conformance diff tool on two sample logs | **PASS** — artifacts + reproductions in the definitive run (no unit test pins exit codes yet — noted follow-up) |
| 15 | Compatibility grade for TownfallItemSpawner | **DELIVERED** — `compat/townfall-item-spawner-grade.json` (v4): grade **partly-compatible**, reason "PC-only input dependency: keyboard keybinds … need runtime/config controller mapping on PS5"; automated pin `test_townfall_grade_partly_compatible_pc_only_input` (+3 companions) in the 267. Grading beyond that requires GPT's console fill-in of the 9 blank APIs plus the runtime's controller-mapping decision — the honest ceiling today |

Cross-cutting caveats the table must be read with: mock-only conformance;
open findings (Section 8); no locking; everything host-only.

## 8. REVIEWER FINDINGS (all 23, classified, CURRENT status)

Source: the independent tester's report `tests/REVIEW.md` (window 2026-10-07
~13:38–14:06; verbatim frozen copy `tests/src-tester/REVIEW.md`; per-test
detail `tests/results/*.json`; final module digests
`tests/src-tester/module_state.json`). Classes: BLOCKER / BUG / DESIGN
CONCERN / EDGE CASE / SUGGESTION. Statuses re-verified against the FINAL
tree by code reads + probes, and by the definitive 267-test run.

| # | Module | Finding | Class | Status (final tree) |
|---|---|---|---|---|
| 1 | installer-core | `NameError: config_writes` on every first install | BLOCKER | **FIXED during review** — pinned by `inst.happy.fresh_install_must_succeed` |
| 2 | installer-core | `need_dir()` never mkdir-plans leaf mod dirs | BLOCKER | **FIXED during review** — rewritten ancestor-chain `need_dir`; pinned by tests |
| 3 | installer-core | No locking; concurrent installs mutually destruct (detected, fail-closed) | DESIGN CONCERN | **OPEN — accepted risk** (0 flock/fcntl in installer/uninstall/adapter); serialized use documented |
| 4 | installer-core | Duplicate zip members normalize to same path, not rejected at inspect | EDGE CASE | **FIXED** — duplicate detection at inspect; pinned by `test_zip_safety.TestDuplicateEntries` |
| 5 | installer-core | `verify` trusts absolute `install_path`s; moving roots dir invalidates installs | DESIGN CONCERN | **OPEN — partially mitigated**: manifests now also record relative `install_rel` (installer.py:1714–1715) and the adapter reconciles through it; `verify` still keys on the absolute path |
| 6 | mod-types | Nexus `/games/<title>/` URL parses to no dependency | BUG | **FIXED (round 3)** — `NEXUS_ID_RES` (detector.py:149–160) handles `/games/<title>/mods/<id>`, locale prefixes, hyphenated slugs; pinned green by `test_nexus_url_with_games_path_prefix` in the definitive run; tester's saved result updated 19/19, frozen original preserved |
| 7 | mod-types | `.lua` content never sniffed: PE binary renamed main.lua classifies ue4ss-lua | BUG | **OPEN** — marker text-scan unchanged; MZ/ELF first-bytes check exists only for `.dll`/native members (detector.py:607/787) |
| 8 | mod-types | Scan truncation at MAX_ENTRIES (5000) noted in `notes` only, not `flags` | EDGE CASE | **OPEN** — unchanged (detector.py:533) |
| 9 | mod-types | Static marker detection bypassable by construction | DESIGN CONCERN | **OPEN BY DESIGN** — shared documented blind spot with the checker |
| 10 | uninstall/conflicts | `strip_lua_comments` mishandles `--[==[...]==]` level-N comments → false-positive KEYBIND_CONFLICT warnings | BUG | **OPEN** — conflicts.py stripper still level-0 only (conflicts.py:127–160); recommendation to share the checker's level-aware stripper stands |
| 11 | uninstall | File moved to trash before the journal records it — crash window | DESIGN CONCERN | **OPEN** — move-then-record ordering remains (the journal is created and flushed per step, uninstall.py:470–500); the window is narrowed but structurally present |
| 12 | uninstall | Install-state read-modify-write unlocked | DESIGN CONCERN | **OPEN — accepted** (same class as #3) |
| 13 | uninstall | Dependency cycles deadlock uninstall by design (safe refusal) | DESIGN CONCERN | **OPEN BY DESIGN** — safe refusal, no corruption; pinned by `un.circular_dependency_deadlock` |
| 14 | compat-checker | String-indexed calls (`_G["LoadAsset"]`) invisible, no dynamic-access flag | BUG | **OPEN** — no dynamic-access-risk flag in checker.py (grep: 0 hits); documented by `compat.obfuscated_indexed_calls` |
| 15 | compat-checker | Syntax errors indistinguishable from empty mods (both unknown, 0 calls) | EDGE CASE | **OPEN** — no separate "doesn't parse" outcome; the bundled Lua 5.4.7 / `lua_syntax.py` could supply it |
| 16 | conformance-static | "Every call in pcall" was FALSE + broken always-truthy guard | BLOCKER | **FIXED during review** — `run_call` pcall-wraps; 17/17 hostile probes survived; log format verified byte-exactly |
| 17 | conformance-static | `rtype` column not sanitized (tab could break TSV) | EDGE CASE | **OPEN — low risk** — params/value columns are sanitized; the rtype emission path is not tab-guarded |
| 18 | conformance-live | Byte-based truncate can split a UTF-8 rune | SUGGESTION | **OPEN — cosmetic** (Rev 2 kept 200-char truncation) |
| 19 | catalogs | Provenance unverifiable: citations point at a tree "not on this host"; port facts presented as upstream | DESIGN CONCERN | **FIXED — resolved and strengthened.** The tree exists (read-only); all citations verified programmatically in the PORT tree (523/523 types, 238/238 globals); "FName→FNameUserdata is a port rename" was FALSE (upstream's own convention — corrected). Round 3 then went further: `catalog/update_upstream_refs.py` re-derived REAL upstream UE4SS 3.0.1 file:line citations into `upstream_reference`/`upstream_implementation` fields, wrapped both files with `catalog_version` + `citation_tree`, kept port citations as the secondary reference, and flagged `loadfile`/`dofile` as port-only additions. Pre-edit backups under `catalog/backup-before-upstream-refs-2026-10-07/` |
| 20 | catalogs | `ps5_runtime_status` blank everywhere → every cataloged call lands unknown-status; a clean mod mis-grades partly-compatible | DESIGN CONCERN | **HALF-FIXED** — checker buckets blank statuses separately (`status-blank`) and grades `unknown - runtime status not filled` (never a fake pass/fail); the v4 PC-ONLY INPUT category additionally grades flag-able differences without statuses. The fill-in itself is GPT's (open by design): still 106/106 globals, 476/476 type rows, 599/599 merged blank |
| 21 | compat sample catalog | Sample catalog drift from real catalogs | BUG | **FIXED** — renamed `sample_catalog_TEST_FIXTURE.json` + invented-status wrapper; frozen original untouched |
| 22 | integration | I-1: installer/uninstaller record contracts differ; no shipped adapter (only the tester's ~40-line bridge) | BLOCKER (system) | **FIXED (round 3)** — `installer/adapter.py` ships the production `InstallPipeline` + `installer_manifest_to_uninstall()` + unified `install_state/<TITLE>.json` (one state file feeds uninstall AND conflicts); ploader cleanup after transaction commit (shared-line aware, with rollback snapshot). Pinned by `test_adapter.py` (19) + `test_adapter_edge.py` (13) |
| 23 | integration | I-2: conflicts.py never wired into the install path; installer will write colliding files | BLOCKER (system) | **FIXED (round 3, pipeline level)** — `InstallPipeline._preflight` builds the `InstalledRegistry` from the unified state and calls `conflicts.check_install` (adapter.py:648); PATH_CONFLICT/VERSION_MISMATCH blockers refuse before any write; warnings recorded in the result. Residual: the raw `Installer.install()` used directly still performs no path/keybind/hook conflict check — integrators must go through the pipeline (the separate case-conflict wiring remains inside installer.py) |

**Tally: 23 findings — 10 FIXED (#1, #2, #4, #6, #16, #19, #21, #22, #23,
plus #20's checker half), 1 partially mitigated (#5), 12 OPEN** (of which
#9/#13/#20-fill-in are open by design, #3/#12 are accepted risks, #17/#18
are low/cosmetic). **No system BLOCKER remains open** — the two that made
this "five good parts, not a system" are closed; what remains open is
documented risk, not integration failure.

### Fixes added across the review rounds (each pinned by green tests)

- R1 (during the tester's window): #1, #2, #16, zip-duplicate inspect (#4),
  checker blank-status bucketing (#20 half).
- R2 (post-review hardening): case-conflict suite (23), zip-safety suite
  (21), `ue4ss-runtime` detector type + install refusal, cataloged-stdlib
  overrides, fixture rename (#21), merged contract proposal v1.1.
- R3/R4 (this round): mods.txt + enabled-flag writer (`test_config_outputs`,
  11), Nexus URL fix (#6), UE4SS real-layout detection
  (`docs/UE4SS-RELEASE-LISTING.txt` reference), conformance Rev 2 (TArray/
  hooking/scheduling fixes + 6 catalog additions), REAL upstream citations
  (#19), the integration adapter (#22/#23; 19 + 13 tests), Key/ModifierKey
  catalog v4 + PC-ONLY INPUT grading + the Townfall grade pins, audience
  how-tos, unified counts, security scan, pushes (`docs/CHANGES-ROUND4.md`).

### Tester's positive verification (balance)

Journal-first crash recovery (crash injection → rollback → clean reinstall),
ownership guards, traversal/symlink/encrypted/10k-entry/BOM-CRLF rejection,
unicode + NFC/NFD determinism, magic-byte sniffing, zip-in-zip depth 4,
tar device-entry safety, semver cue-word discipline, byte-exact multiset
config-line removal, full uninstall rollback with double-rollback refusal,
checker grading ladder end-to-end, hostile-Lua crash-safety, syntax validity
per the tester's parser AND real Lua 5.4.7. Static danger-scan hits were
doc-string mentions only; `docs/SECURITY-SCAN.md` re-audits the whole tree
plus full git history: **CLEAN** (no credentials, no downloaded mods — all
32 archives are synthetic corpus fixtures, one documented interpreter
binary, zero `.env` files).

## 9. KNOWN LIMITS (honest, current)

1. **Compatibility grading is still mostly unverifiable against the real
   catalog** — all 599 merged entries have blank `ps5_runtime_status`; the
   one graded PS5 difference so far is the PC-only input class
   (TownfallItemSpawner → `partly-compatible`); everything else stays
   `unknown` until GPT fills statuses.
2. **Conformance is mock-only** — 161/580 v3-snapshot rows (27.8%) covered,
   148 invoked (25.5%); 396/476 type-method rows have no call site; mock
   passes prove the scripts run, NOT runtime conformance (the Rev-2
   name-mismatch fixes were derived from the port source, not a console).
   Neither script has run in a real game session.
3. **Coverage map is a v3-era snapshot** — `docs/coverage.json` /
   `COVERAGE-MAP.md` hold 580 rows; the v4 `Key`/`ModifierKey` containers
   are not yet mapped (coverage-convention total 582). Recount pending.
4. **Concurrency: no locking anywhere** — concurrent installs mutually
   destruct (detected, fail-closed); concurrent uninstalls can lose a state
   entry. Single-user serialized use only (documented in the how-tos).
5. **The pipeline gates; the raw APIs do not (residual of #22/#23):** the
   `InstallPipeline` converts records and gates on conflicts, but the raw
   `Installer.install()` / `Uninstaller` classes used directly still emit
   different record shapes and perform no path/keybind/hook conflict check.
   Integrators must go through the pipeline.
6. **Static-analysis blind spots:** dynamic dispatch invisible and unflagged
   (#14); syntax errors indistinguishable from empty mods (#15); marker
   detection bypassable (#9); `.lua` content not binary-sniffed (#7);
   MAX_ENTRIES truncation not in `flags` (#8).
7. **Smaller open items:** manifests verify via absolute `install_path`
   (#5; relative `install_rel` now recorded); level-N Lua comment stripper
   bug in conflicts (#10); uninstall move-before-record window (#11);
   `rtype` not tab-guarded (#17); byte-based live truncation (#18).
8. **7z/RAR rejected** with host-side conversion guidance (no converter
   shipped); pak/IoStore mods classified `needs-conversion`; UE4SS C++ DLL
   mods classified but not runnable; UE4SS runtime zips refused as mods (by
   design).
9. **Tester's frozen harness** (`tests/src-tester/`) keeps original absolute
   staging paths — saved JSONs are the results; re-running requires porting
   path constants (§3.7). Frozen `src-*` build scripts likewise write to
   their original absolute output paths.
10. **Stale sibling docs (known, listed so no reviewer is misled):**
    `docs/REQUIREMENTS-TRACE.md` rows 2/5/15 predate this round's fixes
    (superseded by Section 7); `README.md` and `docs/HOWTO-GPT.md` were
    written mid-round and still say "98 globals" (catalog is 106), and
    README quotes a superseded test state ("155 tests, 2 failures" — the
    definitive run is 267/267); `docs/COVERAGE-MAP.md`/`coverage.json` per
    item 3; `REVIEW-PACKAGE.v2.md` keeps the round-4 lineage with earlier
    totals as labeled history. `docs/COUNTS.md` and this file are the
    authoritative numbers.
11. **The Phase-0 pre-download badge and the wire envelopes in the contract
    are proposal-only** — not implemented (the checker has no `--mode
    predownload`); build against the CLIs/APIs in Section 12.

## 10. OPEN QUESTIONS

The three open review questions are answered in `docs/ANSWERS.md` — each in
its own subsection here, with CURRENT status (the answers were written
mid-round; the annotations reflect what has since landed):

### Q1 — The 2 system-level BLOCKERs (#22/#23): what exactly, who owns them, what's the fix?

- **What:** #22 — three incompatible record contracts (installer manifest:
  absolute `install_path`s + structured `config_entries`; uninstaller:
  relative `files[].path` + per-file config line lists + shared
  `install_state.json`; conflicts: a third path scheme), nothing shipped to
  convert between them — install → uninstall worked only through the
  tester's ~38-line bridge. #23 — `conflicts.check_install` never invoked
  by the install path.
- **Ownership (per the answer):** entirely zcode — all three endpoints are
  this project's modules; neither GPT's runtime nor the mod manager is
  involved. The remaining runtime-status work (#20 fill-in) is GPT's; the
  UI/flow around installs is the manager's.
- **The fix, and its status: IMPLEMENTED.** `installer/adapter.py` ships
  the production `InstallPipeline`: unified `install_state/<TITLE>.json`
  (one registry feeding uninstall AND conflicts),
  `installer_manifest_to_uninstall()` reconciliation (relative paths, owned
  line lists, enabled markers, overrides + stale files + the manifest
  itself), ploader cleanup after transaction commit (shared-line aware),
  and the conflicts gate (adapter.py:648) with documented refusal policies
  (ue4ss-runtime / runnable=no refused; `unsupported` compat blocks;
  PATH_CONFLICT/VERSION_MISMATCH refuse; keybind/hook warn). Pinned by 32
  adapter tests in the 267.

### Q2 — The 6 APIs TownfallItemSpawner uses that are not in the catalog

- They are `Key.F7 / UP_ARROW / DOWN_ARROW / RETURN / LEFT_ARROW /
  RIGHT_ARROW` — constant references (each read exactly once), members of
  the UE4SS `Key` Windows-virtual-key table, passed to `RegisterKeyBind`.
  Not function calls.
- **Resolution (was a catalog gap; now closed):** catalog v4 added `Key`
  (165 constants) and `ModifierKey` (3 active: SHIFT/CONTROL/ALT) TYPE
  entries — port `register_input_globals` cited at LuaMod.cpp:446–627,
  upstream cross-checked at :438–618, constant lists verified identical
  between trees, `pc_only_input: true`, `needs_live_game: false`. The
  checker resolves the specific constants (`Key.F7`) and reports a
  dedicated **PC-ONLY INPUT** PS5-difference — a confirmed difference
  outranks unverifiability, so the grade moved `unknown` →
  `partly-compatible` (Section 5). No per-constant function entries were
  added (that would have been the wrong fix).
- **Follow-on (now open question 9 / contract §8):** cataloging the Key
  table is necessary but not sufficient — the runtime must decide the
  keyboard→controller mapping strategy before keyboard-dependent mods can
  grade higher than `partly-compatible`.

### Q3 — The test that now checks the Townfall grade automatically

- **Created:** class `TestTownfallItemSpawnerGrade` in
  `compat/test_checker.py`. Originally 4 tests pinning `unknown - runtime
  status not filled`; updated with catalog v4 to pin the current verdict —
  **`test_townfall_grade_partly_compatible_pc_only_input`** (THE required
  test) + `test_townfall_inputs_exist` +
  `test_townfall_blank_status_core_apis` +
  `test_townfall_six_key_constants_are_pc_only_input` — plus 8
  `TestPcOnlyInput` unit tests for the category itself (distinct from
  missing/status-blank/unknown, dedicated reason and suggestion text,
  real-catalog entry shape). All pass in the definitive 267.

### Remaining open questions (for GPT and the mod manager)

From `docs/CONTRACT-PROPOSAL.md` §8 (single merged list) and
`docs/ANSWERS.md`:

1. Where do manifests live? (Proposal: `/data/modbridge/roots/<TITLE>/manifests/`.)
2. Who owns UE4SS-settings.ini? (Proposal: GPT; installer writes overrides only.)
3. Load order: installer computes, runtime enforces — is loadlist row order
   = load order? (Note: mods.txt rows also carry load order.)
4. Hot-reload: if the runtime ever supports mod hot-unload, the installer
   needs a notification API.
5. Catalog versioning: who bumps `catalog_version` when GPT fills statuses;
   does the manager auto-re-grade on a bump?
6. Phase-0 grade names: will the manager accept `unverified-likely-*` /
   `likely-*` outline badges (provisional never renders as verified)?
7. `compat_manifest.json`: will mod authors ship it; will the manager offer
   the submit-back flow?
8. `Key`/`ModifierKey` are now cataloged — should other enum/constant
   tables be cataloged the same way?
9. **PC keyboard → PS5 controller mapping** (`docs/ANSWERS.md` Q4, contract
   §6.4/§8.8): is PS5Pad.ini chord mapping (L3+R3 = F7) the general answer
   for arbitrary `Key.*` constants and ModifierKey combinations, or does
   the runtime need a translation layer? What happens when a mod registers
   a keybind the config doesn't map (today: registers but never fires —
   should it warn)? This decides the eventual `ps5_runtime_status` of
   `Key`/`ModifierKey`/`RegisterKeyBind`/`IsKeyBindRegistered`, and whether
   keyboard-dependent mods can ever grade above `partly-compatible`.
10. **Lowercase mod paths — runtime lookup vs installer refusal**
    (`docs/ANSWERS.md` Q5, contract §8.9): a mod shipping lowercase
    `scripts/main.lua` works on Windows (case-insensitive filesystem) but
    is refused by our installer (case-conflict detection). Should the
    runtime look up mod files case-insensitively on PS5, or should the
    installer keep refusing? Current behavior (Mac-only / host-tested,
    `installer/installer.py`): the installer detects the case mismatch
    between `Scripts/` (expected) and `scripts/` (shipped) and refuses
    with a runtime/config note — it never renames mod files (exact-case
    entrypoint requirement in `find_mod_root()` + `CaseConflictError` via
    `detect_caseConflicts()`/`_runtime_case_notes()`).

## 11. PROPOSED CONTRACT CHANGES (merged, one document)

`docs/CONTRACT-PROPOSAL.md` **v1.1 (merged)** is the single proposal (the
former `compat/CONTRACT-PROPOSAL.md` was folded in and deleted; frozen
original kept at `compat/src-compat-checker/CONTRACT-PROPOSAL.md`;
`compat/README.md` points there). Proposal-only: nothing integrated, nothing
sent; the canonical contract is owned by the mod manager team.

- §1 component boundaries (+`ps5_runtime_status` owner row: GPT fills,
  never authors structure) · §2 five core interfaces (install / uninstall /
  compatibility-check / config entries / conformance fill-in) · §3 ONE grade
  vocabulary incl. Phase-0 provisional names · §4 the two-phase pre-download
  badge contract · §5–7 requests to manager / GPT / catalog maintainers ·
  §8 the merged open-questions list (9 items, incl. keybind mapping and the
  lowercase-paths `scripts/` vs `Scripts/` refusal) · §9
  implementation checklist.
- Updated this round: §6 (requests to GPT) gained item 4 — document the PC
  keyboard → PS5 controller mapping mechanism; §8 gained the corresponding
  open question.
- Contradictions resolved in the original merge:
  `lua_api_calls_detected` dropped from the pre-download sketch (a source
  scan is impossible pre-download); `partly-compatible` canonical spelling;
  one `catalog_version` wrapper field; one grade/badge table; one scope
  preamble.
- Wire schemas: install/uninstall requests + results, badge envelope
  (`schema: 1`, `confidence`, `blockers` with runtime-side workarounds),
  caching key `(nexus_mod_id, file_id, archive_sha256, catalog_version,
  checker_version)`, and the honesty rules (unknown never guessed;
  description-only never shown verified). **The wire envelopes are
  proposal-only** — the shipped surface is the CLIs/APIs in Section 12.

## 12. HOW-TO-CALL

Full guides: `docs/HOWTO-GPT.md` (runtime owner) and
`docs/HOWTO-MOD-MANAGER.md` (manager integration — exact metadata schema,
manifest schema, exit codes, badge vocabulary mapping).

### For GPT (runtime owner)

1. **Fill `ps5_runtime_status`** in `catalog/globals.json` (106 entries) and
   `catalog/types.json` (42; methods inherit their type's status) after
   console testing. Values: `implemented+console-proven` /
   `implemented-untested` / `missing` / `behaves-differently` (input tables
   may instead carry the `pc_only_input: true` flag, as `Key`/
   `ModifierKey` already do); blank stays "unverifiable". Highest value
   first: `RegisterKeyBind`, `ExecuteInGameThread`, `StaticFindObject`,
   `FText`, `StaticConstructObject`, `FName`, `FindFirstOf`, `LoadAsset`,
   `print` (the 9 blank APIs of the Townfall grade), plus
   `Key`/`ModifierKey`/`IsKeyBindRegistered` once the controller-mapping
   strategy is decided (open question 9). Then
   `python3 compat/merge_catalogs.py` and re-run §3.5 to push the
   TownfallItemSpawner grade past `partly-compatible`. Bump
   `catalog_version` whenever statuses change (badge caches key on it).
2. **Run the conformance scripts** on the console: static as
   `<UE4SS>/Mods/ConformanceStatic/Scripts/main.lua` + empty `enabled.txt`;
   live as `<Game>/Mods/conformance-live/Scripts/main.lua` + a
   `conformance-live : 1` row in `<Game>/Mods/mods.txt` (10 s startup
   delay, 240 s in-game poll, DEGRADED mode on timeout, manual re-run via
   the global `ConformanceLiveRun()`; details in
   `conformance/src-conformance-live/README.md`). Extract with
   `grep $'^CALL\t' UE4SS.log`.
3. **Diff runs** against a PC UE4SS reference:
   `python3 conformance/diff_results.py RUN_PC.log RUN_PS5.log --json
   pc_vs_ps5.diff.json` (exit 0 equivalent / 1 differences / 2 usage).
4. **Confirm the config-entry formats** (loadlist byte-size law,
   ploader.ini, the mods.txt row + enabled.txt marker pair) and that the
   runtime never reads mod files outside `Mods/` (contract §6).

### For the mod manager team

1. Read `docs/CONTRACT-PROPOSAL.md` v1.1 and accept/modify/reject each
   interface; the badge contract is §4.
2. **Preferred entry point — the pipeline adapter** (closes I-1/I-2):
   `installer/adapter.py` — Python `InstallPipeline` (install_mod /
   check_mod / uninstall_mod / verify_mod / installed_mods /
   register_existing / rollback_uninstall) or CLI
   `python3 installer/adapter.py install|check|uninstall|verify …`
   (`--archive --metadata --title --game-root [--plugins --state --catalog
   --game-version]`). It refuses ue4ss-runtime/runnable=no archives, blocks
   on `unsupported` compat grades and conflict blockers, warns on
   keybind/hook conflicts and `partly-compatible`/`unknown` grades,
   maintains the unified install state, and returns structured
   Install/Uninstall/CheckResult objects.
3. Low-level modules remain callable directly: `Installer` in
   `installer/installer.py` (install/verify/recover CLI, case-conflict
   APIs); `compat/checker.py` (`scan(mod_path, catalog_path)` or CLI with
   `--json`); `installer/detector.py` (`classify_archive` → 9 labels);
   `installer/uninstall.py` `Uninstaller`; `installer/conflicts.py`
   `check_install()/check_batch()`. Build the checker catalog with
   `python3 compat/merge_catalogs.py`; `compat/sample_catalog_TEST_FIXTURE.json`
   is invented-status test data — never ship badges from it.
4. Serialize installs/uninstalls per title root (no locking exists).

## 13. SOURCE ATTRIBUTION (upstream vs port, pinned version)

Pinned subject: **upstream UE4SS 3.0.1** (vendored
`ue4ss-unreal-native-build-1/.../UE4SS/`, `version.cache` = 3.0.1.0.0;
cross-check clone `ue4ss-binding-research-1/upstream/RE-UE4SS/UE4SS/`).
Port (PS5/Prospero) = `.superpowers/native-dependency-spike/
native-coherent-runtime-1/source/port/ue4ss/`. All trees read-only. Audited
in `compat/real-catalog-run.md` + `docs/CLEAN-RUN-REPORT.md` §3; implemented
this round by `catalog/update_upstream_refs.py`.

- **Both catalog files are versioned wrappers** —
  `{catalog_version: "2026-10-07-v4", ue4ss_version: "3.0.1", citation_tree,
  entries}` — and entries carry **real upstream citations**:
  `upstream_reference` (globals) / `upstream_source_file` +
  `upstream_implementation` (types/methods), re-derived by comment-aware
  scan of the upstream tree (live `register_function` / `lua_setglobal` /
  `make_global` matching; dotted names resolved via the last `add_pair`;
  32/34 cited files are line-identical to upstream, the two divergent files
  resolved by unique content match). 142 entries carry the attribution note
  (98 v2 globals + 42 types + the 2 v4 input tables); the 6 Rev-2 additions
  cite via `upstream_reference` fields instead.
- **Port citations kept as the secondary reference** (`registration_sites` /
  `source_file` / `implementation`, port line numbers); `citation_tree`
  metadata distinguishes the two. Pre-edit backups:
  `catalog/backup-before-upstream-refs-2026-10-07/`.
- **Port additions honestly labeled:** `loadfile`/`dofile` are marked PS5
  port additions (absent from vendored 3.0.1); port/PS5 behavior lives in
  `notes`, not `pc_only_behavior`.
- **v4 input tables:** `Key` (165 constants) and `ModifierKey` (3 active)
  cite port `src/Mod/LuaMod.cpp:446–627` (`register_input_globals`;
  `make_global("Key")` at :614, `make_global("ModifierKey")` at :626),
  cross-checked upstream at `UE4SS/src/Mod/LuaMod.cpp:438–618` (:606/:618);
  constant lists verified identical between trees (values are Windows
  virtual-key codes, `Input::Key` enum in
  `deps/first/Input/include/Input/KeyDef.hpp`: `RETURN=0x0D`,
  `LEFT_ARROW=0x25`, `F7=0x76`; the six `LEFT_SHIFT`…`RIGHT_ALT`
  ModifierKey variants are commented out in BOTH trees and unregistered).
- **"FName → FNameUserdata is a port rename" was FALSE** — `*Userdata`
  metatable literals exist verbatim in upstream 3.0.1 at the same lines;
  corrected in `catalog/src-catalog-types/SUMMARY.md`.
- **UE4SS runtime layout evidence:** the real `UE4SS_v3.0.1.zip` (public
  GitHub release URL, 5,523,402 bytes, sha256 `4b47d4bc…c57ec`, downloaded
  Mac-only to `/tmp`) — full 46-file listing pinned at
  `docs/UE4SS-RELEASE-LISTING.txt`; the detector's flat/nested layouts and
  default-mods list are derived from it.
- **Conformance scripts:** call lists derived by static reading of the same
  port tree, cross-checked against upstream (per-section provenance in the
  two conformance READMEs); host-tested only against mocks; bundled
  interpreter is Lua 5.4.7 arm64.
- **TownfallItemSpawner mod source:** read-only `git show` from the
  mod-manager worktree branch `origin/claude/townfall-item-spawner-hyj9eg`
  @ `2e1ef8be267c4bf811f83f2da168cd52e6b0e16b`; 25/26 copies byte-identical
  (sha256 `f586043a…b6559658`); staged at
  `compat/townfall-item-spawner-mod/Scripts/main.lua`. The runtime and the
  manager branch were read, never written.
- **Everything Mac-only / host-tested.** No console contact anywhere in
  this package.

## 14. COUNTS (one authoritative set)

From `docs/COUNTS.md` (the count sheet — every number in this package is
one of these rows; re-verify commands are in the sheet):

| quantity | value |
|---|---|
| global entries | **106** (98 v2 + 6 conformance v3 + Key/ModifierKey v4) |
| types | **42** |
| method entries (raw) | **456** |
| metamethods | **20** |
| type rows | **476** |
| API rows (coverage convention) | **582** (coverage snapshot: **580**) |
| raw catalog rows | **624** |
| merged-catalog entries (as shipped, v4) | **599** (= 103 globals-after-fusion + 42 + 454) |
| attribution-note entries | **142** |
| coverage: covered / invoked / presence-only / not-covered | **161 (27.8%) / 148 (25.5%) / 13 / 419** of 580 |
| blank `ps5_runtime_status` | **106/106** globals, **476/476** type rows, **599/599** merged |
| conformance CALL rows | **353 static + 86 live = 439** (+6 live PREQ rows) |
| unit tests (definitive run) | **267/267 PASS** (212 installer-side + 55 checker) |
| TownfallItemSpawner grade | **partly-compatible** (PC-only input dependency) |
| tester frozen results | 88 tests — originally 87 pass / 1 fail; detector row updated to PASS after the round-3 fix; frozen original preserved |

Rules the sheet enforces: **456** is the method count, 476 is
methods+metamethods, 454 is the merged-method figure; **582/580** differ by
the two unmapped v4 type containers; **599** is the on-disk merged file;
**142** is not a size — it is the attribution-note count. If any entry is
added again, recount the sheet first.
