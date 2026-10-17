# PAD-DISPATCH-PATH — OPTIONS(DualSense) → F7 → Lua RegisterKeyBind

All paths relative to worktree root `/WORKSPACE/.codex/worktrees/translation-core/what`.
`SRC` = `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port`.

## 0. Physical artifacts (what is actually linked)

| hop | source file | linked object |
|---|---|---|
| pad provider (DualSense SDK calls) | `.superpowers/native-dependency-spike/ue4ss-native-pad-provider-1/port/PS5PadSource.cpp` / `PS5PadNative.cpp` | `coherent-full-1/native-pad-PS5PadSource.o`, `native-pad-PS5PadNative.o` (provenance in `coherent-full-1/native-pad-*.d`, `*.command.json`) |
| ini config bridge | `.superpowers/native-dependency-spike/native-coherent-pad-runtime-1/build-1/source/PS5PadConfig.cpp` + `NativePadInput.cpp` (identical to `ue4ss-native-pad-config-1/src/`, `diff` clean) | inside `worker-composition-5-rebuild/prx-candidate-composition5-2/build/combined-supplier.o` (symbols verified) |
| backend calling the bridge | `outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-pad-runtime-1/build-1/source/ue4ss_backend.cpp` (ack-enabled revision, line 26) | `combined-supplier.o` section `.text.…UE4SSBackendC2…RuntimeAckSink` contains the call reloc |
| UE4SS runtime | `SRC/ue4ss/src/...` | composition objects from `native-coherent-runtime-1` (e.g. `coherent-full-1/input-keydef.o` ← `SRC/first/src/Input/KeyDef.cpp` per its `.d`) |

Provenance warning (honest): `.superpowers/.../composition-4/runtime.map` does **not** contain
`parse_ps5_pad_config` / `ConfigureRetainedPS5PadInput` (grep = 0 hits) — that older composed
object has no pad bridge. The composition-5 candidates DO (verified on the built objects):

```
nm -C --defined-only .../prx-candidate-composition5-2/build/combined-supplier.o
  → RC::Input::parse_ps5_pad_config(...)            (T)
  → RC::Input::make_native_ps5_pad_source(...)      (T)
  → modbridge::native::ConfigureRetainedPS5PadInput (T)
objdump -r: section .text.…UE4SSBackendC2… offset 0xee
  R_X86_64_REX_GOTPCRELX modbridge::native::ConfigureRetainedPS5PadInput(...)-0x4
```
Strings `ue4ss/PS5Pad.ini`, `PS5Pad input unavailable`, `provider factory returned no source`
match `NativePadInput.cpp:13,22,18` byte-for-byte.

## 1. Hop-by-hop trace

### Hop 1 — PS5Pad.ini parse (`PS5PadConfig.cpp`)
- `parse_ps5_pad_config` `.../native-coherent-pad-runtime-1/build-1/source/PS5PadConfig.cpp:52`.
- Hard refusals: size/ASCII `:54-58`; unknown/repeated section `:70-74`; exactly one `=` `:76-78`;
  ≥1 binding `:123`; ≤128 bindings `:91`; no overlapping chords `:115-119` (a chord that contains,
  or is contained in, another configured chord is rejected).
- Chord left side: `+`-separated button names `:26-37`, names→bitmask table `:41-46`
  (`OPTIONS`→`PS5_PAD_BUTTON_OPTIONS` at `:42`; UP/RIGHT/DOWN/LEFT `:43`; `CROSS` `:45`).
- Right side: trailing token is a UE4SS key via `string_to_key` `:104` (unknown key → invalid `:105`);
  optional `SHIFT/CONTROL/ALT` prefixes `:109-113`.

### Hop 2 — provider activation (`NativePadInput.cpp` + provider)
- `ConfigureRetainedPS5PadInput` `.../native-coherent-pad-runtime-1/build-1/source/NativePadInput.cpp:10`
  reads exactly `ue4ss/PS5Pad.ini` from the activated mod source `:13`; absent file = return, no pad
  bridge `:14`; a second input source would conflict-throw `:15`.
- Parse `:16`, factory `:17`; **throws** if factory returns null `:18` and if `!source->is_available()`
  `:19-24` (phase+error in the message). Therefore: the task premise holds — if the composed runtime
  got past startup, the provider activated.
- `make_native_ps5_pad_source` `ue4ss-native-pad-provider-1/port/PS5PadNative.cpp:16-22` wires the
  real SDK (`sceUserServiceInitialize`, `scePadInit`, `sceUserServiceGetLoginUserIdList`,
  `scePadOpen`, `scePadReadState`, `scePadClose`) at `:17-19` and **eagerly activates** `:20`.
- `PS5PadSource::activate` `PS5PadSource.cpp:25-48`: user service `:29-31` (already-initialized
  `0x80960003` accepted `:31`), `scePadInit` `:35-36`, first logged-in user for `User=-1` `:37-42`,
  `scePadOpen` `:43-44`; `is_available()` == `handle >= 0` `:24`, status set Active `:45`.

### Hop 3 — wiring into the input Handler (runtime startup ordering)
- Backend ctor (composed ack revision) calls `ConfigureRetainedPS5PadInput(*context.mod_source,
  context.input_source)` when `enable_buffered_mods && context.mod_source` —
  `worker-composition-3/bootstrap-stage/.../ue4ss_backend.cpp:21-26` — i.e. BEFORE `program_->init()`
  (`:47` in the same file) and before `start_native_mods()` (`:36`).
- `UE4SSProgram` ctor (PROSPERO) → `validate_native_services()` `SRC/ue4ss/src/UE4SSProgram.cpp:250`
  → `m_input_handler.init(m_native_context.input_source)` `:731-734` (throws on failure `:733`).
- `Handler::init(shared_ptr<PlatformInputSource>)` `SRC/first/src/Input/Handler.cpp:53-64`:
  availability check `:59`, activation `:60`, injected-source retention `:62`; on `__PROSPERO__`
  `m_source_injected_mode` defaults **true** `SRC/first/include/Input/Handler.hpp:57`.
- Keybind registration later re-checks availability and **throws** `"Input registration unavailable:
  no active input source"` if there is no active source `Handler.cpp:109,123`.

### Hop 4 — button state polling (who / which thread / cadence)
- Poller = the UE4SS event-loop thread: `UE4SSProgram::update` `SRC/ue4ss/src/UE4SSProgram.cpp:1500`,
  thread name `"UE4SS-UpdateThread"` `:1507`, started as `std::jthread{&UE4SSProgram::update, this}`
  `:673`. Not the game thread, not a separate pad thread.
- Loop body: drain up to 5 queued events (PROSPERO batch path `:1605-1618`), then
  `m_input_handler.process_event()` under `#ifdef HAS_INPUT` `:1659-1661`, then mod `fire_update()`
  `:1665-1671`, then `sleep_for(5ms)` `:1682`. So `scePadReadState` is polled at ~200 Hz worst case,
  gated only by this loop being alive (`HAS_INPUT` is in the build: `-DHAS_INPUT` present in
  `final-recipe-capture/recipes.json`).
- `Handler::process_event` `Handler.cpp:66-103`: no active source → immediate return `:68-71`;
  `m_platform_handler->process_event(this)` pulls events `:75`; `m_allow_input` gate `:76`
  (default **true** `Handler.hpp:50`; the only setter call site is inside a commented-out block
  `UE4SSProgram.cpp:1639-1658`); key/subscriber match `:83-94`; callbacks invoked outside the lock
  `:99-102`.
- `PS5PadSource::process_event` → `poll(subscribed, allow)` `PS5PadSource.cpp:65-68`; poll body
  `:69-94`: `scePadReadState` `:73`; disconnect resets edge state `:75-78`; **duplicate sample guard**
  (same `timestamp` → no second keydown) `:81`; chord match `down = (data.buttons & b.buttons) ==
  b.buttons` `:84`; event emitted iff `!baseline && down && !m_down[i] && allow && subscribed[b.key]`
  `:85` (dedupe of two bindings mapping to the same key+modifiers `:86-88`); `m_down[i]` tracked
  every poll even when unsubscribed (no phantom edge after registration) `:90`.

### Hop 5 — key event synthesis (`KeyDef.cpp`)
- The `InputEvent.key` was resolved at ini-parse time: `string_to_key` `SRC/first/src/Input/KeyDef.cpp:180-195`
  lowercases then table-looks-up; `f7`→`Key::F7` `:117`, `return`→`Key::RETURN` `:22`,
  `up_arrow`/`down_arrow`/`left_arrow`/`right_arrow` `:44-47`, `escape`→`Key::ESCAPE` `:34`;
  unknown name throws `:193-196` (surfaced as `PS5Pad.ini line N: unknown UE4SS key`,
  `PS5PadConfig.cpp:105`).

### Hop 6 — dispatch to registered keybinds and the Lua callback
- Registration (`main.lua` top level, during `LuaMod::start_mod`):
  `RegisterKeyBind` `SRC/ue4ss/src/Mod/LuaMod.cpp:1915`; key integer 0..255 `:1929-1933`; callback
  stored via registry ref `:1957-1960`; `mod->m_program.register_keydown_event(...)` `:1964-1969`
  (modifier overload `:2006-2012`) → `UE4SSProgram::register_keydown_event` `UE4SSProgram.cpp:2882-2887`
  → `Handler::register_keydown_event` `Handler.cpp:105-116`, which appends `KeyData` `:111` and sets
  `m_subscribed_keys[key]=true` `:115` (this is what makes `subscribed[b.key]` true in Hop 4's poll).
- **The Lua callback runs synchronously on the UpdateThread** (inside `Handler::process_event`):
  `lua_keybind_callback_lambda` `LuaMod.cpp:1937-1948` takes `m_thread_actions_mutex` `:1938`,
  `lua.call_function(0,0)` `:1942`; errors are caught and logged `:1944-1947`
  (`Output::send<LogLevel::Error>` → `UE4SS.log`, file device set `UE4SSProgram.cpp:256-257`,
  `m_log_directory = m_root_directory` `:780-782` on PROSPERO). There is no per-press queue between
  poll and Lua for the keybind itself.
- `main.lua`'s F7 callback then defers the menu work:
  `RegisterKeyBind(Key.F7, function() ExecuteInGameThread(toggleMenu) end)` `main.lua:694-696`.
  `ExecuteInGameThread` `LuaMod.cpp:4690-4768` uses `m_default_game_thread_method` = **EngineTick**
  (`LuaMod.hpp:169`; shipped `UE4SS-settings.ini:12` `DefaultExecuteInGameThreadMethod=EngineTick`;
  applied at `UE4SSProgram.cpp:1475-1476`), throws at registration if the EngineTick hook never
  became available `:4716-4721`, and enqueues a `SimpleLuaAction` into `m_engine_tick_actions`
  `:4762-4767` (pending queue while dispatching `:4749-4767`).
- Game-thread drain: `engine_tick_hook` `LuaMod.cpp:4472-4478` → `process_simple_actions(m_engine_tick_actions)`
  `:4478` (implementation `:4113`), gated by `NativeLuaActionDispatchScope` ownership
  `:4085-4108` (only the thread published by the real Tick dispatches). Hook registered via
  `Unreal::Hook::RegisterEngineTickPreCallback(engine_tick_hook, ...)` `:4524-4528`; the real
  engine tick detour itself is `HookedEngineTick` registered at
  `SRC/unreal/src/UnrealInitializer.cpp:1321` (`bHookEngineTick` gate `:1252`).
- `toggleMenu` → `showMenu(not menuOpen)` `main.lua:670-672`.

### Timing recap (who runs before what)
1. `module_start` → `StartActivatedRuntimeWithAck` → `PrepareActivatedUE4SSRuntime` (4-arg ack form,
   `worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.cpp:68-95`) → backend ctor:
   pad ini read+parse+`scePadOpen` **and** `UE4SSProgram` construction (which runs
   `validate_native_services` → `Handler::init(pad source)` at `UE4SSProgram.cpp:250,731-734`), then
   `start_native_mods()` inventory (`ue4ss_backend.cpp:31-36` ack revision; startup throw surfaces via
   `RuntimeAckSink::failed` `:96-110` of that file).
2. `owner->start()` → `init()` → engine init → event-loop jthread `UE4SSProgram.cpp:673`.
3. On the UpdateThread, `on_program_start` → `install_lua_mods(); start_lua_mods()`
   `UE4SSProgram.cpp:1484-1489` (PROSPERO gate `ShouldRunNativeModCallbacks` `:1481-1483`) →
   `LuaMod::start_mod` `LuaMod.cpp:6440-6489` runs `main.lua` → six `RegisterKeyBind` calls
   (`Handler` already has the active pad source, so `Handler.cpp:109` cannot fire) → success log
   `main.lua:746` (`"[TownfallItemSpawner] Loaded v1.0 …"` via Lua `print`, `main.lua:6`).
4. User presses OPTIONS → Hop 4/5/6 → `toggleMenu` on the game thread.

## 2. Edge vs level; exact button semantics

- **EDGE-triggered.** `PS5PadSource.cpp:85` fires only on the `!m_down[i] → down` transition; `:81`
  suppresses duplicate `scePadReadState` samples; `:90` keeps edge state fresh while gated. One
  keydown per physical press; **no** autorepeat, **no** keyup events are synthesized anywhere in the
  chain (`register_keydown_event` is the only registration API used).
- **Chord = OPTIONS ALONE.** `PS5Pad.ini` line `OPTIONS = F7` (final package, sha256
  `1d919fbf…`, 2960 B) is a single-button chord. Match is `(data.buttons & b.buttons) == b.buttons`
  `PS5PadSource.cpp:84`: the OPTIONS bit must be down; any additional unrelated buttons do **not**
  suppress the event. It is not "OPTIONS as a modifier with another button" — nothing else is
  required or used. (Config-side, overlapping chords are refused at parse time,
  `PS5PadConfig.cpp:115-119`.)
- **Exact user action:** with a loaded save, briefly press and release OPTIONS once. Holding it
  longer still produces exactly one F7. Pressing OPTIONS+UP simultaneously would fire both
  `F7` and `UP_ARROW` bindings (independent chords), which is harmless when the menu is closed.

## 3. Do the arrow/RETURN mappings matter? Other keys?

Yes — they are the entire navigation surface, and they line up 1:1 with the mod's registrations
(`main.lua:694-729`), via the staged `PS5Pad.ini:35-40`:

| pad | key | registration | effect |
|---|---|---|---|
| OPTIONS | `F7` | `main.lua:694-696` | `ExecuteInGameThread(toggleMenu)` → `showMenu(not menuOpen)` `:670-672` — **same key opens and closes (toggle)** |
| UP | `UP_ARROW` | `:698-701` | `moveSelection(-1)` `:674-682` |
| DOWN | `DOWN_ARROW` | `:703-706` | `moveSelection(1)` |
| CROSS | `RETURN` | `:708-711` | `activateSelection()` `:684-692` |
| LEFT | `LEFT_ARROW` | `:713-720` | page −1 `:716-719` |
| RIGHT | `RIGHT_ARROW` | `:722-729` | page +1 `:725-728` |

- The five nav binds early-return while the menu is closed (`if not menuOpen then return end`,
  `main.lua:699,704,709,714,723`) — a closed-menu press is a **silent no-op by design** (no log).
- No ESCAPE bind exists anywhere in `main.lua` (only those six `RegisterKeyBind` calls); ESC is not
  needed. F7 is the only open/close key; a map load also force-closes the menu and resets state
  (`RegisterLoadMapPreHook`, `main.lua:731-744`).
- Documented limitation: pad keybinds are non-exclusive notifications; while the menu is open the
  same buttons still reach the game (`PS5Pad.ini:45-48`).
