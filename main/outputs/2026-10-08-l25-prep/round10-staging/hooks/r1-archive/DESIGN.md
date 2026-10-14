# HOOKS track (round 10): map-load, game-state and actor-start hooks on PS5

Everything in this note is **Mac-only** (code reading, syntax check, gates). Nothing ran on the console.

## What it does, in plain words

Some mods ask "tell me when a new map has loaded" (LoadMap), "when the game rules object is set up"
(InitGameState), "when an actor appears in the world" (BeginPlay) or "when it leaves" (EndPlay).
On PC the mod loader patches game code to catch these moments. On PS5 game code cannot be patched,
so until now the runtime said "OK" to these requests and then never called the mod back.

The runtime already keeps a list of every game object and notices new ones (this is how
`NotifyOnNewObject` works today: `[R9-NOTIFY]`). This change reuses that list:

- A new **GameStateBase** object = a map has finished loading (the engine makes exactly one per map load).
- A new **GameModeBase** object = InitGameState has run.
- A new **Actor** = that actor has started (BeginPlay).

The runtime then calls the mods' callbacks on the game thread, from the same pump that already runs
`ExecuteInGameThread` and `NotifyOnNewObject` work, at most ~1 ms per frame, and one mod's Lua error
never stops the others. No game code is touched, no new offsets or hand-written values: the four
engine class names are standard Unreal names, resolved once by path (the same lookup the pump
carriers already use), and the only new flag (`RF_WasLoaded = 0x80000`) has two sources
(UE4SS `UnrealFlags.hpp` and the local Epic `ObjectMacros.h`; it is a public engine constant).

## Per hook (R05-R12)

| ID | Lua call | PS5 now | How it differs from PC |
|---|---|---|---|
| R05 | `RegisterLoadMapPreHook` | **Delivered** | Runs **after** the load (PC: before). `World` is an invalid object (the old world is gone), `Engine` invalid, `URL` nil, `Error` "". Return value ignored. |
| R06 | `RegisterLoadMapPostHook` | **Delivered** | Runs at the first pump after the load (PC: right when LoadMap returns), after the BeginPlay callbacks of that load (same as PC). `World` = the new world (from GameState -> Level -> World, class-checked). `Engine` invalid, `URL` nil, return value ignored. |
| R07 | `RegisterInitGameStatePreHook` | **Delivered** | Runs **after** InitGameState (PC: before). Argument = the GameMode (same as PC). |
| R08 | `RegisterInitGameStatePostHook` | **Delivered** | Same moment as R07, right after it. |
| R09 | `RegisterBeginPlayPreHook` | **Delivered** | Runs **after** BeginPlay (PC: before), at the next pump. Argument = the actor. |
| R10 | `RegisterBeginPlayPostHook` | **Delivered** | Next pump after the actor appears (PC: right after BeginPlay). |
| R11 | `RegisterEndPlayPreHook` | **Still refused** (counted gap) | Reason `endplay-no-data-only-route`: the object list sees an actor go only after it is freed, so there is no live actor to give the mod. The Blueprint `ReceiveEndPlay` swap would only cover some actors and would use up the 256 script-thunk slots the pump carriers depend on. |
| R12 | `RegisterEndPlayPostHook` | **Still refused** (counted gap) | Same. |

Order inside one map load is kept like PC: LoadMap pre -> InitGameState pre/post -> BeginPlay pre/post
for each new actor -> LoadMap post. All of them wait until the whole batch of new objects from the load
has been checked (a big load is checked over several frames, so post may come ~0.1-1 s after the load).

Known gaps (honest, logged in the stats line):

- Actors of a level **streamed in later** (not part of the map load) get no BeginPlay call
  (`streamed_unreported=` counter): their real BeginPlay waits until the level becomes visible,
  which this route cannot see. Actors spawned during play are reported.
- An actor created and destroyed between two pumps is missed. A dead object is never handed to Lua.
- Seamless travel (no LoadMap on PC) would still look like a map change here (new GameState).
- More than 65,536 waiting actor events: extra ones are dropped and counted (`dropped=`). Map events
  are never dropped.

## Fail closed

If the four engine classes cannot be found, or the object list never becomes usable within 60 s,
the route switches off: one `[WORLD-EV] route refused` line plus one counted `[REFUSED]` line, and
every later registration of these hooks is counted as a gap again.

The old one-shot LoadMap detour attempt is unchanged (it is refused on the console every launch);
its error line now says the callbacks go through the object-list route, and it no longer adds a
counted `[REFUSED]` line. If that detour ever did arm, the new route skips LoadMap events (no doubles).

## Self-test GAPS

The deployed self-test already counts a gap when the runtime's refused counter (`GetRefusedHookCount`)
goes up during a registration. With the route live, LoadMap x2, InitGameState x2 and BeginPlay x2 no
longer raise it; EndPlay x2 still do. Expected next run (Mac-only prediction): `GAPS 11` (was 17 in
launch 10), list without RegisterLoadMap*/InitGameState*/BeginPlay*.

## How the next launch proves it (log markers, exact text)

- `[WORLD-EV] registration kind=RegisterLoadMapPreHook route=object-index timing=...` (once per hook kind; not counted)
- `[WORLD-EV] route live: object index + engine classes Actor/GameModeBase/GameStateBase/World resolved (tries=N); ...`
- `[WORLD-EV] map change seen (object index): maps=1 gamemodes=1 actors=N -> LoadMap pre, InitGameState, BeginPlay, LoadMap post queued (PS5: all after the load)`
- `[WORLD-EV] first delivery kind=LoadMapPre callbacks=N object='...'` (also for InitGameState, BeginPlay, LoadMapPost)
- `[WORLD-EV] LoadMap post delivered: world='<map name>' callbacks=N`
- `[WORLD-EV] stats maps= gamemodes= actors= lua_calls= skipped_dead= streamed_unreported= dropped= errors= queued= paced_skips=` (every 10 s, only when something changed)
- Errors: `[WORLD-EV] callback error (#n, kind=..., mod=...): ...`, `[WORLD-EV] callback threw a non-standard exception (#n, kind=...)`
- Fail closed: `[WORLD-EV] route refused: reason=...` + `[REFUSED] kind=RegisterLoadMap/InitGameState/BeginPlayHooks target='object-index-world-events' ... effect=never-delivered`
- EndPlay: `[REFUSED] kind=RegisterEndPlayPreHook target='-' mod='...' reason=endplay-no-data-only-route effect=accepted-undelivered`

Spawner test (TownfallItemSpawner `main.lua:731-744`): after loading a save / changing map, the menu
opened with F7 should show a fresh list ("Load complete" status), and the log should show the
`map change seen` + `first delivery kind=LoadMapPre ... callbacks=1` lines at that moment.

## Risks

- **Pump cost.** While any of these hooks are registered, every new object's class chain is walked once
  (same work NotifyOnNewObject already does), and each new actor costs one Lua call per registered
  BeginPlay callback. The self-test registers BeginPlay pre+post and never removes them, so after it
  runs, every spawned actor costs 2 Lua calls. Capped at ~1 ms per ~16 ms window; the rest waits.
- **Lua calls under the actions lock**, same as the existing NotifyOnNewObject delivery (so a mod
  uninstall cannot free a Lua state mid-call). The registrations now take the same lock for the
  one append (kept to one statement so no Lua error can jump over it).
- **"Pre" hooks run late.** A mod that, in a LoadMap pre hook, touches objects of the old map will find
  them gone. The spawner only resets its own Lua tables (and closes its menu if open).
- **Memory.** Event lists use the normal heap (16 bytes per event, capped at ~1 MB, freed after use).
- Not console-proven: the GameState-per-load signal, the RF_WasLoaded rule for map-loaded actors, and
  the cost per batch all need the next launch's lines.

## Files

- `apply_hooks.py` - applies the change (18 exact-once edits, all-or-nothing) to `src/LuaMod.cpp`
  of the root given as argv[1] (default: the real candidate). Self-contained.
- `world_ev_block.inc` - the new code block, same text as embedded in the script (for review).
- `delta.diff` - `diff -ru base-tree tree`.
- `tree/` - base-tree with the change; `check-root/` - src-only copy used for the syntax check;
  `syntax/` - syntax, field-gate and no-detour gate output.
- New includes: none. New imported symbols: none expected (only calls this unit already makes:
  StaticFindObject, steady_clock, snprintf, Lua C API, Output).
