# REVIEW-PACKAGE.md — Installer + Compatibility Layer (side project) — round-5 rewrite

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
> **This rewrite was produced from the CURRENT code, after all round-5 agents
> reached quiescence** (2026-10-07 16:00 local; tree stable for 90 s). Every
> test number below was produced by a fresh single-agent sequential run on
> that final tree (`test-results/raw-test-output.txt`, 16:10); every code
> claim was re-verified against the files as they stand. Earlier versions are
> preserved frozen: `REVIEW-PACKAGE.v3.md` (the round-5 intermediate state
> this file replaces — it still described the pre-round-5 grade policy and
> the un-ported tester harness), `REVIEW-PACKAGE.v2.md` (round-4 final) and
> `REVIEW-PACKAGE.v1.md` (first package). Do not cite their numbers.
>
> **Git position:** last push = commit
> `fbeaae7e22df4d9db6fdbdba6a8028bab679a190` ("Round 4 docs", 2026-10-07;
> parent of the lineage `…390ef65 → 69c88ed → fbeaae7`) on the private
> remote `github.com/Sammsamy02/ps5.git`, branch `side/installer-compat`
> (= local `main`). **All round-5 work (grade-policy change, tester-suite
> port, detector short-id fix, upstream-citation application, Lua-binary
> removal, this rewrite) is UNCOMMITTED working-tree state on top of that
> commit.** Round-4 change log: `docs/CHANGES-ROUND4.md`; pre-push security
> scan: `docs/SECURITY-SCAN.md` (verdict CLEAN, with a post-scan update note
> for the removed interpreter binary).
>
> **One set of numbers everywhere:** every catalog count comes from
> `docs/COUNTS.md` (the authoritative sheet); every test result comes from
> the fresh clean run cited per table. Pending or stale items are labeled as
> such, never papered over.

## 1. WHAT WAS BUILT

The INSTALLER + COMPATIBILITY layer between GPT's PS5 runtime and the mod
manager, per the side-project spec. Six modules plus test/review/audit
infrastructure:

- **Catalog** (`catalog/`): machine-readable UE4SS Lua API surface —
  `globals.json` (**106 global entries**: 98 functions + 6 Rev-2 conformance
  additions + the v4 `Key` (165-constant) and `ModifierKey` (3-active)
  PC-only input TYPE tables) + `types.json` (**42 types / 456 method entries
  / 20 metamethods**). Both files are versioned wrappers
  (`catalog_version: "2026-10-07-v4"`, `citation_tree: "upstream"`) whose
  entries carry **real upstream UE4SS 3.0.1 file:line citations**
  (`upstream_reference` on globals, `upstream_source_file` +
  `upstream_implementation` on types/methods, re-derived by
  `catalog/update_upstream_refs.py`), with the port-tree citations kept as
  the secondary reference; `ps5_runtime_status` is empty everywhere
  (106/106 globals, 476/476 type rows) awaiting GPT's console fill-in.
- **Conformance** (`conformance/`): `conformance_static.lua` (353 calls,
  read-only, game-state-independent) + `conformance_live.lua` (**Rev 2**:
  86 CALL rows + 6 prerequisite rows) + `diff_results.py`
  (MATCH/MISMATCH/MISSING/EXTRA log differ) + a host-test mock environment.
  The formerly bundled Lua 5.4.7 arm64 interpreter binary was REMOVED from
  the repo (2026-10-07, `git rm`, directory `.gitignore`d); host-side Lua is
  now `brew install lua` (mock runs re-verified on brew Lua 5.5.1 — same
  totals).
- **Installer** (`installer/`): transactional journal-first install engine
  (crash-safe, sha256 verify) writing **all four runtime config outputs** —
  loadlist.txt block, `ue4ss/Mods/mods.txt` enabled row (`<Name> : 1`),
  `ue4ss/Mods/<Name>/enabled.txt` marker, ploader.ini line — plus the
  mod-type detector (**9 types incl. `ue4ss-runtime`**, real UE4SS 3.0.1
  release-layout rules), journalled uninstall/rollback, conflict detection
  (path/keybind/hook/deps), and case-conflict detection for the PS5's
  case-sensitive disk.
- **Integration adapter** (`installer/adapter.py`, rounds 3–4): the
  production `InstallPipeline` — **the mod manager's ONE entry point**
  (`install_mod`, adapter.py:666): detect → compat → conflicts → install →
  verify → register, over a unified `install_state/<TITLE>.json` registry
  feeding both uninstall and conflicts; closes tester findings #22/#23
  (Section 8).
- **Compat checker** (`compat/`): static Lua scanner + grade ladder
  (**compat-checker 1.1**): `compatible` / `partly-compatible` /
  `unsupported` / `unknown` / **`does not parse`** (new in round 5), with
  blank-runtime-status bucketing, cataloged-stdlib-override handling, a
  dedicated **PC-ONLY INPUT category** (`Key.*`/`ModifierKey.*` keyboard
  dependencies), and known-PS5-difference reporting inside `unknown`
  (Section 5); catalog merge tool; real-mod grading of TownfallItemSpawner.
- **Tests/review/audit** (`tests/`, `docs/`, `test-results/`): the
  independent adversarial tester's harness — **now ported to this tree's
  layout** (path-relative, `SIDEPROJECT_ROOT` overridable) and re-run green
  (88/88) — plus coverage map, requirements trace, merged contract proposal
  v1.1, audience how-tos, and the pre-push security scan (verdict CLEAN).

**Round-5 changes this rewrite documents** (all uncommitted, all verified by
the fresh run): (1) grade-policy reversal — blank statuses now force
`unknown` and a PC-only keyboard difference rides INSIDE it as a KNOWN PS5
DIFFERENCE instead of upgrading to `partly-compatible`; (2) new
`does not parse` grade + heuristic Lua syntax check (finding #15 fixed);
(3) the tester's five suites ported off `/tmp/sideproject` paths and re-run
(88/88, live on this tree); (4) Nexus URL short-mod-id fix (1–2 digit ids
in URL form; prose stays strict); (5) upstream citations applied to both
catalog files; (6) bundled interpreter binary removed (brew Lua instead);
(7) new open question + contract §8.9 (lowercase `scripts/` vs `Scripts/`).

## 2. FILE LIST

**216 files** (after the interpreter-binary removal), ~9.4 MB including
`.git`. Sizes in bytes; frozen provenance originals under `src-*/` are
byte-copies of the module-authoring state (the working copies above them
have since diverged — Section 8). Reproduce with
`find . -type f -not -path './.git/*' -exec stat -f '%z %N' {} \; | sort -k2`.

```
.gitignore                                1042  ignores __pycache__/*.pyc/.DS_Store/*.dmg
                                                + conformance/src-conformance-static/bin/
                                                (no host interpreter in the repo)
README.md                                8525  project readme (refreshed by the count audit:
                                                current counts + adapter/tester status)
REVIEW-PACKAGE.md                       (this file)
REVIEW-PACKAGE.v3.md                    60758  round-5 intermediate (frozen; superseded by this
                                                rewrite — grade policy + tester port predate it)
REVIEW-PACKAGE.v2.md                    57332  round-4 final (frozen)
REVIEW-PACKAGE.v1.md                     9301  first package (frozen)
catalog/
  globals.json                          123343  106 entries, wrapper {catalog_version:
                                                2026-10-07-v4, citation_tree: upstream, entries}
  types.json                            184605  42 types / 456 methods / 20 metamethods (v4
                                                wrapper, citation_tree: upstream)
  SUMMARY.md                             10600  98-build provenance + v4 update note
  update_upstream_refs.py                13476  re-derived the upstream 3.0.1 citations (host tool)
  backup-before-upstream-refs-2026-10-07/  pre-edit backups (globals.json 82001,
                                                types.json 131416)
  src-catalog-globals/  (build_catalog.py 31776, globals.json 74646, SUMMARY.md 9865 — frozen)
  src-catalog-types/    (build_types.py 36828, types.json 128208, SUMMARY.md 11425 — frozen,
                         FNameUserdata CORRECTION)
compat/
  checker.py                             59873  compat-checker 1.1: does-not-parse grade,
                                                blank-status precedence, KNOWN PS5 DIFFERENCE,
                                                PC-ONLY INPUT category
  test_checker.py                        38095  63 tests (TestDoesNotParse x8, TestPcOnlyInput x9,
                                                Townfall pins x4)
  merge_catalogs.py                       8633  globals+types -> catalog-real-merged.json
  catalog-real-merged.json              315438  599 merged entries (v4; 599/599 blank status)
  sample_mod.lua                          4500  test mod (~25 APIs)
  sample_catalog_TEST_FIXTURE.json        6157  INVENTED-status test fixture (never ship badges)
  sample-mod-real-catalog-report.json    34018  saved sample-mod report (pre-v4 run)
  townfall-item-spawner-grade.json       70947  THE required grade artifact (compat-checker 1.1
                                                scan: grade unknown + KNOWN PS5 DIFFERENCE)
  townfall-item-spawner-grade.md         10811  human summary (round-5 grade policy)
  townfall-item-spawner-mod/Scripts/main.lua 21415  staged mod source (read-only copy)
  real-catalog-run.md                    11815  attribution audit + real-catalog checker runs
  README.md                                522  pointer to docs/CONTRACT-PROPOSAL.md §4
  src-compat-checker/   (frozen: checker.py 41155, test_checker.py 17231,
                         CONTRACT-PROPOSAL.md 7876, demo-report.json 20637,
                         sample_catalog.json 4583, sample_mod.lua 4500)
conformance/
  conformance_static.lua                 37578  353-call static suite
  conformance_live.lua                   52368  Rev-2 live suite (86 CALL + 6 PREQ rows)
  diff_results.py                        10496  log differ
  README.md                              11664  static-suite spec + host-test results (brew Lua)
  sample_a_vs_sample_b.diff.json        162993  diff artifact (top-level copy)
  src-conformance-live/  (conformance_live.lua 52368 = Rev 2, README.md = Rev-2 spec)
  src-conformance-static/ (conformance_static.lua 37578, diff_results.py 10496,
      hosttest_mock_env.lua 14010, README.md 11664,
      sample_a_vs_sample_b.diff.json 162993, samples/: sample_a.log 22857,
      sample_b.log 22784, sample_b_full.log 22868,
      sample_a_vs_sample_b.diff.json 162919, sample_a_vs_sample_b_full.diff.json 162902)
      — bin/lua (298384 B, Lua 5.4.7 arm64) was REMOVED 2026-10-07 (git rm,
      now .gitignored); mock variants re-verified on brew Lua 5.5.1
docs/
  CONTRACT-PROPOSAL.md                  17084  merged v1.1 (§8 has 9 open questions incl. #9
                                                lowercase paths)
  COVERAGE-MAP.md                      132088  582 coverage rows (rev-3 map — §9 item 3)
  coverage.json                        194569  machine companion (582 rows)
  REQUIREMENTS-TRACE.md                21757  15 spec rows (all PASS after the round-5 refresh)
  COUNTS.md                            12533  THE authoritative count sheet (v4 + rev-3-coverage
                                                recounts, corrected by the count audit)
  COUNT-AUDIT.md                        14660  final count audit: every numeric claim cross-checked
  CLEAN-RUN-REPORT.md                   12842  clean-run verification + proposal merge + audit
  ANSWERS.md                           16815  Q1–Q3 answered + Q4/Q5 for GPT (round-5 grade
                                                notes added by the count audit)
  HOWTO-GPT.md                          7615  runtime-owner guide (current counts after audit)
  HOWTO-MOD-MANAGER.md                  13519  manager integration guide (updated this round: §12)
  SECURITY-SCAN.md                       8265  pre-push audit: CLEAN (+ post-scan interpreter
                                                removal note)
  UE4SS-RELEASE-LISTING.txt               3630  official UE4SS_v3.0.1.zip listing (URL+sha256)
  CHANGES-ROUND4.md                      6057  round-4 change log (242→267 arithmetic)
installer/
  installer.py                          82794  transactional install engine (+mods.txt/enabled
                                                flags, case-conflict wiring)
  adapter.py                            50189  InstallPipeline — THE entry point (install_mod
                                                adapter.py:666; check/uninstall/verify/rollback;
                                                CLI install|check|uninstall|verify)
  detector.py                           42958  9-type classifier (+UE4SS release layouts,
                                                /games/ URLs, 1–9-digit URL mod ids)
  uninstall.py                          26164  journalled uninstall + rollback (+markers)
  conflicts.py                          17884  path/keybind/hook/dependency conflicts
  test_installer.py                     51786  55 tests
  test_config_outputs.py                22413  11 tests (four config outputs + round trip)
  test_detector.py                      38243  52 tests (incl. short-mod-id URL/prose pair)
  test_uninstall.py                     13583  11 tests
  test_conflicts.py                      8734   9 tests
  test_case_sensitivity.py              20790  23 tests (case conflicts + APFS volume)
  test_zip_safety.py                    21597  21 tests (duplicates/bombs/UE4SS-as-mod)
  test_adapter.py                       26175  19 tests (InstallPipeline lifecycle, closes
                                                #22/#23)
  test_adapter_edge.py                  25875  13 tests (edge cases + HOWTO doc-match test)
  src-installer-core/  (installer.py 55747, test_installer.py 40720 — frozen)
  src-mod-types/       (detector.py 32992, test_detector.py 17044 — frozen)
  src-uninstall/       (uninstall.py 22514, conflicts.py 17884, test_uninstall.py 13345,
                        test_conflicts.py 8496 — frozen)
tests/
  REVIEW.md                             18263  tester report, verbatim (paths localized)
  results/*.json                         13 files: tester suite JSONs, RE-RUN green on this
      tree 2026-10-07 15:49 after the port (suite_installer 27, suite_detector 19,
      suite_uninstall 19, suite_compat 20, integration_tests 3, 8 static scans)
  src-tester/  (PORTED harness — path-relative since round 5: framework.py 18060
      resolves everything from this file's location, SIDEPROJECT_ROOT overridable;
      suite_installer.py 24005, suite_detector.py 10935, suite_uninstall.py 16893,
      suite_compat.py 10161, integration_tests.py 14017, lua_syntax.py 22970,
      corpus.py 11370, adapters.py 4462, review_static.py 4622, poll.py 2232,
      module_state.json 735, REVIEW.md 18160,
      results/ [fresh 15:49 re-run JSONs], runs/ [mock logs + diff artifact],
      corpus/ [on-disk fixtures: detector/: 15 zips incl. many_files.zip
      1128894; installer/: 18 zip archives incl. bomb.zip 108872 + 18 hostile
      config JSONs; lua/: 19 hostile files; uninstall/: 14 manifests;
      corpus.py builds 65 adversarial cases (14+18+19+14)])
test-results/
  raw-test-output.txt                   33147  THE definitive round-5 clean run (16:10, this
                                         rewrite's numbers: 277 unit + 88 tester)
```

## 3. HOW TO RUN EVERY TEST

All Mac-only, stdlib-only Python 3.9+, host Lua via `brew install lua`.
Every command below was executed for this rewrite; outputs are in
`test-results/raw-test-output.txt`.

```bash
# 3.1 ALL installer-side suites in one command — 214 tests
cd /WORKSPACE/Projects/ps5-installer-compat/installer
python3 -m unittest test_installer test_config_outputs test_zip_safety \
                       test_case_sensitivity test_detector test_uninstall \
                       test_conflicts test_adapter test_adapter_edge

# 3.2 Compatibility checker — 63 tests (includes the Townfall grade pins)
cd /WORKSPACE/Projects/ps5-installer-compat/compat
python3 -m unittest test_checker

# 3.3 Independent tester suites — NOW PORTED AND RUNNABLE (88 tests;
#     framework.py resolves paths relative to itself; override the
#     project root with SIDEPROJECT_ROOT if needed)
cd /WORKSPACE/Projects/ps5-installer-compat/tests/src-tester
python3 framework.py suite_installer.py suite_detector.py \
                       suite_uninstall.py suite_compat.py integration_tests.py

# 3.4 Conformance static suite under the host mock (variants A and B;
#     needs `brew install lua` — the repo no longer bundles an interpreter)
cd /WORKSPACE/Projects/ps5-installer-compat/conformance/src-conformance-static
lua hosttest_mock_env.lua conformance_static.lua
MOCK_VARIANT=b lua hosttest_mock_env.lua conformance_static.lua

# 3.5 Conformance diff tool on the committed sample logs
cd /WORKSPACE/Projects/ps5-installer-compat/conformance/src-conformance-static
python3 diff_results.py samples/sample_a.log samples/sample_b.log --verbose

# 3.6 Rebuild the merged real catalog, then the checker runs against it
cd /WORKSPACE/Projects/ps5-installer-compat
python3 compat/merge_catalogs.py
python3 compat/checker.py compat/sample_mod.lua compat/catalog-real-merged.json \
        --json /tmp/sample-mod-report.json
python3 compat/checker.py compat/townfall-item-spawner-mod compat/catalog-real-merged.json
# ^ re-runs the required TownfallItemSpawner grade (Section 5)

# 3.7 The full pipeline through the integration adapter (the ONE entry point)
cd /WORKSPACE/Projects/ps5-installer-compat/installer
python3 adapter.py --help        # install | check | uninstall | verify
python3 adapter.py check --archive mod.zip --metadata mod.json \
        --title PPSA33286 --game-root ./sandbox/roots/PPSA33286 \
        --plugins ./sandbox/plugins --state ./sandbox/state
```

## 4. FULL TEST RESULTS (fresh clean run on the final round-5 tree)

**Source: `test-results/raw-test-output.txt` (2026-10-07 16:10)** — one
single-agent sequential pass over the tree as it stood after all round-5
agents quiesced, stdout+stderr unfiltered. It supersedes the round-4 log
(267 unit tests): checker 55→63, detector 50→52, and the tester layer moved
from "frozen, not runnable" to a live re-run. History: 196 → 242 → 267 →
**277** unit tests (`docs/COUNTS.md` §5, `docs/CHANGES-ROUND4.md`).

### 4.1 Unit suites — 277 / 277 PASS, 0 FAIL, 0 ERROR (raw log §1–§2)

| suite | tests | result |
|---|---|---|
| installer/test_installer.py | 55 | OK |
| installer/test_config_outputs.py | 11 | OK |
| installer/test_zip_safety.py | 21 | OK |
| installer/test_case_sensitivity.py | 23 | OK |
| installer/test_detector.py | **52** | OK (+2 round-5: short-mod-id URL/prose) |
| installer/test_uninstall.py | 11 | OK |
| installer/test_conflicts.py | 9 | OK |
| installer/test_adapter.py | 19 | OK |
| installer/test_adapter_edge.py | 13 | OK |
| compat/test_checker.py | **63** | OK (+8 round-5: TestDoesNotParse, blank-status precedence) |
| **TOTAL** | **277** | **277 PASS / 0 FAIL** |

Cross-check embedded in the run: the combined installer invocation reports
`Ran 214 tests … OK` (55+11+21+23+52+11+9+19+13 = 214), checker
`Ran 63 tests … OK`. The case-sensitivity volume test attached a real
case-sensitive APFS image (hdiutil) and re-ran a full installer-side suite
on it — passed, not skipped. `test_installer` emits cosmetic
`ResourceWarning: unclosed file` notices and `test_zip_safety` emits
duplicate-name `UserWarning`s while building adversarial fixtures; both
preserved verbatim in the raw log.

### 4.2 Independent tester suites — ported and re-run LIVE: 88 / 88 PASS (raw log §6)

The tester's five suites (`tests/src-tester/suite_*.py`,
`integration_tests.py`) were **ported off their original `/tmp/sideproject`
layout** in round 5: `framework.py` now resolves every module path relative
to its own location (legacy module names map to the consolidated
directories; `SIDEPROJECT_ROOT` overrides the project root). Re-run on this
tree 2026-10-07 (15:49 and again in the 16:10 clean run):

| suite | pass | fail |
|---|---|---|
| suite_installer | 27 | 0 |
| suite_detector | 19 | 0 |
| suite_uninstall (+conflicts) | 19 | 0 |
| suite_compat | 20 | 0 |
| integration_tests (L1/L2/L3) | 3 | 0 |
| **total** | **88** | **0** |

History: the tester's frozen 14:08 results were 87 pass / 1 fail (the fail:
`det.requirements_adversarial_text`, the Nexus `/games/<title>/` URL gap —
finding #6, fixed in round 3). The ported suites' saved JSONs
(`tests/results/*.json`, mirrored under `tests/src-tester/results/`) now
record 88/88 on this tree. The 8 static danger-scans also re-ran clean
(doc-string mentions only).

### 4.3 Conformance host/mock results (raw log §3–§4; brew Lua 5.5.1)

| run | result |
|---|---|
| conformance_static, mock variant A | 353 calls, **353 ok / 0 fail** |
| conformance_static, mock variant B (deliberate value changes) | 353 calls, 352 ok / 1 fail (the designed fail) |
| diff_results.py: sample_a vs sample_b | **347 MATCH / 4 MISMATCH / 2 MISSING / 1 EXTRA** (354 keys), exit 1 (differences found, by design) |
| conformance_live (Rev 2), fresh full-battery mock (all APIs stubbed, Pawn+PC present) | **86 CALL rows / 6 PREQ rows: 82 PASS / 1 FAIL / 3 SKIP** — the FAIL is the intentional `string.toFName` absence probe; SKIPs are LoopAsync (uncancelizable by design) + two TArray read-only exclusions (evidence: `conformance/src-conformance-live/README.md` "Host verification") |
| conformance_live, degraded mock (constructors absent, no Pawn/PC) — Rev-1-era figures, retained for history | 27 PASS / 7 FAIL / 18 SKIP — the 7 FAILs are exactly the absent-API probes |
| hostile missing-global probes (tester, 17 globals deleted one at a time) — frozen tester evidence | **17/17 survived** — all 353 static calls always emitted, zero crashes |

**Mock caveat (required honesty statement):** pass under mock only proves
the scripts run against our own stand-ins, not against the real runtime —
`ps5_runtime_status` is blank in 106/106 globals and 476/476 type rows
(599/599 merged), so no catalog entry has been proven against a real PS5
UE4SS runtime (Section 6). Neither conformance script has run in a real
game session.

### 4.4 Fresh real-catalog grades (raw log §5)

| run | grade |
|---|---|
| TownfallItemSpawner vs catalog-real-merged.json (599 entries) | **unknown — runtime status not filled (9 blank APIs); KNOWN PS5 DIFFERENCE: PC-only input dependency** (Section 5) |
| sample_mod.lua vs the real catalog | `unknown - runtime status not filled` (uses no `Key.*` constants; saved report `compat/sample-mod-real-catalog-report.json` is a pre-v4 run with the same outcome) |

## 5. TOWNSFALL ITEMSPAWNER GRADE (the required test result)

Artifacts: `compat/townfall-item-spawner-grade.json` (verdict + provenance +
FULL compat-checker-1.1 scan, regenerated 15:55 against the round-5
checker), `compat/townfall-item-spawner-grade.md` (summary),
`compat/townfall-item-spawner-mod/Scripts/main.lua` (staged source).

```
GRADE:   unknown
REASON:  unknown - runtime status not filled (9 cataloged API(s) this mod
         calls have an empty ps5_runtime_status); KNOWN PS5 DIFFERENCE:
         PC-only input dependency - keyboard keybinds (Key.DOWN_ARROW,
         Key.F7, Key.LEFT_ARROW ... +3 more) need runtime/config
         controller mapping on PS5 (PS5 has no keyboard; see PS5Pad.ini)
```

- **What was graded:** the real mod "TownfallItemSpawner v1.0" (F7 menu item
  spawner), source read-only `git show`n from the mod-manager worktree branch
  `origin/claude/townfall-item-spawner-hyj9eg` @ `2e1ef8be…`; 25 of 26 branch
  copies byte-identical (sha256 `f586043a…b6559658`, 746 lines); the one
  divergent copy is the intentionally corrupted `root-badsize` host-test
  fixture (excluded). `Mods/shared/UEHelpers` ships outside the mod dir and
  is out of scope.
- **Catalog used:** `compat/catalog-real-merged.json` (v4) — **599 entries**
  (106 globals − 3 fused string-ctors = 103, + 42 types + 454 method entries;
  arithmetic in `docs/COUNTS.md` §3), **599/599 blank `ps5_runtime_status`**
  (the `Key.*` classification comes from the entries' `pc_only_input: true`
  flag, not a filled status).
- **Numbers (verified in the JSON artifact and re-run live, raw log §5):**
  total 66 calls | 32 catalog-matched | **0 unknown** | 34 unresolved
  (game-side Blueprint calls — never counted against the grade) | 85
  lua-builtins | 0 parse errors. **PC-ONLY INPUT — the six `Key.*` keybind
  constants, all central** (top-level `RegisterKeyBind` args): `Key.F7`
  main.lua:694 (menu toggle), `Key.UP_ARROW`/`Key.DOWN_ARROW` :698/:703
  (navigate), `Key.RETURN` :708 (confirm/spawn), `Key.LEFT_ARROW`/
  `Key.RIGHT_ARROW` :713/:722 (category cycle). **9 cataloged-but-blank
  APIs**: RegisterKeyBind, ExecuteInGameThread, StaticFindObject, FText,
  StaticConstructObject, FName, FindFirstOf, LoadAsset, print. **Not in
  catalog: none.**
- **Grade policy (round 5 — this is the change Claude's review asked for):**
  the round-4 policy let a confirmed PC-only keyboard difference upgrade the
  grade to `partly-compatible`. Round 5 reversed that
  (`compat/checker.py:1085` "GRADE POLICY (Claude round-5 review)"): when
  ANY API the mod calls resolves to a catalog entry with a blank
  `ps5_runtime_status`, NO verdict is possible — the grade is
  `unknown - runtime status not filled`, and known differences (the keyboard
  dependency) ride INSIDE that result as a `known_ps5_differences[]` list +
  note, never upgrading the grade. Rationale: claiming `partly-compatible`
  while the mod's core APIs (including `RegisterKeyBind` itself, the
  consumer of the flagged constants) are unverifiable overstated what the
  catalog actually knows. With NO blank statuses anywhere, a PC-only input
  dependency alone would still grade `partly-compatible` with its dedicated
  reason (checker.py grade ladder; pinned by
  `TestPcOnlyInput::test_blank_status_outranks_pc_only_input`).
- **The automated pin (THE required test):**
  `compat/test_checker.py::TestTownfallItemSpawnerGrade::test_townfall_grade_unknown_with_keyboard_note`
  — loads the real staged mod + the real merged catalog, runs
  `checker.scan()`, asserts grade `unknown` with both the "runtime status
  not filled" clause and the KNOWN PS5 DIFFERENCE keyboard note. Companions:
  `test_townfall_inputs_exist`, `test_townfall_blank_status_core_apis`,
  `test_townfall_six_key_constants_are_pc_only_input` (six constants in the
  `pc_only_input` bucket, all central; `unknown` empty). All pass in the 277.
  The pins intentionally go red if the catalog/checker changes without
  re-grading — that is their job. (Earlier pin names
  `test_townfall_grade_unknown_reason_runtime_status_not_filled` and
  `test_townfall_grade_partly_compatible_pc_only_input` are the round-3/4
  predecessors, each superseded when the grade changed.)
- **To grade for real:** (1) catalog side — GPT fills `ps5_runtime_status`
  for at least the 9 blank APIs; (2) runtime/config side — the controller
  mapping decision for the six keybinds (open question 9, Section 10);
  then re-run §3.6's last command.

## 6. CONFORMANCE COVERAGE

Source: `docs/coverage.json` + `docs/COVERAGE-MAP.md` (rev-3 map, the
final-round regeneration after catalog v4) — every catalog entry mapped to
the conformance script that calls it, or the reason it is not called.

| scope | rows | invoked | presence-only | any CALL row | not covered |
|---|---|---|---|---|---|
| globals (98 v2 + 6 Rev-2 + Key/ModifierKey v4) | 106 | 70 | 13 | 83 | 23 |
| type methods (456) + metamethods (20) | 476 | 80 | 0 | 80 | 396 |
| **total (rev-3 snapshot)** | **582** | **150 (25.8%)** | **13** | **163 (28.0%)** | **419** |

Two conventions to read it with (both defined in `docs/COUNTS.md` rows 6,
12–14):

- **The rev-3 map includes the v4 containers (582 rows).** The two
  `Key`/`ModifierKey` TYPE entries map to the static suite's existing
  constant spot-check rows, so the coverage snapshot now equals the
  coverage-convention total of **582 API rows** (covered rows grew
  161 → 163 vs the pre-v4 map).
- **Call-count reconciliation:** static 76 literal sites loop-expand to 353
  CALL rows; live Rev 2 emits 86 CALL rows (+6 PREQ); total 439 — matching
  every claim in this package.

**The mock caveat (verbatim from the coverage map):** *"Pass under mock only
proves the scripts run against our own stand-ins, not against the real
runtime."* Mock agreement is evidence the scripts are syntactically valid,
crash-safe and log-format-correct; it is NOT evidence of runtime conformance.

**What Rev 2 fixed here (was a named mock blind spot):** the old live TArray
battery matched no catalog TArray member — Rev 2 rewrites it to the port's
real members; `UClass:GetFunction` (never a catalog method — does not exist
in this port) is replaced with `ForEachFunction`/`GetSuperStruct`/`IsChildOf`
/`GetCDO`; the fictional `Loop` table is replaced with the real scheduling
globals (self-owned register/cancel cycles); every cataloged hooking global
now has a gated no-op call. Mocks still cannot catch this defect class —
the fix came from source reading, not a console. Remaining flagged gaps:
`string.toFName` (labeled absence probe), FString/FText `__tostring`,
FString `__concat`; `LoopAsync` intentionally not called (no cancellation
API — SKIP-noted).

## 7. REQUIREMENTS TRACEABILITY

Full per-row detail (implementation file → test file → test names → result):
`docs/REQUIREMENTS-TRACE.md`. (Its rows 2, 5 and 15 were written before the
rounds-3–5 fixes landed and were superseded by the table below; the final
round-5 pass refreshed the trace file itself — all 15 rows now read PASS
with current wording, verified by the 365/365 clean run.)

| # | Spec item | Result (current tree, fresh run) |
|---|---|---|
| 1 | Input: archive + Nexus metadata + title ID | **PASS** — 55-test installer suite; tester 27/27 |
| 2 | Output: install layout (loadlist, **mods list, enabled flags**, ploader.ini) | **PASS (round 3; was PARTIAL in the trace file)** — all four outputs written on install and restored byte-for-byte on uninstall: loadlist block, `mods.txt` row `<Name> : 1`, empty `enabled.txt` marker (unless the archive ships one), ploader.ini line. Pinned by `installer/test_config_outputs.py` (11 tests) |
| 3 | NEVER-EDIT rule: byte-for-byte + manifest + verify | **PASS** — sha256 verify, CRLF preserved, foreign rows preserved, overrides-not-settings rule |
| 4 | Mod-type detection (Lua / C++ DLL / pak-IoStore / LogicMods / other) | **PASS** — 9 types incl. `ue4ss-runtime`, with real UE4SS 3.0.1 release-layout detection (reference `docs/UE4SS-RELEASE-LISTING.txt`). Open note: `.lua` content never binary-sniffed (finding #7, Section 8) |
| 5 | Requirements: UE4SS version + other mods | **PASS (round 3; was "PASS (open defect)" in the trace file)** — the Nexus `/games/<title>/` URL gap is FIXED (`NEXUS_ID_RES`, detector.py:149–163; pinned by `test_nexus_url_with_games_path_prefix`); round 5 additionally fixed 1–2-digit mod ids in URL form (`/mods/7`, `/mods/77` — real ids exist that short) while keeping prose forms strict at 3–9 digits to avoid ambiguous bare numbers (`test_nexus_url_short_mod_ids`, `test_nexus_prose_short_id_not_matched`); the ported tester suite's `det.requirements_adversarial_text` now passes (19/19) |
| 6 | Uninstall + rollback | **PASS** — 11/11 + tester 19/19; the round trip now ships WITHOUT the tester's bridge: `installer/adapter.py` `InstallPipeline` reconciles installer manifests → uninstaller records (finding #22 closed), pinned by 19 + 13 adapter tests |
| 7 | Conflict detection | **PASS** — 9/9 module tests; wired into the install path by `InstallPipeline._preflight` (`conflicts.check_install`, adapter.py:648): PATH_CONFLICT/VERSION_MISMATCH/MISSING_DEPENDENCY blockers refuse before any write; keybind/hook conflicts warn and proceed (finding #23 closed at pipeline level — the raw `Installer.install()` path itself does not gate, Section 9 item 5). Level-N comment bug still open (finding #10) |
| 8 | Archives: zip supported, 7z/rar report needed | **PASS** — magic sniff, conversion guidance, tar.gz/xz/bz2 also supported |
| 9 | Install/verify/uninstall round trips | **PASS** — crash injection + recovery verified; end-to-end through the one entry point (`test_adapter` lifecycle + `test_adapter_edge` full round trip + ported INT-L1) |
| 10 | Tampered file detected | **PASS** — verify exit 3; uninstall confirm flow; adapter tamper test; ported INT-L3 |
| 11 | Each mod type classified correctly | **PASS** — 52 detector tests, all types |
| 12 | Two mods writing the same path | **PASS (was "PASS at conflicts-module level" only)** — conflicts module AND the pipeline pre-install gate (adapter conflict-blocking + shared-ploader-line edge tests + ported INT-L2, which exercises detect→block→uninstall→install live) |
| 13 | Corrupt archive | **PASS** — truncated/empty/not-a-zip/encrypted/bomb all rejected (tester corpus + zip-safety suite) |
| 14 | Conformance diff tool on two sample logs | **PASS** — artifacts + live reproductions in the fresh run (no unit test pins exit codes yet — noted follow-up) |
| 15 | Compatibility grade for TownfallItemSpawner | **DELIVERED** — `compat/townfall-item-spawner-grade.json`: grade **unknown — runtime status not filled + KNOWN PS5 DIFFERENCE (PC-only input)**; automated pin `test_townfall_grade_unknown_with_keyboard_note` (+3 companions) in the 277. A compatible/unsupported verdict requires GPT's console fill-in of the 9 blank APIs plus the runtime's controller-mapping decision — that is the honest ceiling today |

Cross-cutting caveats the table must be read with: mock-only conformance;
open findings (Section 8); no locking; everything host-only.

## 8. REVIEWER FINDINGS (all 23, classified, CURRENT status)

Source: the independent tester's report `tests/REVIEW.md` (window 2026-10-07
~13:38–14:06; per-test detail `tests/results/*.json`; final module digests
`tests/src-tester/module_state.json`). Classes: BLOCKER / BUG / DESIGN
CONCERN / EDGE CASE / SUGGESTION. Statuses below were re-verified against
the FINAL round-5 tree by code reads + direct probes, and by the fresh
277-unit + 88-tester run.

| # | Module | Finding | Class | Status (final tree) |
|---|---|---|---|---|
| 1 | installer-core | `NameError: config_writes` on every first install | BLOCKER | **FIXED during review** — pinned by `inst.happy.fresh_install_must_succeed` (tester suite, still green in the ported re-run) |
| 2 | installer-core | `need_dir()` never mkdir-plans leaf mod dirs | BLOCKER | **FIXED during review** — rewritten ancestor-chain `need_dir`; pinned by tests |
| 3 | installer-core | No locking; concurrent installs mutually destruct (detected, fail-closed) | DESIGN CONCERN | **OPEN — accepted risk** (0 flock/fcntl in installer/uninstall/adapter); serialized use documented in the how-tos |
| 4 | installer-core | Duplicate zip members normalize to same path, not rejected at inspect | EDGE CASE | **FIXED** — duplicate detection at inspect; pinned by `test_zip_safety.TestDuplicateEntries` (5 tests) |
| 5 | installer-core | `verify` trusts absolute `install_path`s; moving roots dir invalidates installs | DESIGN CONCERN | **OPEN — partially mitigated**: manifests also record relative `install_rel` (installer.py:1715) and the adapter reconciles through it; `verify` still keys on the absolute path |
| 6 | mod-types | Nexus `/games/<title>/` URL parses to no dependency | BUG | **FIXED (round 3), refined (round 5)** — `NEXUS_ID_RES` (detector.py:149–163) handles the `/games/<title>/mods/<id>` layout, locale prefixes, hyphenated slugs, AND (round 5) 1–9-digit ids in URL form (real ids as short as `77`); prose forms stay 3–9 (ambiguity guard). Pinned green by `test_nexus_url_with_games_path_prefix` + the short-id pair; the ported tester's `det.requirements_adversarial_text` passes (19/19) |
| 7 | mod-types | `.lua` content never sniffed: PE binary renamed main.lua classifies ue4ss-lua | BUG | **OPEN** — verified in the final tree: the `.lua` branch (detector.py:783–789) decodes text and marker-scans only; the MZ/ELF first-bytes check exists for `.dll`/native members only. The ported tester's `det.lua_actually_pe_in_layout` observes-and-records rather than pins. Recommended MZ/ELF check on `.lua` members remains future work |
| 8 | mod-types | Scan truncation at MAX_ENTRIES (5000) noted in `notes` only, not `flags` | EDGE CASE | **OPEN** — verified: detector.py:537–538/560–561 append to `notes` only |
| 9 | mod-types | Static marker detection bypassable by construction (`_G["Find".."FirstOf"]`) | DESIGN CONCERN | **OPEN BY DESIGN** — shared documented blind spot with the checker |
| 10 | uninstall/conflicts | `strip_lua_comments` mishandles `--[==[...]==]` level-N comments → false-positive KEYBIND_CONFLICT warnings | BUG | **OPEN** — verified: conflicts.py:127–160 strips `--[[ ]]` and line comments only (no `(=*)` level handling); the checker's level-aware `strip_lua` (checker.py:230) is right there to share |
| 11 | uninstall | File moved to trash before the journal records it — crash window | DESIGN CONCERN | **OPEN, window narrowed** — uninstall.py:478–483 now writes the journal atomically after EVERY single move (per-file granularity), so the residual window is between one `shutil.move` and its journal write; structurally still present |
| 12 | uninstall | Install-state read-modify-write unlocked | DESIGN CONCERN | **OPEN — accepted** (same class as #3) |
| 13 | uninstall | Dependency cycles deadlock uninstall by design (safe refusal) | DESIGN CONCERN | **OPEN BY DESIGN** — safe refusal, no corruption; pinned by `un.circular_dependency_deadlock` |
| 14 | compat-checker | String-indexed calls (`_G["LoadAsset"]`) invisible, no dynamic-access flag | BUG | **OPEN** — no dynamic-access-risk flag in checker.py (grep: 0 hits); documented by `compat.obfuscated_indexed_calls` |
| 15 | compat-checker | Lua syntax errors indistinguishable from empty mods (both unknown, 0 calls) | EDGE CASE | **FIXED (round 5)** — new heuristic `lua_syntax_error()` (checker.py:302: unterminated string/long-string/long-comment, invalid tokens, block balance) → dedicated grade **`does not parse`** naming file:line, outranking everything; broken files are excluded from scanning while clean files still scan; report gains `parse_errors[]`. Pinned by `TestDoesNotParse` (8 tests, incl. outranks-blank-status and valid-constructs-not-flagged) |
| 16 | conformance-static | "Every call in pcall" was FALSE + broken always-truthy guard | BLOCKER | **FIXED during review** — `run_call` pcall-wraps; 17/17 hostile probes survived; log format verified byte-exactly |
| 17 | conformance-static | `rtype` column not sanitized (tab could break TSV) | EDGE CASE | **OPEN — low risk** — verified: `sanitize(params)` is applied but the `rtype` field is concatenated raw (conformance_static.lua:214) |
| 18 | conformance-live | Byte-based truncate can split a UTF-8 rune | SUGGESTION | **OPEN — cosmetic** (Rev 2 kept 200-char truncation) |
| 19 | catalogs | Provenance unverifiable: citations point at a tree "not on this host"; port facts presented as upstream | DESIGN CONCERN | **FIXED — resolved and strengthened.** The tree exists (read-only); all citations verified programmatically in the PORT tree; "FName→FNameUserdata is a port rename" was FALSE (upstream's own convention — corrected in `catalog/src-catalog-types/SUMMARY.md`). Round 5 then APPLIED the documented remediation: `catalog/update_upstream_refs.py` re-derived real upstream UE4SS 3.0.1 file:line citations into `upstream_reference` (globals) / `upstream_source_file` + `upstream_implementation` (types/methods), wrapped both files with `catalog_version` + `citation_tree: "upstream"`, kept the port citations as secondary, and marked `loadfile`/`dofile` PORT-ONLY ADDITION. Pre-edit backups: `catalog/backup-before-upstream-refs-2026-10-07/` |
| 20 | catalogs | `ps5_runtime_status` blank everywhere → cataloged calls land unverifiable | DESIGN CONCERN | **HALF-FIXED, and the grading semantics tightened in round 5** — checker buckets blank statuses separately (`status-blank`) and they now FORCE grade `unknown - runtime status not filled` (never a fake pass/fail, and — new in round 5 — never masked by a PC-only-input difference either, Section 5). The fill-in itself is GPT's (open by design): still 106/106 globals, 476/476 type rows, 599/599 merged blank |
| 21 | compat sample catalog | Sample catalog drift from real catalogs | BUG | **FIXED** — renamed `sample_catalog_TEST_FIXTURE.json` + invented-status wrapper; frozen original untouched |
| 22 | integration | I-1: installer/uninstaller record contracts differ; no shipped adapter (only the tester's ~40-line bridge) | BLOCKER (system) | **FIXED (round 3)** — `installer/adapter.py` ships the production `InstallPipeline` + `installer_manifest_to_uninstall()` + unified `install_state/<TITLE>.json` (one state file feeds uninstall AND conflicts); ploader cleanup after transaction commit (shared-line aware, with rollback snapshot). Pinned by `test_adapter.py` (19) + `test_adapter_edge.py` (13) |
| 23 | integration | I-2: conflicts.py never wired into the install path; installer will write colliding files | BLOCKER (system) | **FIXED (round 3, pipeline level)** — `InstallPipeline._preflight` builds the registry from the unified state and calls `conflicts.check_install` (adapter.py:648); blockers refuse before any write. Residual, stated plainly: the raw `Installer.install()` used directly still performs NO path/keybind/hook conflict check and emits a manifest the uninstaller cannot consume — `InstallPipeline.install_mod` is the entry point; `Installer.install` alone skips the compat and conflict gates (Sections 9/12) |

**Tally: 23 findings — 11 FIXED (#1, #2, #4, #6, #15, #16, #19, #21, #22,
#23, plus #20's checker half), 1 partially mitigated (#5), 11 OPEN** (of
which #9/#13/#20-fill-in are open by design, #3/#12 are accepted risks,
#17/#18 are low/cosmetic). **No system BLOCKER remains open** — the two
that made this "five good parts, not a system" are closed at the pipeline
level; what remains open is documented risk, not integration failure.

### Fixes added across the review rounds (each pinned by green tests)

- R1 (during the tester's window): #1, #2, #16, zip-duplicate inspect (#4),
  checker blank-status bucketing (#20 half).
- R2 (post-review hardening): case-conflict suite (23), zip-safety suite
  (21), `ue4ss-runtime` detector type + install refusal, cataloged-stdlib
  overrides, fixture rename (#21), merged contract proposal v1.1.
- R3/R4: mods.txt + enabled-flag writer (`test_config_outputs`, 11), Nexus
  URL fix (#6), UE4SS real-layout detection, conformance Rev 2, upstream
  citation re-derivation (#19), the integration adapter (#22/#23; 19+13
  tests), Key/ModifierKey catalog v4 + PC-ONLY INPUT grading, audience
  how-tos, unified counts, security scan, pushes (`docs/CHANGES-ROUND4.md`).
- R5 (this round, uncommitted): grade-policy reversal + `does not parse`
  grade (#15 fixed, 8 new tests; Townfall grade re-issued as
  `unknown + KNOWN PS5 DIFFERENCE`), Nexus short-mod-id refinement (+2
  tests), tester-suite port + 88/88 live re-run, upstream citations applied
  to the catalog files (#19 remediation executed), interpreter binary
  removed from the repo, contract §8.9 lowercase-paths question.

### Tester's positive verification (balance)

Journal-first crash recovery (crash injection → rollback → clean reinstall),
ownership guards, traversal/symlink/encrypted/10k-entry/BOM-CRLF rejection,
unicode + NFC/NFD determinism, magic-byte sniffing, zip-in-zip depth 4,
tar device-entry safety, semver cue-word discipline, byte-exact multiset
config-line removal, full uninstall rollback with double-rollback refusal,
checker grading ladder end-to-end, hostile-Lua crash-safety. Static
danger-scan hits were doc-string mentions only; `docs/SECURITY-SCAN.md`
re-audits the whole tree plus full git history: **CLEAN** (no credentials;
all archives are synthetic corpus fixtures; the one real binary — the Lua
interpreter — has since been removed from the repo, post-scan note added).

## 9. KNOWN LIMITS (honest, current — renumbered this round)

1. **Compatibility grading is unverifiable until statuses are filled.** All
   599 merged entries have blank `ps5_runtime_status`; round 5's grade
   policy makes this explicit: ANY blank-status API the mod calls forces
   `unknown - runtime status not filled` (known PS5 differences, including
   the PC-only keyboard class, ride inside as `known_ps5_differences[]` and
   never upgrade the grade). TownfallItemSpawner therefore grades
   `unknown` (Section 5).
2. **Conformance is mock-only** — 163/582 rev-3-map rows (28.0%) covered,
   150 invoked (25.8%); mock passes prove the scripts run, NOT runtime
   conformance. Neither script has run in a real game session.
3. **Coverage map is a rev-3 snapshot** — 582 rows; the v4
   `Key`/`ModifierKey` containers ARE mapped (to the static suite's
   existing constant spot-checks), matching the coverage-convention total.
4. **Concurrency: no locking anywhere** — concurrent installs mutually
   destruct (detected, fail-closed); concurrent uninstalls can lose a state
   entry. Single-user serialized use only (documented in the how-tos).
5. **`InstallPipeline.install_mod` is the entry point; the raw APIs do not
   gate (residual of #22/#23, stated plainly):** calling
   `Installer.install()` directly SKIPS the compat grade and the
   path/keybind/hook/dependency conflict gate, and its manifest is not the
   uninstaller's record — the pipeline converts and gates. The same applies
   to driving `Uninstaller`/`conflicts` by hand. (Case-conflict detection
   IS wired inside installer.py itself.)
6. **Static-analysis blind spots that remain:** dynamic dispatch invisible
   and unflagged (#14); marker detection bypassable (#9); `.lua` content
   not binary-sniffed (#7); MAX_ENTRIES truncation surfaced in `notes`
   only (#8). (Syntax errors ARE now detected — #15 fixed by the
   `does not parse` grade.)
7. **Smaller open items:** verify keys on absolute `install_path` (#5;
   `install_rel` now recorded); level-N Lua comment stripper bug in
   conflicts (#10); uninstall move-before-journal-record window, narrowed
   to per-file granularity (#11); `rtype` not tab-guarded (#17); byte-based
   live truncation (#18).
8. **7z/RAR rejected** with host-side conversion guidance (no converter
   shipped); pak/IoStore mods classified `needs-conversion`; UE4SS C++ DLL
   mods classified but not runnable; UE4SS runtime zips refused as mods (by
   design).
9. **Frozen `src-*` provenance originals keep their authoring-time absolute
   output paths** (build scripts would write to their original locations if
   re-run). The tester harness itself is NO LONGER in this category: it was
   ported to path-relative resolution in round 5 and re-runs green (88/88).
10. **Stale-docs status after the final count audit (2026-10-07):**
    `docs/REQUIREMENTS-TRACE.md` was refreshed in the final round-5 pass
    (all 15 rows PASS, current wording). The count audit then corrected
    every remaining stale number: `README.md` (catalog 106 entries; test
    status 63 + 214 + 88 = 365; 106/106 blank; adapter/tester notes),
    `docs/HOWTO-GPT.md` (106 globals, 599 merged), `docs/ANSWERS.md`
    (106/106 blank; checker 63/63; round-5 grade notes; Q2 qualified as
    the round-2 review-time state), `docs/COUNTS.md` (coverage rows 12–14
    recounted to the rev-3 snapshot: 163/150/419). Frozen snapshots
    (`REVIEW-PACKAGE.v1/v2/v3.md`, `catalog/backup-before-upstream-refs-*`,
    dated run reports, `src-*` originals) keep their authoring-time
    numbers as labeled history. `docs/COUNTS.md` and this file are the
    authoritative numbers.
11. **The Phase-0 pre-download badge and the wire envelopes in the contract
    are proposal-only** — not implemented (the checker has no `--mode
    predownload`); build against the APIs in Section 12.
12. **Commit state:** round 5 was committed and pushed as `a0353ff`
    ("Round 5 final: … 365/365 verified, coverage map rev 3 …"); the
    count-audit corrections listed in item 10 are local working-tree
    edits on top of it, uncommitted at audit time.

## 10. OPEN QUESTIONS

The three open review questions are answered in `docs/ANSWERS.md` — each
summarized here with CURRENT status (the answers' UPDATE notes were written
mid-round and partially superseded, Section 9 item 10):

### Q1 — The 2 system-level BLOCKERs (#22/#23): what, who, fix?

**IMPLEMENTED** (round 3). `installer/adapter.py` ships the production
`InstallPipeline`: unified `install_state/<TITLE>.json`,
`installer_manifest_to_uninstall()` reconciliation, post-commit ploader
cleanup (shared-line aware, rollback-snapshotted), and the conflicts gate
(adapter.py:648) with documented refusal policies (ue4ss-runtime /
runnable=no refused; `unsupported` compat blocks; PATH_CONFLICT-class
blockers refuse; keybind/hook warn). Pinned by 32 adapter tests in the 277.
Ownership was entirely this project's — all three endpoints live in
`installer/`.

### Q2 — The 6 APIs TownfallItemSpawner uses that are not in the catalog

They were `Key.F7 / UP_ARROW / DOWN_ARROW / RETURN / LEFT_ARROW /
RIGHT_ARROW` — constant references into the UE4SS `Key`
Windows-virtual-key table. **Catalog gap closed in v4** (`Key` 165
constants + `ModifierKey` 3 active, TYPE entries, `pc_only_input: true`);
the checker resolves the specific constants and reports the PC-ONLY INPUT
difference. No per-constant function entries were added (that would have
been the wrong fix). Follow-on: the keyboard→controller mapping strategy
(open question 9 below).

### Q3 — The test that checks the Townfall grade automatically

`TestTownfallItemSpawnerGrade` in `compat/test_checker.py`, now pinning the
round-5 verdict: **`test_townfall_grade_unknown_with_keyboard_note`** (THE
required test) + `test_townfall_inputs_exist` +
`test_townfall_blank_status_core_apis` +
`test_townfall_six_key_constants_are_pc_only_input`. Each grade-policy
change has rewritten the pin (round 3 `unknown` → round 4
`partly-compatible` → round 5 `unknown + KNOWN PS5 DIFFERENCE`) — the pin
tracks the shipped artifact by design.

### Remaining open questions (for GPT and the mod manager)

From `docs/CONTRACT-PROPOSAL.md` §8 (single merged list, 9 items) and
`docs/ANSWERS.md` Q4/Q5:

1. Where do manifests live? (Proposal: `/data/modbridge/roots/<TITLE>/manifests/`.)
2. Who owns UE4SS-settings.ini? (Proposal: GPT; installer writes overrides only.)
3. Load order: installer computes, runtime enforces — is loadlist row order
   = load order? (mods.txt rows also carry load order.)
4. Hot-reload: if the runtime ever supports mod hot-unload, the installer
   needs a notification API.
5. Catalog versioning: who bumps `catalog_version` when GPT fills statuses;
   does the manager auto-regrade on a bump?
6. Phase-0 grade names: will the manager accept `unverified-likely-*` /
   `likely-*` outline badges (provisional never renders as verified)?
7. `compat_manifest.json`: will mod authors ship it; will the manager offer
   the submit-back flow?
8. `Key`/`ModifierKey` are cataloged — should other enum/constant tables be
   cataloged the same way?
9. **PC keyboard → PS5 controller mapping** (`docs/ANSWERS.md` Q4, contract
   §6.4/§8.8): is PS5Pad.ini chord mapping (L3+R3 = F7) the general answer
   for arbitrary `Key.*` constants and ModifierKey combinations, or does
   the runtime need a translation layer? What happens when a mod registers
   a keybind the config doesn't map (today: registers but never fires —
   should it warn)? Decides the eventual `ps5_runtime_status` of
   `Key`/`ModifierKey`/`RegisterKeyBind`/`IsKeyBindRegistered` and whether
   keyboard-dependent mods can ever grade above `partly-compatible`.
10. **Lowercase mod paths — runtime lookup vs installer refusal**
    (`docs/ANSWERS.md` Q5, contract §8.9): a mod shipping lowercase
    `scripts/main.lua` works on Windows (case-insensitive filesystem) but is
    refused by our installer. Should the runtime look up mod files
    case-insensitively on PS5, or should the installer keep refusing? The
    installer never renames mod files (exact-case `Scripts/main.lua`
    requirement in `find_mod_root()` + `CaseConflictError` via
    `detect_caseConflicts()`/`_runtime_case_notes()`).

## 11. PROPOSED CONTRACT CHANGES (merged, one document)

`docs/CONTRACT-PROPOSAL.md` **v1.1 (merged)** is the single proposal (the
former `compat/CONTRACT-PROPOSAL.md` was folded in and deleted; frozen
original kept at `compat/src-compat-checker/CONTRACT-PROPOSAL.md`;
`compat/README.md` points there). Proposal-only: nothing integrated,
nothing sent; the canonical contract is owned by the mod manager team.

- §1 component boundaries (+`ps5_runtime_status` owner row: GPT fills,
  never authors structure) · §2 five core interfaces (install / uninstall /
  compatibility-check / config entries / conformance fill-in) · §3 ONE grade
  vocabulary incl. Phase-0 provisional names · §4 the two-phase pre-download
  badge contract · §5–7 requests to manager / GPT / catalog maintainers ·
  §8 the merged open-questions list (9 items, incl. keybind mapping and the
  lowercase-paths question) · §9 implementation checklist.
- Round-5 note: the contract's grade-vocabulary section predates the
  `does not parse` grade and the round-5 unknown-precedence policy; the
  shipped ladder is Section 5 of this file / `compat/checker.py`'s
  docstring. Vocabulary refresh is a follow-up when the contract is next
  revised.
- Wire schemas: install/uninstall requests + results, badge envelope
  (`schema: 1`, `confidence`, `blockers` with runtime-side workarounds),
  caching key `(nexus_mod_id, file_id, archive_sha256, catalog_version,
  checker_version)`, and the honesty rules (unknown never guessed;
  description-only never shown verified). **The wire envelopes are
  proposal-only** — the shipped surface is Section 12.

## 12. HOW-TO-CALL

Full guides: `docs/HOWTO-GPT.md` (runtime owner) and
`docs/HOWTO-MOD-MANAGER.md` (manager integration — updated this round to
lead with the adapter).

### For GPT (runtime owner)

1. **Fill `ps5_runtime_status`** in `catalog/globals.json` (106 entries) and
   `catalog/types.json` (42; methods inherit their type's status) after
   console testing. Values: `implemented+console-proven` /
   `implemented-untested` / `missing` / `behaves-differently` (input tables
   may instead carry the `pc_only_input: true` flag, as `Key`/
   `ModifierKey` already do); blank stays "unverifiable" and — per the
   round-5 policy — forces `unknown` for any mod using the API. Highest
   value first: `RegisterKeyBind`, `ExecuteInGameThread`,
   `StaticFindObject`, `FText`, `StaticConstructObject`, `FName`,
   `FindFirstOf`, `LoadAsset`, `print` (the 9 blank APIs of the Townfall
   grade), plus `Key`/`ModifierKey`/`IsKeyBindRegistered` once the
   controller-mapping strategy is decided (open question 9). Then
   `python3 compat/merge_catalogs.py` and re-run §3.6 to re-grade
   TownfallItemSpawner. Bump `catalog_version` whenever statuses change
   (badge caches key on it).
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
2. **THE entry point — `InstallPipeline.install_mod`**
   (`installer/adapter.py:666`), Python API:

   ```python
   import sys; sys.path.insert(0, "installer")
   from adapter import InstallPipeline

   pipe = InstallPipeline(plugins_dir=".../plugins",   # <game_root>/plugins default
                          state_dir=".../state",       # <game_root>/state default
                          catalog_path="compat/catalog-real-merged.json",
                          game_version="1.004.000")

   res = pipe.install_mod("mod.zip", {"name": "My Mod", "version": "1.0"},
                          title_id="PPSA33286", game_root=".../roots")
   # res.ok / res.stage / res.refusal / res.manifest / res.verify /
   # res.compat (grade) / res.conflict_blockers / res.conflict_warnings /
   # res.warnings / res.state_path
   ```

   or CLI: `python3 installer/adapter.py install|check|uninstall|verify
   --archive … --metadata … --title … --game-root … [--plugins --state
   --catalog --game-version]` (exit 0 ok / 2 refused/failed / 3 verify).
   The pipeline refuses ue4ss-runtime and runnable=no archives, blocks on
   `unsupported` grades (optional strict mode blocks `unknown` too) and on
   conflict blockers, warns on keybind/hook conflicts, maintains the unified
   install state, and returns structured Install/Check/UninstallResult
   objects. Companions on the same object: `check_mod` (dry run, zero
   writes), `uninstall_mod`, `verify_mod`, `installed_mods`,
   `register_existing` (adopt installs made directly through installer.py),
   `rollback_uninstall`.
3. **Do NOT call `Installer.install` directly for user-facing installs.**
   The raw `Installer` class (`installer/installer.py:1258`) is the
   transactional write engine the pipeline wraps; invoked alone it SKIPS
   the compat gate and the path/keybind/hook/dependency conflict gate (it
   retains only its own case-conflict check), and its manifest is not the
   uninstaller's record — the pipeline does that conversion. `register_existing`
   exists precisely to adopt installs that somehow bypassed the pipeline.
4. Low-level modules remain callable for tooling: `compat/checker.py`
   (`scan(mod_path, catalog_path)` or CLI with `--json`; catalog from
   `python3 compat/merge_catalogs.py`; `sample_catalog_TEST_FIXTURE.json`
   is invented-status test data — never ship badges from it);
   `installer/detector.py` (`classify_archive` → 9 labels);
   `installer/uninstall.py` `Uninstaller`; `installer/conflicts.py`
   `check_install()/check_batch()`.
5. Serialize installs/uninstalls per title root (no locking exists).

## 13. SOURCE ATTRIBUTION (upstream vs port, pinned version)

Pinned subject: **upstream UE4SS 3.0.1** (vendored
`ue4ss-unreal-native-build-1/.../UE4SS/`, `version.cache` = 3.0.1.0.0;
cross-check clone `ue4ss-binding-research-1/upstream/RE-UE4SS/UE4SS/`).
Port (PS5/Prospero) = `.superpowers/native-dependency-spike/
native-coherent-runtime-1/source/port/ue4ss/`. All trees read-only. Audited
in `compat/real-catalog-run.md` + `docs/CLEAN-RUN-REPORT.md` §3; the
remediation was APPLIED in round 5 by `catalog/update_upstream_refs.py`
(pre-edit backups in `catalog/backup-before-upstream-refs-2026-10-07/`).

- **Both catalog files are versioned wrappers** —
  `{catalog_version: "2026-10-07-v4", ue4ss_version: "3.0.1",
  ue4ss_source, citation_tree: "upstream", entries}` — and entries carry
  **real upstream citations** (applied, no longer "documented remediation"):
  `upstream_reference` (globals; comment-aware scan of upstream
  `LuaMod.cpp`/`Signatures.cpp`/`LuaLibrary.cpp` for live
  `register_function`/`lua_setglobal`/`make_global` sites; dotted names
  resolve via the last `add_pair` before the table's `make_global`) and
  `upstream_source_file` + `upstream_implementation` (types/methods; 32/34
  cited port files are line-identical to upstream, the two divergent files
  — `src/Mod/LuaMod.cpp`, `src/LuaType/LuaUObject.cpp` — resolved by
  unique whole-file content match). 142 entries carry the attribution note
  (98 v2 globals + 42 types + the 2 v4 input tables); the 6 Rev-2 additions
  cite via their `upstream_reference` fields.
- **Port citations kept as the secondary reference**
  (`registration_sites` / `source_file` / `implementation`, port line
  numbers); `citation_tree` distinguishes the two.
- **Port additions honestly labeled:** `loadfile`/`dofile` are marked
  PORT-ONLY ADDITION (absent from vendored 3.0.1; verified during the
  re-derivation, none left TBD); port/PS5 behavior lives in `notes`, not
  `pc_only_behavior`.
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
  Mac-only to /tmp) — full 46-file listing pinned at
  `docs/UE4SS-RELEASE-LISTING.txt`; the detector's flat/nested layouts and
  default-mods list are derived from it.
- **Conformance scripts:** call lists derived by static reading of the same
  port tree, cross-checked against upstream (per-section provenance in the
  two conformance READMEs); host-tested only against mocks, on Homebrew
  Lua 5.5.1 (the bundled 5.4.7 binary was removed from the repo after the
  recorded 5.4.7 results; mock totals re-verified identical on 5.5.1).
- **TownfallItemSpawner mod source:** read-only `git show` from the
  mod-manager worktree branch `origin/claude/townfall-item-spawner-hyj9eg` @
  `2e1ef8be267c4bf811f83f2da168cd52e6b0e16b`; 25/26 copies byte-identical
  (sha256 `f586043a…b6559658`); staged at
  `compat/townfall-item-spawner-mod/Scripts/main.lua`. The runtime and the
  manager branch were read, never written.
- **Everything Mac-only / host-tested.** No console contact anywhere in
  this package.

## 14. COUNTS (one authoritative set)

From `docs/COUNTS.md` (the count sheet — every catalog number in this
package is one of its rows; test numbers are from the fresh run, raw log):

| quantity | value |
|---|---|
| global entries | **106** (98 v2 + 6 conformance v3 + Key/ModifierKey v4) |
| types | **42** |
| method entries (raw) | **456** |
| metamethods | **20** |
| type rows | **476** |
| API rows (coverage convention) | **582** (rev-3 coverage snapshot: **582**) |
| raw catalog rows | **624** |
| merged-catalog entries (as shipped, v4) | **599** (= 103 globals-after-fusion + 42 + 454) |
| attribution-note entries | **142** |
| coverage: covered / invoked / presence-only / not-covered | **163 (28.0%) / 150 (25.8%) / 13 / 419** of 582 |
| blank `ps5_runtime_status` | **106/106** globals, **476/476** type rows, **599/599** merged |
| conformance CALL rows | **353 static + 86 live = 439** (+6 live PREQ rows) |
| unit tests (fresh run, 16:10) | **277/277 PASS** (214 installer-side + 63 checker) |
| independent tester suites (ported, live re-run) | **88/88 PASS** (0 fail, 0 error) |
| TownfallItemSpawner grade | **unknown** — runtime status not filled (9 APIs) + KNOWN PS5 DIFFERENCE (PC-only input, 6 constants) |

Rules the sheet enforces: **456** is the method count, 476 is
methods+metamethods, 454 is the merged-method figure; **582** is both the
coverage-convention total and the rev-3 snapshot row count; **599** is the on-disk merged file;
**142** is not a size — it is the attribution-note count. If any entry is
added again, recount the sheet first.
