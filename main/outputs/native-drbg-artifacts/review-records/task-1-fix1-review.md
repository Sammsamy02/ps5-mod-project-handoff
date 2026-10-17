**[P2, plan-mandated closure requirement] Authenticate the complete source container** — ADDRESSED. `crypto_build.py:585-586` derives the expected member hashes from the verified Mbed and framework archive contents; `crypto_build.py:604-605` checks the complete `cache/sources` tree against exactly those two prefixed member sets. Extra source files, altered bytes, links, and special files therefore fail reuse while empty-directory tolerance remains unchanged. The coordinated forged-sibling regression is at `tests/test_crypto_build.py:283-298`.

### New Breakage in the Fix Diff

None found. The Task2 plan-file edit clarifies file ownership and does not change Task1 requirements.

### Out-of-Scope Observations

Task2 consumer wiring, native runtime behavior, and PS5 execution remain separate integration gates assigned outside Task1.

### Checks

- Read the supplied fix diff once; no Git commands were run and no tests were rerun.
- Inspected retained focused RED/GREEN output: the regression failed before the fix because `ValueError` was not raised, then passed after the fix.
- Inspected the retained final suite output: 15 tests passed in 181.749s. The fix report also records fresh/reuse descriptor equality and preservation of all 15,819 historical generated files.
- Current host and native fresh/reuse descriptor pairs show matching config IDs and archive hashes per mode; retained audit output reports verified status, 81 native members, and include origins `llvm_resource`, `overlay`, `sdk`, and `source`.

### Verdict

**Fix round:** All findings addressed, no new Critical/Important breakage.
