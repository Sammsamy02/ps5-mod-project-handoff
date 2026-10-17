# SESSION-2026-10-04-SUMMARY — read this first

End-of-day checkpoint for the ZCode team (PS5 mod-manager / UE4SS ladder).
Authoritative state: `.superpowers/native-observations/transport-experiments-progress.json`
(134 `zcode_*` entries, each with hashes + evidence; last entry 2026-10-04T22:15:04Z).
Controller files: `outputs/2026-10-04-zcode-controller/`. Worker deliverables:
`outputs/2026-10-04-zcode-team/<slug>/` (CLAIM.json + REPORT.md per dir).

## Proven facts (with evidence)
- **134** `zcode_*` checkpoint entries; **127** worker dirs under `outputs/2026-10-04-zcode-team/`
  (first `ls | wc -l` = 125; `worker-seh64-design` and this snapshot dir appeared during the count).
  Classification (see `worker-classification.json` here): **115 delivered, 11 partial, 1 blocked**
  (`worker-itemspawner-dryrun`). The 11 partials are claimed-but-no-REPORT dirs, including this one
  until its REPORT lands.
- Module loading + all imports proven **inside Townfall**: in-game probes v1 and v2 ran in-game
  (`console/ingame-v1-facts-2026-10-04.jsonl`, `ingame-v2-facts-2026-10-04.jsonl`); module quiesces inert.
- Segment math byte-cited (worker-vaddr-semantics): mapped_start(LOAD0)=0x400000 =>
  runtime Tick=0x33ac8c0, StaticConstructObject=0x890aa0 (validated in-span). GUA 0x93ba110 is
  LOAD7 BSS; **LOAD7 mapping not yet found** (remaining LOADs plausibly in [0x787c000,0x8018c000)).
- igf6 in-game run: `MODULE_LOADED_MAPSCAN_OK_EXEC_CRASHED_MODULE_GAME_SURVIVED`, 3 of 8 expected
  records (`zcode_igf6_RESULTS`, `console/ingame-v3-full-results.jsonl`).
- Host-side verification: rung3 chunk probe 20/20 (156 assertions, ASAN/UBSan clean); CRT startup
  9/9 host+PS5 cross-compile clean; CreateMutexA 7/7; ingame-log parser 3,450 lines validated,
  2 bugs fixed; fileio host+SDK passed; 45/45 final stageable package (worker-loadlist_final).

## Console passes (with build IDs) — day tally: 8 launches, 6 passes, 1 no-evidence failure, 3 recorded crash events
| Build | Artifact | Result |
|---|---|---|
| 6d295f56 | NEXT_DLL_THREADS.elf (workers/sync) | PASSED, final record all expected (10:29 local) |
| c49108fe | INITONCE attempt 1 | Dispatched, result unreadable — controller omitted `--report-path`; superseded, counted as controller error |
| 1a6b37b7 | INITONCE_DLL_TEST.elf (CURRENT) | PASSED all expectations: 46/10 imports, init_once:enabled, [86,93] (10:55 local) |
| lfp-84fdce4c | LIVEFACTS_PROBE.elf | 8/8 target run completed (11:20 local) |
| igd1 v1/v2 | in-game fact probes | Records collected (12:19/12:27 local) |
| igd1-fc26d6c0 | in-game v3 full run | Module loaded, map scan OK, exec crashed module, game survived (14:14 local) |
| 6f5bf85a | MUTEX_DLL_TEST.elf | PASSED (16:27 local) |
Crash events (checkpoint keys): `zcode_console_unreachable` (suspected argv-probe crash of payload
manager), `zcode_console_crash_2` (user-reported PS5 restart), igf6 exec crash (module died, game
survived). Game-file install HOLD honored all day; no controller-initiated Townfall launch.

## Built and waiting (with hashes, `outputs/CURRENT-PS5-TEST/`)
- `INITONCE_DLL_TEST.elf` 1a6b37b7 — sha256 `bc0574d662909eb60471984a693be2ffc57490f8095902ee794b3f9272992379`, 241,784 B (CURRENT, passed)
- `NEXT_DLL_THREADS.elf` 6d295f56 — `0dc08b3766c6d8d690745562462ced538030c9be11883f9318b2e3f53473b724`, 240,960 B (passed)
- `TEST_THIS_DLL.elf` f72faea5 — `588d6e6ddf476fe764153df9a3235a3558f0e98e2a3b4b6070a6fb645c47bff4`, 202,680 B (passed)
- `LIVEFACTS_PROBE.elf` lfp-84fdce4c — `bfc113b719e6e42a2efd71af92ff152d179fa746a9675ffe7eda8b13cf4a5030`, 113,104 B (8/8)
- `MUTEX_DLL_TEST.elf` 6f5bf85a — `fc4b09afc034525678aee2a5d4544b7a72308eb4cac3141b9a841b067fd055fb`, 259,568 B (passed)
- worker-liveprobe PRX: built + statically verified, **not executed**.
- Composition-4 v2 staging package + 45/45 loadlist: rehearsal reported READY-TO-EXECUTE,
  zero corrections; execution not run.
- worker-web-ui-connect: 5 canonical patches (p1-p3 backend, f1-f2 frontend), 5/5 `git apply --check`,
  sequential apply byte-verified — **proposal-only, unapplied**.

## Open items (the honest list)
1. **v3 in-module exec crash** (igf6, 3/8 records) — gates PRX staging and runtime-ack integration
   (worker-prx-link-gaps: NOT staged/launched until resolved).
2. **LOAD7 mapping missing**; GUA runtime address pending arbitration; staged-pair-live code anchors
   not byte-confirmed.
3. **10 worker dirs in flight without REPORT**: combined-igf7, crt-integrate, launch-automator,
   module-enum, multi-mod, patch-conflict, patch-conflict-retry, seh64-design, static-tls,
   streaming-bridge (plus this snapshot until its REPORT lands).
4. worker-itemspawner-dryrun **blocked**.
5. Web UI patches unapplied; they are fallbacks — prefer the sibling outline if it lands.
6. v2 facts 56-vs-63 record-count discrepancy unreconciled (worker-session-audit).
7. Mutex worker-side blocking path not proven invoked (CURRENT-BUILD caveat).
8. worker-c2-alt-correspondence needs one probe launch.
9. Game-file install HOLD in force; any in-game launch needs explicit user go (crash history).

## The immediate next action
Reconcile the 10 in-flight worker slots (deliver or mark blocked), then — with explicit user go —
run the next bounded in-game probe continuing the igf6 exec-crash diagnosis (build on
MODULE_LOADED_MAPSCAN_OK); nothing further on the acceptance ladder can be staged until it resolves.
In parallel, with no console dependency: apply the verified 5-patch web UI set in a worktree.
Standing rule: never re-run an already-passed diagnostic without changed implementation or a
specific open question.
