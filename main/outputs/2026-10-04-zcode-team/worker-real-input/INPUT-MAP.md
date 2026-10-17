# INPUT-MAP.md — rung 5 bridge: real pad input → Item Spawner keybinds

worker-real-input, 2026-10-04. Design only; nothing here ran on console.
Follows the actual retained sources over any brief wording (sources cited inline).

## 0. What already exists and is retained unchanged

| Piece | Where | Fact |
|---|---|---|
| Pad provider | `.superpowers/native-dependency-spike/ue4ss-native-pad-provider-1/port/PS5PadSource.{hpp,cpp}` | `PS5PadSource(std::vector<PadBinding>, PadBackend, int user)`; `poll(subscriptions, allow)` → rising-edge `InputEvent{key,modifiers}`; compiled, `native_ps5_pad_provider` checkpoint: 32 owned host controls, native consumer compiled, **no hardware run yet** |
| Shared config | same spike `ue4ss-native-pad-config-1/src/PS5PadConfig.{hpp,cpp}` | `parse_ps5_pad_config` — `ue4ss/PS5Pad.ini`, sections `[PS5Pad]` (User) and `[Bindings]` (`CHORD = MODIFIERS+KEY`), UE4SS key names only, refuses chord containment/duplicates; "One caller-owned shared chord/key/modifier vector; **no per-mod matching or filesystem configuration parser**" |
| Backend wiring | `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/ue4ss_backend.cpp:21-22` | `ConfigureRetainedPS5PadInput(*context.mod_source, context.input_source)` — activated-mod runtime opens the provider from retained `ue4ss/PS5Pad.ini`; missing ini preserves any supplied source |
| Keybind consumer | coherent port `.../ue4ss/src/Mod/LuaMod.cpp:1915+` (`RegisterKeyBind`) | Lua `RegisterKeyBind(Key.F7, cb)` → `mod->m_program.register_keydown_event(...)` → `Input::Handler` key set |
| Dispatch | coherent port `.../first/src/Input/Handler.cpp:66+` (`Handler::process_event`) | program tick (`UE4SSProgram.cpp:1660`) pulls `m_platform_handler->process_event(this)`, matches `required_modifier_keys == event.modifier_keys`, snapshots callbacks, invokes them on the processing thread |
| Item Spawner intake | `.superpowers/native-observations/item-spawner-real-mod-intake-1/member-00000000.bin` (sha256 f586…658) main.lua:694–729 | 6 binds, every callback wrapped in `ExecuteInGameThread(...)`: F7, UP_ARROW, DOWN_ARROW, RETURN, LEFT_ARROW, RIGHT_ARROW |
| Game-thread refusal | `.superpowers/native-dependency-spike/ue4ss-native-game-thread-1/include/Unreal/Platform/NativeGameThreadIdentity.hpp` + worker-3 INTEGRATION-MAP §S3 | action dispatch refuses unknown / other-thread identity (`in_current_raw()` gate; identity published only by genuine `UGameEngine::Tick`) |

## 1. Mapping table (proposed default profile)

One shared, profile-driven `ue4ss/PS5Pad.ini`; the mod itself stays unchanged and
recipe-free — it only registers its 6 logical keys, the profile decides which pad
chords produce those keys.

```ini
[PS5Pad]
User = -1                 ; first logged-in user

[Bindings]
; pad chord (chords allowed)      ; logical UE4SS key (exact KeyDef names)
OPTIONS      = F7                  ; toggle Item Spawner menu (main.lua:694)
UP           = UP_ARROW            ; selection previous (main.lua:698)
DOWN         = DOWN_ARROW          ; selection next     (main.lua:703)
CROSS        = RETURN              ; activate selection / spawn (main.lua:708)
LEFT         = LEFT_ARROW          ; page previous       (main.lua:713)
RIGHT        = RIGHT_ARROW         ; page next           (main.lua:722)
```

Rationale:

- `OPTIONS` for F7: the pad's own "menu" button; game-routing of OPTIONS during the
  spawn check is acceptably rare, and it is a single non-chord press like F7 on PC.
- D-pad direct 1:1 for arrows — the mod's own menu navigation semantics.
- `CROSS` for RETURN: the pad's primary affirm button matches RETURN's role in the
  menu (`activateSelection`), and spawns happen on press-edge, matching menu Enter.
- Alternatives are user config, not code: e.g. `TOUCH_PAD = F7` or `L1+CROSS = RETURN`
  are legal profiles (chord syntax; the parser rejects chords containing other
  configured chords, `PS5PadConfig.cpp:105-108`). No per-mod recipe can exist: the
  provider receives "one caller-owned shared chord/key/modifier vector".

Known, accepted limitation (documented, not hidden): UE4SS keybinds are
non-exclusive notifications. While the menu is open, D-pad/CROSS still reach the
game's own input. The mod already compensates on the game side
(`SetIgnoreLookInput/SetIgnoreMoveInput`, main.lua via C4 in RUNTIME-REQUIREMENTS).
No suppression layer is added in the bridge.

## 2. Where edge detection lives (three layers, all retained or vendored-here)

1. **Retained provider rising edge** — `PS5PadSource::poll` (PS5PadSource.cpp:69-93):
   emits only on `down && !m_down[i]` (rising edge); swallows the first sample after
   (re)attach (`baseline`); skips duplicate cached reads (`data.timestamp == m_timestamp`);
   tracks held state even while unsubscribed/disabled so no phantom edges appear on
   re-enable; disconnect clears state without emitting.
2. **Vendored hardening (this worker, `vendored/pad_edge_adapter.{hpp,cpp}`)** — same
   `PadBackend` + `PadBinding` API, adds what the provider does not guarantee for
   rung-5 "one press = one spawn":
   - **re-arm debounce**: a logical key re-fires only after a configurable pad-time
     delta (`min_rearm_ms`, default 40 ms) since its last emitted edge; contact
     bounce (down/up/down inside the window) collapses to one event.
   - **explicit-release requirement**: an edge suppressed by debounce is dropped, never
     queued — delivery stays at-most-once per real press.
   - **timestamp regression reset**: a backwards pad timestamp re-baselines (next
     sample never emits), instead of being treated as a new edge source.
   - Parity contract: with `min_rearm_ms = 0` the adapter emits exactly the retained
     provider's events (host test asserts subset-equality on non-bounce sequences and
     strict-fewer on bounce sequences).
3. **Subscription/allow gating** stays where it is: the Handler passes
   `get_subscribed_keys()` + `get_allow_input()` into the provider each poll
   (Handler.cpp:75 → PS5PadSource.cpp:65-67). The adapter keeps that signature.

## 3. Path from pad edge to the Lua callback (dispatch discipline)

```
ScePadReadState (genuine pad, game process)
  → PS5PadSource::poll            [retained; rising edge; subscriptions/allow gate]
  → PadEdgeAdapter::poll          [vendored; debounce/no-repeat/regression hardening]
  → Handler::process_event        [retained; key+modifier match; callback snapshot]
  → Lua keybind callback          [RegisterKeyBind ref, LuaMod.cpp:1940-1961]
        mod calls ExecuteInGameThread(fn)          (main.lua:695,700,705,710,716,725)
  → EngineTick delivery gate      [worker-3 seam; native_game_thread_identity]
        refuses unknown identity  → action NOT run (logged refusal)
        refuses other thread      → action NOT run (logged refusal)
  → mod action on the real game thread (menu UMG / trace / spawn)
```

- Keybind callbacks still fire on the program's processing thread exactly as
  upstream UE4SS does (Handler.cpp:96-99 snapshot-then-invoke; registration inside a
  callback stays safe). The bridge never moves that seam.
- The **game-thread dispatch discipline is preserved unchanged**: nothing in this
  bridge executes mod actions; the mod's own `ExecuteInGameThread` wrapper remains
  the only route into the game thread, and the gate keeps refusing while the
  identity is unknown (before the first genuine `UGameEngine::Tick`) or when the
  caller is any other thread. The vendored `GameThreadGate` mirrors
  `NativeThreadDetail::Identity`'s contract for host tests of that refusal.
- Ordering: input polling (UE4SSProgram.cpp:1660) precedes mod update processing in
  the same tick, so a pad-edge callback observed at tick N is delivered no earlier
  than the next game-thread pump after the gate sees a published identity.

## 4. Config surface the user sees

- One file: `ue4ss/PS5Pad.ini` staged by the same activation bundle that stages
  `ue4ss/Mods/TownfallItemSpawner` (worker-activation-bundle layout); parsed by the
  retained `parse_ps5_pad_config` under `PS5PadConfigLimit` (16 KiB), ASCII, line
  numbers in every error (`PS5Pad.ini line N: …`), unknown button/key/chord-duplicate
  are hard refusals, provider assignment happens only after the provider reports
  available (`ConfigureRetainedPS5PadInput` contract, NativePadInput.hpp:8-11).
- Absent ini ⇒ no pad bridge (a supplied input source is preserved untouched).
- The mod-facing surface stays exactly `RegisterKeyBind(Key.<K>, cb)`: profiles are
  per-install, not per-mod, and no mod name ever appears in the input path.

## 5. What is NOT in this design

- No per-mod recipes, no keybind rewrites of the mod, no engine input-asset edits
  (the earlier `controller_input_conversion` ExportBundleData D-pad remap route is a
  separate, retired experiment — its Townfall keyboard ambiguity is why this bridge
  reads the pad directly instead of relying on engine keyboard recognition).
- No synthetic/injected events on console; host tests only inject `PadBackend`
  samples, which is the provider's explicit owned-test surface
  (PS5PadSource.hpp:14-15).
- No game-thread delivery implementation — that is worker-3's seam; this rung's
  console acceptance additionally needs it (see REPORT gaps).
