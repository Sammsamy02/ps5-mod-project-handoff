# VISIBILITY-TEST.md — rung 5 acceptance: "visible spawn through real input"

worker-real-input, 2026-10-04. Acceptance design for ladder rung 5
(`zcode_acceptance_ladder`: genuine game-thread callback → valid object query →
harmless reflected access → unchanged Item Spawner activation → **visible spawn
through real input**). Nothing here has run on console yet.

## 1. What counts as PASS (all must hold in one continuous capture)

1. **Real input origin**: the press comes from the physical DualSense through
   `scePadReadState` via the retained pad provider chain — no injected/synthetic
   events, no keyboard emulation (the earlier `controller_input_conversion` keyboard
   route is explicitly NOT evidence: its Townfall D-pad/keyboard test read
   ambiguously and produced no gameplay change).
2. **Loaded 3D scene**: the game is in a loaded save/world (post `ItemDatabaseSubsystem`
   availability; the mod's own error path says "Load a save first"), camera positioned
   so the spawn point (player-facing trace hit) is unobstructed and in view.
3. **Visible spawned actor**: after the mapped pad press (CROSS = RETURN on a selected
   menu row), a NEW pickup actor appears in the captured frame at the trace location —
   an actor that did not exist in the immediately-prior frame of the same fixed camera.
4. **Input-correlated timing**: the capture or log shows the causal chain within one
   observable window: pad press (user-visible) → keybind callback (`log` line /
   status line change) → `ExecuteInGameThread` delivery (no refusal) → spawn → actor
   visible. Correlation = timestamps + frame ordering, not just "eventually appeared".
5. **Repeatability and menu-path proof**: N ≥ 5 presses → N distinct spawns (debounce:
   one press = one spawn, no double-spawn per press); at least one UP/DOWN navigation
   before a CROSS that spawns a DIFFERENT item than the first spawn (proves
   UP_ARROW/DOWN_ARROW/RETURN all reach Lua through real input); OPTIONS toggles the
   menu open/closed (F7 path).
6. **Negative controls (same session)**:
   - CROSS with the menu CLOSED → no spawn (Lua-side `menuOpen` gating intact).
   - Pad disconnected/turned off mid-session → no phantom events (disconnect clears
     edge state; reconnect needs a fresh press).
   - Before the first genuine `UGameEngine::Tick` identity publication (or if
     delivery is refused) → callback may fire but NO mod action runs; acceptance
     requires the delivered case, and the refusal log line proves the gate is real.

## 2. What does NOT count (explicit exclusions)

- Title screen / main menu / any non-gameplay world state.
- A log line alone (callback fired, `ExecuteInGameThread` delivered) without the actor
  in a captured in-game frame.
- Host fixtures: Mac host tests (including `vendored/build_and_test.sh`), synthetic
  `PadBackend` sequences, the rung-5 mini-chain in `vendored/host_test.cpp`.
- Menu-only visibility (UMG overlay appears but no spawn) — that is the D-family
  milestone, not rung 5.
- Spawns from console-injected commands, scripted input replay, pre-existing actors,
  or actors spawned "on a timer" without a correlated press.
- A single lucky frame: no repeat trials, no negative controls.

## 3. Exact controller console procedure (operator checklist)

Preconditions: rungs 1–4 accepted (game-thread callback, object query, reflected
access, unchanged Item Spawner activation); worker-3 EngineTick delivery live;
`ue4ss/PS5Pad.ini` staged with the INPUT-MAP §1 profile; Item Spawner staged
unchanged (sha256 f586…658 of main.lua verified at activation).

1. Boot title to a loaded save with the character in an open area; stand still;
   position camera so the ground in front of the player fills the view.
2. Start console-side capture (video or burst frames) covering screen + timestamps;
   confirm UE4SS log tail is being collected on the console side.
3. Press OPTIONS once → menu overlay appears (F7 path). Record frame.
4. Press UP, then DOWN (selection visibly changes). Record.
5. Press CROSS on a selected row → within the same second, the new pickup appears in
   front of the player. Record frame + log lines.
6. Navigate to a different row; CROSS again → second, different pickup. Repeat to
   N ≥ 5 total spawns, mixing rows.
7. Close the menu (OPTIONS). Press CROSS → confirm NO spawn (negative control).
8. Power off the pad, press nothing for 10 s, power on, press CROSS with menu closed →
   no spawn; reopen menu, CROSS → spawns again.
9. Collect: capture file, log excerpt with per-press keybind/delivery/spawn lines,
   count table (press # → item id → frame timestamp).

PASS criterion: §1 items 1–6 all evidenced from step 9 artifacts; any single failure
item is a documented rung-5 gap, not a partial pass.

## 4. Evidence boundaries (honesty)

- Everything in `vendored/` is host-only; its ALL PASS receipt is NOT rung-5 evidence.
- Rung 5 additionally depends on: worker-3 game-thread delivery being live on console
  (A9), live PlayerController/Pawn (C5), trace + spawn callables (C1/C2), and the UMG
  menu family (D1–D5) for the menu-path steps; without D-family only the menu-less
  subset (auto-spawn on a direct keybind) could be tested if the mod had one — it
  does not, so rung 5 waits on those rungs' acceptance.
- The pad provider itself has `hardware_runtime_validated: false`
  (`native_ps5_pad_provider` checkpoint): first real-pad read happens at this rung's
  first console attempt; ScePad open/read failures are expected findings, not fakes.
