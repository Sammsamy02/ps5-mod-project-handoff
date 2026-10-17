# README-RETURN — start here (launch-10-build refresh, 2026-10-05; originally session-eve 2026-10-04, post-launch-9 rev 2026-10-05a is superseded)

Root: `.codex/worktrees/translation-core/what`. `W` = `outputs/2026-10-04-zcode-team`; `C` = `outputs/2026-10-04-zcode-controller`.
Every path below was re-verified to exist on disk 2026-10-05 by this refresh (29/29). Hashes quoted are
last-VERIFIED reads, not promises for today — re-verify every identity at open.

## 1. The status, honestly

- **Console state UNCHANGED**: 2-plugin SHORTEST ploader.ini `1216aab6…` (composition=2 + path_test=2
  only), settings v3 `d8faa4ec`, loadlist `4d98ecd7`, mod tree verified. Nothing has touched the
  console since launch-9 (2026-10-05, pid 273: no crash, menu reached, NO mods loaded — HISTORY;
  verdict `C/launch9-evidence/LAUNCH9-RESULT.md`).
- **Launch-10 rebuild is staged on the host, not landed**: EDITION `launch10-final-b` pins 5 edits +
  bridge (geometryfix `440a5786`, initializer v3 `c3a1c9eb` — v4 fold PENDING, sink `f5e96c41`,
  staged header `67170b21` = anchor 16 + capture 16,384, walkguard fold-0 `694af016`,
  consolidated+repaired bridge `663e7ce9`, freshen {cppmod,luamod,settings}); `launch10_one_pass.py`
  is the hardened receipt-tied runner. **The full pass has NOT completed** — no gate GO ×2, no
  determinism proof; `prx-candidate-l10-1`'s last attempt ended in a lineage gap + accessor FAIL
  (pre-hardening). Gate §1 in `C/LAUNCH-DAY-10.md` is not green; do not stage.
- **The gate**: the full pass fires when `worker-fname-validator-v4` folds (RE-corrected pool
  predicate: CurrentBlock 0-based, image-resident blocks legitimate per static-BSS allocator,
  entry-0 'None' anchor, outside-image clause dropped; host fixtures 69/0; the MUST-PASS fixture is
  **candidate/synthetic** — `worker-blocks0-provenance`: NO captured Blocks[0] exists).
- **Spawn capability (source-established, SPAWN-CAPABILITY.md)**: the launch-10 candidate is
  LOAD-CAPABLE, **NOT SPAWN-CAPABLE** — first-OPTIONS-press silence after a successful load is
  EXPECTED (fold-0 walkguard refuses the exec-only text read loudly; NO-CRASH holds). Visible spawn
  is NOT a launch-10 verdict input. The working paths are launch-11: R3 FuncPtr-marshal (primary)
  and the IGW row-0 route (secondary).

## 2. Read in this order

1. Launch-10 execution runbook (owns gate/evidence/verdict/restore; rev 2026-10-05b):
   `C/LAUNCH-DAY-10.md`
2. Build receipts: `C/launch10-build/EDITION.json` + spawn-capability record:
   `C/launch10-build/SPAWN-CAPABILITY.md`
3. Cheat sheet rev 2026-10-05c: `C/RETURN-SESSION-COMMANDS.md`
4. Acceptance oracle: `W/worker-launch10-acceptance-criteria/ORACLE.md` (83 predictions) + final-build
   deltas `W/worker-oracle-final-sync/ORACLE-SUPPLEMENT.md` (walkguard rows W1–W7, spanpub, capacity,
   freshen; predicted pair anchor_count=7 / intervals_found=1)
5. Historical overview (pre-launch-9, still the full cited tables):
   `W/worker-index-final-edition/CURRENT-STATE-DIGEST-FINAL.md` (INDEX-FINAL.md same dir);
   session-EVE baseline `W/worker-session-eve-coherence/EVE-STATE.md`
6. Decision routers: `W/worker-launch10-matrix-prep/LAUNCH10-MATRIX.md` (launch-10) and
   `W/worker-launch11-integration-matrix/LAUNCH11-MATRIX.md` (post-launch-10 integration)

Session bookkeeping: ledger at the workspace root,
`.superpowers/native-observations/transport-experiments-progress.json`, entry `zcode_return_session_log`
(commit `bfdb5f6`; record: `W/worker-session-cleanup-owner/CONSOLIDATION-RECORD.json`). The former
stray `W/.superpowers/` shadow is GONE from disk (audit `W/worker-stray-root-audit/`,
hardening drop-in `find_root_v2.py` in `W/worker-stray-root-hardening/`; the frozen `package_prx.py`
`e6fbe6c5…` itself remains unmodified).

## 3. First commands on any return (game fully CLOSED throughout)

Already happened on 2026-10-05 (not on the list again): reachability OK; comp6 `76ee6af3…` staged +
readback-verified; dll8/ALLFAM8 ran (ledger PASS); launch-9 ran; e2e dispatched with a
missing-intake-dir blocker recorded in the ledger (never confirmed complete).
1. `curl -sS --connect-timeout 10 "http://ps5-host-not-configured.invalid:8084/list_payloads"` → 200 + catalog OK;
   refused/empty = console still DOWN: STOP, no dispatches. (Manager is 8084, never 9223; FTP
   `ftp://ps5-host-not-configured.invalid:2121` anonymous.)
2. `curl -sS --connect-timeout 10 --max-time 60 "ftp://ps5-host-not-configured.invalid:2121/data/plugins/ploader.ini" -o /tmp/pl.now && shasum -a 256 /tmp/pl.now`
   → expect `1216aab61236b0fc9734b1647c96e03db48beb3d333efe174ca08e97abededa0` (ONLY
   supplier_composition=2 and path_test=2). Divergent = STOP + preserve + reconcile; never re-send.
3. HOLD on any staging: the next fself is the launch-10 rebuild and the order is fixed —
   fold v4 per `W/worker-fname-validator-v4/DIFF.md` (whole-file initializer + SeedValidation
   replacement + the one-line bridge rename) → refresh `EDITION.json` pins →
   `python3 outputs/2026-10-04-zcode-controller/launch10-build/launch10_one_pass.py --full` →
   gate GO ×2 + determinism + witness PASS → only then
   `python3 outputs/2026-10-04-zcode-team/worker-console-batch-runner/console_batch.py stage-launch10 --fself <path> --expect-sha <sha>`
   (single-PRX; the ini is NEVER written by that flow; `JOB FAILED` anywhere = STOP for the whole
   launch path).
4. dll8/e2e: already ran 2026-10-05; rerun only inside a dedicated flow, with the e2e
   missing-intake-dir blocker reconciled first (do not treat it as verified-complete).

Then follow the cheat sheet and `C/LAUNCH-DAY-10.md`. Archive each flow via
`W/worker-session-archive-scaffold/session_run.sh`.

## 4. User game actions (Phase 2; full guide: `W/worker-user-launch-guide/USER-LAUNCH-GUIDE.md`)

- Launch-10 instruction: read `C/LAUNCH-DAY-10.md` §2 verbatim. Standing rule: the user tests
  OPTIONS only AFTER mods-started evidence exists (`[Lua] [TownfallItemSpawner] Loaded …` line) —
  not before. The FIRST OPTIONS press after a map load is the free drain probe (≥1 NEW
  `[TownfallItemSpawner]` line MUST appear; press #2+ is mute by design).
- EXPECTED for this candidate (SPAWN-CAPABILITY.md): the menu does NOT open on first OPTIONS after a
  successful load (fold-0 walkguard loud refusal; no crash). This is not a drain stall and not a
  failure to record honestly.
- The launch-9-era gate "EXACTLY eight `OK (<ms>)` lines" described the 9-entry ini — HISTORY; it
  does not describe the current 2-plugin config. Any FAIL line → STOP + `rollback-ridealongs`.
- Item Spawner activation has still NEVER been observed in-game; verdicts come from the collected
  evidence files only (evidence.sh `76559126…`, 94/0 fixtures; verdict ladder BEST/GOOD/
  MINIMAL-ACCEPT/FAIL; verdict rehearsal discriminated all five synthetic sets 5/5).

## 5. Do NOT (session-earned rules)

- No re-dispatch on ambiguity: a divergent hash, a FAIL line, or `JOB FAILED` means STOP + preserve +
  reconcile; never retry or replay an interrupted/ambiguous step.
- No upload before gates: reachability (cmd 1) and the ploader.ini pre-image gate (cmd 2) precede any
  staging; staging waits for the launch-10 build to LAND (gate §1 green). The armed candidate does
  NOT run GMalloc V1/V2 without the validator diff (`W/worker-launch10-gmalloc-integration/gmalloc-validator.diff`,
  +75/−1, host-tested) — it is already integrated in the launch-10 initializer.
- Extent law (arithmetic-corrected — applies to every derivation): runtime extent =
  **[0x400000, 0xc0602c8)**; `kImageExtentVaddr` = **0xbc602c8** for FILE vaddrs (uniform slide =
  file + 0x400000). `0xfc602c8` is a known-wrong 60 MiB-biased figure (the launch-8 clamp;
  0x400000+0xbc602c8 double-adds the base) — never reuse it.
- Freshness gates: re-verify every identity at open; ini/settings staging is byte-exact
  readback-gated; CP2 tick constant is `0x33ac8c0` (NOT `0x37ac8c0`).
- No assumed tests: predicted values (anchor_count=7, intervals_found=1, lease spans 6/reg 6) are
  PREDICTIONS, not results — 6 is NOT expected for intervals (coalesced count), the hard gate is >0;
  the image-resident pool fixture is candidate/synthetic until a real captured Blocks[0] exists.
  Walkguard fold-0 means every walk text read is refused LOUDLY (`[WalkRxGuard] REFUSED …`, rows
  W1–W7) — W1 is the expected fold-0 shape; W3/W7 at fold-0 = composition identity drift (REGRESSION).
- Timing measurements come free from t=-prefixed log lines + PATH_TEST monotonic stamps — NO separate
  diagnostic launch; runtime-status + the UE4SS mirror remain the always-available durable verification.

## 6. Knowledge base, evidence chain, L11 assets, restore paths

- Knowledge base ("READ THIS FIRST"): `W/worker-knowledge-consolidation/KNOWLEDGE-BASE.md`
- Evidence chain: post-popup layer `W/worker-post-popup-evidence/POST-POPUP.md` +
  `FAILURE-TAXONOMY.md` (16 error payloads across the 5 spawn stages; OPTIONS→F7 per PS5Pad.ini);
  classifier `W/worker-launch10-evidence-upgrade/evidence.sh` (+ STAGE-TABLE.md); launch-10
  baselines `C/launch10-prep/` (entry/status/ue4ss/xwp)
- Launch-11 assets (proposal-level, nothing staged): luamod variant-B admission GO with packaging
  UNPROVEN (`W/worker-luamod-l11-dryrun/VERDICT.md`); marshal-consumer linkage delivered,
  integration in flight (`W/worker-l11-marshal-integration/RUNBOOK.md`); IGW row-0 probe rebuilt,
  1 semantic byte (`W/worker-igw-row0-rebuild/REPORT.md`, armed `igd1-e030a09eb4d1`); PC acquisition
  pack awaiting the user's drop (`inputs/pc-townfall/` + `W/worker-pc-acquisition-pack/ACQUIRE.md`)
- Restore (cheat sheet §Rollbacks): ini — current console state is the 2-plugin `1216aab6…` (the
  3-plugin `32d0c0d9…` and 9-entry `fd9c7393…` are HISTORY; the 9-entry canonical stays preserved at
  `C/launch9-evidence/ploader-ini-9entry-restore.txt`; the tool's `rollback-ridealongs` does two
  single-char 2→0 flips + `.bak`; `restore-ini` restores byte-exact from a saved pre-publish copy).
  settings — verified v3 `d8faa4ec` at `/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini`
  (loadlist `4d98ecd7` intact; mod tree intact); rollback = recorded v2 pair in the stage-settings
  freeze inventory. comp6 slot — launch-9 ran fself `76ee6af3…`; the r11 backup chain (`0911af0c`,
  `W/worker-composition-5-rebuild/prx-candidate-composition5-1/`, see
  `W/worker-post-spawn-closure-prep/RESTORE-PATH.md` §A.5) remains the documented fallback;
  stage-launch10 additionally secures a console `.bak` + local pre-image before any swap.
