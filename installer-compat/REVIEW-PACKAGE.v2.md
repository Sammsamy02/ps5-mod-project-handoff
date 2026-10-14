# REVIEW-PACKAGE.md — Installer + Compatibility Layer (side project)

> Mac-only / host-tested throughout. No console contact at any point (no FTP,
> no payloads, no launches). Work confined to this project directory, plus
> documented exceptions: read-only `git show` of the mod-manager worktree to
> stage the TownfallItemSpawner mod source, read-only inspection of the UE4SS
> source trees for catalog attribution (both recorded in
> `compat/townfall-item-spawner-grade.md` and `compat/real-catalog-run.md`),
> and one Mac-only download of the public UE4SS v3.0.1 release zip to /tmp
> used as the detector's release-layout reference (public URL + sha256 +
> listing pinned in `docs/UE4SS-RELEASE-LISTING.txt`). Previous versions of
> this file are preserved as `REVIEW-PACKAGE.v1.md` and
> `REVIEW-PACKAGE.v2.md` (v2 = the exact round-4 pushed state).
>
> Round-4 state (2026-10-07): definitive FINAL clean run **267/267 PASS**
> (Section 4.1; re-verified live on the current tree by this pass:
> installer-side 212/212 OK, checker 55/55 OK). Pre-push security scan verdict
> **CLEAN** (`docs/SECURITY-SCAN.md`). Round-4 content pushed to the private
> remote `https://github.com/Sammsamy02/ps5.git`, branch
> **`side/installer-compat`**, commit
> **`69c88ede930168e6070f8fdca16f9c23eca0c850`** (2026-10-07 15:35 local;
> supersedes the first round-4 push `390ef65f…` of 15:27). The full round-4
> change summary with the test-count arithmetic is `docs/CHANGES-ROUND4.md`.

## 1. WHAT WAS BUILT

The INSTALLER + COMPATIBILITY layer between GPT's PS5 runtime and Claude's mod
manager, per the side-project spec. Five modules plus test/review/audit
infrastructure:

- **Catalog** (`catalog/`): machine-readable UE4SS Lua API surface — 106 global
  entries (`globals.json` v4: 98 functions + 6 Rev-2 conformance additions +
  the `Key` (165-constant) and `ModifierKey` (3-active-constant) PC-only
  input TYPE tables) + 42 types with 456 method entries and 20 metamethods
  (`types.json`), every entry citing file:line registration sites, PC-only
  behavior, and an empty `ps5_runtime_status` awaiting GPT's console fill-in.
  Attribution notes on 142 entries (98 v2 globals + 42 types + the 2 v4 input
  tables; the 6 Rev-2 additions carry `upstream_reference` citations instead;
  Section 13). **Every count in this package is defined once, with its
  formula, in `docs/COUNTS.md`** (e.g. 456 raw methods vs 454 merged vs 476
  methods+metamethods; 582 API/coverage rows vs 599 merged entries vs 624 raw
  rows; the `docs/coverage.json` snapshot is the 580-row v3 state) — cite
  that sheet before quoting any number.
- **Conformance** (`conformance/`): `conformance_static.lua` (353 calls,
  read-only, game-state-independent) + `conformance_live.lua` (Rev 2: 86 CALL
  rows + 6 prerequisite rows, prerequisite-gated, read-only) +
  `diff_results.py` (MATCH/MISMATCH/MISSING/
  EXTRA log differ) + a host-test mock environment and bundled Lua 5.4.7.
- **Installer** (`installer/`): transactional journal-first install engine
  (crash-safe, sha256 verify, loadlist.txt/ploader.ini/mods.txt+enabled-flag
  config editing, ownership guards) + mod-type detector (9 types incl.
  `ue4ss-runtime`, real UE4SS-release-layout rules) + journalled
  uninstall/rollback + conflict detection (path/keybind/hook/deps) +
  case-conflict detection for the PS5's case-sensitive disk + the
  `InstallPipeline` adapter (`adapter.py`: detect → compat → conflicts →
  install → verify → register, one unified install-state record).
- **Compat checker** (`compat/`): static Lua scanner + 4-grade ladder
  (compatible / partly-compatible / unsupported / unknown), blank-runtime-status
  bucketing, a dedicated **PC-only-input category** (`Key.*`/`ModifierKey.*`
  keyboard dependencies), catalog merge tool, real-mod grading of
  TownfallItemSpawner.
- **Tests/review/audit** (`tests/`, `docs/`, `test-results/`): independent
  adversarial tester (88 tests, 65-fixture corpus, pure-Python Lua 5.3
  validator), coverage map of the 580 catalog rows (v3 snapshot), requirements
  trace, definitive final clean-run verification (267/267), merged contract
  proposal v1.1, and a pre-push security scan (verdict CLEAN).

Post-review hardening added 2026-10-07 after the tester's report: case-sensitivity
suite (incl. a real case-sensitive APFS volume test), zip-safety suite
(duplicates/bombs/backslash/UE4SS-as-mod), `ue4ss-runtime` detector type with
install refusal, checker blank-status and cataloged-stdlib-override fixes,
catalog attribution audit + merge tool, and the two contract proposals merged
into one `docs/CONTRACT-PROPOSAL.md` v1.1.

Rounds 3-4 additions (2026-10-07, same day): the `InstallPipeline` adapter
closing integration findings #22/#23, mods.txt/enabled-flag outputs, the
Nexus `/games/<title>/` URL fix, conformance Rev 2, the definitive
single-agent clean run, the `Key`/`ModifierKey` catalog v4 with the checker's
PC-only-input category (TownfallItemSpawner now grades `partly-compatible`,
Section 5), real UE4SS 3.0.1 release-archive detector hardening
(`docs/UE4SS-RELEASE-LISTING.txt` as reference), a 13-test adapter edge
suite, and the pre-push security scan + push record (header).

## 2. FILE LIST

217 files, ~6.6 MB. Sizes in bytes. Frozen provenance originals under `src-*/`
are byte-copies of the module-authoring state (the working copies above them
have since diverged — see Section 8).

```
REVIEW-PACKAGE.md                        57332   this file (older versions: v1 9301; v2 45056 = pushed round-4 state)
catalog/
  globals.json                          123343   106 entries v4: 98 functions + 6 Rev-2 + Key/ModifierKey input TYPE tables
  types.json                            184605   42 types / 456 methods / 20 metamethods (v4 version bump)
  SUMMARY.md                             10600   globals summary (98-build provenance + v4 update note)
  src-catalog-globals/build_catalog.py   31776   frozen generator (writes to original absolute path)
  src-catalog-globals/globals.json       74646   frozen original
  src-catalog-globals/SUMMARY.md          9865   frozen original summary
  src-catalog-types/build_types.py       36828   frozen generator
  src-catalog-types/types.json          128208   frozen original
  src-catalog-types/SUMMARY.md           11425   frozen original (FNameUserdata CORRECTION added)
compat/
  checker.py                             48743   static Lua scanner + grader (blank-status, stdlib-override, PC-only-input)
  test_checker.py                        30752   55 tests (Townfall pins + 8 TestPcOnlyInput)
  merge_catalogs.py                       7047   globals+types -> catalog-real-merged.json
  catalog-real-merged.json              315438   599 merged entries v4 (599/599 blank status)
  sample_mod.lua                          4500   test mod (25 APIs)
  sample_catalog_TEST_FIXTURE.json        6157   renamed+wrapped TEST FIXTURE (invented statuses)
  sample-mod-real-catalog-report.json    34018   checker JSON report, sample mod vs real catalog (pre-v4 run)
  townfall-item-spawner-grade.json       68966   THE required grade artifact (verdict + full scan, v4)
  townfall-item-spawner-grade.md          9630   human summary (partly-compatible: PC-only input)
  townfall-item-spawner-mod/Scripts/main.lua  21415   staged mod source (read-only copy)
  real-catalog-run.md                    11815   attribution audit + real-catalog checker runs
  README.md                                522   pointer to docs/CONTRACT-PROPOSAL.md Section 4
  src-compat-checker/  (frozen originals: checker.py 41155, test_checker.py 17231,
      CONTRACT-PROPOSAL.md 7876, demo-report.json 20637, sample_catalog.json 4583,
      sample_mod.lua 4500)
conformance/
  conformance_static.lua                37578   353-call static suite
  conformance_live.lua                  52368   live suite (Rev 2: 86 CALL + 6 PREQ rows)
  diff_results.py                       10496   log differ
  README.md                             11247   spec + host-test results
  sample_a_vs_sample_b.diff.json       162993   diff artifact
  src-conformance-static/  (conformance_static.lua 37578, diff_results.py 10496,
      hosttest_mock_env.lua 14010, README.md 11247, bin/lua 298384 [Lua 5.4.7 arm64],
      samples/: sample_a.log 22857, sample_b.log 22784, sample_b_full.log 22868,
      sample_a_vs_sample_b.diff.json 162919, sample_a_vs_sample_b_full.diff.json 162902)
  src-conformance-live/  (conformance_live.lua 32267, README.md 9722)
docs/
  CONTRACT-PROPOSAL.md                  16430   merged v1.1 (+ keybind-mapping asks §6.4/§8.8)
  COVERAGE-MAP.md                      129554   580 catalog rows (v3 snapshot) -> coverage
  coverage.json                        193846   machine-readable companion (580 rows)
  REQUIREMENTS-TRACE.md                 13903   15 spec rows -> code -> tests -> result
  CLEAN-RUN-REPORT.md                   12842   fresh-copy verification + proposal merge + audit
  COUNTS.md                            11620   THE authoritative count sheet (recounts v3+v4)
  ANSWERS.md                           13220   blockers #22/#23, Key.* constants, Townfall test, Q4 for GPT
  SECURITY-SCAN.md                      7733   pre-push security scan (verdict CLEAN)
  UE4SS-RELEASE-LISTING.txt             3630   official UE4SS v3.0.1 zip listing (URL+sha256; detector reference)
  CHANGES-ROUND4.md                     6057   round-4 change summary (242 -> 267 arithmetic)
installer/
  installer.py                          82794   transactional install engine (+case-conflict, mods.txt wiring)
  adapter.py                            50189   InstallPipeline: detect->compat->conflicts->install->verify
  detector.py                           42546   9-type classifier (+ue4ss-runtime, release layouts)
  uninstall.py                          26164   journalled uninstall + rollback
  conflicts.py                          17884   path/keybind/hook/dependency conflicts
  test_installer.py                     51786   55 tests
  test_config_outputs.py                22413   11 tests (four config outputs, mods.txt+enabled flags)
  test_detector.py                      36356   50 tests (ue4ss-runtime, release layouts, /games/ URLs)
  test_uninstall.py                     13583   11 tests
  test_conflicts.py                      8734   9 tests
  test_case_sensitivity.py              20790   23 tests (case conflicts + APFS volume)
  test_zip_safety.py                    21597   21 tests (duplicates/bombs/UE4SS-as-mod)
  test_adapter.py                       26175   19 end-to-end pipeline tests (closes #22/#23)
  test_adapter_edge.py                  25875   13 edge tests (round 4)
  src-installer-core/  (installer.py 55747, test_installer.py 40720)
  src-mod-types/       (detector.py 32441, test_detector.py 17044)
  src-uninstall/       (uninstall.py 22514, conflicts.py 17884, test_uninstall.py 13345,
                        test_conflicts.py 8496)
tests/
  REVIEW.md                             18263   tester report, verbatim (paths localized)
  results/*.json                          13 files, 407–3504 bytes each: the tester's final-run
      JSONs (suite_installer 27, suite_detector 19, suite_uninstall 19, suite_compat 20,
      integration_tests 3, 8 static scans)
  src-tester/  (frozen harness: framework.py 16282, lua_syntax.py 22970, corpus.py 11370,
      suite_installer.py 24087, suite_detector.py 11011, suite_uninstall.py 16824,
      suite_compat.py 10228, integration_tests.py 12982, adapters.py 4462,
      review_static.py 4622, poll.py 2232, module_state.json 735, REVIEW.md 18160,
      runs/ [mock_a.log 22857, mock_b.log 22868, hostile_mock.lua 1496, synchk.lua 103,
      mock_a_vs_mock_b.diff.json 162863], results/ [same 13 JSONs],
      corpus/ [65 adversarial fixtures: 16 detector zips incl. many_files.zip 1128894,
      20 installer zips incl. bomb.zip 108872 + many_files.zip 1128894 + 16 hostile config
      JSONs, 19 hostile Lua files, 12 uninstall manifests incl. huge_list.json 58915])
test-results/
  raw-test-output.txt                   29007   DEFINITIVE FINAL clean-run raw output (267 tests)
```

## 3. HOW TO RUN EVERY TEST

All Mac-only. Working directory shown for each command.

```bash
# 3.1 Installer side: all 9 suites in one command (212 tests — the
#     definitive-run command, raw-test-output.txt Section 1)
cd /WORKSPACE/Projects/ps5-installer-compat/installer
python3 -m unittest test_installer test_config_outputs test_zip_safety \
        test_case_sensitivity test_detector test_uninstall test_conflicts \
        test_adapter test_adapter_edge

# 3.2 Case-sensitivity suite (23 tests; the hdiutil APFS-volume test
#     auto-skips with a stated reason if the image cannot attach)
cd /WORKSPACE/Projects/ps5-installer-compat/installer
python3 -m unittest test_case_sensitivity

# 3.3 Zip-safety suite (21 tests: duplicates, bombs, backslash, UE4SS-as-mod)
cd /WORKSPACE/Projects/ps5-installer-compat/installer
python3 -m unittest test_zip_safety

# 3.4 Compatibility checker (55 tests, incl. the TownfallItemSpawner grade
#     pins and the 8 TestPcOnlyInput tests from the v4 input-table work)
cd /WORKSPACE/Projects/ps5-installer-compat/compat
python3 -m unittest test_checker

# 3.5 Conformance static suite under the host mock (variants A and B)
cd /WORKSPACE/Projects/ps5-installer-compat/conformance/src-conformance-static
./bin/lua hosttest_mock_env.lua conformance_static.lua
MOCK_VARIANT=b ./bin/lua hosttest_mock_env.lua conformance_static.lua

# 3.6 Conformance diff tool on the committed sample logs
cd /WORKSPACE/Projects/ps5-installer-compat/conformance/src-conformance-static
python3 diff_results.py samples/sample_a.log samples/sample_b.log --verbose

# 3.7 Rebuild the merged real catalog, then checker runs against it
cd /WORKSPACE/Projects/ps5-installer-compat
python3 compat/merge_catalogs.py
python3 compat/checker.py compat/sample_mod.lua compat/catalog-real-merged.json \
        --json /tmp/sample-mod-report.json
python3 compat/checker.py compat/townfall-item-spawner-mod compat/catalog-real-merged.json
# ^ re-runs the required TownfallItemSpawner grade (Section 5)

# 3.8 Tester's frozen adversarial suites (NOT runnable as-is from here —
#     the frozen harness hard-codes its original /tmp/sideproject staging
#     paths; port the path constants at the top of tests/src-tester/
#     framework.py first. Authoritative saved results: tests/results/*.json)
cd /WORKSPACE/Projects/ps5-installer-compat/tests/src-tester   # caveat applies
python3 framework.py suite_installer.py suite_detector.py \
        suite_uninstall.py suite_compat.py integration_tests.py
```

## 4. FULL TEST RESULTS

Three evidence layers, each labeled with what state it covers:

### 4.1 Module-authored unit suites — DEFINITIVE FINAL clean-copy run, 2026-10-07 (all green)

One agent, fully sequential, from a fresh `cp -R` of the whole tree to
`/tmp/final-run-v2/` (216/216 files, no prior use, no concurrent runs). Raw
unfiltered output in **`test-results/raw-test-output.txt`** (REPLACED by this
final run — it supersedes the 15:14 242-test run and every earlier
concurrent-run total); the same per-suite numbers are in `docs/COUNTS.md`
Section 5, the round-4 arithmetic in `docs/CHANGES-ROUND4.md`. Re-verified
live on the current tree by this review pass (installer-side
`Ran 212 tests … OK`, checker `Ran 55 tests … OK`).

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
| **total** | **267** | **267 PASS / 0 FAIL** |

(The case-sensitivity volume test attaches a real case-sensitive APFS image and
re-runs the full installer-side suite on it; in one earlier run the attach
failed and the test skipped by design — in the definitive run it attached and
passed. `test_installer` emits 6 cosmetic `ResourceWarning: unclosed file`
notices, preserved verbatim in the raw log. The independent tester's suites
under `tests/src-tester/` are NOT in this total — hardcoded `/tmp/sideproject`
paths, noted and skipped; see Section 4.2.)

**Totals history (labeled, nothing silently merged):** 196 (round-2 tree) →
242 (the 15:14 clean run, before the last round-4 test additions) → **267**
(final run = 242 + 25: +8 `TestPcOnlyInput` checker tests, +3 detector
release-layout tests, +1 installer release-bundling test, +13 adapter edge
tests). The same final run also re-executed the conformance static mocks, the
diff tool and a fresh TownfallItemSpawner grade against the v4 merged catalog
(Sections 3–5 of the raw file).

### 4.2 Independent tester adversarial suites — frozen results, 14:05 snapshot

From `tests/results/*.json` (verbatim copies in `tests/src-tester/results/`):

| suite | pass | fail |
|---|---|---|
| suite_installer | 27 | 0 |
| suite_detector | 18 | 1 — `det.requirements_adversarial_text` (Nexus `/games/<title>/` URL gap, finding #6) |
| suite_uninstall (+conflicts) | 19 | 0 |
| suite_compat | 20 | 0 |
| integration_tests (L1/L2/L3) | 3 | 0 |
| **total** | **87** | **1** |

The one failing behavior was re-verified against the FINAL detector on
2026-10-07: `parse_requirements` still extracts no dependency from
`nexusmods.com/games/cyberpunk2077/mods/12345` while `nexusmods.com/<game>/mods/12345`
works — the gap remained open at 14:05. **Round 3 later FIXED it (finding #6,
Section 8)**: `NEXUS_ID_RES` in `detector.py` now handles the `/games/<title>/`
layout, pinned green by `test_nexus_url_with_games_path_prefix` in the
definitive run. The tester's frozen suites themselves were not re-executed
(hard-coded staging paths, Section 3.8); their module-level findings were
instead re-checked by direct code reads and probes (Section 8).

### 4.3 Clean-run verification — definitive FINAL fresh-copy run (REPLACES earlier snapshots)

**`test-results/raw-test-output.txt` now holds the DEFINITIVE FINAL clean-run
output** (Section 4.1): the full `cp -R` tree copied to `/tmp/final-run-v2/`
(216/216 files) and every suite re-run sequentially by a single agent —
**267/267 PASS** (combined installer-side `Ran 212 tests` plus per-suite
`Ran N` lines), followed by the conformance static mock runs, the diff tool on
the sample logs, and a fresh TownfallItemSpawner grade against the v4 merged
catalog (`partly-compatible - PC-only input dependency`, 66 calls / 32
matched / 0 unknown); stdout and stderr merged with nothing filtered.
History of superseded snapshots (both proved location independence): the
14:16:33 run to `/tmp/clean-run-test` (144-test intermediate state; report
kept in `docs/CLEAN-RUN-REPORT.md`) and the 15:14 run to
`/tmp/final-clean-run/` (242/242). Post-run greps prove no test depends on
paths outside the project (only frozen `src-*` provenance artifacts and
checker-output provenance fields contain absolute paths).

### 4.4 Conformance host/mock results

| run | result |
|---|---|
| conformance_static, mock variant A | 353 calls, **353 ok / 0 fail** (re-run on final tree) |
| conformance_static, mock variant B (deliberate value changes) | 353 calls, 352 ok / 1 fail (expected, by design) |
| hostile missing-global probes (tester, 17 globals deleted one at a time) | **17/17 survived** — all 353 calls always emitted, zero crashes |
| fixed-format log claim | verified byte-exactly: 353 CALL rows, exactly 6 tab-separated fields, malformed = 0 |
| conformance_live (Rev 2), mock harness 1 (full stubs, in-game) | 86 CALL rows / 6 PREQ rows: **82 PASS / 1 FAIL / 3 SKIP** (the FAIL is the intentional `string.toFName` absence probe; the SKIPs are LoopAsync + prerequisite gates) |
| conformance_live, mock harness 2 (degraded) | 27 PASS / 7 FAIL / 18 SKIP — the 7 FAILs are exactly the absent-API probes |
| diff_results.py: sample_a vs sample_a | 353 MATCH, exit 0 |
| diff_results.py: sample_a vs sample_b | **347 MATCH / 4 MISMATCH / 2 MISSING / 1 EXTRA** (354 keys), exit 1 (re-run on final tree) |
| diff_results.py: sample_a vs sample_b_full | 3 MISMATCH, exit 1 |
| tester's Lua 5.3 validator self-test | 46 valid + 38 invalid cases, 0 failures; 18/19 agree with real Lua 5.4.7 (shebang fixed) |

**Mock caveat (required honesty statement):** pass under mock only proves the
scripts run against our own stand-ins, not against the real runtime —
`ps5_runtime_status` is blank in 106/106 globals and 476/476 type rows
(599/599 merged entries), so no catalog entry has been proven against a real
PS5 UE4SS runtime (Section 6).

### 4.5 Real-catalog checker runs

| run | grade |
|---|---|
| sample_mod.lua vs catalog-real-merged.json | **UNKNOWN** — "runtime status not filled (16 cataloged API(s) … empty ps5_runtime_status) and 9 API(s) not in the catalog at all" (report: `compat/sample-mod-real-catalog-report.json`, a pre-v4 run; the sample mod uses no `Key.*` constants, so the v4 PC-only-input category does not change its outcome) |
| TownfallItemSpawner vs catalog-real-merged.json (v4) | **PARTLY-COMPATIBLE** — "PC-only input dependency: keyboard keybinds … need runtime/config controller mapping on PS5" — see Section 5 |

## 5. TOWNSFALL ITEMSPAWNER GRADE (the required test result)

Artifacts: `compat/townfall-item-spawner-grade.json` (verdict + provenance +
FULL checker scan JSON), `compat/townfall-item-spawner-grade.md` (summary),
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
  fixture (excluded). `Mods/shared/UEHelpers` ships outside the mod dir and is
  out of scope.
- **Catalog used:** the two real catalog files merged by
  `compat/merge_catalogs.py` into `compat/catalog-real-merged.json` (v4) — 599
  entries = 103 globals (106 passthrough − 3 fused string-ctor globals) + 42
  type entries + 454 method entries (456 raw − 2 collapsed override dupes;
  metamethods never flattened) — full arithmetic in `docs/COUNTS.md` —
  **599/599 with empty `ps5_runtime_status`** (the `Key.*` classification
  comes from the entries' `pc_only_input: true` flag, not a filled status).
- **Regenerated artifact numbers (v4, on disk):** total 66 calls | 32
  catalog-matched | 0 unknown | 34 unresolved (+85 lua-builtins).
  **PC-ONLY INPUT — the six `Key.*` keybind constants, all central**
  (top-level `RegisterKeyBind` args): `Key.F7` main.lua:694 (menu toggle),
  `Key.UP_ARROW`/`Key.DOWN_ARROW` :698/:703 (navigate), `Key.RETURN` :708
  (confirm/spawn), `Key.LEFT_ARROW`/`Key.RIGHT_ARROW` :713/:722 (category
  cycle). **9 cataloged-but-blank APIs** (RegisterKeyBind x6 [central, the
  consumer of the Key.* constants], ExecuteInGameThread x6 [central],
  StaticFindObject x4 [central], FText x2 [central], StaticConstructObject x2,
  FName x2, FindFirstOf x2, LoadAsset x1, print x1 — `print` surfaced when
  the merged catalog was rebuilt past its stale v2 build). **Not in catalog:
  none** (the six `Key.*` constants were the previous "unknown" entries).
- **How the grade evolved:** the first grade was
  `unknown - runtime status not filled` (591 blank statuses, 6 `Key.*`
  constants "not in catalog"). Claude's review reframed the Key uses as a
  **PC-only keyboard dependency** rather than a minor catalog gap: the
  constants exist and are readable (pure Windows VK integers,
  `needs_live_game: false`), but the PS5 has no keyboard — every one of these
  keybinds **needs runtime/config controller mapping** (current approach:
  `PS5Pad.ini` chords, e.g. L3+R3 = F7). With the v4 `Key`/`ModifierKey`
  TYPE entries in the catalog (`Key` 165 constants, `ModifierKey` 3 active:
  SHIFT/CONTROL/ALT; port-verified against `register_input_globals`,
  LuaMod.cpp:446-627, upstream cross-checked at UE4SS LuaMod.cpp:438-618) and
  the checker's new PC-ONLY INPUT category (`bucket_for_entry`, distinct from
  `missing` and from `status-blank`; type-ref resolution now reports the
  specific constant, e.g. `Key.F7`), the six constants resolve, are flagged,
  and a confirmed difference outranks unverifiability — hence
  `partly-compatible`. This grade is pinned automatically by
  `TestTownfallItemSpawnerGrade::test_townfall_grade_partly_compatible_pc_only_input`
  and `test_townfall_six_key_constants_are_pc_only_input` in
  `compat/test_checker.py` (superseding the two pre-v4 pin tests).
- **Honest reading:** PC-only input alone never forces `unsupported` (that
  stays reserved for a central *missing* API); the 9 blank-status APIs —
  including `RegisterKeyBind` itself, the consumer of these constants —
  remain unverifiable, so this grade is neither a pass nor a fail for them.
  A genuine `missing`/`behaves-differently` status still outranks blanks
  (never masked).
- **To grade for real:** (1) runtime/config side — provide controller
  mapping for the six keybinds (PS5Pad.ini chords such as L3+R3 = F7 is the
  current approach; see open question 9 / `docs/ANSWERS.md` Q4);
  (2) catalog side — fill `ps5_runtime_status` for at least the 9 blank APIs
  above; then re-run Section 3.7's last command.

## 6. CONFORMANCE COVERAGE

Source: `docs/COVERAGE-MAP.md` (580 rows, v3 snapshot) + machine companion
`docs/coverage.json`. Every catalog entry mapped to the conformance script
that calls it, or the reason it is not called (`invoked` vs `presence-only`
vs `not-covered`, with per-row evidence).

| scope | rows | invoked | presence-only | any CALL row | not covered |
|---|---|---|---|---|---|
| globals (v3: 98 + 6 Rev-2) | 104 | 68 | 13 | 81 | 23 |
| type methods (456) + metamethods (20) | 476 | 80 | 0 | 80 | 396 |
| **total (v3 snapshot)** | **580** | **148 (25.5%)** | **13** | **161 (27.8%)** | **419** |

(104 + 476 = 580 snapshot rows, per `docs/COUNTS.md` rows 12–14. The two v4
`Key`/`ModifierKey` TYPE containers are not function/method rows and are
outside the coverage convention — the convention total is now 582 API rows,
`docs/COUNTS.md` row 6; the coverage map/JSON were not recounted for v4.)

- **Call-count reconciliation:** static 76 literal sites loop-expand to 353
  CALL rows; live (Rev 2) 86 CALL rows + 6 PREQ rows; total 439 — matches
  every claim made in this package.
- **The mock caveat (verbatim from the coverage map):** *"Pass under mock only
  proves the scripts run against our own stand-ins, not against the real
  runtime."* Mock agreement is evidence the scripts are syntactically valid,
  crash-safe and log-format-correct; it is NOT evidence of runtime conformance.
- **Name mismatches (RESOLVED by conformance Rev 2):** the original live
  suite's TArray battery, `UClass:GetFunction` and `Loop.*` layer did not
  match the catalog — Rev 2 re-derived them from the port source:
  `UClass:GetFunction` does not exist in this port and the `Loop` table does
  not exist (scheduling is plain globals); the live suite now calls the real
  members (`ForEachFunction`/`GetSuperStruct`/`IsChildOf`/`GetCDO`, the
  scheduling globals with register/cancel cycles) and probes
  `string.toFName` as a labeled absence probe. Mocks still cannot catch this
  defect class — the fix came from source reading, not from a console.
- **Follow-up flags (Rev-2 closures + remainder):** the 6 formerly-uncataloged
  globals were added in v3 (`EGameThreadMethod`, `EngineTickAvailable`,
  `ProcessEventAvailable`, `ue4ss_loaded_modules`, `__OriginalReturnValue`,
  `NAME_None`, all with citations in `upstream_reference`); the live suite now
  exercises the delayed-action/scheduling globals (`LoopAsync` intentionally
  SKIP-noted as uncancelizable); remaining known gaps: `string.toFName`
  (probed absence), FString/FText `__tostring`, FString `__concat`. The
  `Key`/`ModifierKey` tables are now cataloged (v4) — the coverage map
  predates them and was not recounted.

## 7. REQUIREMENTS TRACEABILITY

Full table: `docs/REQUIREMENTS-TRACE.md` (15 rows; implementation file → test
file → test names → result; module suites were re-verified fresh when it was
written). Condensed here with the final-tree updates:

| # | Spec item | Result |
|---|---|---|
| 1 | Input: archive + Nexus metadata + title ID | **PASS** (54-test installer suite; tester 27/27) |
| 2 | Output: install layout (loadlist, mods list, enabled flags, ploader.ini) | **PASS (round 3)** — all four config outputs written and restored (11-test `test_config_outputs.py`); mods list = `mods.txt` rows `Name : 1/0` or an `enabled.txt` marker |
| 3 | NEVER-EDIT rule: byte-for-byte + manifest + verify | **PASS** (sha256 verify, CRLF preserved, foreign rows preserved, overrides-not-settings rule) |
| 4 | Mod-type detection (Lua / C++ DLL / pak-IoStore / LogicMods / other) | **PASS** — 9 types incl. the new `ue4ss-runtime`; open note: `.lua` content never sniffed (finding #7) |
| 5 | Requirements: UE4SS version + other mods | **PASS** — the Nexus `/games/<title>/` URL gap (finding #6) was fixed in round 3 (`NEXUS_ID_RES`; `test_nexus_url_with_games_path_prefix` green in the definitive run) |
| 6 | Uninstall + rollback | **PASS** (11/11 + tester 19/19 + INT-L1/L3) — the shipped `InstallPipeline` adapter now bridges the record contracts (finding #22 fixed) |
| 7 | Conflict detection | **PASS** (9/9 + INT-L2) — wired into the shipped pipeline (finding #23 fixed at pipeline level; the raw `Installer.install()` path itself still does not gate); level-N comment parsing bug (finding #10) |
| 8 | Archives: zip supported, 7z/rar report needed | **PASS** (magic sniff, conversion guidance, tar.gz/xz/bz2 also supported) |
| 9 | Install/verify/uninstall round trips | **PASS** (crash injection + recovery verified; INT results from tester JSONs) |
| 10 | Tampered file detected | **PASS** (verify exit 3; uninstall confirm flow; INT-L3) |
| 11 | Each mod type classified correctly | **PASS** (12 detection tests, all types) |
| 12 | Two mods writing the same path | **PASS** — conflicts module + `InstallPipeline` pre-install conflict gate (round 3, #23); the raw installer write path alone still performs no conflict check |
| 13 | Corrupt archive | **PASS** (truncated/empty/not-a-zip/encrypted/bomb all rejected; tester corpus) |
| 14 | Conformance diff tool on two sample logs | **PASS** — artifacts + reproductions (no unit test pins exit codes yet; noted follow-up) |
| 15 | Compatibility grade for TownfallItemSpawner | **DELIVERED** — `compat/townfall-item-spawner-grade.json` (v4): grade **partly-compatible**, reason "PC-only input dependency: keyboard keybinds … need runtime/config controller mapping on PS5" (Section 5). Grading beyond that requires GPT's console fill-in of the 9 blank-status APIs plus the runtime's controller-mapping decision (open question 9) — that is the honest ceiling today. |

Cross-cutting caveats the table must be read with (from the trace file):
mock-only conformance; no locking; everything host-only. (The trace file's
own rows predate the round-3 fixes — findings #22/#23 and the Nexus URL gap
are closed on the current tree; this table's Result column is the
current-tree status.)

## 8. REVIEWER FINDINGS (every finding, classification, resolution)

Source: the independent tester's report `tests/REVIEW.md` (window 2026-10-07
~13:38–14:06; verbatim frozen copy `tests/src-tester/REVIEW.md`; per-test
detail `tests/results/*.json`; final module digests
`tests/src-tester/module_state.json`). Classes: BLOCKER / BUG / DESIGN
CONCERN / EDGE CASE / SUGGESTION. Statuses below are re-verified against the
FINAL tree (code reads + direct probes, 2026-10-07 ~14:20–14:27).

| # | Module | Finding | Class | Status (final tree) |
|---|---|---|---|---|
| 1 | installer-core | `NameError: config_writes` dead reference — hit on EVERY first install (traceback, exit 1) | BLOCKER | **FIXED during review** — 0 occurrences; writes via `add_write` (installer.py:1254); pinned by `inst.happy.fresh_install_must_succeed` |
| 2 | installer-core | `need_dir()` never mkdir-planned leaf mod dirs (first write died ENOENT) | BLOCKER | **FIXED during review** — rewritten ancestor-chain `need_dir` (now installer.py:1159); pinned by tests |
| 3 | installer-core | No locking: concurrent different-mod installs mutually destruct 12/12 (exits 4+3, both mods gone, loadlist removed); detected, never silent | DESIGN CONCERN | **OPEN — accepted risk** (0 flock/fcntl in installer.py/uninstall.py); lock file per title root is future work; pinned by `inst.concurrent.*` |
| 4 | installer-core | Duplicate zip members normalizing to the same path not rejected at inspection (landed in verify exit 3 + rollback) | EDGE CASE | **FIXED 2026-10-07** — duplicate detection at inspect (installer.py:494–501); pinned by `test_zip_safety.TestDuplicateEntries` (5 tests: exact, `./`, backslash, directory, case-variant) |
| 5 | installer-core | `verify` trusts absolute `install_path`s in manifests; moving the roots dir invalidates every install | DESIGN CONCERN | **OPEN — accepted** (relative-path records future work) |
| 6 | mod-types | Nexus URL regex misses `/games/<title>/` segments (`nexusmods.com/games/cyberpunk2077/mods/12345` → no dependency) | BUG | **FIXED (round 3)** — `NEXUS_ID_RES` (detector.py:149–160) now handles `/games/<title>/mods/<id>`, locale prefixes and hyphenated slugs (the literal `/mods/` stop keeps slugs out of the id); pinned green by `test_nexus_url_with_games_path_prefix` in the definitive run |
| 7 | mod-types | `.lua` content never sniffed: a PE binary renamed `main.lua` classifies `ue4ss-lua` | BUG | **OPEN** — detector.py:615–623 still marker-scans text only; recommended MZ/ELF first-bytes check pending |
| 8 | mod-types | Scan truncation at MAX_ENTRIES (5000) noted in `notes` only, not `flags` | EDGE CASE | **OPEN** |
| 9 | mod-types | Static marker detection bypassable by construction (`_G["Find".."FirstOf"]`) | DESIGN CONCERN | **OPEN BY DESIGN** — shared documented blind spot with the checker |
| 10 | uninstall/conflicts | `strip_lua_comments` mishandles `--[==[...]==]` level-N comments → false-positive KEYBIND_CONFLICT warnings | BUG | **OPEN** — conflicts.py unchanged (17884 bytes, byte-identical to frozen original); recommended sharing the checker's level-aware `strip_lua` |
| 11 | uninstall | File moved to trash BEFORE the journal records it — crash window leaves a file in trash, silently skipped by rollback | DESIGN CONCERN | **OPEN** |
| 12 | uninstall | Install-state read-modify-write unlocked; concurrent uninstalls can lose a state entry | DESIGN CONCERN | **OPEN — accepted** (same class as #3) |
| 13 | uninstall | Dependency cycles (A↔B) deadlock uninstalls by design (both refuse `ModStillRequiredError`) | DESIGN CONCERN | **OPEN — accepted behavior** (safe refusal, no corruption; `un.circular_dependency_deadlock` pins it) |
| 14 | compat-checker | String-indexed calls invisible: `_G["LoadAsset"]` grades `unknown` with `total_calls: 0` instead of flagging dynamic-access risk | BUG | **OPEN** — no dynamic-access-risk flag in checker.py (grep: 0 hits); documented by `compat.obfuscated_indexed_calls` |
| 15 | compat-checker | Lua syntax errors indistinguishable from empty mods (both `unknown`, 0 calls) | EDGE CASE | **OPEN** — no separate "doesn't parse" outcome; the bundled Lua 5.4.7 / tester's `lua_syntax.py` could supply it |
| 16 | conformance-static | 13:47 version: "every call wrapped in pcall" was FALSE (36/109 raw sites) + broken always-truthy `and/or` guard | BLOCKER | **FIXED during review** — `run_call` wraps in pcall (conformance_static.lua:177), guard gone; 17/17 hostile probes survived; log format verified byte-exactly |
| 17 | conformance-static | `rtype` column not sanitized (tab could theoretically break TSV) | EDGE CASE | **OPEN — low risk** |
| 18 | conformance-live | Byte-based truncate can split a UTF-8 rune (cosmetic); everything else clean | SUGGESTION | **OPEN — cosmetic** |
| 19 | catalogs | Provenance unverifiable: catalogs cite a UE4SS tree "not on this host" | DESIGN CONCERN | **RESOLVED 2026-10-07** — the tree DOES exist (worktree `.superpowers/native-dependency-spike/…`); every file:line ref verified programmatically to resolve in the PORT tree (523/523 types, 238/238 globals, spot-checked correct); attribution note added to 140/140 entries; the tester's subsidiary claim "port renames FName→FNameUserdata" was FALSE (upstream's own convention — corrected in `catalog/src-catalog-types/SUMMARY.md` with upstream evidence). Residual (documented, not applied): citation line numbers are PORT-tree numbers; upstream re-derivation is the proposed remediation (Section 13) |
| 20 | catalogs | `ps5_runtime_status` `""` in 98/98 → every cataloged call lands `unknown-status`, a clean mod mis-grades `partly-compatible` | DESIGN CONCERN | **HALF-FIXED** — checker now buckets blank statuses separately (`status-blank`) and grades `unknown` with reason "runtime status not filled" (+7 tests, `TestBlankRuntimeStatus`); the mis-grade is gone. The fill-in itself awaits GPT console testing — open by design |
| 21 | compat sample catalog | Sample catalog drift: 10 globals + 3 methods absent from the real catalogs | BUG | **FIXED 2026-10-07** — renamed `sample_catalog_TEST_FIXTURE.json` + wrapper stating statuses are INVENTED for bucket testing; real data is `catalog-real-merged.json`; tests updated; frozen original left untouched |
| 22 | integration | I-1: installer manifest vs uninstaller/conflicts contracts differ; no shipped adapter (only the tester's ~40-line `adapt_install_to_uninstall`) | BLOCKER (system) | **FIXED** — `installer/adapter.py` (InstallPipeline) ships the manifest-to-uninstall converter + unified install_state.json; 19 end-to-end adapter tests pass |
| 23 | integration | I-2: conflicts.py never wired into the install path; installer will write colliding files | BLOCKER (system) | **FIXED (round 3, pipeline level)** — `InstallPipeline` calls `conflicts.check_install` (adapter.py:648) and refuses on blockers BEFORE any write; 19 adapter + 13 edge tests. Residual: the raw `Installer.install()` used directly still performs no path/keybind/hook conflict check — integrators must go through the pipeline (the separate case-conflict wiring remains in installer.py itself) |

**Tally:** 23 findings — 10 FIXED (#1, #2, #4, #6, #16, #19, #21, #22, #23,
and #20's checker half), 13 OPEN (of which #9/#13/#20-fill-in are open by
design and #3/#5/#12 accepted risks; no system BLOCKERs remain open).

### Fixes and hardening added AFTER the tester's window (verified by the new suites)

- F1 **Case-conflict detection** (`installer.py` + `test_case_sensitivity.py`,
  23 tests): in-code `detect_caseConflicts` (whole-path + directory variants;
  backslash inputs normalized), wired into install within one mod AND across
  installed mods' manifests (`CaseConflictError` before anything is written);
  exact-case `Scripts`/`Mods`/`Config` gate; runtime folder-case mismatches
  REPORTED (`mod_root_case_variants`), never renamed; and a real
  case-sensitive APFS volume (hdiutil) test that proves the volume's
  case-sensitivity in code and re-runs the full installer-side suite on it.
- F2 **Zip safety** (`test_zip_safety.py`, 21 tests): duplicate members
  rejected at inspect (#4); Windows backslash paths normalized and
  backslash-traversal/drive letters rejected; nested zips stored opaque,
  declared-size bomb guards (member + total) verified against a patched
  central directory; real-lua+fake-dll installs byte-identical with detector
  warning.
- F3 **UE4SS-as-mod** (`detector.py` + tests + installer refusal): a UE4SS
  runtime zip (UE4SS.dll/dwmapi.dll at root, no Mods/ tree) classifies
  `ue4ss-runtime`, `runnable_on_ps5: "not-applicable"`, and the installer
  refuses it with "provided by the PS5 runtime layer; do not install as a
  mod"; +7 detector tests; case-insensitive name variants and full-runtime-
  with-mods-tree (installs the mod only) covered.
- F4 **Blank-status grading** (#20 checker half) and **F5 cataloged stdlib
  overrides** (`print`/`dofile`/`loadfile` are graded against the catalog
  instead of skipped as builtins; regression test added) — compat suite 35→43,
  now 47 (+4 `TestTownfallItemSpawnerGrade` tests pinning the real-mod grade
  automatically, `compat/test_checker.py`).
- F6 **Catalog attribution audit + merge tool** (#19/#21) and **F7 the merged
  contract proposal v1.1** (Section 11) — `docs/CLEAN-RUN-REPORT.md`.

Round 3-4 hardening (after the definitive run, verified by the suites above):

- F8 **UE4SS-release detector hardening** (round 4, `detector.py` + tests +
  installer refusal): flat and nested full UE4SS 3.0.1 release archives
  (UE4SS.dll + dwmapi.dll + default mods, with or without a `ue4ss/`
  wrapper) classify `ue4ss-runtime` and are refused; a real user mod inside a
  UE4SS-release subfolder installs while skipping the runtime files (+3
  detector tests, +1 installer test). The official release zip's full listing
  (public URL + sha256) is pinned as `docs/UE4SS-RELEASE-LISTING.txt` — the
  Mac-only detection reference.
- F9 **Adapter edge suite** (round 4, `installer/test_adapter_edge.py`, 13
  tests): empty archive, missing/occupied game root, corrupted/wrong-schema/
  missing manifests, non-zip/7z inputs, shared ploader lines, full
  install→verify→check→uninstall round trip, and doc-match tests for the
  HOWTO schema and grade vocabulary.
- F10 **Key/ModifierKey catalog v4 + PC-only-input checker category**
  (round 4): `Key` (165 constants) and `ModifierKey` (3 active) TYPE entries
  in `catalog/globals.json` with `pc_only_input: true`, blank
  `ps5_runtime_status`, `needs_live_game: false`; checker `bucket_for_entry`
  buckets them as **pc-only-input** (winning over an EMPTY status, distinct
  from `missing` and `status-blank`), reports the specific constant
  (`Key.F7`), emits a `pc_only_input` JSON array + `PC-ONLY INPUT` text
  section with the "needs runtime/config controller mapping on PS5"
  suggestion, and grades `partly-compatible` with a dedicated reason (+8
  `TestPcOnlyInput` tests; TownfallItemSpawner re-graded — Section 5).

### Tester's positive verification (balance)

Journal-first crash recovery (crash injection → full rollback → clean
reinstall), ownership guards, traversal/symlink/encrypted/10k-entry/BOM-CRLF
rejection, unicode + NFC/NFD determinism, magic-byte sniffing, zip-in-zip
depth 4, tar device-entry safety, semver cue-word discipline, byte-exact
multiset config-line removal, full uninstall rollback with double-rollback
refusal, checker grading ladder end-to-end, hostile-Lua crash-safety, syntax
validity per the tester's parser AND real Lua 5.4.7. Static danger-scan hits
in detector.py/conflicts.py were doc-string mentions only — no
network/shell/eval/console-contact patterns in any module.

## 9. KNOWN LIMITS

1. **Compatibility grading is still mostly unverifiable against the real
   catalog** — all 599 merged entries have blank `ps5_runtime_status`; the one
   graded PS5 difference so far is the PC-only input class (`Key.*`/
   `ModifierKey.*` keyboard dependencies → TownfallItemSpawner
   `partly-compatible`); everything else stays `unknown` until GPT fills
   statuses.
2. **Conformance is mock-only** — 161/580 v3-snapshot rows (27.8%) are
   covered, 148 invoked (25.5%); mock passes prove the scripts run, NOT
   runtime conformance (the Rev-2 name-mismatch fixes were derived from the
   port source, not from a console). Neither conformance script has run in a
   real game session.
3. **Case-sensitivity (host limitation, mitigated in code):** the Mac disk is
   case-insensitive; detection is in-code (`detect_caseConflicts`, exact-case
   `Scripts`/`Mods`/`Config` gate, across-mods manifest checks), and the APFS
   volume test can SKIP when hdiutil cannot attach (observed once; it
   attached and passed on the final tree). Arbitrary runtime file lookups
   beyond the gated dir list are not case-validated.
4. **Concurrency:** no locking anywhere — concurrent installs mutually destruct
   (detected, fail-closed, never silent); concurrent uninstalls can lose a
   state entry. Acceptable for single-user serialized use only.
5. **Pipeline exists; the raw APIs do not gate (findings #22/#23, fixed at
   pipeline level):** `installer/adapter.py` `InstallPipeline` converts
   installer manifests → unified uninstaller/conflicts state and calls
   `conflicts.check_install` before any write — but the raw
   `Installer.install()`/`Uninstaller` classes used directly still emit
   different record shapes and perform no path/keybind/hook conflict check.
   Integrators must go through the pipeline.
6. **Static-analysis blind spots:** dynamic dispatch invisible and unflagged
   (#14); syntax errors indistinguishable from empty mods (#15); marker
   detection bypassable (#9); `.lua` content not sniffed for binaries (#7).
7. **Nexus `/games/<title>/` URL dependency gap (#6)** — FIXED in round 3
   (`NEXUS_ID_RES`; pinned green by `test_nexus_url_with_games_path_prefix`).
   Historical note: it had been the package's one pinned red test.
8. **Catalog citations are PORT-tree line numbers** (Section 13) — upstream
   re-derivation documented, not applied; both catalog files are now
   versioned wrappers (`catalog_version` `2026-10-07-v4`), and
   `compat/catalog-real-merged.json` is rebuilt on disk from v4.
9. **7z/RAR rejected** with host-side conversion guidance (no converter
   shipped); pak/IoStore mods classified `needs-conversion`, conversion
   pipeline is future work; UE4SS C++ DLL mods classified but not runnable;
   UE4SS runtime zips refused as mods (by design).
10. **Manifests record absolute `install_path`s** — moving the roots dir
    invalidates verify (#5); uninstall trash-before-journal crash window
    (#11); `strip_lua_comments` level-N bug (#10); `rtype` not sanitized
    (#17). (The former "no `mods.txt`/enabled-flags writer" gap was closed in
    round 3 — see Section 7 row 2.)
11. **Tester's frozen harness** (`tests/src-tester/`) keeps its original
    absolute staging paths — results are the saved JSONs; re-running requires
    porting path constants (Section 3.8). Frozen `src-*` build scripts
    likewise write to their original absolute output paths.

## 10. OPEN QUESTIONS

For GPT (runtime owner) and the mod manager team — the single merged list is
Section 8 of `docs/CONTRACT-PROPOSAL.md`:

1. Where do manifests live? (Proposal: `/data/modbridge/roots/<TITLE>/manifests/`.)
2. Who owns UE4SS-settings.ini? (Proposal: GPT; installer writes overrides to
   a separate file, never edits it.)
3. Load order: installer computes it, runtime enforces it — is loadlist row
   order = load order?
4. Hot-reload: if the runtime ever supports mod hot-unload, the installer
   needs a notification API.
5. Catalog versioning: who bumps `catalog_version` when GPT fills statuses,
   and does the manager auto-re-grade installed mods on a bump?
6. Phase-0 grade names: will the manager accept `unverified-likely-*` /
   `likely-*` outline badges (provisional grades must never render as verified)?
7. `compat_manifest.json`: will mod authors ship it; will the manager offer
   the submit-back flow?
8. (From this review; partially resolved in round 4) `Key`/`ModifierKey`
   are now cataloged (v4 TYPE entries, `pc_only_input: true`), so the
   Townfall `Key.*` blockers resolve. Still open: should other enum/constant
   tables be cataloged the same way, and should the port-tree citation fields
   be re-derived against upstream 3.0.1 (Section 13 remediation)?
9. (New, round 4 — also `docs/ANSWERS.md` Q4 and contract §6.4/§8.8)
   **How does the runtime map PC keyboard keybinds to PS5 controller
   inputs?** The PS5Pad.ini chord mechanism (L3+R3 = F7) is the current
   approach — is it generalizable to arbitrary `Key.*` constants and
   ModifierKey combinations, or does the runtime need a translation layer?
   And what happens when a mod registers a keybind the config doesn't map
   (today: it registers but never fires — should it warn?). This decides
   what `ps5_runtime_status` `Key`/`ModifierKey`/`RegisterKeyBind`/
   `IsKeyBindRegistered` should carry once console-verified, and whether
   keyboard-dependent mods can ever grade above `partly-compatible`.

## 11. PROPOSED CONTRACT CHANGES (merged, one document)

`docs/CONTRACT-PROPOSAL.md` **v1.1 (merged)** is the single proposal (the
former `compat/CONTRACT-PROPOSAL.md` was folded in and deleted; frozen
original kept at `compat/src-compat-checker/CONTRACT-PROPOSAL.md`;
`compat/README.md` points there). Proposal-only: nothing integrated, nothing
sent; the canonical contract is owned by the mod manager team.

- §1 component boundaries (+`ps5_runtime_status` owner row: GPT fills, never
  authors structure) · §2 five core interfaces (install / uninstall /
  compatibility-check / config entries / conformance fill-in) · §3 ONE grade
  vocabulary incl. Phase-0 provisional names · §4 the two-phase pre-download
  badge contract · §5–7 requests to manager / GPT / catalog maintainers ·
  §8 the merged open-questions list · §9 implementation checklist.
- Contradictions resolved in the merge: `lua_api_calls_detected` dropped from
  the pre-download sketch (a source scan is impossible pre-download);
  `partly-compatible` canonical spelling; one `catalog_version` wrapper field;
  one grade/badge table; one scope preamble.
- Wire schemas: install/uninstall requests + results, badge envelope
  (`schema: 1`, `confidence`, `blockers` with runtime-side workarounds),
  caching key `(nexus_mod_id, file_id, archive_sha256, catalog_version,
  checker_version)`, and the honesty rules (unknown never guessed;
  description-only never shown verified).

## 12. HOW-TO-CALL

### For GPT (runtime owner)

1. **Fill `ps5_runtime_status`** in `catalog/globals.json` (106) and
   `catalog/types.json` (42; methods inherit their type's status) after
   console testing, values: `"implemented+console-proven"` /
   `"implemented-untested"` / `"missing"` / `"behaves-differently"`. Highest
   value first: `RegisterKeyBind`, `ExecuteInGameThread`, `StaticFindObject`,
   `FText`, `StaticConstructObject`, `FName`, `FindFirstOf`, `LoadAsset`,
   `print` (the 9 blank-status APIs of the Townfall grade), plus
   `Key`/`ModifierKey`/`IsKeyBindRegistered` once the controller-mapping
   strategy is decided (open question 9). Then
   `python3 compat/merge_catalogs.py` and re-run Section 3.7 to push the
   TownfallItemSpawner grade past `partly-compatible`.
2. **Run the conformance scripts** on the console: deploy
   `conformance/conformance_static.lua` as
   `<UE4SS>/Mods/ConformanceStatic/scripts/main.lua` + empty `enabled.txt`;
   `conformance/conformance_live.lua` as
   `<Game>/Mods/conformance-live/Scripts/main.lua` + `mods.txt` row
   `conformance-live : 1` (details in each module's README).
3. **Diff runs** against a PC UE4SS reference:
   `python3 conformance/diff_results.py RUN_A.log RUN_B.log --json diff.json`
   (exit 0 equivalent / 1 differences / 2 usage).
4. Confirm the config-entry formats (loadlist byte-size law, ploader.ini) and
   that the runtime never reads mod files outside `Mods/` (contract §6).

### For the mod manager team

1. Read `docs/CONTRACT-PROPOSAL.md` v1.1 and accept/modify/reject each
   interface; the badge contract is §4.
2. Installer API — the `Installer` class in `installer/installer.py`:
   `Installer(roots_base, plugins_dir, state_dir)` then
   `.install(archive_path, metadata, title_id)` → manifest dict (or
   `MetadataError`/`UnsafeArchiveError`/`CaseConflictError`/…),
   `.verify()`/`verify_installed()`, plus `.find_case_conflicts(title_id,
   rel_paths, mod_dir_name)`. Journalled crash recovery is automatic on the
   next constructor call. (CLI-less by design; wire JSON per contract §2.1.)
3. Compat API — `compat/checker.py`: `scan(mod_path, catalog_path)` → report
   dict, or CLI `python3 compat/checker.py <mod_dir_or_lua> <catalog.json>
   [--json out.json]`. Build the checker catalog from the real files with
   `python3 compat/merge_catalogs.py` → `compat/catalog-real-merged.json`.
   `compat/sample_catalog_TEST_FIXTURE.json` is invented-status test data —
   never ship badges from it.
4. Detector API — `installer/detector.py`: `classify_archive(path)` →
   `Classification` (9 labels incl. `ue4ss-runtime` → do-not-install).
5. Uninstall — `installer/uninstall.py` `Uninstaller`; conflicts —
   `installer/conflicts.py` `check_install()/check_batch()`. The recommended
   integration path is the `InstallPipeline` in `installer/adapter.py`
   (round 3; findings #22/#23 closed there): detect → compat → conflicts →
   install → verify → register over one unified `install_state.json` record
   (19 adapter tests + 13 edge tests).

## 13. SOURCE ATTRIBUTION (upstream vs port, verified)

From `compat/real-catalog-run.md` (attribution audit, read-only trees) and
`docs/CLEAN-RUN-REPORT.md` §3 — all verified 2026-10-07 on this host:

- **Trees:** PORT = `.superpowers/native-dependency-spike/native-coherent-runtime-1/
  source/port/ue4ss/` (PS5/Prospero port); VENDORED UPSTREAM baseline =
  `ue4ss-unreal-native-build-1/.../UE4SS/` with `version.cache` = 3.0.1.0.0
  (UE4SS 3.0.1); UPSTREAM clone cross-check = `ue4ss-binding-research-1/
  upstream/RE-UE4SS/UE4SS/`.
- **Every catalog citation points at the PORT and now says so:** all 523
  type-side and 238 global-side file:line references resolve in the port tree
  (100%, programmatic check, line numbers in range, binding lines
  spot-checked); every v2 entry carries "Source attribution: verified against
  port; upstream reference: TBD" (98/98 globals, 42/42 types, verified
  post-edit; with the 6 Rev-2 `upstream_reference` entries and the 2 v4 input
  tables that is 142 attributed entries — `docs/COUNTS.md` row 11). Port
  findings are no longer presented as upstream facts.
- **"FName → FNameUserdata is a port rename" was FALSE:** the `*Userdata`
  metatable literals exist verbatim in upstream 3.0.1 at the same lines
  (LuaFName.cpp:17 etc.); the port kept upstream naming. Corrected in
  `catalog/src-catalog-types/SUMMARY.md` (CORRECTION 2026-10-07).
- **Port additions honestly labeled:** `loadfile`/`dofile` are marked "PS5
  port addition" in `ue4ss_version` (absent from vendored 3.0.1); port/PS5
  behavior was moved out of `pc_only_behavior` into `notes` in 12 entries.
- **v4 input tables (round 4):** the `Key` (165 constants) and `ModifierKey`
  (3 active: SHIFT/CONTROL/ALT) TYPE entries cite port
  `src/Mod/LuaMod.cpp:446-627` (`register_input_globals`;
  `key_table.make_global("Key")` at :614, `modifier_key_table.make_global("ModifierKey")`
  at :626), cross-checked upstream at `UE4SS/src/Mod/LuaMod.cpp:438-618`
  (:606/:618); the constant lists were verified identical between the trees
  (values are Windows virtual-key codes, `Input::Key` enum in
  `deps/first/Input/include/Input/KeyDef.hpp`: `RETURN=0x0D`,
  `LEFT_ARROW=0x25`, `F7=0x76`; the six `LEFT_SHIFT`…`RIGHT_ALT` ModifierKey
  variants are commented out in BOTH trees and therefore unregistered). The
  official UE4SS v3.0.1 release zip used as the detector's layout reference
  was downloaded Mac-only to /tmp; its public URL, size, sha256 and full
  `unzip -l` listing are pinned in `docs/UE4SS-RELEASE-LISTING.txt`.
- **Residual (documented remediation, NOT applied):** the citation fields
  carry port-tree line numbers; upstream re-derivation (e.g. RegisterHook →
  upstream LuaMod.cpp:4199 vs port :4565) plus a `{catalog_version,
  citation_tree: "upstream-ue4ss-3.0.1"}` wrapper are the agreed next steps
  (CLEAN-RUN-REPORT §3.3, contract §7).
- **Conformance scripts:** call list derived by static reading of the same
  port tree (per-section provenance in `conformance/README.md`); host-tested
  only against mocks; bundled interpreter is Lua 5.4.7 arm64.
- **TownfallItemSpawner mod source:** read-only `git show` from the
  mod-manager worktree branch `origin/claude/townfall-item-spawner-hyj9eg` @
  `2e1ef8be267c4bf811f83f2da168cd52e6b0e16b`; 25/26 copies byte-identical
  (sha256 `f586043a…b6559658`); staged copy
  `compat/townfall-item-spawner-mod/Scripts/main.lua`. The runtime and the
  manager branch were read, never written.
- **Everything Mac-only / host-tested.** No console contact anywhere in this
  package.
