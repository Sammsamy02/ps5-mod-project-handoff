# REPORT — worker-return-docs-refresh-2 (2026-10-05, LOCAL ONLY)

Mission: refresh the two session-continuity runbooks (`README-RETURN.md` at the workspace root and
`outputs/2026-10-04-zcode-controller/RETURN-SESSION-COMMANDS.md`, previously rev 2026-10-05a) with the
2026-10-05 launch-10-build ground truth, read fresh from the sources — never from memory. Nothing else
was touched except this output directory.

## What changed in the two runbooks

### RETURN-SESSION-COMMANDS.md → rev 2026-10-05c
- Rev header notes the supersession (2026-10-05a) and clarifies that 2026-10-05b is the
  `LAUNCH-DAY-10.md` rev (no rev-b of this file existed).
- Console-state block kept UNCHANGED as the lead: 2-plugin ini `1216aab6…`, settings v3 `d8faa4ec`,
  loadlist `4d98ecd7`, mod tree verified — labeled last-verified reads, not promises.
- New "Launch-10 build state" section: EDITION `launch10-final-b` with all five unit shas
  (geometryfix `440a5786`, initializer v3 `c3a1c9eb`, sink `f5e96c41`, staged header `67170b21`
  anchor 16 + capture 16,384, consolidated+repaired bridge `663e7ce9`) plus the driver-hardcoded
  walkguard edit 5 (`694af016`, fold-0) and the {cppmod,luamod,settings} freshen; the hardened
  `launch10_one_pass.py` (receipt-tied verifier, artifacts-receipt hook, determinism twin); the
  wrapper NO-GO→fixes-applied story worded exactly as evidenced (fixes in driver `f4103572`, post-fix
  harness GO not recorded); and the honest on-disk build status (full pass NOT completed —
  lineage gap + accessor FAIL-MISSING-PRX on l10-1's last attempt, no gate receipts → do not stage).
- New "The gate" section: full pass fires when `worker-fname-validator-v4` folds (RE-corrections,
  69/0 fixtures, staging shas `4a2b9400`/`b84f8585`, the REQUIRED one-line bridge rename, EDITION
  refresh before `--full`), with the candidate/synthetic labeling law per
  `worker-blocks0-provenance` (NO CAPTURE EXISTS).
- Tool block updated: `console_batch.py` rev `b982f794`, selftest 462/0 + fake-drive 143/0, the new
  `stage-launch10`/`restore-ini` flows, and `run_stage_launch10.sh` now delivered (supersedes the
  LAUNCH-DAY-10 rev-b "not yet delivered" note). Old rev `5ca8d1c7`/405-0 labeled HISTORY.
- Evidence-chain + L11-assets sections added (oracle 83 predictions, W1–W7, predicted pair
  anchor_count=7/intervals_found=1, evidence.sh `76559126` 94/0, verdict rehearsal 5/5, post-popup
  layer, luamod GO/packaging-unproven, marshal consumer in flight, IGW row-0 `igd1-e030a09eb4d1`
  1 semantic byte, PC acquisition pack).
- Rollbacks updated with the extent law and the stage-launch10 `.bak`/pre-image restore path;
  all superseded ini/settings values labeled HISTORY.

### README-RETURN.md → launch-10-build refresh
- §1 status rewritten honestly: console unchanged; build staged on host but NOT landed (gate §1 not
  green); the v4 fold gate; LOAD-CAPABLE NOT SPAWN-CAPABLE expectation.
- §2 read order now leads with `LAUNCH-DAY-10.md`, `EDITION.json` + `SPAWN-CAPABILITY.md`, the
  oracle pair, and keeps the digest/eve/router documents with their historical scope labeled.
- §3 first commands: same two console checks, then the fixed fold → EDITION refresh →
  `launch10_one_pass.py --full` → stage-launch10 ordering as a HOLD.
- §4 user actions: LAUNCH-DAY-10 §2 verbatim pointer, OPTIONS-after-mods-started rule,
  first-OPTIONS silence EXPECTED on this candidate, 9-entry OK-line chorus labeled HISTORY.
- §5 do-nots: extent law (runtime extent `[0x400000, 0xc0602c8)`, `kImageExtentVaddr=0xbc602c8`,
  `0xfc602c8` known-wrong 60 MiB-biased — never reuse), predictions-are-not-results, fold-0 walkguard
  W1 expected / W3+W7 = drift, stray-root blocker resolved (stray gone, hardening delivered, frozen
  `package_prx.py` unmodified).
- §6 knowledge base / evidence chain / L11 assets / restore paths, every path verified on disk.

## Verification performed (all read-only)

- All EDITION unit shas recomputed on disk and matched: `440a5786`, `c3a1c9eb`, `f5e96c41`,
  `67170b21`, `663e7ce9`, `694af016`.
- Tool hashes recomputed: `console_batch.py` = `b982f794…` (matches SELFTEST-RECEIPT),
  `evidence.sh` (launch10-evidence-upgrade) = `76559126…`, `launch10_one_pass.py` = `a93b4b34…`,
  driver = `f4103572…`.
- Counts sourced from receipts, not memory: selftest 462/0 + fake-drive 143/0
  (`worker-launch10-staging-packet/SELFTEST-RECEIPT.txt`); evidence fixtures 94 passed / 0 failed
  (`worker-launch10-evidence-upgrade/receipts/fixture-test-pass.txt`); v4 host fixtures 69/0
  (`worker-fname-validator-v4/receipts/PASS-pool-bounds-v4-fixtures.json`); verdict rehearsal 5/5
  (`worker-verdict-rehearsal/REPORT.md` results table); oracle 83 predictions
  (ORACLE-SUPPLEMENT header); 16-payload taxonomy (LAUNCH-DAY-10 §5 / FAILURE-TAXONOMY).
- Wrapper story read at source: `GO-NOGO.md` NO-GO + post-fix GO procedure; both fixes confirmed
  present in driver `f4103572` (env_extra LLVM_CONFIG line + stage_aware_reconciliation detail dict);
  no post-fix re-validation receipt exists — the docs say exactly that.
- Build status read at source: `prx-candidate-l10-1/GAPS.json` (lineage gap, "do not link/promote"),
  `receipts/accessor-verification-prx-candidate-l10-1.json` verdict FAIL (09:26, pre-hardening);
  no `prx-candidate-l10-2`, no gate receipts → "build has not landed; do not stage" wording.
- 29 cited paths existence-checked in one batch (29/29 OK) before the docs claimed it.
- Extent law cross-checked in four sources (ORACLE.md:114, ORACLE-SUPPLEMENT S2.2 with the
  lmf_extent tag values, bridge-r12 REVIEW-VERDICT arithmetic, anchor-cap-analysis note).

## Honest scope

- No console contact, no network calls, no builds or test suites executed, no frozen or source files
  modified; the refresh is documentation-only, sourced from on-disk evidence.
- No outcome promises: every launch-10/launch-11 statement is recorded as ready/pending/in-flight at
  the level the receipts support; the full pass, staging, and any launch remain gated exactly as the
  runbooks say.
- Files written: `README-RETURN.md`, `outputs/2026-10-04-zcode-controller/RETURN-SESSION-COMMANDS.md`,
  and this directory's `CLAIM.json` + `REPORT.md`.
