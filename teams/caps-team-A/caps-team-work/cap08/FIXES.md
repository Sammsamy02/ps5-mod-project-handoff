# Fixes for the capability #8 review (Team A)

Everything here is **Mac-only**. Nobody touched the console, the candidate runtime tree, frozen builds or
mod files. No PRX was built. R11 `bfd95db6` stays the runtime. All the changes are in this folder.

Final host run: `bash tests/run_tests.sh` gives **59 tests, 0 failing, 0 skipped, 0 empty modules, PASS**
(`tests/test-output.txt`). Before the fixes it was 46.

## Findings

| # | Level | Result | What changed | Test evidence |
|---|---|---|---|---|
| F1 | FIX-SOON | **fixed** | `zzintake_template.lua` v2 probes every 30 s **until the game closes**. The 5-run stop is gone. A `P` line is printed only when it changes, and a `SUM` line every run. The card says to read the **last** `SUM` line. | `test_all_found_is_pass_and_probing_never_stops` (8 of 8 runs, no stop). `test_late_load_is_not_yet_then_pass`: objects appear at run 4, the first line says `NOT-LOADED-YET`, the last says `PASS`. |
| F2 | FIX-SOON | **fixed** | Probes are now chosen generically from the mod's own import graph (new `load_graph` in the report). First come the "linkers": replaced objects that import the most of the mod. Then come the mod's new objects that a linker imports. Each probe carries `b=` (which replaced probes import it). Live rule: if an importer is found, the object must be found (`f=0:missing` gives `state=MISSING`). Otherwise it is only `f=0:notloaded`, never a FAIL. Objects reached only from themselves are skipped (this drops `MI_HeadSynthesized_Baked*` and the non-optimised `Simon_Head_FaceMesh`). `par=0:none` is now its own state, `PARENT-UNREAD`, separate from `PARENT-WRONG`. For Leon: 21 probes (8 Simon body/jacket meshes as linkers, 13 new Leon objects, all linked), sha256 `b37fc12f…`. | `test_probe_set_is_linked_and_skips_self_only`, `test_unused_replaced_object_is_inconclusive_not_fail`, `test_new_object_missing_under_found_importer_is_fail`, `test_wrong_parent_is_visible`, `test_unread_parent_is_its_own_case`, `test_pure_add_mod_falls_back_to_unlinked_probes`. Real file: `test_diag_probes_are_linked` and `test_diag_runs_pass_under_stub` (Leon: all present gives PASS; Leon's new objects absent gives MISSING). |
| F3 | FIX-SOON | **fixed** | LAUNCH-CARD "Before the launch" now requires the loose `zzz_Leon_Townfall_2K_P.pak/.utoc/.ucas` to be removed or moved out of the paks folder (a console action, so it needs the user's OK), with a fresh read-only LIST confirming it. It points at `../PRISTINE-PAKS.md` for the clean paks state. That file does not exist yet; the integrator is expected to write it. | Card text. |
| F4 | FIX-SOON | **fixed** | `run_tests.sh` now FAILS on any skipped test, on any module with 0 tests, on a total of 0, and on any failure. It prints `skipped=` per module and in the total line. The module list is fixed (no override). `test_leon_real` now requires `luac`/`lua` instead of silently skipping that check. | Planted on scratch copies: failing test gives exit 1, skipped test gives exit 1, empty module gives exit 1 (`evidence/fixes-20261012/planted-*.txt`). |
| F5 | NOTE | **fixed** | `PATCH/APPLY.md` uses absolute paths (`CAP=` and `L25=` set once) in every step. The gate command was dry-run against the Leon report: it checks 5 base files and fails only on today's `INCOMPLETE` verdict, as expected. | Dry run (Mac). |
| F6 | NOTE | **fixed** | A fetched base chunk that fails its IoHash is now `INCOMPLETE ... re-fetch it (transfer error, not a mod defect)` and is re-planned. `REFUSED` is kept for defects in the mod. | `test_corrupt_fetch_is_not_used`: corrupt gives exit 4 with the chunk planned again; a good re-fetch then gives exit 0. |
| F7 | NOTE | **fixed** | `parent_candidates` now plans a fetch for base imports of unknown class (before, it skipped them silently), so an UNKNOWN chain always says what is missing. APPLY step 4 says a second fetch round may be needed. Leon's fetch plan stays byte-identical (89 chunks). | New `test_base_parent_chain_plans_each_missing_header` (mod MI, then base MI, then base MI, then base material) takes 3 rounds to `EXIT_OK`. With the old rule it fails "INCOMPLETE but nothing planned" (`evidence/fixes-20261012/old-code-fails-new-tests.txt`). |
| F8 | NOTE | **fixed** | (a) `intake_gate.py` now needs `--base-utoc` (each) and `--script-objects-utoc`, and checks them by sha256 against the report, so a report made before a game update fails. The intake records the ScriptObjects sha. (b) The gate needs `--record` and always writes a JSON record (PASS or FAIL, who accepted, all the shas). (c) `chunk_intake.py` deletes any old report and plan at `--out`/`--fetch-plan` before it runs. Any unexpected error (`KeyError`, `UnicodeDecodeError`, …) becomes `REFUSED` with a fresh report. | `test_other_base_files_fail`, `test_record_is_written_with_acceptance`, `test_unexpected_error_writes_fresh_refused_report`. |
| F9 | NOTE | **fixed** | NOTES 1(b) now cites the integrator's 2026-10-13 pull (`FINDING-bright-outdoors.md`). The claim is labelled "file-level, from console-pulled files (Mac analysis)", with the reviewer's run on the pulled inis as the second source. The "fresh LIST" need is dropped. | NOTES text. |
| F10a | NOTE | **deferred** | Refusing a mod whose compression block size differs from the game's is kept. It is stricter than needed, but it fails closed. Relaxing it needs two sources showing that the loader reads each container's own block size. That belongs with the Oodle/compressed-mod follow-up. | none |
| F10b | NOTE | **fixed** | `config_disclosure.py` matches by (section, key). A later line overrides an earlier one, and the same key in other sections is listed separately. The `PoolSize 1536 (game: 1024)` cross-section match is gone; it now shows "game, other sections: [RendererSettings] 1024". The evidence txt/json was regenerated. | `test_section_aware_and_repeated_keys`, plus the real-file Engine.ini test (`r.ShadowQuality` 1 vs 3 still holds). |
| F10c | NOTE | **fixed** | The 2026-10-02 trial is labelled "background only (earlier session, not a verdict)" in NOTES. | NOTES text. |
| F10d | NOTE | **deferred** | Oodle-compressed mods and shared-shader-code mods are still refused or UNKNOWN. These are new features (decompress and re-store; shader-library lookup), not fixes. They limit #8's reach until a follow-up adds them. | none |

## Files changed

- `PATCH/pak-intake/chunk_intake.py`: tool `cap08-intake-2`. Adds `load_graph`, the generic probe choice, F6, F7, the stale-report and broad-error handling, and the ScriptObjects sha.
- `PATCH/pak-intake/zzintake_template.lua`: v2 (no stop, linked rule, states).
- `PATCH/pak-intake/intake_gate.py`: base binding and `--record`.
- `PATCH/pak-intake/config_disclosure.py`: section-aware.
- `PATCH/APPLY.md`, `LAUNCH-CARD.md` (rewritten short and plain), `NOTES.md`.
- `tests/run_tests.sh`, `tests/test_diag_lua.py`, `tests/test_intake_synthetic.py`, `tests/test_config_and_gate.py`,
  `tests/test_leon_real.py`, `tests/test-output.txt`.
- Regenerated by the tests or the tool: `evidence/leon-intake/*` (the fetch plan is byte-identical) and
  `evidence/engine-ini-mod-disclosure.{txt,json}`.
- New: `evidence/fixes-20261012/` (proof that the planted failures and the old code fail).

## Still needed before the #8 launch (unchanged order, each console step needs the user's OK)

#7 passes. The paks folder is clean (`../PRISTINE-PAKS.md`, loose Leon trio gone). Base chunks are fetched
(read-only). The intake is re-run until exit 3. The user accepts the visual loss, and the gate PASSes with
its record. zzintake is regenerated from that run, deployed as an exact mirror, and the row-check passes.
