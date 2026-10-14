# HOOKS track (round 10, r2 after review): map-load, game-state and actor-start hooks on PS5

Everything in this note is **Mac-only** (code reading, syntax check, gates). Nothing ran on the console.
r1 files are kept in `r1-archive/`.

## What it does, in plain words

Some mods ask "tell me when a new map has loaded" (LoadMap), "when the game rules object is set up"
(InitGameState), "when an actor appears in the world" (BeginPlay) or "when it leaves" (EndPlay).
On PC the mod loader patches game code to catch these moments. On PS5 game code cannot be patched,
so until now the runtime said "OK" to these requests and then never called the mod back.

The runtime already keeps a list of every game object and notices new ones (this is how
`NotifyOnNewObject` works today: `[R9-NOTIFY]`). This change reuses that list:

- A new **GameStateBase** object = a map has finished loading (the engine makes one per map load).
- A new **GameModeBase** object = InitGameState has run.
- A new **Actor** = that actor has started (BeginPlay).

No game code is touched and there are no new offsets. The four engine classes are found once by their
standard names, then checked to really be classes (their own class is called "Class"; GameMode and
GameState come from Actor; World does not). The only new flag (`RF_WasLoaded = 0x80000`) has two
sources (UE4SS `UnrealFlags.hpp` and the local Epic `ObjectMacros.h`).

## r2: how events are tied to a map (the reviewer's main finding)

The object list does not report one map load in one piece. When a map is loaded again, the new
objects reuse freed slots, and those are found a little at a time. So r2 no longer ties events
to a map by "same batch". It uses the **level** each actor sits in:

1. When a new GameState appears, its level is the new map's main level. **LoadMap pre** is queued
   right away, and a "load window" opens for that level.
2. GameModes and actors from a level we do not know yet wait in a **hold list** for up to 5 s.
   If a GameState for their level shows up in time, they join that map's load window.
   If not, GameModes and spawned actors are delivered after the wait. Actors that were loaded
   from disk are counted as `streamed_unreported` and not delivered: they belong to a streamed
   level / World Partition cell.
3. The load window collects that level's GameModes and actors. It closes after 3 s with no new
   loaded actor or GameMode for that level. It also closes after 30 s in any case, or when the
   next map starts. Then it queues **InitGameState**, then **BeginPlay** for each actor, then
   **LoadMap post**. That is the PC order.
4. After the window has closed, actors spawned in the current map are delivered at least 50 ms after they are
   seen (r3: a nested pump inside the engine's spawn must never hand a half-built actor to a mod; 50 ms is a
   heuristic, not a guarantee, since a very long spawn could run past it). A GameMode seen with no open window
   starts a new load (catches a same-map reload); still-settling spawns of the old map are queued before the
   new map's LoadMap pre.
   A loaded actor that turns up late waits in the hold list, then is delivered and counted
   `late_after_post`.

Callbacks run on the game thread, inside the pump that already runs `ExecuteInGameThread` and
`NotifyOnNewObject` work. They use at most ~1 ms per frame. Each callback is run on its own, so
one mod's Lua error never stops the others.

## Per hook (R05-R12)

| ID | Lua call | PS5 now | How it differs from PC |
|---|---|---|---|
| R05 | `RegisterLoadMapPreHook` | **Delivered** | Runs **after** the load (PC: before), at the first pump after the GameState is seen. `World` invalid (old world gone), `Engine` invalid, `URL` nil, `Error` "". Return value ignored. |
| R06 | `RegisterLoadMapPostHook` | **Delivered** | Runs when the load window closes (at least 3 s after the last load object). Comes after that map's InitGameState and BeginPlay calls, like PC. `World` = the new world. `Engine` invalid, `URL` nil. |
| R07 | `RegisterInitGameStatePreHook` | **Delivered** | Runs **after** InitGameState (PC: before), when the load window closes. Argument = the GameMode. |
| R08 | `RegisterInitGameStatePostHook` | **Delivered** | Same moment as R07, right after it. |
| R09 | `RegisterBeginPlayPreHook` | **Partial, still counted as a gap** | Runs **after** BeginPlay (PC: before). Actors of streamed levels are not reported. |
| R10 | `RegisterBeginPlayPostHook` | **Partial, still counted as a gap** | Same as R09. |
| R11 | `RegisterEndPlayPreHook` | **Refused** (counted gap) | `endplay-no-data-only-route`: the object list sees an actor go only after it is freed. |
| R12 | `RegisterEndPlayPostHook` | **Refused** (counted gap) | Same. |

BeginPlay is still counted as a gap, as the review asked. Its `[REFUSED]` line says
`reason=beginplay-partial-streamed-unreported effect=partial-delivered`, and the callbacks do run.
It stays counted until a launch shows how much is covered (see the checklist).

## Other review fixes in r2

- **Bounded memory:** the delivery queue drops its finished part as it goes. Everything waiting is
  capped at 32,768 events (about 1.5 MB at most). If memory runs out, the event is counted as
  `dropped`; the pump never throws.
- **Startup lines corrected:** the `detour-block` [REFUSED] line no longer lists InitGameState and
  BeginPlay. It is still one counted line, so the count does not change. The `[R3] LoadMap detour
  registrations deferred` warning now says these hooks go through the `[WORLD-EV]` route.
- **Class check at delivery:** each event remembers the actor's class. If the slot now holds an
  object of another class, it is skipped (`skipped_dead`).
- **Fail closed:** if a class is missing or fails the check, or the object list is not usable after
  60 s, there is one `[WORLD-EV] route refused` line, plus one counted `[REFUSED]` line for each
  hook kind that has registrations (with the number of registrations). Note: the self-test counts
  gaps at registration time, so a self-test that ran before such a refusal does not see it. The
  log does.
- LoadMap registrations are no longer labelled `queued-pending`.

## Self-test GAPS (Mac-only prediction)

The self-test counts a gap when the runtime's refused counter goes up during a registration.
LoadMap ×2 and InitGameState ×2 no longer raise it. BeginPlay ×2 (partial) and EndPlay ×2 still do.
Expected next run: **GAPS 13** (was 17 in launch 10).

## Launch checklist (how the next launch proves it)

1. `[WORLD-EV] route live: ... resolved and class-checked (tries=N)` appears once.
2. On a map load or save load: `[WORLD-EV] map change seen (object index): world='<map>' held_joined=N`.
   A few seconds later: `[WORLD-EV] map load window closed after N ms (reason=quiet): gamemodes=1 actors=N ...`,
   then `[WORLD-EV] LoadMap post delivered: world='<map>' callbacks=N`.
   Expect exactly **one** map change per load. Do a second load (reload a save) as well: that is the case the review flagged.
3. `[WORLD-EV] first delivery kind=LoadMapPre callbacks=N ...` (also InitGameState, BeginPlay, LoadMapPost).
4. **BeginPlay coverage:** in the `[WORLD-EV] stats` lines, compare `streamed_unreported` with `actors`.
   If `streamed_unreported` is large next to `actors`, BeginPlay stays a gap. `late_after_post` should be small.
   `dropped`, `errors` and `held` should be 0 or small.
5. Spawner (TownfallItemSpawner `main.lua:731-744`): after loading a save, the F7 menu shows a fresh list.

## Log markers (exact text)

- `[WORLD-EV] registration kind=%s route=object-index timing=%s` (once per hook kind; not counted)
- `[WORLD-EV] route live: object index + engine classes Actor/GameModeBase/GameStateBase/World resolved and class-checked (tries={}); LoadMap/InitGameState/BeginPlay Lua hooks deliver after the event on the game-thread pump`
- `[WORLD-EV] map change seen (object index): world='{}' held_joined={} -> LoadMap pre queued; this map's InitGameState/BeginPlay/LoadMap post follow when its load window closes (PS5: all after the load)`
- `[WORLD-EV] map load window closed after {} ms (reason={}): gamemodes={} actors={} held_joined={} -> InitGameState, BeginPlay, LoadMap post queued` (reason = quiet / max-30s / next-map)
- `[WORLD-EV] first delivery kind={} callbacks={} object='{}'`
- `[WORLD-EV] LoadMap post delivered: world='{}' callbacks={}`
- `[WORLD-EV] stats maps={} gamemodes={} actors={} lua_calls={} skipped_dead={} streamed_unreported={} late_after_post={} held={} window_open={} dropped={} errors={} queued={} paced_skips={}` (every 10 s, only when something changed)
- `[WORLD-EV] callback error (#{}, kind={}, mod={}): {}` / `[WORLD-EV] callback threw a non-standard exception (#{}, kind={})`
- `[WORLD-EV] route refused: reason={} (...)`, plus for each registered kind: `[REFUSED] kind=Register<...>Hook target='object-index-world-events registrations=N' mod='*' reason=<engine-class-lookup-failed|engine-class-check-failed|object-index-unusable-60s> effect=accepted-undelivered`
- `[REFUSED] kind=RegisterBeginPlayPreHook|PostHook target='-' mod='<mod>' reason=beginplay-partial-streamed-unreported effect=partial-delivered`
- `[REFUSED] kind=RegisterEndPlayPreHook|PostHook target='-' mod='<mod>' reason=endplay-no-data-only-route effect=accepted-undelivered`
- Changed text: `[REFUSED] kind=detour-block target='EndPlay,StaticConstructObject(NotifyOnNewObject),...'`, the `[R3] LoadMap detour registrations deferred: ...` warning, and `[R3] LoadMap deferred arm refused ...`.

## Risks

- **Not console-proven.** Still unproven: one GameState per load, the level rule, the 3 s / 5 s windows,
  and the cost per load.
- **"Pre" hooks run late,** and InitGameState / BeginPlay for a map load are held until its window closes,
  at least 3 s after the load. A mod that touches old-map objects in LoadMap pre will find them gone.
  The spawner only resets its own tables.
- **Wrong order, rare case:** if a new map reuses the old main level's memory address, a few actors
  spawned before the new GameState is seen can be delivered before LoadMap pre.
- **Seen as a map change by mistake:** a GameState created without a map load (seamless travel, or a
  game that respawns its GameState).
- **Delay:** actors spawned into a level other than the current map's wait up to 5 s.
- **Missed:** actors created and destroyed between two pumps.
- **Pump cost:** the self-test registers BeginPlay pre+post and never removes them. After it runs, every
  new actor costs 2 Lua calls, capped at about 1 ms per frame.
- **Lua runs under the actions lock,** the same rule as the existing NotifyOnNewObject delivery.
- The `fs_throw_gate.py` FAIL is not from this change. The unchanged base gives the same 35 violations
  in LuaMod.cpp, UE4SSProgram.cpp and PosixFile.cpp.

## Files

- `apply_hooks.py`: 20 exact-once edits, all-or-nothing, self-contained, touches `src/LuaMod.cpp` only.
- `world_ev_block.inc`: the new code block, same text as embedded in the script.
- `delta.diff`: `diff -ru base-tree tree`.
- `tree/`, `check-root/` (src-only copy used for the syntax check), `syntax/` (check and gate output).
- `r1-archive/`: the blocked r1 script, diff and design note.
- New includes: none. Only calls this unit already makes.

# r3 (Claude) review: SHIP (3 low + doc). Applied: comment fix, settle flush before new LoadMap pre, DESIGN.md. Not applied: per-frame counter for the settle rule (the engine-tick path it would count does not run on PS5; noted as heuristic).
