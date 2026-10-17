# ZCode controller claim — PS5 mod-manager project

- Claimed by: ZCode root session (GLM-5.3 Max), 2026-10-04, per
  outputs/ZCODE-HANDOFF-2026-10-04/START_HERE.txt.
- Handoff validated: ZIP sha256 26a980602968fa789310b3a947259234747e2e11c25dba7e8a3578708e8617fb,
  19 manifest files, CRC clean, on-disk handoff directory byte-identical.
- Frozen artifacts re-verified this session:
  - 6d295f56 diagnostic ELF = 0dc08b376c6d8d690745562462ced538030c9be11883f9318b2e3f53473b724
  - proven PRX modbridge_runtime_aligned_entry.prx = 3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654
  - console alias TEST_THIS_DLL.elf (f72) = 588d6e6ddf476fe764153df9a3235a3558f0e98e2a3b4b6070a6fb645c47bff4
- Codex heartbeat ps5-app-development: PAUSED (AUTOMATION_BEFORE.json retained). One controller only.
- Sole owner of: TRANSPORT_CHECKPOINT.json updates, runtime composition, private
  publication, console/hardware action queue. Workers never touch these or the console.

## Console policy (reconciled 2026-10-04)
- Standing gates honored: game_file_mutations_permitted_now=false (installation HOLD);
  no game-file writes, no Townfall launch, no autoload changes.
- console.new_dispatch_allowed_without_review=false is superseded ONLY for the single
  handoff-directed dispatch of the reviewed 6d295f56 standalone diagnostic
  (NEXT_ACTIONS item 1: stage, record identity, dispatch once, collect results).
  No other dispatch without explicit user/review approval.
- Historical agent-upload precedent followed (f72: agent uploaded + verified readback,
  user selected/ran). If the dispatch route misbehaves, stop and leave staged.

## Seat budget (10 open requests max, account-wide)
1 controller (this session) + workers below; no nested subagents anywhere.

| slot | worker slug | area | status |
|---|---|---|---|
| 2 | worker-1-live-supplier | owning live-module facts supplier + firmware profile | DONE — host-verified contract; 7 target facts open (checkpoint zcode_worker_1_live_supplier) |
| 3 | worker-2-layout-facts | Townfall UE5.6 build/layout profile | DONE — validator VALID, 39 tests OK |
| 4 | worker-4-win-services | InitOnce family | DONE + INTEGRATED by controller — host + TARGET PASSED (1a6b37b7) |
| 5 | worker-6-containers | archive/container capability breadth | DONE (magic interpretation corrected per user: full header/version dispatch) |
| 6 | worker-7-installer | modforge-service/1 transactional install slice | DONE — 108/108 host tests |
| 7 | worker-3-gamethread | UE4SS seam map + refusal adapters | DONE — found PublishStagedNativeLayoutMaps has NO production caller (controller wires next) |
| 8 | worker-9-evidence-review | 6d295f56 milestone audit | DONE — milestone stands 41/44; wording applied |
| 9 | worker-modreqs | representative-mod requirements census | DONE — Item Spawner API inventory + services backlog |
| 10 | worker-8-app-backend | app/backend audit + drop-in service modules | running |

Controller integrations this session: InitOnce wired into build.py/owned_runtime/
fixtures/result schema (host + target PASSED); libarchive skew resolved (3.8.7
aligned); UE4SS seam wiring (supplier + layouts + PublishStagedNativeLayoutMaps
production call) is the active critical path.

Worker outputs: outputs/2026-10-04-zcode-team/<slug>/ (CLAIM.json, sources, REPORT.md).
Console action log: CONSOLE-LOG.md in this directory.
