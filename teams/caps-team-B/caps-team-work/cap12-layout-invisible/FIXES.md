# Fixes folded in after the review (all 12 findings)

The Leon result is unchanged after the fixes: 3 reparent, 4 reparent-partial, 50 refused (21 of them never drawn), and the target is still `M_Hide`.

Tests: **59/59** (`tests/test_invisible_route.py`) and **11/11** (`tests/test_layout_tools.py`).

| # | Fix | Test |
|---|---|---|
| 1 | Property blocks are read through `present_indices()`. A missing export or a `PropsError` is a refusal with its reason (`present_why`), never a crash | `test_flag_unreadable_block_refused` |
| 2 | Prints `checked N mod Materials; M proven to draw nothing`. Exit 1 when N = 0 | `test_zero_mod_materials_gate_fails` |
| 3 | The usage flag is learned once on all game materials. The tool prints the index and its counts (index 53, absent in 32 of 37 static-only masters) and fails if none is learned. Per candidate it is learned again without that candidate, and the candidate is refused if the index differs | `test_flag_unstable_refused`, `test_usage_flag_reported` |
| 4 | `decide_row` picks the best-ranked proven target with the SAME blend group as the mod parent; otherwise it refuses. The report lists the blend groups of the proven targets | `test_target_blend_group_must_match_parent` |
| 5 | `coverage` stays 1.0 (no parameter lost). New field `user_coverage` (share of users proven). `partial_reason` says the `allow-partial` decision also covers this. NOTES and APPLY say the user must be told | `test_mixed_users_partial` |
| 6 | New optional `--script-utoc/--script-ucas` (the game's global ScriptObjects). Mod-only packages get their class from their top-level export's script import. Without it they stay `unknown` (fail closed). `run_invisible.sh` passes it | `test_unknown_class_counted`, real run |
| 7 | `blend_group` is a whitelist (Opaque / Masked / Translucent); anything else is refused | `test_blend_whitelist`, `test_additive_target_refused` |
| 8 | `effective_users(state=...)` reports a truncated walk, and `decide_row` refuses it | `test_depth_truncation_reported`, `test_truncated_users_refused`, `test_cycle_terminates` |
| 9 | Missing package ids are refused in `decide_row` | `test_missing_ids_refused` |
| 10 | `--out` is written only on success | `test_zero_rows_gate_fails_and_writes_nothing` |
| 11 | A name that also has a negation token (`not`, `non`, `no`, `un`) is never a name source | `test_no_false_tokens` |
| 12 | All the listed gaps are now tests. Running Team A's `derive_substitutes.py` on the applied plan (from a temp copy of its tools) derives 7 rows to `M_Hide` | `test_derive_substitutes_accepts_the_rows`, `test_overridden_importer_that_still_imports_counts_once`, `test_one_below_min_absent` |

On the reviewer's remaining risk ("draws nothing" rests on the name): the console check is in `NOTES.md` (Q2, "In-game check"). The launch must confirm that the 7 parts show **nothing**, not a flat colour.
