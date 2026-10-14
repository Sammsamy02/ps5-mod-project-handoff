# Cap #4 notes — early loading proof / deferred arm

Evidence: `outputs/2026-10-08-l25-prep/evidence/launch-l26-R11-launch14-20261013/`. Reproduced by
`tests/launch14-early-timeline.txt` (tool `PATCH/tools/early_timeline.py`).

## 1. Findings from launch 14 (console-proven, this run's existing logs; no new console contact)

| Point (kernel / sink ms clock) | +s after game start | Source |
|---|---|---|
| Game process EXEC | 0 | klog.decoded.txt:2645 `EXEC /app0/eboot.bin [user] ... <51277108 msec>` |
| Runtime `module_start` | +68.9 | supplier-diag pid 365 seq 1 `module_start_entered t_ms=51345995` |
| Mods started | +75.1 | diag seq 1107 `on_program_start_entered`; ue4ss.decoded.log:905 `[L25] start gate: 7 started mod(s)` |
| First game-thread pump | +86.6 | diag seq 1122 `r3_marshal_first_invoke`; ue4ss:937 |
| WORLD-EV route live | ~+87 | ue4ss:1011 |
| First map seen (`Level_Townfall`, a NEW GameState) | ~+88 | ue4ss:1060-1061 |

- Two sources agree on the ~69 s injection delay. The klog wall time just before EXEC (klog:2611 `atime 01:50:31.217`) plus 68.9 s lands within about 1 s of the UE4SS first line (20:51:45 local = 01:51:45 UTC).
- **Cause (Mac-only, from the pinned ploader source):** `wait_stable()` in `.superpowers/native-dependency-spike/ploader-cleanup-candidate-1/original/main.c:518-559` waits for the module count to hold steady for 1 s, with a 500-tick budget of ~25 s sleep plus per-tick kernel calls. That matches the earlier `worker-ploader-70s` analysis (~64-70 s burn). So the runtime does not arrive at "process create". Everything in the first ~75 s is missed: the boot map(s), the front-end map, and their LoadMap / InitGameState / BeginPlay. PC UE4SS starts mods during engine init (`UnrealInitializer.cpp` "Waiting for object construction", ≥10000 objects) and would have seen them.
- Also console-proven: no PlayerController existed when mods started (ue4ss.decoded.log:659 `[ST] fao PlayerController PASS count=0`). The pump did not run until 11.5 s after mods started, because carriers only fire when their UFunctions run.
- In R11 a LoadMap code-detour WAS attempted on PS5. It was soft-refused: ue4ss:939-941 has two `Failed to add hook` lines and then `deferred arm refused`. This breaks the "no detour reachable on PS5" rule. Patch E1 removes it, and the no-detour gate goes from 4 grandfathered notes to 0.
- Objects alive when the index is first used are never reported (`UObjectGlobals.cpp` r9 notify-arm). So if mods start after a map is loaded, that map's events never reach Lua. Patch E2 replays them once.
- In launch 14, only TownfallItemSpawner had a world hook at startup (LoadMapPre). Nothing was lost there, because Level_Townfall loaded after mods started. The ueselftest hooks were registered at 20:52:30, which is late.

## 2. What PC UE4SS delivers before the first map that we still miss (Mac-only analysis)
1. LoadMap / InitGameState / BeginPlay for the boot and front-end maps. The **current** map is now covered by the replay. Earlier maps can only be covered by earlier injection (loader change, see 4).
2. NotifyOnNewObject for objects created before mod start. This is upstream behaviour too (constructions only), but on PC that window is tiny. Not replayed on purpose: that would not match PC.
3. A per-frame pump from the first frame (PC: EngineTick hook). Here the pump waits for a carrier. ExecuteInGameThread, delayed actions and keybind drains start late: 11.5 s in launch 14.
4. CDO edits in main.lua before the first actors spawn. Actors of maps loaded before mod start keep their old values.

## 3. Mac-only verification done (after the review fixes, see FIXES.md)
- `tests/run_tests.sh` → `tests/test-output.txt`: 40 steps, 0 failed.
- The world_ev, feed and snapshot code is extracted verbatim from the patched tree and run in 11 scenarios (71 checks incl. the snapshot unit test, ASan+UBSan). s11 is the reviewer's double-delivery case.
- 10/10 planted mutants are caught (M8/M9: the route goes live before startup closes; S1: a stale class pointer reaches `Want`).
- no_detour_gate and fs_throw_gate both pass, with no new sites.
- PS5 `-fsyntax-only` passes on R11 + patch and on the live tree + patch, and the negative control fails as it should.
- Verifier: 16 fixture cases pass.
- The simulation found a real bug in the first draft: an empty new batch returned before the replay ran. It is fixed (edit `E2-feed-empty-batch`).

## 4. ChatGPT prior work (re-checked, UNVERIFIED before)
- `deferred-arm-stage`: its pinned source hash `895a1365…` matches neither R11 (`95591da5…`) nor the live tree. Its 6 tests now ERROR (`tests/chatgpt-prior-work-recheck.txt`). It guards the partial native LoadMap registration. This patch removes that registration path entirely, so the stage is **superseded** and should not be integrated.
- `verify_milestones.py`: 8/8 tests still pass. Its normalized contract is replaced by `early_timeline.py`, which reads the raw harvest (decoded with the project decoder) and does the same ordering check plus clock identity.

## 5. Open questions / next steps
- **Earlier injection is the real fix for the boot and menu maps.** It is a loader (ploader) change: the existing proposals are `worker-ploader-70s` PATCH 02/05. That is a payload change, not a game executable, but it needs a user decision and its own launch. It also needs a check that the runtime copes with arriving during engine init (upstream waits for ≥10000 objects; the port's admission paths have not been tested that early).
- The replay delivers BeginPlay late for actors that began play long ago. This matches the existing "after the event" semantics. A mod that registers within the few seconds before the replay is delivered also gets the replayed events (small deviation, documented).
- Allocation failure during the snapshot is reported as `skip-index-unusable` (fail closed).
- **Open NOTE (review finding 4, not fixed):** the replay does not skip actors that are already marked
  PendingKill/Garbage but not yet garbage-collected (only Unreachable is skipped). The flag bit is
  engine-version dependent and the runtime has no layout-data value for it:
  - the only constant in the runtime is UE4SS's compile-time `EInternalObjectFlags::PendingKill = 1 << 29`
    (UE4 numbering; `FUObjectItem::IsPendingKill()` reads it);
  - the local Epic 5.6.1 `ObjectMacros.h` has `Garbage = 1 << 21` and bit 29 = `RefCounted`.
  So reading bit 29 would be wrong on newer engines (it would skip ref-counted live actors), and a
  per-version table would be a hand-written value with one source per version. Rule "two sources or
  fail closed" -> left out. Exposure: an actor destroyed during a map teardown that is still in
  progress when mods start could get one replayed BeginPlay (launch 14: the only startup hook was
  LoadMapPre, so not hit). Fix later together with the layout data (profile generator), with two sources.

## 6. User report relayed in this run (Leon textures checkered grey/white; outdoors bright "like day")
Not cap #4. It belongs to the pak work (#7/#8/#12). Mac-only hypothesis:
- A grey/white checker is what UE draws when a material cannot render: the engine falls back to its default grid material.
- PC mod paks carry PC-cooked materials with PC shader code and PC texture data. A PS5 build cannot use them, so it silently substitutes the default material.
- The too-bright outdoors fits the same cause: a lighting, fog or post-process material from the paks failing, so the game's darkness is lost.
- It is not a timing problem: a late mount would show the original asset, not a checker.
- To confirm (integrator, game closed, read-only): the `LogMaterial`/`LogShader` lines, if the game logs them. Then one launch with the Leon pak removed (with the user's OK), to see whether the darkness comes back.
