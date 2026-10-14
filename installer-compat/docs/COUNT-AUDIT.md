# COUNT-AUDIT.md — final cross-check of every numeric claim

**Date:** 2026-10-07 (final pass, after all round-5 agents reached quiescence).
**Constraints honored:** Mac-only, no console contact, no test runs, no catalog
rebuilds. Edits were limited to numeric/current-state corrections in docs plus
this report. `src-*` frozen copies, `REVIEW-PACKAGE.v1/v2/v3.md`, the catalog
backup directory, dated run reports and `.git/` were audited read-only.

## 1. Method

1. **Quiescence:** polled file mtimes, `git status` and running test processes
   every 2–5 min from 15:14 until the tree was change-free for ~7 min and
   `git status` was clean at HEAD `a0353ff` (16:25). One late coverage-agent
   pass (coverage.json rev 3 + COVERAGE-MAP rev 3 + REQUIREMENTS-TRACE
   refresh, committed as `a0353ff`) landed during polling and was picked up.
2. **Ground truth:** every count was recomputed directly from the JSON
   sources with the wrapper-aware reader from `docs/COUNTS.md` §6 (globals
   106, types 42, methods 456, metamethods 20, type rows 476, API rows 582,
   raw total 624, merged 599, coverage 582 rows / 163 covered / 419
   not-covered), plus `docs/coverage.json`'s own `grand_total` block
   (163 / 150 / 419 / 28.0% / 25.8%) and the raw test log
   (`Ran 214 tests`, `Ran 63 tests`, `PASS: 88` → 365 total).
3. **Scan:** all 217 tracked files were scanned (text files only; binaries
   skipped) for every audit-relevant number appearing near a quantity
   keyword (global/function/type/method/metamethod/catalog/coverage/
   covered/invoked/tests/suite/entries/rows/constants/percent), then every
   hit was read in context and classified: **correct**, **correct for a
   different (labeled) quantity or time**, or **wrong**.

## 2. Authoritative values used (docs/COUNTS.md, rows verified against JSONs)

| quantity | value | recomputed | verdict on COUNTS.md |
|---|---|---|---|
| global entries | 106 | 106 | PASS |
| types | 42 | 42 | PASS |
| method entries (raw) | 456 | 456 | PASS |
| metamethod entries | 20 | 20 | PASS |
| type rows | 476 | 476 | PASS |
| API rows (coverage convention) | 582 | 582 | PASS |
| raw catalog rows | 624 | 624 | PASS |
| merged globals / methods / total | 103 / 454 / 599 | 101+2+44+454 = 599 on disk | PASS |
| attribution-note entries | 142 | 100 globals + 42 types = 142 | PASS |
| coverage covered | 163 (28.0%) | **163 (28.0%)** — COUNTS.md said 161 (27.8%), "91 static" | **FAIL — fixed in COUNTS.md** (actual: 93 static + 24 both + 46 live) |
| coverage invoked | 150 (25.8%) | **150 (25.8%)** — COUNTS.md said 148 (25.5%) | **FAIL — fixed in COUNTS.md** |
| coverage not-covered | 419 | 419 (582 − 163; the printed formula "582 − 161" was arithmetically wrong) | value PASS, formula fixed |
| tests (round-5 definitive) | 365 = 214 + 63 + 88 | raw log: `Ran 214` + `Ran 63` + `PASS: 88` | PASS |

The two coverage rows in COUNTS.md were stale against the rev-3
`docs/coverage.json` (regenerated ~16:13, after the sheet's last recount);
the file's own `grand_total` block, `docs/COVERAGE-MAP.md` rev 3 and commit
`a0353ff`'s message all say 163/28.0%/150/25.8%. Per the sheet's own charter
("recounted … directly from the tree"), rows 12–14 were corrected to the
recomputed values, along with four prose notes that still described the
coverage snapshot as pre-v4 ("580 rows", "predates v4").

## 3. Discrepancies found and fixed (editable files)

Legend: "said → fixed". Line numbers are pre-edit.

### docs/COUNTS.md
| line | said | fixed to | why |
|---|---|---|---|
| row 12 | 161 (27.8%), "91 static + 24 both + 46 live" | 163 (28.0%), "93 static + 24 both + 46 live" | recomputed from coverage.json |
| row 13 | 148 (25.5%) | 150 (25.8%) | 163 − 13 presence-only |
| row 14 | "419 = 582 − 161" | "419 = 582 − 163" | arithmetic + data |
| row 6 / §2 / §6 | "snapshot predates v4 and holds 580 rows" / expected output "covered 161" | rev-3 snapshot holds the same 582 rows / covered 163 | coverage.json rev 3 includes the two v4 TYPE rows |

### README.md
| line | said | fixed to | why |
|---|---|---|---|
| 103 | "globals.json 98 entries" | 106 entries | catalog v4 |
| 131 | "compat: 47 tests, all pass" | 63 tests, all pass | round-5 suite |
| 132–139 | "installer: 155 tests, 2 failures …" | 214 tests, all pass (9 files; per-suite 55+11+21+23+52+11+9+19+13) | 16:10 clean run; the two described failures were fixed rounds ago |
| 124–126 | test command omitted 3 of 9 installer suites | full 9-module invocation + tester framework line | command must produce the claimed 214/88 |
| 144–147 | tester harness "must be ported before it can run" | ported (round 5), 88/88 PASS | stale |
| 168 | "98/98 globals" | 106/106 globals | catalog v4 |
| 78–82, 171–174, 90–92 | "no official adapter shipped" / "conflicts not wired" / 8 label values | adapter.py `InstallPipeline` is the entry point (raw APIs skip the gates — matches REVIEW-PACKAGE §9 item 5); 9 enum values incl. `ue4ss-runtime` | adjacent prose corrected for consistency with the fixed numbers |

### docs/HOWTO-GPT.md
| line | said | fixed to |
|---|---|---|
| 25 | "98 global-function entries" | 106 global entries (98 + 6 Rev-2 + Key/ModifierKey) |
| 69 | "catalog-real-merged.json (591 entries today)" | 599 entries today |
| 74 | "all 591 merged entries have an empty status" | all 599 |

### docs/ANSWERS.md
| line | said | fixed to |
|---|---|---|
| 81 | "blank in 98/98 globals and 476/476 type rows" | 106/106 globals (476/476 unchanged) |
| 91–94 | Q2 preamble "current merged catalog … (98 entries, none named Key)" | qualified as the round-2 review-time state (v2 98-globals build), with pointer to the v4 UPDATE |
| 196–208 | Q3 UPDATE "Current suite: 55/55 OK. Current live grade: partly-compatible…" | round-5 UPDATE appended: `test_townfall_grade_unknown_with_keyboard_note`, 63/63 OK, live grade `unknown` |
| 158–160, 226–230 | round-4 "partly-compatible - PC-only input dependency" grade strings | labeled as round-4 policy (round 5: rides inside `unknown` as a KNOWN PS5 DIFFERENCE) |
| 5–8 | header framing | extended to mention the round-5 grade-policy reversal |

### docs/REQUIREMENTS-TRACE.md
| line | said | fixed to |
|---|---|---|
| 35 (row 4) | "ModType enum (8 values: …)" | 9 values, `ue4ss-runtime` added to the list (enum in installer/detector.py:191–200 has 9) |
| 35 (row 4 result) | "all 8 types classified" | all 9 types incl. `ue4ss-runtime` |
| 47 (row 11) | "(all 8 types incl. mixed/unknown …)" | all 9 enum values (8 mod types + ue4ss-runtime via row 4's release tests) |

### REVIEW-PACKAGE.md (the live final; v1/v2/v3 untouched)
| line | said | fixed to |
|---|---|---|
| 436–444 (§6) | "(v3-era snapshot, 15:07–15:11)"; globals row "104 / 68 / 13 / 81 / 23"; total row "580 / 148 (25.5%) / 13 / 161 (27.8%) / 419" | rev-3 map; globals "106 / 70 / 13 / 83 / 23"; total "582 / 150 (25.8%) / 13 / 163 (28.0%) / 419" (all match coverage.json's summaries and grand_total) |
| 449–452 | "snapshot is v3-era (580 rows) … no coverage rows yet … PENDING" | rev-3 map includes the v4 containers (582 rows; covered grew 161 → 163) |
| 584–587 (§9 item 2) | "161/580 … (27.8%) … 148 (25.5%)" | 163/582 (28.0%), 150 (25.8%) |
| 588–590 (§9 item 3) | "580 rows; … not yet mapped … Recount PENDING" | rev-3 snapshot, 582 rows, containers mapped |
| 614–621 (§9 item 10) | stale-sibling-docs list (README/HOWTO/ANSWERS/TRACE) | post-audit status: those docs corrected; frozen snapshots keep labeled history |
| 622–623 (§9 item 12) | "uncommitted … remote sits at fbeaae7" | round 5 committed+pushed as `a0353ff`; audit edits local |
| 477–479 (§7) | "rows 2/5/15 … superseded … trace file still carries older wording" | trace refreshed in the final pass (all 15 rows PASS) |
| 902 / 906 / 914 | "coverage snapshot: 580"; "161 (27.8%) / 148 (25.5%) / 13 / 419 of 580"; "582/580 differ by …" | snapshot 582; "163 (28.0%) / 150 (25.8%) / 13 / 419 of 582"; 582 = convention = snapshot |
| 14–15, 282, 321, 917 | run timestamp "16:01" | 16:10 (the raw log's own header: "Date: Wed Oct 7 16:10:00 CDT 2026") |
| listing sizes | COVERAGE-MAP 129554, coverage.json 193846, REQUIREMENTS-TRACE 13979, COUNTS 11620, ANSWERS 15371, HOWTO-GPT 7536, HOWTO-MOD-MANAGER 8521, raw-test-output 7402, README 8177 | 132088, 194569, 21757, 12533, 16815, 7615, 13519, 33147, 8525 (post-edit actuals; all other ~50 listed sizes re-verified and already exact) |
| 218–221 | corpus bracket "65 adversarial fixtures: detector/: 16 zips; installer/: 20 zips + 16 hostile config JSONs; lua/: 19; uninstall/: 12 manifests" | on-disk actuals: detector 15 zips, installer 18 zip archives + 18 config JSONs, lua 19, uninstall 14 manifests; `corpus.py` builds 65 cases (14+18+19+14) — the tester's "65" is the generator case count, not the file count |

## 4. Files checked and passing with no change (current-state claims)

- **docs/COVERAGE-MAP.md** — fully consistent (582/163/28.0%/150/25.8%/419;
  per-scope table 106/70/13/83/23 and 476/80/0/80/396; rev-1/rev-2 history
  rows explicitly labeled; "572 distinct + 10 dotted dupes = 582" verified).
- **docs/coverage.json** — data + `grand_total` block verified row-by-row
  (93 static / 24 both / 46 live; 13 presence-only).
- **compat/townfall-item-spawner-grade.md / .json** — 599/599 blank, 106
  globals, 42 types, 456 method entries (pre-dedupe, correctly labeled),
  Key 165 / ModifierKey 3 constants, 9 blank APIs, 63/63 checker tests.
- **catalog/globals.json / types.json** — metadata carries no count claims;
  Key signature "165 constants" verified (165/3 on disk).
- **catalog/SUMMARY.md** — "98 entries total" heading is correct-as-labeled:
  a prominent UPDATE note reconciles to 106 and defers to COUNTS.md.
- **catalog/update_upstream_refs.py, compat/merge_catalogs.py,
  compat/checker.py** — all counts computed dynamically at run time; no
  stale literals.
- **docs/HOWTO-MOD-MANAGER.md** (4096-entry zip limit only),
  **docs/SECURITY-SCAN.md** (10,000-entry bomb fixture),
  **docs/CONTRACT-PROPOSAL.md** (§8 has exactly 9 open questions),
  **conformance/README.md**, **conformance/*.lua** (the "353 calls" claim
  matches the coverage reconciliation block: 353 static CALL rows),
  **compat/sample_catalog_TEST_FIXTURE.json**, **compat/sample_mod.lua**,
  **docs/UE4SS-RELEASE-LISTING.txt**, **.gitignore**, all **installer/*.py**
  and **compat/test_checker.py** (per-suite counts 55/11/21/23/52/11/9/19/13/63
  confirmed by the raw log; the only doc reference in tests is a
  HOWTO-MOD-MANAGER comment — that file was not edited).
- **test-results/raw-test-output.txt** — data; 214 + 63 + 88 = 365 verified.

## 5. Historical / frozen files — audited read-only, numbers correct for their labeled time

- **REVIEW-PACKAGE.v1.md** (98 globals/456 methods, 137+88 tests) — first
  package, explicitly frozen.
- **REVIEW-PACKAGE.v2.md** (round-4 final: 267 = 212+55, 580-row v3
  snapshot figures) and **REVIEW-PACKAGE.v3.md** (round-5 intermediate:
  pre-port tester, 212-era installer counts) — frozen by the live
  REVIEW-PACKAGE header ("preserved frozen … do not cite its numbers").
- **catalog/backup-before-upstream-refs-2026-10-07/** — pre-edit backup.
- **catalog/src-catalog-globals/SUMMARY.md** (98), **src-catalog-types/**
  (42/456/20 — still true), **compat/src-compat-checker/**,
  **conformance/src-conformance-live/README.md** ("104 entries" — the v3-era
  count, frozen original), **conformance/src-conformance-static/**,
  **installer/src-***, **tests/src-tester/** (frozen-portrait parts; its
  REVIEW.md "65 adversarial fixtures" verified as the generator case count).
- **Dated run reports:** compat/real-catalog-run.md (14:18-era: 98/591/43
  tests), docs/CLEAN-RUN-REPORT.md (14:16–14:25: 144 tests, 96/98 globals),
  docs/CHANGES-ROUND4.md (round-4 changelog: 267/55/50/591→599),
  tests/REVIEW.md (tester narrative), tests/results/*.json and
  tests/src-tester/results/*.json (15:49 re-run artifacts; static scans
  frozen at 14:08 per COUNTS.md §5).

## 6. Flagged, deliberately not hand-edited

1. **compat/sample-mod-real-catalog-report.json** — generated artifact
   (`generated_at` 14:18) whose embedded `catalog_version` string reads
   "globals.json(98 entries …) 591/591". It truthfully describes the run
   that produced it, but that run predates catalog v3/v4. Hand-editing a
   generated report would falsify provenance; regenerating requires running
   the checker (outside this read-only audit). **Action for the owner:**
   re-run `python3 compat/checker.py compat/sample_mod.lua
   compat/catalog-real-merged.json --json compat/sample-mod-real-catalog-report.json`
   to refresh it against the 599-entry v4 catalog.
2. **Git commit messages** (e.g. `35d7e59` "104 globals … 242 tests",
   `6f096a5` "98+476 … 242", `38fd2bc` "196+ unit tests") — immutable
   history in `.git/`; each was accurate for its commit time.
3. **Non-numeric staleness noted but out of scope:** REVIEW-PACKAGE §9 item
   11 (proposal-only badge), §2's round-4 lineage notes — verified accurate
   as written.

## 7. Final verification (post-fix, pure local reads)

```
globals 106 | types 42 | methods 456 | metamethods 20 | type rows 476 |
API rows 582 | raw total 624
merged-as-shipped 599
coverage rows 582 | covered 163 | not-covered 419
raw log: Ran 214 tests … OK / Ran 63 tests … OK / PASS: 88  → 365 total
```

This matches `docs/COUNTS.md` §1/§6 exactly, `docs/COVERAGE-MAP.md` rev 3,
`docs/coverage.json`'s grand_total, and the REVIEW-PACKAGE quick-reference
table. Working-tree changes from this audit: `README.md`, `REVIEW-PACKAGE.md`,
`docs/COUNTS.md`, `docs/ANSWERS.md`, `docs/HOWTO-GPT.md`,
`docs/REQUIREMENTS-TRACE.md`, plus this file.

**Incident note:** during verification, importing `tests/src-tester/corpus.py`
regenerated the fixture files as a side effect (the build functions rewrite
them). All corpus files were restored to their committed state with
`git checkout -- tests/src-tester/corpus/`; the final diff contains only the
doc edits listed above. No tests were executed by this audit; the pinned
evidence remains the 16:10 round-5 clean run.

**Bottom line:** after the fixes above, every current-state numeric claim in
the package matches `docs/COUNTS.md`, which itself now recomputes exactly
from the JSON sources. The only intentionally retained divergent numbers are
(a) labeled historical quantities (456/454/476; 591/574/140; 580/161/148;
196/242/267; per-round test totals) and (b) the stale generated artifact
flagged in §6.1.
