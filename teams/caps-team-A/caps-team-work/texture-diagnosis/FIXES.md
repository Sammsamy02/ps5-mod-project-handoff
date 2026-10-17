# FIXES: Team A texture diagnosis, after the independent review (REVIEW.md)

Everything here is Mac-only. No console contact, no build, no edits outside this folder. The live
`outputs/2026-10-08-l25-prep/pak-overlay/` files were only read: sha256 of all 7 files is the same before and after the
final run.

Test evidence: `tests/run_tests.sh` → `RESULT: PASS` (`tests/test-output.txt`). 58 tests: 26 of ours
(hashes 3, reader 4, diagnosis 10, rewrite 4, fetch 2, plan 3) and 32 in the patched overlay suite. There are 32 count
categories, all > 0. All 11 read-only inputs are unchanged.

| # | Finding | Status | What changed | Test evidence |
|---|---|---|---|---|
| 1 | BLOCK: the "Leon off" card left the files inside `paks/` | **fixed** | `LAUNCH-CARD.md` rewritten. The files move to a folder **outside** `content/paks/`, following `../PRISTINE-PAKS.md`. If that file is missing, the card says stop. The card then LISTs the whole `paks/` tree (subfolders too): no `zzz_Leon` file may be left, and the game paks must match their pristine checksums. In game, the player must look like the normal character with no checker; if not, the result is **INVALID**. Also added: console offline, decrypt-off readback, row-check, "not in the current order, you decide". | Doc change. Note: `../PRISTINE-PAKS.md` does not exist yet; the integrator writes it. |
| 2 | FIX-SOON: the material gate could never say OK | **fixed** | New `material_verdict()` in `tools/texture_diag.py`. It says OK when the mod's own store entry has shader-map hashes, or when the export carries the game's own platform tokens (read from the game's files). Instances resolve their mod parents recursively. Every row now has `will_draw_checker`. | `test_verdict_table`: 12 cases, 6 positive. `test_positive_control_real_container_with_planted_store_hashes`: the real container, with game store hashes planted on 7 of the 13 masters (scratch copy). 30 materials flip to OK and 37 stay negative, exactly as predicted. The Leon verdicts are unchanged (54 INHERITS + 13 NO_PS5, same as before the fix). |
| 3 | FIX-SOON: hard-coded `"townfall/content/"` | **fixed** | The blast radius now compares PackageIds (`dep_ids` vs the mod's own package ids), so no path is rewritten. The game name was also removed from the module docstring and from a `material_plan.py` comment. | `test_tools_name_no_game_or_mod`: it takes the project folder and the mod folder **from the data** and scans 8 tool files; no game or mod name is found. `non_mod_root_packages` output is identical to before (157 outside-mod dependents). `test_outside_mod_dependents_use_package_ids`: 442 dependents. |
| 4 | FIX-SOON: option B ignored static-permutation shader maps | **fixed** | `material_plan.py`: each B row gets a `static_permutation_check` (own shader tokens, own store hashes). Per candidate master it also counts the game's packages importing that master and how many carry their own store hashes. An instance with its own permutation becomes `B_BLOCKED_OWN_STATIC_PERMUTATION` and is never an option-B row. | `test_every_reparent_row_has_static_permutation_check`: 11 rows and 66 master rows. Leon: 0 instances have their own permutation. `test_own_static_permutation_blocks_reparent`: 4 cases. Limit: static parameters are inferred from the cooked permutation evidence, not read by name (no schema on the Mac). |
| 5 | FIX-SOON: no end-to-end successful closest-match | **fixed** | New `test_build_overlay_admits_rewritten_container_end_to_end`. It rewrites the 11 B instances toward a real base master chosen from data (the name→id mapping is proven, not assumed), packs with retoc, and runs a full `build_overlay`. The ordering is written down in `PATCH/APPLY.md` §3. | The test asserts that the derived container's own store entry still names the OLD parent, then that 11/11 rows are admitted as `derived-import-repoint` with the new parent in and the old one out. `validate_lookup` passes and mod payloads stay verbatim. |
| 6 | FIX-SOON: the APPLY gate rewrote a SUSPECT file | **fixed** | In the staged diff, the Leon replay test writes `leon-replay-drift.json` to `$OVERLAY_TEST_OUT` (else a temp folder), never next to itself. `run_tests.sh` builds the relocated copy from `PATCH/new/` on every run (the stale static `PATCH/run-new/` was removed) and sends outputs to `tests/out/overlay/`. APPLY §0 makes this relocated run the only gate. | The drift receipt lands in `tests/out/overlay/`. The live `pak-overlay/` sha256 is unchanged (7 files). The live `test_overlay_builder.py` was added to the read-only input check. |
| N1 | NOTE: the name parser ignored NumStringBytes; UTF-16 errors raised `UnicodeDecodeError` | **fixed** | `zen_imported_package_ids`: a bad UTF-16 name and any NumStringBytes mismatch now raise `Refused("closest_match_package_unparsable")`. | The e2e test (11 packages) and the guard test (54 packages) parse real headers with the new check. 32/32 overlay tests pass. |
| N2 | NOTE: the diff did not create `cityhash64.py` | **fixed** | The diff now has a new-file hunk; the `cp` step is gone. | Applied to copies of the live files: clean, and byte-identical to `PATCH/new/` (all 3 files). Diff sha256 `d2d49748…a9e3`. |
| N3 | NOTE: "no engine detours" over-cited lines 794/941 | **fixed** | `DIAGNOSIS.md` now says the lines prove only that the LoadMap detour stayed off. "No other detours" rests on the build-time no-detour gate (Mac-only). | Doc change. |
| N4 | NOTE: card should say offline / readback / row-check / scheduling | **fixed** | In `LAUNCH-CARD.md`. | Doc change. |
| N5 | NOTE: `FAIL: zero items…` alarms a non-programmer | **fixed** | The negative test captures that output and prints it as `(expected, negative test) …`. | Visible in `tests/test-output.txt`. |
| N6 | NOTE: launch-14 log markers exist | no change needed | — | — |
| N7 | NOTE: fetch ranges are IoHash-checked; base `.utoc` dated 2026-10-01 | no change needed | Already fails closed with `IOHASH_MISMATCH` after a game update. | — |
| N8 | NOTE: test fixtures name game assets | no change needed | Allowed in tests. | — |

## Test runner proof (rule: print the count, fail on zero or on any failure)
`tests/run_tests.sh` now prints the test count for each suite and the total. It fails on: any failed test, any skipped
test, a suite that ran zero tests, a missing "OK", any zero item count, no counts at all, or a changed read-only input.

Planted run on a scratch copy (`tests/planted-failure-proof.txt`; the copy was deleted afterwards). It had 5 plants:
a failing test, a skipped test, a zero-test module, a zero count, and a failing overlay test. The runner flagged all 5,
ended with `RESULT: FAIL`, and exited 1.

## Also updated
- `DIAGNOSIS.md`: added a pointer to the integrator's finding that the bright outdoors comes from the changed
  `PS5Engine.ini` (`../CONSOLE-FACTS.md`). Fix-plan item 3 is marked done.
- `NOTES.md`: tool notes and scale.
- `out/texture-diag-report.json` and `out/material-plan.json` were regenerated with the fixed tools (same Leon verdicts).
