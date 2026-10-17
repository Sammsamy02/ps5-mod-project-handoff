# Agent FU — host Lua harness for the 12-mod ladder

2026-10-10. Mac-local Lua 5.5.1 only; no builds, no console, no mod edits.
Deliverables: `outputs/2026-10-08-l25-prep/host-lua-harness/` —
`stub-ue4ss.lua`, `runner.lua`, `run-all.sh`, `runs/*.log` (26 logs),
`RESULTS.md` (full per-mod detail + gap-queue feed).

## What was built

- **stub-ue4ss.lua**: recording stub of the composed console runtime's global
  surface (globals mirror the typescan-r4 LuaMod.cpp registration grep).
  Unified chainable UObject stub ("cell"): every method call recorded and
  returns a stub; every property read returns a stub (X/Y/Z/W/Roll/Pitch/Yaw
  → 0.0; array idx 1..4 then nil); writes recorded and persisted;
  fidelity specials: IsValid/GetFullName/ToString/GetClass/IsA/IsClass→true,
  HasAnyInternalFlags→false, RemoteUnrealParam get/set, weak-object Get,
  GetSuperStruct/GetSuperClass→TERMINAL invalid (see H1), LineTraceSingle
  fills the OUT HitResult, CreateInvalidObject→invalid object, FName/FText
  ctors + NAME_None with FName __eq, auto-enums (Key/ModifierKey/EObjectFlags/
  EInternalObjectFlags/EFindName/PropertyTypes), UnrealVersion@UE5.6, UE4SS,
  ModRef, FPackageName. io/os ABSENT by default; --with-io provides recorded
  shim stubs. debug = getinfo/traceback/getlocal whitelist (MiniMap's
  settingsPath relies on it). 5.2–5.5 common syntax subset only.
- **runner.lua**: one process per subject+mode: compile → run startup chunk →
  fire all keybind callbacks (incl. Async) → fire all RegisterHook callbacks →
  fire all console-command handlers (sample args) → fire LoadMap pre/post +
  BeginPlayPost → fire NotifyOnNewObject. Custom events never fired (no BP
  caller can exist). Sandbox require resolves UE4SS search paths incl.
  shared/jsbProfiler/jsbProfi.lua pattern and loads modules into the same
  sandbox. Watchdog: 20 s wall + instruction-count hook (it fired once, see
  H1). Execute* run immediately depth-capped; Loop* record-and-return.
- **run-all.sh**: 13 subjects × 2 io modes; verdict table to stdout, raw logs
  to runs/.

## Calibration law — upheld

`evidence/townfall-main.lua.console-copy` (KNOWN-GOOD on console) is
LOADS-CLEAN in both modes; 6/6 keybinds + LoadMapPreHook fire clean (844
recorded calls; full menu-build path executes). The law forced two STUB fixes
(never mod edits): (1) added FPackageName (port LuaMod.cpp:7827; live use
dump_object:137), (2) taught GetSuperStruct/GetSuperClass to terminate after
dump_object's unbounded `while Object:IsValid()` walk spun the watchdog.

## Results (predicted console behavior, one-liners)

- TownfallItemSpawner console copy — LOADS-CLEAN both modes (calibration proof).
- 01 LineTraceMod — LOADS-CLEAN; F3 fires clean at Lua level (struct-OUT is P5's question).
- 02 CheatManagerEnablerMod — LOADS-CLEAN; ClientRestart hook fires clean.
- 03 ConsoleEnablerMod — LOADS-CLEAN; load-time EIT + hook + UnregisterHook clean.
- 04 ConsoleCommandsMod — LOADS-CLEAN; all 3 console handlers register AND fire clean at Lua level.
- 05 SplitScreenMod — LOADS-CLEAN; CTRL+Y/U/I + all EIT closures clean (PAD-GAP remains).
- 06 TownfallMiniMap — io-off FAILS-AT main.lua:251 (unguarded io.open at load via loadSettings :4266); io-shim-on LOADS-CLEAN. Reconciled with FT §0.1.3 (luaL_openlibs → io open): LOADS-CLEAN on console, settings degrade non-fatally.
- 07 TownfallModMenu — LOADS-CLEAN; 4 keybinds + 9 hooks + 5 map hooks fire clean; ExecuteAsync 2-arg fallbacks never reached (FT gap #19 latent).
- 08 Keybinds — LOADS-CLEAN; all 7 tool callbacks fire (all seven registered in typescan-r4 — confirms FT's correction to FO).
- 09 BPML_GenericFunctions — LOADS-CLEAN; 2 custom events registered, never fire (correct).
- 10 BPModLoaderMod — FAILS-AT main.lua:117 (its own guard, IterateGameDirectories→nil assumption); SKIP unchanged.
- 11 ActorDumperMod — LOADS-CLEAN at Lua level; 0x98 offset law unchanged (SKIP, never run).
- 12 jsbLuaProfilerMod — io-off FAILS-AT jsbProfi.lua:115 (os) inside require; io-shim-on LOADS-CLEAN; SKIP stands.
- bonus: stamina mod (FT §E.1) — LOADS-CLEAN (1 run, io-off).

## Aggregate (feeds FT's gap queue; see RESULTS.md 'Host harness results')

- Missing globals union across all subjects: only `io` (MiniMap:251) and `os`
  (jsbProfi:115) — both dissolve under FT's luaL_openlibs source evidence.
  Everything else FO called c2 is BOUND in the current tree (reconciled).
- NEW (execution-only) finding H1: dump_object's unbounded super-struct walks
  (:156/:181/:210) hang if port GetSuperStruct/GetSuperClass doesn't
  terminate — recommend a wall-clock guard on the P2 dump_object dispatch.
- 21 keybind callbacks fired across subjects, zero errors — remaining
  keybind risk is PAD-GAP only.
- Harness greens are Lua-level only; native marshal questions stay with
  P3/P5/P6/P7 probes (chainable stubs make every reflected call "succeed").

## Honest caveats

- IterateGameDirectories→nil is my documented PS5 assumption (LuaMod.cpp:3280
  warning path); a real Dirs tree would move BPModLoader's failure past io.
- The io-off mode encodes the "io absent" hypothesis; FT's source evidence
  says libs are open — io-shim-on is the realistic mode; both are logged.
- Loop* tick bodies and custom-event handlers are never executed host-side by
  design; MiniMap/ModMenu tick-path runtime behavior remains P7/console-side.
