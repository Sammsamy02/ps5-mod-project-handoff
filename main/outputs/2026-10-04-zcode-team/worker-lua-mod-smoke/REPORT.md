# REPORT.md — worker-lua-mod-smoke

2026-10-04. Prediction delivered BEFORE the console cycle: **main.lua will LOAD without a
script error on the port UE4SS tree.** The only Lua-level failure mode at F7 press is the
EngineTick-availability gate on `ExecuteInGameThread`, which the port's keybind dispatch
catches and logs (menu silently never opens — a burned interaction, not a crash).
Full evidence: SMOKE-RESULTS.md, harness/PHASE-LOGS/.

## Bottom line for composition-5

After `mods-started`, staging `Mods/TownfallItemSpawner` (byte-identical to the audited
bundle) is **safe at load**: no syntax errors, no nil-index, no require mismatch, exactly
6 keybinds registered (F7/UP_ARROW/DOWN_ARROW/RETURN/LEFT_ARROW/RIGHT_ARROW = 0x76/0x26/
0x28/0x0D/0x25/0x27, no modifier chords; PS5Pad.ini maps OPTIONS/D-pad/CROSS onto them),
map hook accepted, load banner printed. Worst-case load-time console noise is zero.

## Divergence / risk matrix (vs port registrations, cross-checked to LUA-API-MATRIX.md)

Every global main.lua/UEHelpers calls at load or F7 is registered by the port
(`src/Mod/LuaMod.cpp`; full extraction in harness/ue4ss_host.lua). **Zero "mod calls it at
load but port does not register it" entries → no predicted LOAD failure.** Residual risks,
ranked:

### Ranks 1-3 — fire at F7 PRESS (pad OPTIONS), not at load

1. **ExecuteInGameThread EngineTick gate — THE gate.** Port registers it (LuaMod.cpp:4690)
   but the staged `UE4SS-settings.ini:12` sets `DefaultExecuteInGameThreadMethod=EngineTick`,
   and plain `ExecuteInGameThread` (unlike `ExecuteInGameThreadWithDelay`/`LoopInGameThreadWithDelay`,
   LuaMod.cpp:4795-4812) does **not** fall back to ProcessEvent on `__PROSPERO__` — it throws
   when `UEngine::TickInternal.is_ready()` is false (LuaMod.cpp:4715-4720). Every one of the
   mod's 6 keybind callbacks is wrapped in `ExecuteInGameThread` (main.lua:695-728), so if the
   EngineTick AOB scan did not succeed before F7, the menu can never open. The throw is caught
   by the keybind dispatch lambda (LuaMod.cpp:1937-1948) and logged to UE4SS.log — the
   predicted console symptom is the log line "ExecuteInGameThread: EngineTick method requested
   but EngineTick hook is not available (AOB scan failed)" and a dead menu, NOT a script error
   at load and NOT a runtime crash. *Console check after mods-started: confirm EngineTick scan
   success / fire F7 and read UE4SS.log for that line.* (Matrix concurs: "THE single observed
   hard gate", there on the host composition; on the PORT it is conditional on scan readiness.)
2. **F7 depends on engine-backed lookups that are PRESENT-BUT-GATED in the port.**
   No-save F7 is fully self-guarded (verified: "Item database not ready. Load a save first.").
   With a save loaded, first F7 needs real `FindAllOf("PlayerController")`/`FindFirstOf`
   (needs GUObjectArray), `require("UEHelpers")` executes then, and buildUI needs
   `StaticFindObject("/Script/UMG.Default__WidgetBlueprintLibrary")`, the six `/Script/UMG.*`
   classes, FText, property writes, and ProcessEvent calls. Lua-side, none of this can
   nil-index (all pcall/isValid-guarded — verified end-to-end); engine-side, these are the
   matrix's PRESENT-BUT-GATED rows (FProperty offsets for shipping UE5.6, ProcessEvent
   callable addresses). Failure shape: mod's own guards log "UI creation failed:"/"Scan
   failed:" and the menu stays closed.
3. **Out-param marshalling for `LineTraceSingle` (main.lua:245-269).** Lua-side contract
   verified (mod passes a plain `hit` table, reads `.ImpactPoint/.ImpactNormal.Z`). Port-side
   is the matrix's highest-risk row (modreqs B5, never engine-executed). If marshalling
   fails live, spawnSelected logs "Failed at ground trace." — again guarded, no crash.

### Fire at LOAD — none found

4. **RegisterKeyBind return shape**: task prompt expected "a handle table"; the port returns
   **zero values/nil** (LuaMod.cpp:2029). Divergence from the prompt, NOT from the mod —
   main.lua discards the return. No action needed.
5. **FindFirstOf/StaticFindObject return truthy invalid-object wrappers on failure** (not nil;
   LuaMod.cpp:1524/1553/1655 via `auto_construct_object(nullptr)`), while **FindAllOf returns
   nil when empty** (LuaMod.cpp:1716). The mod's `isValid()` (main.lua:8-12) and UEHelpers'
   `or FindAllOf("Controller")` handle both shapes; verified in no-save and save modes.
   This asymmetry is a footgun for future mods, not for this one.
6. **`require("UEHelpers")` timing**: deferred to first F7 (main.lua:15 is inside
   getPlayerController). A hypothetical require failure would therefore burn the F7 cycle,
   not the load cycle; verified resolvable via the staged `Mods/shared/UEHelpers/UEHelpers.lua`
   layout through the port's `Mods/shared/?/?.lua` pattern (and the custom loader's
   `ue4ss/Mods/shared/<mod>/<mod>.lua` candidate, LuaMod.cpp:868-874).
7. **`LoadAsset` throws off the game thread** (LuaMod.cpp:3626). Only reachable inside the
   F7 spawn path, which runs in a game-thread closure — correct thread by construction.
   Verified both directions in the harness.
8. **LoadMap pre-hook**: registered and optional; mod pcall-guards it (main.lua:731), so even
   a composition missing it degrades silently. Port registers it (LuaMod.cpp:2711) — no issue.

## Method + limits (honesty per QUALITY-RUBRIC)

- Harness executes the real mod bytes on the port's own Lua 5.4.7 (LuaRaw deps) with stubs
  for exactly the port's registered surface and port-faithful return shapes/error texts.
  Build: `cc -O2 -DLUA_USE_MACOSX -I LuaRaw/include -o bin/lua LuaRaw/src/*.c (minus
  luac.c/luauser.c) lua_lock_shim.c -lm`; shim replaces the Windows-only LuaLock hooks
  (luaconf.h:796-800). No port source modified; all writes in worker-lua-mod-smoke/.
- Limits: (a) stub world models API *shapes*, not engine truth — Ranks 2-3 remain
  engine-evidence questions for the console run; (b) single-Lua-state model — the port's
  keybind/game-thread callbacks run in `m_hook_lua` threads of the same lua_State
  (LuaMod.cpp:988-991), so globals are identical (faithful); (c) `luauser.c` (Windows-only,
  zero exports) excluded from the build — any Windows-specific VM behavior untested;
  (d) 941-call trace covers the exercised paths only (menu build, nav, one spawn, map reset,
  reopen); unexercised port functions are registered as recording stubs.
- Nothing here claims engine-side acceptance; it predicts Lua-level load safety (high
  confidence) and F7 behavior conditioned on EngineTick scan readiness (the one thing to
  check in UE4SS.log after `mods-started`).

## Deliverables

- `CLAIM.json` — first (status updated to complete)
- `harness/` — bin/lua (5.4.7), ue4ss_host.lua, run_phases.lua, run_all.sh,
  lua_lock_shim.c, key_table_gen.lua, staged Mods/ layout, PHASE-LOGS/ (all-phases.txt + 5 dumps)
- `SMOKE-RESULTS.md` — PASS/FAIL per phase with captured output
- `REPORT.md` — this file (divergence matrix, ranked)
