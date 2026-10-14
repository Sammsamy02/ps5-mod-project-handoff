# REPORT — worker-pad-chord-audit

Verdict: **the OPTIONS→F7 dispatch path is fully wired and edge-triggered in the composition-5
candidate lineage, and the press the user must make is a single OPTIONS tap at a loaded save** —
provided the PRX actually installed on the console is one of
`worker-composition-5-rebuild/prx-candidate-composition5-1|-2` (both verified to contain the pad
bridge). Full trace with file:line citations: `PAD-DISPATCH-PATH.md`. Claims: `CLAIM.json`.

## 1. Direct answers

- **Edge or level?** Edge. One `InputEvent` per press transition — `down && !m_down[i]`
  (`PS5PadSource.cpp:85`), duplicate-sample suppression by `timestamp` (`:81`), edge state tracked
  even while gated (`:90`). No autorepeat, no keyup synthesis. Holding OPTIONS fires F7 exactly once.
- **Chord semantics?** OPTIONS ALONE. `OPTIONS = F7` (`PS5Pad.ini:35`), AND-match
  `(data.buttons & b.buttons) == b.buttons` (`PS5PadSource.cpp:84`) — extra unrelated buttons do not
  suppress it. Not a modifier chord; no second button required. Overlapping chords are refused at
  parse time (`PS5PadConfig.cpp:115-119`).
- **What must the user press?** At a loaded save: press and release OPTIONS once. Menu opens;
  press OPTIONS again to close (toggle, `main.lua:670-672,694-696`). Navigate with
  UP/DOWN (selection), CROSS (activate), LEFT/RIGHT (pages) — mappings verified against the six
  registrations at `main.lua:694-729` (`PS5Pad.ini:36-40`). ESC is not bound and not needed;
  a map load force-closes the menu (`main.lua:731-744`).
- **Which thread polls?** The UE4SS-UpdateThread (`UE4SSProgram::update`, jthread at
  `UE4SSProgram.cpp:673`; poll at `:1659-1661`, 5 ms cadence `:1682`). The Lua keybind callback also
  runs on that thread (`LuaMod.cpp:1937-1948` called from `Handler::process_event`,
  `Handler.cpp:99-102`); only the menu mutation is deferred to the game thread through
  `ExecuteInGameThread` → EngineTick queue (`LuaMod.cpp:4690-4768`, drained by `engine_tick_hook`
  `:4472-4478`, real detour registered `UnrealInitializer.cpp:1321`).

## 2. Failure-mode table — silent "OPTIONS pressed, no popup"

Distinguishers assume two artifacts on the console: `UE4SS.log` (file device,
`UE4SSProgram.cpp:256-257`, under the runtime root `:780-782`) and the runtime ack status file
(`runtime-status.txt` written by `RuntimeAckSink`, ack points in
`worker-composition-3/bootstrap-stage/.../ue4ss_backend.cpp:28-45,96-110`).

| # | failure | precondition that hides it | thread that must be alive | last observable that still happens | how to distinguish from logs |
|---|---|---|---|---|---|
| A | Wrong artifact: pad bridge never linked (old composition-4 object or the minimal comp5 entry-only PRX) | startup does NOT throw — nothing ever calls `ConfigureRetainedPS5PadInput` (`composition-4/runtime.map` has 0 hits for it) | UpdateThread runs normally | `runtime-status.txt` activated/mods_started; `UE4SS.log` "Event loop start" (`UE4SSProgram.cpp:1581`); `[TownfallItemSpawner] Loaded v1.0…` (`main.lua:746`); all six binds registered; OPTIONS does nothing forever | Everything looks healthy yet zero response. Verify installed PRX sha256 against `prx-candidate-composition5-2/build-receipt.json`; `nm` the shipped object for `ConfigureRetainedPS5PadInput` (present in candidates 1/2, absent from composition-4 map and from `worker-composition-5-minimal` combined object) |
| B | Provider fails to activate | none — CANNOT be silent | n/a | runtime never reaches "activated": `NativePadInput.cpp:19-24` throws → backend ctor throws → `ack->failed(1, "PS5Pad input unavailable: phase=…error=…")` | `runtime-status.txt` shows failed line instead of activated/mods_started; module start error |
| C | Polling never runs: UpdateThread dead after mods started | PROSPERO catch in the loop logs and requests shutdown (`UE4SSProgram.cpp:1689-1698`) | the UpdateThread itself | "Event loop start" logged (`:1581`), mod lines present (mods start ON that thread, `:1484-1489`), then nothing ever reacts to any input | `UE4SS.log` shows "Event loop end" (`:1686`) or a loop exception text; if genuinely hung (not exited), neither marker appears and B/D lines are absent |
| D | Key synthesized but bind table empty (mods not started / main.lua failed early) | `LuaMod::start_mod` catches and only logs (`LuaMod.cpp:6485-6488`); `m_is_started` was already true (`:6453`) | UpdateThread (for registration) | provider works (B passed), poll runs; no callbacks match | `UE4SS.log`: `Exception during mod startup: …` (`LuaMod.cpp:6486-6488`), or `Failed to execute main script` (`:6470-6472`), or `Input registration unavailable: no active input source` (`Handler.cpp:109`); the `[TownfallItemSpawner] Loaded v1.0…` line (`main.lua:746`) is ABSENT — its presence proves all six binds registered |
| E | ExecuteInGameThread queue starved (tick detour dead) — the truly silent one | callback itself SUCCEEDS on the UpdateThread and queues the action (`LuaMod.cpp:4764`); registration would have thrown if the EngineTick scan had failed (`:4716-4721`), so its absence is expected | game thread executing the EngineTick hook (`UnrealInitializer.cpp:1321`) | `Loaded v1.0` line present; each OPTIONS press runs Lua, queues `toggleMenu`, and produces no log at any layer | No error lines, no per-press log, menu never appears. Distinguish from C by tick-dependence: EVERYTHING game-thread-queued is dead (delays, other mods' game-thread work), while pure-UpdateThread behavior is fine. Instrumentation (a Lua `log()` in the F7 callback) is the cheap separator — it would print on the UpdateThread even when the menu can't appear |
| F | Lua callback errors at press time | caught inside the keybind lambda (`LuaMod.cpp:1944-1947`) | UpdateThread | press → error logged each time | timestamped `[ERROR]` lines from `lua.handle_error` in `UE4SS.log` at press moments (file device, `UE4SSProgram.cpp:256-257`) |

Ordering note for triage: B excludes everything else (runtime never starts). With a healthy start:
check the `Loaded v1.0` line first (absent ⇒ D), then "Event loop end"/loop exceptions (C), then
per-press `[ERROR]` lines (F), then tick-starvation instrumentation (E), and only then the artifact
identity (A).

## 3. Requested cross-reference and caveats (stated, not smoothed over)

- **worker-tick-callback-audit RUNG1-EVIDENCE is NOT available in this worktree.** Its output
  directory `outputs/2026-10-04-zcode-team/worker-tick-callback-audit/` exists but is empty
  (`total 0`), and the string `RUNG1-EVIDENCE` occurs nowhere in the tree. Row E's tick-gate facts
  are therefore established from source only: `RegisterEngineTickPreCallback(engine_tick_hook)`
  (`LuaMod.cpp:4524-4528`), real detour `HookedEngineTick` (`UnrealInitializer.cpp:1321`,
  `bHookEngineTick` `:1252`), and the dispatch-ownership gate (`NativeLuaActionDispatchScope`,
  `LuaMod.cpp:4085-4108`).
- **On-console artifact identity cannot be verified from this worktree.** Three different "comp5"
  artifacts exist: full candidates `prx-candidate-composition5-1` and `-2` (both verified to define
  and CALL `ConfigureRetainedPS5PadInput`; candidate-2 built 2026-10-04T19:11:51Z per
  `build-receipt.json`), and `worker-composition-5-minimal/composition-5/run-1` — an entry-only
  module whose own receipt calls it a "deliberate minimal-build delta vs the full composition-3
  runtime". If the minimal PRX or any composition-4-lineage object is what boots, failure mode A
  applies and no press will ever work.
- The compiled `NativePadInput.cpp`/`PS5PadConfig.cpp`/backend-revision provenance differs across
  staged directories; the versions cited here are the ones proven (`.d` files, `diff`, symbol and
  relocation inspection) to be in the candidate lineage — see `PAD-DISPATCH-PATH.md` section 0.
- The `m_allow_input` gate is permanently open (default true, `Handler.hpp:50`; only setter call is
  commented out, `UE4SSProgram.cpp:1639-1658`) — it cannot be the cause of a no-popup.
- Nav keys are silent no-ops while the menu is closed BY DESIGN (`main.lua:699,704,709,714,723`) —
  do not mistake that for a dispatch failure; only F7 gives a pre-menu signal.

## 4. Deliverables

| file | order |
|---|---|
| `CLAIM.json` | first |
| `PAD-DISPATCH-PATH.md` | second |
| `REPORT.md` | last (this file) |

Tool budget: 41 tool calls, all reads plus these three writes; no writes outside
`outputs/2026-10-04-zcode-team/worker-pad-chord-audit/`; no console contact; no nested subagents.
