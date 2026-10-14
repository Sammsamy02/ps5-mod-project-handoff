# Cap #4 review (independent reviewer, one pass)

Reviewer verdict: **PASS-WITH-FIXES**. Fold fix 1 into the patch before the one build (build-once policy: fixing it now is cheaper than a second launch). Everything here is **Mac-only**, apart from the launch-14 log lines cited as console-proven.

## What I re-ran (Mac-only)
- `sh tests/run_tests.sh` on a byte-copy of the cap04 folder, so the deliverable was not overwritten. Result: **35 steps, 0 failed**, 62 simulation checks, 7/7 mutants caught, exit 0. The regenerated `cap04-vs-R11.diff` is byte-identical to the staged one. The deliverable sha256s did not change.
- `no_detour_gate.py` on the live candidate + patch: PASS, 48 sources scanned, **0 notes** (R11 has 4).
- `fs_throw_gate.py` on the real live candidate (read-only): PASS, 48 files, 0 new. On scratch copies the gate always reports "35 NEW" because of the path-keyed baseline, on R11 as well. That is why T5 compares site sets, and that comparison is valid.
- clang AST dump of the patched live `LuaMod.cpp`: `RC::early4_mods_started` has external linkage in `RC`, which matches the declaration in `UE4SSProgram.cpp`. So it should link, not just pass `-fsyntax-only`.
- The PS5 syntax logs have no new warnings (luamod: 13 vs 23 on R11; program and uobjectglobals unchanged).
- The cited evidence checks out (console-proven, launch 14):
  - `klog.decoded.txt:2645` shows EXEC `<51277108 msec>` for pid 365.
  - `supplier-diag.jsonl:105290` has pid 365 `module_start_entered t_ms=51345995` (+68.9 s).
  - `ue4ss.decoded.log:939-941` has the two "Failed to add hook" lines plus "deferred arm refused".
  - `ue4ss.decoded.log:905` is the start gate (20:51:47) and `:937` is the first carrier (20:51:59).
  - `ue4ss.decoded.log:875` is the only startup world hook (RegisterLoadMapPreHook).

## Hard rules
- **Detours:** the patch adds no hooks and no detours. The PS5 path can no longer reach any `Register*Callback` detour: the eager block is `#else` (non-PS5) only. Data-only.
- **Generic:** the added lines contain no game, mod or Leon/Stellar names and no new offsets.
- **Fail-closed:** 2+ GameStates leads to skip. An unusable index or allocation failure leads to skip. A late registration gets no replay. No world hooks left leads to abort.
- **Other rules:** no SDK, no console contact, mod files untouched.
- **Tests are real:** they run the shipping code extracted verbatim, with sanitizers and mutants.

## Findings

### 1. FIX-SOON (shipping runtime): a map or actors delivered twice when the route goes live before the start gate
- **How it happens:** carriers are armed in `LuaMod::on_program_start`, before `start_lua_mods()`. `main.lua` runs without `m_thread_actions_mutex`. So a game-thread pump can run while mods are still starting.
  - If a mod has already registered a world hook and the index is usable, `ensure_ready()` takes the route live and `observe()` delivers new objects live.
  - At the first feed after the gate, the snapshot contains those same objects. They are synced and are not in the unmatched backlog, so they are replayed a second time.
  - `on_game_state` only de-duplicates while the window is open, so the whole map goes out again: LoadMapPre/Post, InitGameState and every BeginPlay.
- **Reproduced (Mac-only):** I added scenario s11 to a scratch copy of `sim_main.cpp`, run against the same extracted code. The route goes live, MapB arrives as new objects and is delivered, then `mods_started()` runs. Result: `calls: 1 3 4 5 6 5 6 5 6 5 6 2 1 3 4 5 6 5 6 5 6 5 6 2`. The full MapB sequence is delivered twice, with 5 of 6 checks failing.
- **Townfall exposure:** low for this launch. In launch 14 the gate was at 20:51:47 and the first carrier at 20:51:59 (console-proven, lines above). But if the runtime arrives while gameplay is already running (carriers fire 800-2900/s), the window is real. It is also likely on other games (Stellar Blade blind test).
- **Fix (tested in the scratch sim):** do not take the route live until startup has closed.
  - In the feed, change `if (world_ev::s_wanted) ensure_ready()` to `if (world_ev::s_wanted && world_ev::s_startup_closed) ...`.
  - In `note_index`, change `if (s_wanted && ...)` to `if (s_wanted && s_startup_closed && ...)`.
  - Objects consumed before the gate are then delivered by the replay instead. With this change s1-s8 and s10 pass, and s11 delivers MapB exactly once (`calls: 1 3 4 5 6 5 6 5 6 5 6 2`).
  - Two expectations change and must be updated: s9's "world-route-live before the gate" check, and s11's "delivered live before the gate" check.
  - Add s11 to the suite plus a mutant that removes the guard.

### 2. FIX-SOON (launch card / order): base and rollback are not what will actually be built
- **The build is not R11 + cap #4:** APPLY says to apply with `--allow-drift` to the live candidate, which already carries the R12 follow-ups (`[R12-WORLD]` emit_counted, the 60 s pending counted line, and a `UObjectGlobals.cpp` comment). So the build is R11 + R12 changes + cap #4, while the card says rollback = R11 `bfd95db6`. That breaks "one launch per capability" attribution.
- **Order (CLAUDE.md GOALS):** caps 1-12 come only after the Townfall step-1 launch (B7, map-load/BeginPlay refused hooks, 15 fields). That step touches the same world_ev / LoadMap code.
- **Change the card to say:**
  - (a) build only after the step-1 launch is clean;
  - (b) re-stage on that tree (the exact-once anchors will refuse if the region moved, which is good);
  - (c) rollback = the last clean build at that time, by its sha.

### 3. NOTE (shipping runtime): the snapshot can hand `Want()` a stale class pointer
- **Where:** `r9_index_snapshot_live` uses `S.ClassAt[i]` once `LiveAt(i) == ObjAt[i]`.
- **When it goes wrong:** at call site 1 the snapshot runs on the previous feed's Sync, before this feed's collect. If a slot is freed and refilled at the same address between two syncs (the live-tree comment at `UObjectGlobals.cpp:1421` says this happens "rarely"), `ClassAt` can still name the old class. A Blueprint class can be unloaded at a map change. `chain_has()` would then walk a freed class, and that object would also be collected as new right after (fed twice).
- **Fix:** read `Object->GetClassPrivate()` (the object is live) and skip the slot unless it equals `ClassAt[i]`. The comment "never a stale class pointer" is overstated until then.

### 4. NOTE (shipping runtime): actors already being destroyed are replayed
- **What happens:** the snapshot and `replay_step` filter only Unreachable. An actor that is destroyed but not yet garbage-collected (marked Garbage/PendingKill, as in a map teardown in progress) passes and gets a replayed BeginPlay. If it is the only GameState, it also gets a LoadMapPre.
- **This launch:** not hit, because the only startup hook is LoadMapPre (ue4ss.decoded.log:875).
- **Fix:** also skip objects with the PendingKill/Garbage internal flag, with the layout taken from data, never hard-coded.

### 5. NOTE (verifier `early_timeline.py`): false FAILs would trigger a needless rollback
- **K2:** requires mods-started <= first-pump. A carrier firing during mod start is legitimate on PS5 (the team's own s9 shape), and K2 would then FAIL.
- **K1:** a K1 FAIL only means the two clocks differ. It is not a runtime fault.
- **Change:** the card lists "K1/K2/K3 FAIL" as roll-back conditions. Make K1/K2 "investigate the timeline", and keep rollback for runtime symptoms and K3.

### 6. NOTE (performance, one-time)
- **Snapshot cost:** the snapshot scans the whole index (launch 14: 153229 slots, ue4ss.decoded.log:944) in one go, at -O0, while holding the index mutex. The delete listener blocks meanwhile.
- **Possible drops:** a startup BeginPlay hook on a big map could exceed `kMaxPending` (32768) and show `dropped>0`.
- **Card PASS line to add:** `snapshot_ms` and `took_ms` on the `[EARLY] stage=startup-replay` lines are under ~20 ms, and the user feels no hitch at that moment.

### 7. NOTE (user report: Leon checkered textures, outdoors too bright)
- **Routing:** I agree with the team. This is not cap #4 and not timing (a late mount would show the original asset, not the engine's grey/white default-grid material). The "PC-cooked material/shader cannot render on PS5" idea is a Mac-only hypothesis, still unproven.
- **Card change:** the launch card should tell the user that both symptoms are known, already present before this build, and not part of this launch's PASS/FAIL. Otherwise the user may judge cap #4 by them.
- **Pak team, to confirm (game closed, read-only):** check whether the game log has material/shader errors, then decide on one launch without the Leon pak (with the user's OK).

## Checked and fine
- **Apply script:** exact-once anchors, refuses a drifted base and a second application, prints the count, and fails on zero. No CRLF issue (the files have 0 CR). The live-tree and R11 patch diffs are the same size.
- **Linkage:** `r9_index_snapshot_live` declaration and definition are in the same namespace with the same type. `early4_mods_started` is called only on PS5 and only after the gate.
- **Locks:** the replay state is touched only on the game thread. The snapshot runs outside `m_thread_actions_mutex` (index lock only), so no lock-order inversion.
- **Non-PS5 eager block:** still compiles, and the LoadMap callbacks are `[[maybe_unused]]`. The `common_opts` that is now unused on PS5 is covered by `-Wno-unused-variable`.
- **ChatGPT `deferred-arm-stage`:** correctly marked as superseded. Do not integrate it.
