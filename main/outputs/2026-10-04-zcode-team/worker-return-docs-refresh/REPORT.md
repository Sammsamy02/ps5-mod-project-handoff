# REPORT — worker-return-docs-refresh (2026-10-05)

Task: update the two session-continuity runbooks with what actually happened at launch-9 and the current console
state, honestly. No console contact was made; every console fact below is recorded as controller-verified ground
truth, and every local path/sha was re-verified on disk before writing (README path check: 20/20 exist).

## Files changed

### 1. README-RETURN.md (workspace root) — rewritten in place, structure and reading order preserved

- Header: re-dated "post-launch-9 refresh, 2026-10-05; originally session-eve 2026-10-04"; path-existence claim
  re-verified for the new citation set and updated to 20/20 (was 14/14 on 2026-10-04).
- §1 (status, one line): replaced the "console down / slot holds r11 / first act is staging" line with the launch-9
  outcome (no crash; game reached menu; no mods loaded, Item Spawner popup correctly absent; composition refused at
  the PLH lease read `Owned read range includes unregistered bytes` despite the r11 quantization fix; entry aligned
  start refused at the facts adapter `no observed mapping interval intersects the module image extent with valid
  geometry`, TERMINAL at runtime-status session 273; entry self-contained harness FULL PASS incl. embedded Lua VM)
  plus the current SHORTEST 2-plugin ini state and the launch-10 milestone gated on stray-root disposition.
- §2 (read in this order): inserted `C/launch9-evidence/LAUNCH9-RESULT.md` as item 1 (freshest ground truth); kept
  the original order for the digest, cheat sheet, EVE-STATE (now labeled historical baseline), and both matrices.
  Added a session-bookkeeping paragraph (ledger entry `zcode_return_session_log`, 7 events, commit `bfdb5f6`,
  `W/worker-session-cleanup-owner/CONSOLIDATION-RECORD.json`) and a directory note distinguishing the workspace-root
  `.superpowers/` ledger home from the stray `W/.superpowers/` shadow blocker.
- §3 (first commands): relabeled "First commands on any return"; recorded what already executed on 2026-10-05
  (reachability OK, comp6 `76ee6af3…` staged+verified, dll8 PASS per ledger, launch-9 under the 9-entry ini
  `fd9c7393…`, e2e dispatched with a missing-intake-dir blocker never confirmed complete). Kept cmd 1 verbatim;
  cmd 2 pre-image expectation updated to `1216aab61236b0fc9734b1647c96e03db48beb3d333efe174ca08e97abededa0`
  (2-plugin SHORTEST config); cmds 3-5 kept as verified commands with honest HOLD/already-ran/blocker annotations.
  Added the files-first-config-last law pointer (worker-staging-order, in flight) and that the SHORTEST ini stays.
- §4 (user actions): launch-9 needed none; OPTIONS only after mods-started evidence exists; the "EXACTLY eight OK
  lines" gate explicitly attributed to the 9-entry ini, not the current 2-plugin config; FAIL-line STOP rule kept.
- §5 (do NOTs): kept all session-earned rules; replaced the "Shape-A stop is a PREDICTION, not a result" bullet with
  its recorded outcome (never reached — both refusals upstream of the initializer); added no-rebuild-before-stray-root
  and no-separate-diagnostic-launch (t=-prefixed lines + PATH_TEST stamps; diagnostic-sink opportunistic; runtime-status
  + UE4SS mirror always-available durable verification).
- §6 (knowledge base and restore paths): ini restore now lists the current `1216aab6…` state, the superseded
  `32d0c0d9…` intermediate (history only), and the preserved 9-entry canonical
  `C/launch9-evidence/ploader-ini-9entry-restore.txt`; settings current = verified v3 `d8faa4ec` at
  `/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini`, loadlist `4d98ecd7` intact, mod tree intact, v2-pair
  rollback unchanged; comp6 slot notes launch-9 ran `76ee6af3…` with the r11 chain (`0911af0c`) as documented fallback.
  Also repaired a garbled editing artifact (see inconsistency 6 below).

### 2. outputs/2026-10-04-zcode-controller/RETURN-SESSION-COMMANDS.md — new rev 2026-10-05a (was 2026-10-04c)

- Header: rev bumped; "Console was DOWN at close / slot holds r11" replaced by the launch-9 result line and the
  controller-verified console state block: SHORTEST 2-plugin ploader.ini `1216aab6…eda0` (supplier_composition=2,
  path_test=2; aligned_entry and ALL probes incl. xwp disabled) with the full config history (9-entry `fd9c7393…`
  ran launch-9 → 3-plugin `32d0c0d9…` intermediate → 2-plugin current, user directive: entry is diagnostic-only);
  settings v3 `d8faa4ec` path-verified; loadlist `4d98ecd7` intact; mod tree intact. Tool/flows/archive lines kept verbatim.
- NEW section "Launch-9 result": per-plugin outcomes (composition lease-read refusal code=2 with the exact detail
  string; entry aligned-start refusal code=1 TERMINAL with the exact detail string; harness FULL PASS — 232
  constructors, worker thread, Lua VM lua_load/lua_call chain; xwp all 6 rows SKIPPED, harmless as parked; path_test
  DIAG reached; fc1pool/qh_driver/observer/carrier absent-expected), the never-reached Shape-A note, and all four
  artifact shas/sizes from LAUNCH9-RESULT.md plus the baseline-fetch-failure caveat.
- Phase 0: pre-image expectation updated to `1216aab6…` with `8d331b3f…` and `32d0c0d9…` explicitly marked STALE
  history; comp6 `76ee6af3…` marked already-staged/ran; the verified curl staging law kept verbatim under an
  explicit HOLD for the launch-10 rebuild.
- Phase 1: relabeled "2026-10-05 history + launch-10 law change": dll8 PASS / e2e missing-intake-dir blocker (ledger),
  dll7 attribution note kept, the full 9-entry staging text preserved verbatim as the record of what launch-9 ran,
  canonical restore path added, launch-10 changes stated (files-first-config-last; SHORTEST ini stays).
- Phase 2: launch-9 needed no user actions; SC-5b gate attributed to the 9-entry ini; new launch-10 rule "user tests
  OPTIONS ONLY after mods-started evidence exists"; CP2 pins and FAIL-line STOP kept.
- Phase 3: rewritten to the user-approved launch-10 definition (shortest config + r14 lease fix + armed SeedTable +
  GMalloc V1/V2 validator diff +75/−1 READY, armed candidate does NOT run V1/V2 without it + stray-root disposition
  as controller-priority rebuild blocker), gate v2, files-first-config-last, free timing measurements (t=- lines +
  PATH_TEST stamps), diagnostic-sink opportunistic with `/data/modbridge-supplier-diag.jsonl` as its planned console
  path; in-flight owners and completed-this-session workers listed; candidate lineage listed (session-eve armed
  l10-a `d8cb4a76…`, walkguard l10wg-a/b gate 15/15 GO, r3-armed l10r3-a/b running), none staged.
- Rollbacks: updated to current facts (ini `1216aab6…` current with both predecessors marked superseded + 9-entry
  restore file; settings v3 current + v2-pair rollback; comp6 slot `76ee6af3…` ran + r11 `0911af0c` chain).

## Inconsistencies found between the old docs and ground truth (reported, not silently rewritten — old values are
## preserved in the docs as labeled history)

1. Stale pre-image sha: rev 2026-10-04c (and README §3.2) expected ploader.ini `8d331b3f…` (247 B). That state was
   superseded twice on 2026-10-05 (9-entry `fd9c7393…` staged for launch-9, then 3-plugin `32d0c0d9…`, now the
   2-plugin `1216aab6…`). Both old shas remain in the docs labeled as history.
2. "Console was DOWN at close / slot holds r11" (both docs' opening status) — superseded: console came back,
   staging + dll8 + launch-9 happened. Recorded as superseded, not deleted.
3. Session-eve staging plan assumed nothing had been staged yet; ground truth: comp6 staged+verified and dll8 ran
   (ledger PASS) before launch-9. Docs now carry already-ran/HOLD annotations.
4. The e2e flow's dispatch carried a missing-intake-dir blocker (ledger) — absent from the old docs; now recorded
   with an explicit "do not treat as verified-complete".
5. README §5 "launch-9's `[Seeds] … missing [gmalloc]` loud stop is the PREDICTED outcome … not a result" — launch-9
   happened and the stop was never reached (both refusals upstream of the initializer). Updated to the recorded
   outcome; LAUNCH9-PREDICTION.md reference kept.
6. Editing artifact in old README §6: the restore line read "r11 backup chain `C/console/the r11 restore chain: re-upload…`"
   — a duplicated phrase that garbled the path. Cleaned to the intended form (r11 chain, `0911af0c`,
   worker-composition-5-rebuild/prx-candidate-composition5-1/, RESTORE-PATH §A.5); no historical value lost.
7. Old Phase 3 presented a single "armed candidate READY" (l10-a `d8cb4a76…`, offline gate 15/15 GO) to be staged
   "when the matrix row says so". Ground truth adds: the walkguard GUARD+ARMED candidate also gated 15/15 GO
   (one-define-flip measured), an r3-armed candidate is running, integration of lease fix + SeedTable + gmalloc
   validator is a precondition, the armed candidate does NOT run V1/V2 without the validator diff, and stray-root
   is a rebuild blocker. Docs now present the lineage with none staged.
8. Old SC-5b "EXACTLY eight OK lines" gate was presented as the standing launch expectation; it described the
   9-entry ini only, and launch-9 never reached a loader-line chorus (no mods loaded). Now attributed and superseded
   by the mods-started-evidence rule.
9. Settings current state: old docs described v3 as the staging target; it is now controller-verified present at
   `/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini` (`d8faa4ec`), loadlist `4d98ecd7` intact. Recorded as
   verified-current (consistent with, not contradicting, the old docs).
10. Mid-task supersession handled explicitly: the task brief first gave the 3-plugin `32d0c0d9…` as current; the
    coordinator then superseded it with the 2-plugin `1216aab6…` config ~30 minutes after publication. The docs and
    this report record the full sequence so no reader mistakes an intermediate for current state.

## Verification performed

- All 20 local paths cited in the rewritten README re-checked on disk after writing: 20/20 exist.
- Full ini sha string `1216aab61236b0fc9734b1647c96e03db48beb3d333efe174ca08e97abededa0` present verbatim in both files.
- `ploader-ini-9entry-restore.txt` hash re-computed: `fd9c7393…` (440 B) — matches the canonical cited since rev 2026-10-04c.
- Commit `bfdb5f6` verified in git; `.superpowers` shadow dir and ledger json both verified to exist.
- Launch-9 evidence artifacts verified present in `launch9-evidence/` with the names cited from LAUNCH9-RESULT.md.
- Only the two runbook files were edited; only this worker directory was created.
