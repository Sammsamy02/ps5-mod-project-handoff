# REPORT — worker-stray-root-audit

2026-10-05 · LOCAL-ONLY audit · **No deletions, renames, edits, or writes outside this worker's own deliverables directory.** Controller owns disposition of the stray tree.

## What I verified (direct evidence)

1. **Tree inventory, complete.** `outputs/2026-10-04-zcode-team/.superpowers/` contains exactly 6 paths (4 dirs, 2 files: `owned_profile.h` 568 B, `receipt.json` 13,728 B), all mtimes `2026-10-05 05:42:47` — a single mkdir event. It is a botched staging copy of ONE FAILED owned-dll-sdk-candidate spike attempt (build_id `b33f79f5a88b40e6bdfd1aff15816f99`, `status: FAILED`, `commands: []`), not a cache and not a ledger.
2. **Uniqueness.** The build id appears nowhere else under TEAM; the canonical `.superpowers/native-dependency-spike/owned-dll-sdk-candidate-1/` has 24 other build-id dirs and not this one; the shared ledger `transport-experiments-progress.json` has no `b33f79f5` and its `zcode_worker_seh64_trampoline_fix` entry (11:33:27Z) covers only the final F1–F4 work — the stray attempt is unrecorded.
3. **Owner attribution to worker-seh64-trampoline-fix, mechanically.** The receipt's `source_files` keys are `relative_to(ROOT)` and all begin `worker-seh64-trampoline-fix/vendor/…`, forcing `ROOT == TEAM`; the receipt lists the build script itself with sha `c45a2c68…` which is byte-identical to the file still in that worker's `vendor/windows-candidate/` today; 2 further source hashes match; the 05:42:47 mtime is bracketed by that worker's E-runs (out-e1b 05:41:36, out-e2 05:46:12). The receipt shape is exactly build.py's writer (lines 42/58/118/355). The worker never self-reported it.
4. **Shadow mechanics at file:line.** `TEAM/worker-prx-link-gaps/package_prx.py` `_find_root()` (lines 74–83) walks up max 6 ancestors and returns the first dir containing a `.superpowers` subdir (line 79). With the stray present, ROOT resolves to TEAM (one level too deep); `SPIKE` (line 86) and all ROOT-derived `PIN` paths then miss, producing the known 12 lineage gaps, fail-closed. Frozen sha `e6fbe6c5011b498e0c26ee88bdc6c84aacab204b6ccad5696a78353afef5c313` re-verified unchanged today.
5. **Blast radius.** Exactly ONE script uses the marker-walk (`_find_root`): the frozen packaging file, loaded by 7+ lineage drivers (comp5/6, d1fix, l10pre, l11pre, walkguard, r3). All drivers that ran BEFORE 05:42 have valid receipts (mtimes checked: 21:35, 23:55, 23:58, 02:35, 03:56). Exactly TWO receipts ran with the wrong root and both FAILED CLOSED: walkguard run-a (12 gaps, preserved as `prx-candidate-composition6-l10wg-a.rootgap-firstattempt` + build-receipt `2026-10-05T07:00:29` + logs/run-a.console.log) and r3-armed run-a (same 12 gaps, preserved in its logs/run-a.console.log). **No wrong-root receipt approved any artifact.** Unpatched drivers (e.g. launch10-prebuild) would fail identically on re-run today.
6. **Workarounds in the field.** Walkguard WGP E4 (in-memory ROOT/SPIKE/PIN re-point with per-path existence refusal; runs a2/b applied it and passed 17/17) and r3-armed L10R3 E3 (virtual `__file__` re-anchor; repackage passed, build-receipt records the virtual origin) — both read line-by-line and confirmed working from their logs and rebase JSONs.
7. **Prior art.** `worker-crt-integrate` hit the SAME vendor-build.py parents[2] misroot on 2026-10-04 17:08 (pre-stray) — the stray is one instance of a recurring bug class, not a one-off.

## What I inferred (clearly marked)

- The exact 05:42 invocation/argv: the receipt records `commands: []` because the first lld-link exec raised before any command was appended; the bogus `bin/gcc-ld` lld-link path in the error must have come from the invoking driver/edition, since the vendored build.py's own default is the correct `lib/rustlib/…` form. The run was doubly doomed (bad lld-link arg AND mis-rooted staging) but only the staging has lasting consequence.
- "Most-likely owner" is stated with HIGH confidence; I did not find (nor expect) a positive log line saying "staged to TEAM/.superpowers" — the attribution is arithmetic (path algebra + self-recorded hashes), not testimony.

## Disposition recommendation (controller-owned; I performed nothing)

- **Option A (recommended): rename/quarantine** — one `mv` (exact command in AUDIT.md §5a) preserving all bytes, removing the `.superpowers` marker from TEAM. Unblocks every driver, keeps the frozen packaging file byte-identical (all `e6fbe6c5…` pins hold), keeps every past receipt reproducible.
- **Option B (harden `_find_root`, e.g. require `.superpowers/native-observations`)**: sound idea but BLOCKED in place — the file is sha-pinned by 7+ drivers; must be a new versioned file with new pins. Minimal diff provided in AUDIT.md §5b.
- **Option C (delete)**: provably low value, but it is the sole surviving provenance of an unacknowledged misrun; quarantine gets the same unblock at zero risk. Not recommended while owner reconciliation is pending.
- **Option A is safest for reproducibility**: no pinned bytes change, no driver edits, WGP E4 becomes a verified no-op (its `if m.ROOT != ROOT` guard), L10R3 E3 keeps working unchanged.

## Honesty bounds

Host-side filesystem/document analysis only; no console contact, no builds executed, no provider calls, no engine/root writes. I did not re-run any packaging driver (even read-only verification runs of the drivers would create output dirs and are outside my scope). Line numbers and shas cited in AUDIT.md were read from the files during this audit (2026-10-05 ~07:2x–07:5x local); the frozen file sha was re-verified as unchanged.

## Deliverables (this dir)

`CLAIM.json` (this contract + verdicts) · `AUDIT.md` (inventory, attribution, mechanics file:line, blast-radius tables, ranked disposition) · this `REPORT.md`.
