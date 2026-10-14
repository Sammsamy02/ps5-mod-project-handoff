# Review of the invisible-route rule (one independent reviewer, Mac-only)

Reviewed: `PATCH/tools/invisible_route.py` (first version), `tests/test_invisible_route.py` (44 tests), and the real-data output `evidence/plan-r4-invisible-applied.json`.

**Verdict: APPROVE WITH FIXES. No blockers.**

## What the reviewer ran
- **Tests:** 44/44 pass. Team A's tools ran from a scratch copy, and no `__pycache__` appeared in any team folder.
- **Reproducibility:** a fresh `--apply` run gives JSON byte-identical to the evidence file.
- **Overlay:** Team A's `derive_substitutes.py` on the applied plan with `--allow-partial` derived all 7 rows to `M_Hide` (3 reparent, 4 reparent-partial). The only decision needed was `allow-partial`.
- **Usage flag:**
  - The property indices common to all 20 skeletal-used masters are {53, 140, 149}. 140 and 149 are present in all 37 static-only masters, so they do not count.
  - 53 is absent in 32 of the 37.
  - Leaving out each skeletal master in turn still picks 53 (20 of 20).
- **Import graph:**
  - no store package is missing from the registry;
  - both "overridden" sets agree (68 each);
  - a walk depth of 64 gives the same users as depth 8.

## Answers to the six questions
1. **Fail-open paths:** none found. Both mod-parent sources and every target gate are required, in order. The only bad path is a crash on an unreadable property block (finding 1).
2. **Effective users:** correct on the real data. Undercounting fails closed. Overcounting only turns `reparent` into `reparent-partial`, and on those other users the result is no worse than today's checker.
3. **Usage flag:** sound. The index is unique, leave-one-out excludes the candidate, and there is no leakage.
4. **Overlay fit:** yes (ran it). All fields are present, the store repoint follows the hash law, and partial rows need only `allow-partial`.
5. **Genericness:** no game, mod or asset names or values. The token list is English-only, so mods named in other languages fail closed.
6. **Tests:** gaps listed under finding 12.

## Findings
| # | Severity | Finding |
|---|---|---|
| 1 | SHOULD-FIX | `parse_header(blob)` crashes on a missing or malformed export instead of refusing |
| 2 | SHOULD-FIX | The mod-parent loop prints no count and never fails on zero |
| 3 | SHOULD-FIX | The docstring promises exit 1 when the usage flag cannot be learned, but the flag was only learned per candidate. Learn it once on all game materials (print it, fail if none), then leave-one-out per candidate (refuse if the index differs) |
| 4 | SHOULD-FIX | "Most skeletal users" is not a safe tie-break across games: a translucent constant "highlight" material could win. Require the target's blend group to equal the mod parent's |
| 5 | SHOULD-FIX | `coverage` changed meaning (users instead of parameters). Keep `coverage = 1.0` and add `user_coverage`. Tell the user that `allow-partial` now also covers "non-skeletal users unproven" |
| 6 | SHOULD-FIX | New mod-only meshes get no class, so they count as `unknown`. This fails closed, but it would refuse or demote a mod that adds its own skeletal mesh. Resolve the class via the global ScriptObjects |
| 7 | NIT | `blend_group` maps every unknown label to translucent. Use a whitelist |
| 8 | NIT | `effective_users` stops at depth 8 silently. Report it and refuse |
| 9 | NIT | Missing `_ids` gives None ids. Refuse instead |
| 10 | NIT | `--out` is written even on FAIL |
| 11 | NIT | Negated names (`M_NotHidden`, `M_Un_Hide`) match a token |
| 12 | SHOULD-FIX | Test gaps: the crash path; the zero gates; unknown class, cycle, truncation and override cases in `effective_users`; disagreeing targets; exactly one below `MIN_ST_ABSENT`; missing `_ids`; running `derive_substitutes` on the applied plan |

## Remaining risk named by the reviewer
The claim that `M_Hide` "draws nothing" rests on its name, with the blend mode only as a consistency check. The console launch must confirm visually that the 7 parts are invisible, not drawn as a flat colour.

All findings are fixed: see `FIXES.md`.
