# FIXES — review findings folded in (Team G)

| # | Finding | Fix | Proof |
|---|---|---|---|
| 1 | Medium: missing corlib/config → Mono aborts the game | Planner refuses the probe unless `<lib>/mono/4.5/mscorlib.dll` and `<etc>/mono/config` are regular files of the exact pinned sizes (4,634,112 / 3,097). Sizes come from the pinned asset rows via `compile_argv` (`MODBRIDGE_CAP10_CORLIB_BYTES/CONFIG_BYTES`, sha- and size-checked); `UE4SSProgram.cpp` `#error`s without them. Refusal is one `[CS] refused: …` line; Mono stays off. | target compile 4/4 (`build/tc2.log`); relink raw delta identical (151, 361, 0 sceNet); `.ctors` 1896 B unchanged, no TLS |
| 2 | Low: hidden visibility not effective on PS5 | Comments/docs corrected; post-package check added (APPLY.md §5b: exactly `module_start`/`module_stop` exported, 0 `sceNet*` either way). | docs |
| 3 | Low: label trusted | `admitted()` re-parses every row with the existing `provider_identity()` using the pinned provider entry (with its identity family) and requires identity, kind and encoded NID to match, plus non-empty `source_1`/`source_2`. | `test_team_g_gates` 8/8 (all 143 rows re-checked) |
| 4 | Low: pin chain start | `pin_chain()`: `typescan-r4/CHANGES.json` `cap10_row_sync.cap10_inputs` must equal the loaded INTEGRATION-INPUTS / ADMITTED-IMPORTS shas; called by `admitted()` (row-level unit tests on temp copies pass `_check_chain=False`). | gates + drivers tests |
| 5 | Low: catch blocks can throw | Both `[CS] refused` logs in `setup_mods` wrapped in `try{}catch(...){}`; flag already false there. | compile |
| 6 | Low: tests | GPT's superseded tests documented (TESTS.md). **Not done:** the host planner test was not updated to the new planner signature (it needs the runtime-files argument and real-image cases); it is stale and was not rerun after fix 1. Open item: Team G could not complete the rewrite in this session (the attempt was stopped by a safety classifier), so it is left to the integrator's reviewer pass. Coverage of fix 1 today: target compile + relink only, no host test. | — |
| 7–11 | Info | fs-throw: key comparison shows 0 added (scratch paths only mismatch the baseline prefix). #7-adds-0 assumption noted in GATES.md G7. Library choice noted in IMPORTS.md. Corlib residency: APPLY.md §5 / STARTUP.md (build Stellar Blade and rollback without #10). Launch card: mods.txt enable recommended; runtime-file refusal line documented. | docs |

Current pins after fixes: `UE4SSProgram.cpp` `b6a68e3f…`, `INTEGRATION-INPUTS.json`
`fe2a2063…`, patch `a4c18822…` (47 text files), signal pair and 4 binaries unchanged.
Patch + binaries reconstruct the scratch tree byte-for-byte.

Rebase (live R16d + #11 follow-ups): all fixes carried over unchanged; patch
`cap10-only-on-live-R16d-fu.patch` `784baed5…`; `INTEGRATION-INPUTS.json` `094f6120…`.
