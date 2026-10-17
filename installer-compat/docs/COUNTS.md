# COUNTS.md — the one authoritative count sheet

Recounted 2026-10-07 (Mac-only, static analysis, no console contact) directly
from the tree. Every number below was recomputed from the JSON sources at the
time of writing; re-verify with the commands at the bottom. **When any doc in
this package cites a count, it cites one of the rows below, by these exact
definitions.** Historical confusion resolved here: "456/454/476" were three
different quantities, and so were "591/574/140" — none of them was wrong, they
were measuring different things.

**Recount v3 (2026-10-07 ~15:15, by the final review pass):** the conformance
Rev-2 work added 6 new globals to `catalog/globals.json` (`EGameThreadMethod`,
`EngineTickAvailable`, `ProcessEventAvailable`, `NAME_None`,
`ue4ss_loaded_modules`, `__OriginalReturnValue`; catalog_version
`2026-10-07-v3`), superseding the earlier 98-globals numbers. Per this
sheet's own Section-4 rule the sheet was recounted after that change.
`compat/catalog-real-merged.json` on disk was built from the previous
98-globals catalog and is PENDING rebuild (statuses are blank in both
versions, so no grade outcome changes); rows 8–10 give both the as-shipped
and post-rebuild figures, clearly labeled.

**Recount v4 (2026-10-07, later the same day, Key/ModifierKey work):** the
`Key`/`ModifierKey` input-table task added 2 TYPE entries to
`catalog/globals.json` (`Key` with 165 constants, `ModifierKey` with 3
active constants; both `kind: "type"`, `category: "input"`,
`pc_only_input: true`, blank `ps5_runtime_status`, `needs_live_game: false`)
and bumped both catalog files' `catalog_version` to `2026-10-07-v4`
(types.json: version field only). `compat/catalog-real-merged.json` was then
REBUILT (this also picked up the pending v3 rebuild, which is why `print`
now appears in the Townfall grade's status-blank list). Coverage rows
(12–14) are recounted from the rev-3 `docs/coverage.json` (582 rows; the
two v4 TYPE containers are mapped to the static suite's existing constant
spot-check rows, not new function/method rows).

## 1. Authoritative numbers

| # | quantity | value | formula / source |
|---|---|---|---|
| 1 | global entries | **106** | `len(catalog/globals.json["entries"])` (98 through v2; +6 Rev-2 conformance additions in v3; +2 Key/ModifierKey TYPE entries in v4) |
| 2 | types | **42** | `len(catalog/types.json["entries"])` |
| 3 | method entries (raw, all types) | **456** | sum of `len(t["methods"])` over the 42 types |
| 4 | metamethod entries | **20** | sum of `len(t["metamethods"])` over the 42 types |
| 5 | type rows (methods + metamethods) | **476** | row 3 + row 4 = 456 + 20 |
| 6 | API rows (coverage convention) | **582** (coverage.json snapshot: 582) | row 1 + row 5 = 106 + 476 (excludes the 42 type containers); `docs/coverage.json` rev-3 snapshot holds the same 582 rows |
| 7 | raw catalog rows (grand total) | **624** | row 1 + row 2 + row 3 + row 4 = 106 + 42 + 456 + 20 |
| 8 | merged-catalog globals | **103 as shipped** (v4 rebuild on disk; was 95 in the v2-based file, 101 projected for v3) | 106 − 3 fused string-ctor globals (FString, FUtf8String, FAnsiString fused into their type entries) |
| 9 | merged-catalog method entries | **454** | 456 − 2 collapsed intra-type override duplicates (AActor:GetWorld, UScriptStruct:IsValid — own override kept) |
| 10 | merged-catalog total | **599 as shipped** (rebuilt against v4: 103 + 42 + 454; was 591 v2-based, 597 projected for v3); metamethods deliberately NOT flattened (never invoked via `.:` scan syntax) | `len(compat/catalog-real-merged.json["entries"])` on disk = 599 |
| 11 | attribution-note entries | **142** | 98 v2 globals + 42 types + the 2 v4 Key/ModifierKey entries (which carry attribution notes); the 6 Rev-2 additions carry their citations in `upstream_reference` fields instead |
| 12 | coverage rows with `covered_by != "not-covered"` | **163 (28.0%)** | 93 static + 24 both + 46 live, from `docs/coverage.json` over its 582 rows (rev-3 map) |
| 13 | coverage rows actually invoked | **150 (25.8%)** | 163 − 13 presence-only |
| 14 | coverage rows `not-covered` | **419** | 582 − 163 |

## 2. Which number to use where (the disambiguation rule)

- **"How many methods does the catalog have?"** → **456** (raw method entries
  in `catalog/types.json`). **476** is never a method count: it is methods +
  metamethods ("type rows"). **454** is never a catalog count: it is the
  merged-catalog method figure after override-dedupe.
- **"How big is the catalog?"** → depends on the unit, use the label:
  - *raw catalog rows* (globals + types + methods + metamethods): **624**
  - *API/coverage rows* (globals + methods + metamethods, type containers
    excluded — the unit of COVERAGE-MAP.md and coverage.json): **582**
    (the rev-3 `docs/coverage.json` snapshot holds the same 582 rows,
    including the two v4 Key/ModifierKey TYPE rows)
  - *merged catalog entries* (`compat/catalog-real-merged.json`, what
    `compat/checker.py` consumes; metamethods excluded, ctor fusion and
    override-dedupe applied): **599 as shipped** (rebuilt against the
    106-globals v4 catalog; the earlier 591 file was v2-based)
- **142 is not a size at all** — it is the count of entries carrying the
  attribution note (98 v2 globals + 42 types + the 2 v4 Key/ModifierKey
  entries).
- Blank-status claims are always written against the right unit:
  `ps5_runtime_status` is `""` in **106/106** globals and **476/476** type rows
  (methods/metamethods inherit their type's status), i.e. **599/599** entries
  of the as-shipped merged file. (The Key/ModifierKey pc-only-input
  classification in the checker comes from their `pc_only_input: true` flag,
  not from a filled status.)

## 3. Merge arithmetic (why 599 as shipped)

```
catalog/globals.json      106 globals (v4; 104 v3; 98 v2)
  − 3 fused ctor globals  (FString, FUtf8String, FAnsiString
                            -> folded into their type entries)  = 103
catalog/types.json        42 types                                  = 42
  methods 456 − 2 collapsed override dupes
           (AActor:GetWorld, UScriptStruct:IsValid)                 = 454
  metamethods 20: not flattened by design                            —
                                                        rebuilt total = 599
as-shipped compat/catalog-real-merged.json (rebuilt from v4)        = 599
```

Cross-check: the `catalog_version` string inside the as-shipped
`compat/catalog-real-merged.json` ("106 entries, passthrough + 42 types
flattened + 456 method entries … ; 599/599 EMPTY ps5_runtime_status") counts
methods BEFORE dedupe (456); the entries array itself contains 454
method-kind rows. Both are correct for their unit.

## 4. Conformance-fix-agent check (UPDATED after Rev 2 landed)

The Rev-2 conformance work (2026-10-07, see
`conformance/src-conformance-live/README.md`) resolved the earlier follow-up
list as follows. **6 additions are now cataloged** in `globals.json` v3
(`EGameThreadMethod`, `EngineTickAvailable`, `ProcessEventAvailable`,
`NAME_None`, `ue4ss_loaded_modules`, `__OriginalReturnValue`; all with blank
`ps5_runtime_status`). `UClass:GetFunction` was **deliberately NOT
cataloged** — Rev 2 established it does not exist in this port (the live
suite now calls the real `ForEachFunction`/`GetSuperStruct`/`IsChildOf`/
`GetCDO` instead). The `Loop` table was **deliberately NOT cataloged** (no
such table in this port — scheduling is plain globals, which the live suite
now calls). `string.toFName` is not cataloged (the live suite probes it as a
labeled legacy-API absence probe). The `Key` constant table was uncataloged
at Rev 2 (see `docs/ANSWERS.md` Q2) and **is now cataloged in v4** (Key +
ModifierKey TYPE entries; see the recount-v4 note above and
`compat/townfall-item-spawner-grade.md`). Counts above reflect these files as
they stand; if entries are ever added again, recount and update this sheet
first.

## 5. Test-suite counts (ROUND-5 clean run, 2026-10-07 — current definitive)

Round-5 history: 196 → 242 → 267 (round-4 `/tmp/final-run-v2/` clean run) →
**365** (this round-5 `/tmp/round5-clean/` clean run). The round-5 run is ONE
single-agent, fully sequential pass from a fresh `cp -R` of the entire tree to
`/tmp/round5-clean/` (never used before; `__pycache__`/`.DS_Store` stripped
from the copy; no concurrent test runs; output captured unfiltered with
stdout+stderr merged). Raw verbatim output:
`test-results/raw-test-output.txt` (REPLACED by this run).

What changed vs round 4 (and why the total moved 267 → 365):

- `compat/checker.py` round-5 grading policy: a blank `ps5_runtime_status`
  now forces grade `unknown` outranking everything except parse errors
  (missing/PC-only-input findings ride along inside the result).
  `compat/test_checker.py` grew 55 → **63**.
- `installer/detector.py` UE4SS-release listing hardening; 
  `installer/test_detector.py` grew 50 → **52**.
- The independent tester's suites under `tests/src-tester/` were PORTED to
  the consolidated project layout (paths resolve relative to
  `framework.py`; `SIDEPROJECT_ROOT` env var optional override) and now RUN
  from the project tree — they are part of this clean-run total for the
  first time (previously hardcoded to `/tmp/sideproject/…` and skipped).
  Their historical narrative remains in `tests/REVIEW.md` and
  REVIEW-PACKAGE.md Section 8; the former suite_detector real-gap fail
  (Nexus URL regex) is fixed in this round (19/19 now PASS).

Per-suite totals are unittest's own `Ran N tests` / the tester framework's
own per-suite+totals lines from that run. All 365 tests pass; 0 failures,
0 errors, 0 skips.

| suite | tests | result |
|---|---|---|
| installer/test_installer.py | **55** | OK |
| installer/test_config_outputs.py | **11** | OK |
| installer/test_zip_safety.py | **21** | OK |
| installer/test_case_sensitivity.py | **23** | OK |
| installer/test_detector.py | **52** | OK (was 50; +2 UE4SS-release) |
| installer/test_uninstall.py | **11** | OK |
| installer/test_conflicts.py | **9** | OK |
| installer/test_adapter.py | **19** | OK |
| installer/test_adapter_edge.py | **13** | OK |
| compat/test_checker.py | **63** | OK (was 55; +8 round-5 policy) |
| tests/src-tester suite_installer.py | **27** | 27 PASS |
| tests/src-tester suite_detector.py | **19** | 19 PASS (was 18+1 real fail) |
| tests/src-tester suite_uninstall.py | **19** | 19 PASS |
| tests/src-tester suite_compat.py | **20** | 20 PASS |
| tests/src-tester integration_tests.py | **3** | 3 PASS |
| **TOTAL** | **365** | **365 PASS / 0 FAIL / 0 ERROR** |

Cross-checks: installer suites 55+11+21+23+52+11+9+19+13 = 214, matching the
combined installer invocation's own `Ran 214 tests`; 214 + 63 = 277 unittest
tests; plus the tester framework's own totals block `PASS: 88 / FAIL: 0 /
ERROR: 0` (27+19+19+20+3); 277 + 88 = **365**. The tester framework's
`review_static.py` static scans were not part of the port (their results
JSONs remain the frozen 14:08 artifacts) and are not counted.

## 6. Re-verify commands (pure local reads)

Note: since 2026-10-07 ~14:51 both catalog files are versioned wrappers
(`catalog_version`, metadata + `entries` array); the counts below read
`["entries"]`. The wrapper change did NOT alter any count.

```sh
python3 - <<'EOF'
import json
def entries(p):
    d = json.load(open(p))
    return d['entries'] if isinstance(d, dict) else d  # wrapper-aware
g = entries('catalog/globals.json')
t = entries('catalog/types.json')
m = sum(len(x.get('methods', [])) for x in t)
mm = sum(len(x.get('metamethods', [])) for x in t)
print('globals', len(g), '| types', len(t), '| methods', m,
      '| metamethods', mm, '| type rows', m + mm,
      '| API rows', len(g) + m + mm, '| raw total', len(g) + len(t) + m + mm)
merged = entries('compat/catalog-real-merged.json')
print('merged-as-shipped', len(merged))
cov = json.load(open('docs/coverage.json'))
rows = cov['globals']['entries'] + cov['types']['entries']
covd = [r for r in rows if r['covered_by'] != 'not-covered']
print('coverage rows', len(rows), '| covered', len(covd),
      '| not-covered', len(rows) - len(covd))
EOF
```

Expected output: `globals 106 | types 42 | methods 456 | metamethods 20 |
type rows 476 | API rows 582 | raw total 624`, `merged-as-shipped 599`,
`coverage rows 582 | covered 163 | not-covered 419` (coverage.json is the
rev-3 582-row snapshot; its 582 rows include the two v4 Key/ModifierKey
TYPE containers, matching the coverage-convention total exactly).
