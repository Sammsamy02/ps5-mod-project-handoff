# Reviewer package: Townfall launch 3 (B6b), 2026-10-12

Labels: console-proven = this session's PS5 logs; Mac-only = code/host tests.

## Today in order

| Launch | Build | Result |
|---|---|---|
| 1 (morning) | B4 `d044cf0b` | No crash. Spawner menu never opened, self-test 0/168, early lag. |
| 2 | B5 `28d8331f` | Spawner, SpeedHack, minimap, self-test all worked (60 key presses delivered). Crashed after ~4.5 min: out of memory. |
| 3 (now) | B6b `41728f48` (snapshot 3b6b1b5e) | memory fix + diagnostics (below) |

## Launch 1 root cause (console-proven + Mac-only, verified by a second agent)
- The minimap's 3 Blueprint RegisterHook calls are refused on PS5 (HookProcessInternal=0). The refusal is raised
  with lua.throw_error while LuaMod::m_thread_actions_mutex is held. Lua is compiled as C here, so the raise is a
  longjmp that skips the lock guard: the game thread kept the recursive mutex forever and every other thread
  (pad/keybind UpdateThread, async LoopAsync threads) blocked. One keybind in the whole session.
- Same signature in 2026-10-11's minimap session (pre-B1..B4 build), absent in every no-minimap session.
- Keyboard keys never reach RegisterKeyBind on PS5 (pad-only input source; keyboard source parked per your answer).

## B5 (lock fix) `28d8331f`
- 15 direct lock-then-raise sites + 2 get_mod_ref helper raises become C++ exceptions inside the lock scope;
  process_lua_function's TRY converts them to the identical Lua error after the guard is released.
- Pre-build gate lock_raise_gate.py (direct mode) added: 0 findings. Host test reproduces the leak (old) and
  shows the fix (new), byte-identical error text. Console-proven in launch 2: 60 keybind entries, menu opened, spawns.

## Launch 2 crash (console-proven)
- Lua lives on the game process's libc heap (shared with the game and our C++). Self-test item 133 walks
  ~160k objects; Lua live grew to ~7 MB; 3072-byte allocations failed (fails=1684); std::bad_alloc in an error
  log left m_is_currently_executing_game_action stuck true, so the game-thread queue never drained (depth 2->41:
  minimap/menu/SpeedHack went dead, stutter stopped); a bad_alloc escaped SpeedHack's async thread -> app crash.
- Report: evidence/launch-l26-B5-launch2-20261012/REPORT-l26-B5-launch2-20261012.md (self-test 132/168 table).

## B6b contents (one independent reviewer + re-check)
1. **[R9-MEM] Lua on GMalloc**: new and (B6b) rebased mod Lua states allocate through the engine's own
   allocator (FMalloc Realloc/Free, slots 64/80 already profile-verified; GMalloc already bound by the runtime).
   Probe-checked at install, fail-closed to libc. Log: `[MEM] lua-backend=gmalloc ...` and per mod
   `[MEM] mod=X lua-state-backend=gmalloc rebase=1`. Note: GMalloc out-of-memory is an engine fatal, not null.
2. **[R9-MEM-REBASE]** (reviewer blocker fixed): mod states are created at setup_mods, before GMalloc is bound;
   at start_mod a still-empty state is replaced by a GMalloc state (fail-closed if not empty).
3. **[R9-EIGT]**: RAII guard for the game-action flag + per-action catch so one escaped exception cannot jam the
   queue or drop the remaining actions; C++-throwing ref lookup at both guarded sites.
4. **[REFUSED]** uniform log line `[REFUSED] kind= target= mod= reason= effect=` + `GetRefusedHookCount()`;
   self-test GAPS now counts runtime refusals (Lua-only change, fast deploy). Known gap: CustomEvent / BP
   script-hook path not yet counted (deferred).
5. Stale get_function_ref under the action lock -> C++-throwing variant (residual from the 38-helper list: 38 -> 17;
   the remaining 17 are dispatch helpers covered by B7).
6. **[B6-RATE]**: per-widget StaticConstructObject log line rate-limited (first 20, then 1 per 1000).
- Gates: fs-throw, no-detour, lock-raise (direct), build, packaging, verify_build with 27 markers.

## Test set changes
- InfiniteStamina removed (does nothing on PC either: its on-switch is never set).
- **SpeedHackMod** added (Nexus mod 10, original download, sha256 d0d2b85e...; no RegisterHook): F1-F4 mapped
  to TouchPad+R1 / R1+R2 / TouchPad+L2 / L1+L2 by keymap-gen.py (pinned rows unchanged, 0 conflicts).
- zzdiag v12 (ours): light dispatch probe `[Q] n try/ok/ran`, fixes v11's 'ts' error, drops the full Actor scan.

## Prepared, waiting for your decision
- **B7 (Lua as C++)**: plan + catch(...) audit done (round8-prep/b7-lua-cpp/). Proposal: B7a (runtime edits +
  3 game-boundary catch guards + startup mode probe) then B7b (flag on). Decisions: two builds or one; launch
  only B7b; contained hook-callback errors are logged, not delivered (differs from PC).
- **Refused hooks**: 22 rows (4 loud, 18 silent) with data-only routes (round8-prep/refused-hooks/REFUSED-HOOKS.md).
  Next steps proposed: one console probe launch (zzdiag hooks one BP + one native event), scan-based BP function
  resolver, NotifyOnNewObject via AddUObjectCreateListener (variant C), map-load/BeginPlay via VMT.
- Game version on the PS5 main menu: still to be read (mods were written before patch 1.4.0).

## Launch 3 must show
Spawner menu on every L3+R3; SpeedHack keys; minimap; no crash past 5 minutes; self-test SUMMARY + GAPS
(168 checks) with SLOW timings; `[MEM] lua-state-backend=gmalloc` for every mod; `[REFUSED]` lines; stutter
timeline vs the `[B6-RATE]` lines.
