# Round 4 Change Summary

- Date: 2026-10-07. Repo: `/WORKSPACE/Projects/ps5-installer-compat` (Mac-only, no console contact).
- Scope: round-4 commit `35d7e59..390ef65` (20 files, +4649/−1839) plus the
  final uncommitted refresh of `raw-test-output.txt`, `COUNTS.md`, `REVIEW-PACKAGE.v2.md`.

## Test counts (exact)

| | installer core (8 suites) | adapter edge | compat checker | TOTAL |
|---|---|---|---|---|
| Before round 4 (commit 35d7e59) | 195 | — | 47 | **242** |
| After round 4 (final log + live re-run) | 199 | 13 | 55 | **267** |

- Before: 242 per the round-3 commit/REVIEW-PACKAGE (the raw log committed at
  35d7e59 was stale, showing 198 = 155 + 43).
- After: 267 = 242 + 25 new tests (+8 checker, +3 detector, +1 installer, +13
  edge). `docs/COUNTS.md`: "267 tests pass; 0 failures, 0 errors, 0 skips";
  re-verified live. The commit message's "242 (+13 edge = 255)" predates the
  last 12 core tests; the final log supersedes it.

## Item 1 — Unified test totals
- `test-results/raw-test-output.txt` — replaced with a single-agent sequential
  clean-copy run: combined installer + compat runs, per-suite `Ran N` lines,
  conformance variants, grade re-scan vs v4 catalog, definitive totals.
- `docs/COUNTS.md` — recounted for catalog v3 + v4 (globals 106, API rows 582,
  raw rows 624, merged 599) and made the authoritative test-count sheet (267).
- `REVIEW-PACKAGE.v2.md` (new) — final review package; `REVIEW-PACKAGE.md`
  updated to point at it.

## Item 2 — Key table + Townfall grade
- `catalog/globals.json` — catalog v4: new `Key` (165 constants) and
  `ModifierKey` (3 active constants) TYPE entries; globals 98 → 106 on disk.
- `catalog/types.json` — v4 version bump; 42 types unchanged.
- `catalog/SUMMARY.md` — provenance note (SUMMARY keeps the original build).
- `compat/catalog-real-merged.json` — regenerated against v4: 591 → 599 entries.
- `compat/checker.py` — new `pc_only_input` flag / PC-ONLY INPUT category:
  `Key.*`/`ModifierKey.*` keybind usage graded as a PC-only keyboard dependency
  (PS5 needs controller mapping, e.g. PS5Pad.ini), distinct from missing/not-in-catalog/status-blank.
- `compat/test_checker.py` — +8 tests covering the pc-only-input grading.
- `compat/townfall-item-spawner-grade.{json,md}` — grade changed
  **unknown → partly-compatible - PC-only input dependency** (six `Key.*`
  constants now resolve and are flagged; 9 status-blank APIs remain unverifiable).
- `docs/ANSWERS.md` — new Q4: Key table necessary but not sufficient;
  `docs/CONTRACT-PROPOSAL.md` — open question #8 (PC keyboard → PS5 mapping).

## Item 3 — UE4SS detection
- `installer/test_detector.py` — +3 tests: flat full UE4SS 3.0.1 release zip
  refused, nested installed-style release refused, real user mod inside a
  release subfolder installs while skipping runtime files (50 total).
- `installer/test_installer.py` — +1 test: mod bundled under the release's
  Mods/ tree installs only the mod files (55 total).
- `installer/adapter.py` — adapter now rejects UE4SS-release archives the same
  way the installer does.
- `docs/UE4SS-RELEASE-LISTING.txt` (new) — pinned reference listing of the
  official `UE4SS_v3.0.1.zip` (public GitHub URL, 5,523,402 bytes, sha256
  `4b47d4bc…c57ec`, 46 files): FLAT layout — UE4SS.dll + dwmapi.dll +
  UE4SS-settings.ini at root, 12 default mods under Mods/, no ue4ss/ wrapper
  (nested installed-style layout also supported by the detector).

## Item 4 — Security scan (pre-push)
- `docs/SECURITY-SCAN.md` (new) — whole tree + full git history; verdict
  **CLEAN, cleared to push**:
  - Keywords: 45 hits, all benign (transaction/rollback tokens, Lua tokenizer);
    credential patterns (ghp_/AKIA/sk-/xox*/PEM/URL-creds): zero.
  - Archives: 32, all synthetic `tests/src-tester/corpus/` fixtures.
  - Binaries: only the documented Lua 5.4.7 arm64 interpreter + hostile fixtures; `.env` files: none.
  - Git history: no secret ever committed; no blob > 2 MB.

## Item 5 — Git push
- Commit: `390ef65f36ed6482640c53f80b2b8131b0a3739d` ("Round 4: Key/ModifierKey
  input-table catalog v4, PC-only-input compat status, UE4SS-release detector hardening, …").
- Branch: local `main`, pushed to `origin` (https://github.com/Sammsamy02/ps5.git,
  private) as branch `side/installer-compat`.
- Verified in sync: `origin/side/installer-compat` == `main` == `390ef65`.

## Per-file one-liners (all round-4 changes)

| File | Change |
|---|---|
| REVIEW-PACKAGE.md | Updated to reference v2 package and final v4 counts |
| REVIEW-PACKAGE.v2.md | NEW: final round-4 review package |
| catalog/SUMMARY.md | Provenance note for the v4 catalog additions |
| catalog/globals.json | v4: +Key (165 constants) and +ModifierKey (3) TYPE entries (106 globals) |
| catalog/types.json | Version bump to v4; 42 types unchanged |
| compat/catalog-real-merged.json | Regenerated merged catalog, 591 → 599 entries |
| compat/checker.py | pc_only_input status/flag + PC-ONLY INPUT report category |
| compat/test_checker.py | +8 pc-only-input grading tests (55 total) |
| compat/townfall-item-spawner-grade.json | Regenerated scan JSON against v4 catalog |
| compat/townfall-item-spawner-grade.md | Grade unknown → partly-compatible (PC-only input) |
| docs/ANSWERS.md | +Q4 (Key table necessary but not sufficient) |
| docs/CONTRACT-PROPOSAL.md | +open question #8 (keybind controller mapping) |
| docs/COUNTS.md | Recounted for v3/v4; authoritative 267-test sheet |
| docs/SECURITY-SCAN.md | NEW: pre-push security scan report (CLEAN) |
| docs/UE4SS-RELEASE-LISTING.txt | NEW: pinned UE4SS_v3.0.1.zip listing (detection reference) |
| installer/adapter.py | Rejects UE4SS-release archives like the installer |
| installer/test_adapter_edge.py | NEW: 13-test adapter edge suite (all green) |
| installer/test_detector.py | +3 UE4SS-release detection tests (50 total) |
| installer/test_installer.py | +1 bundled-mod-under-release test (55 total) |
| test-results/raw-test-output.txt | Replaced with fresh definitive clean-run log |
