# PAD-API-AND-INTEGRATION.md — how the retained PS5 pad provider reaches the keybind dispatch gate

worker-real-input-pad, 2026-10-04. Every fact below was read from the actual
retained sources in this worktree (paths + line references inline). Nothing ran
on console. This document answers task question 1: how does
`PS5PadSource::poll` get called, where does edge detection live, and how does
the dispatch gate reach the Lua callback.

## 1. The provider API (retained, `native_ps5_pad_provider`)

Source of truth: `.superpowers/native-dependency-spike/ue4ss-native-pad-provider-1/port/PS5PadSource.{hpp,cpp}`.

```cpp
struct PadBinding { uint32_t buttons; Key key; ModifierKeys modifiers{}; };   // hpp:9
struct PadBackend {   // hpp:15-23 — the ONLY hardware surface, 7 function pointers
    int (*user_initialize)(void*);   // sceUserServiceInitialize
    int (*pad_initialize)();         // scePadInit
    int (*user_list)(int[4]);        // sceUserServiceGetLoginUserIdList
    int (*open)(int,int,int,void*);  // scePadOpen(user, PS5_PAD_PORT_TYPE_STANDARD, 0, nullptr)
    int (*read_state)(int,PS5_PadData*); // scePadReadState(handle, &data)
    int (*close)(int);               // scePadClose
    int (*user_terminate)();         // sceUserServiceTerminate
};
class PS5PadSource final : public PlatformInputSource {                    // hpp:24
    PS5PadSource(std::vector<PadBinding> bindings, PadBackend backend,
                 int requested_user = -1);                                 // hpp:26
    std::vector<InputEvent>& process_event(Handler*) override;             // hpp:31
    std::vector<InputEvent>& poll(const std::array<bool,max_keys>& subscriptions,
                                  bool allow);                             // hpp:33
    const PadStatus& status() const;   // {phase, error, user_id, handle}   // hpp:36
};
std::shared_ptr<PS5PadSource> make_native_ps5_pad_source(std::vector<PadBinding>, int user_id = -1); // hpp:50
```

- `make_native_ps5_pad_source` is the **console** factory (`PS5PadNative.cpp`,
  `#error` unless `__PROSPERO__`): it fills `PadBackend` with the real
  `sceUserServiceInitialize/scePadInit/sceUserServiceGetLoginUserIdList/scePadOpen/scePadReadState/scePadClose/sceUserServiceTerminate`
  through a process-wide `PadServiceLease`, then **eagerly `activate()`s**
  (PS5PadNative.cpp:16-21). `activate()` walks
  user_initialize → pad_initialize → user_list → open and only reports
  available once `handle >= 0` (PS5PadSource.cpp:25-48). This factory is the
  never-run-on-hardware part: `native_ps5_pad_provider` checkpoint says
  `hardware_runtime_validated=false`.
- The `PadBackend` injection point is the provider's explicit owned-test
  surface (PS5PadSource.hpp:14-15) — host tests feed scripted samples there;
  the standalone probe reuses exactly this seam (see §5).

## 2. Where edge detection lives

Inside `PS5PadSource::poll` itself, `PS5PadSource.cpp:69-94`. Per call:

1. Clear the event vector; return empty unless `is_available()` (handle >= 0). (:70-71)
2. `read_state(handle, &data)`; a nonzero error fails the phase and returns
   empty. (:72-74)
3. `!data.connected` → phase `Disconnected`, clear all held-state, swallow next
   sample (`m_have_sample=false`); no events. (:75-78)
4. **Cached-read guard**: if `data.timestamp == m_timestamp`, return empty —
   `scePadReadState` is cached, so a duplicate sample can never create a second
   keydown. (:80-81)
5. First sample after (re)attach is a baseline: held-state is recorded, no
   events emitted. (:82)
6. **Rising edge**: for each binding, `down = (data.buttons & b.buttons) ==
   b.buttons`; emit `{b.key, b.modifiers}` only on `down && !m_down[i] && allow
   && subscribed[b.key]`, deduplicated per key+modifier within one sample. Held
   state is tracked even while disabled/unsubscribed so no phantom edge fires
   on re-enable. (:83-91)

Debouncing beyond this (contact bounce, re-arm window) is NOT in the retained
provider; worker-real-input's vendored `PadEdgeSource` adds it downstream and
is intentionally NOT part of the first hardware probe (fewest variables).

## 3. Composed-runtime integration point (how poll gets called every tick)

Composed runtime: `native-coherent-pad-runtime-1/build-1` — its `link.command.json`
shows `pad-config.o`, `native-pad-input.o`, `ue4ss_backend.o` linked together
with the retained provider objects (`native-pad-PS5PadSource.o`,
`native-pad-PS5PadNative.o`) and `input-handler.o` / `input-keydef.o` into
`coherent-pad-runtime.o`. The wiring chain:

```
PrepareActivatedUE4SSRuntime (ue4ss_backend.cpp:84)
  └─ UE4SSBackend ctor (ue4ss_backend.cpp:20-57)
       ├─ line 26: ConfigureRetainedPS5PadInput(*context.mod_source,
       │                                     context.input_source)
       │    (NativePadInput.cpp:10-26: reads ue4ss/PS5Pad.ini from the
       │     activated mod source; missing ini ⇒ input untouched; present ini ⇒
       │     parse_ps5_pad_config → make_native_ps5_pad_source → requires
       │     is_available() → assign into context.input_source. Any failure
       │     THROWS, so a bad ini blocks the whole activated runtime start.)
       └─ line 47: UE4SSProgram constructed with that context
            └─ UE4SSProgram.cpp:733: m_input_handler.init(m_native_context.input_source)
                 (Handler.cpp:53-64: Handler::init activates the injected source
                  and sets m_source_injected_mode)
            └─ UE4SSProgram.cpp:1653 (update loop, each tick):
                 m_input_handler.set_allow_input(console_state && *console_state == NAME_None)
            └─ UE4SSProgram.cpp:1660 (update loop, each tick):
                 m_input_handler.process_event()
```

So **`PS5PadSource::poll` is called once per program tick** through
`Handler::process_event` → `m_platform_handler->process_event(this)`
(Handler.cpp:75 → PS5PadSource.cpp:65-68), which forwards
`get_subscribed_keys()` and `get_allow_input()` into `poll`.

Two gating facts that matter for the probe and the plan:

- `Handler::m_allow_input` defaults to `true` (Handler.hpp:47). The live
  program overwrites it every tick from console state (UE4SSProgram.cpp:1653);
  a standalone probe that never calls `set_allow_input` keeps the default.
- On `__PROSPERO__`, `Handler::m_source_injected_mode` defaults to `true`
  (Handler.hpp:61-64), and `register_keydown_event` throws
  `"Input registration unavailable: no active input source"` when injected-mode
  is on with no available source (Handler.cpp:109). **On console, the probe
  must `handler.init(source)` before registering any keybind.** (Host builds
  default this flag false — the host test must mirror the console ordering
  anyway so the test is representative.)

## 4. Dispatch gate → Lua callback

`Handler::process_event` (Handler.cpp:66-103), all in the composed runtime:

1. Poll the source (:75); if `!m_allow_input` return (:76) — the program-wide
   input enable gate.
2. Under `m_event_mutex`: for each event, find `m_key_set.key_data[event.key]`
   and select every `KeyData` whose `required_modifier_keys == event.modifier_keys`
   (:80-95). Exact equality — a pad binding with no modifiers matches only
   registrations with no modifiers.
3. Snapshot callbacks, release the mutex, then invoke them (:97-102). A Lua
   keybind registered inside a callback therefore cannot corrupt the iteration.
4. The callbacks are the ones Lua registered: `RegisterKeyBind(Key.F7, cb)` in
   the mod goes LuaMod.cpp (coherent port, :1915+) →
   `m_program.register_keydown_event` (UE4SSProgram.cpp:2885/2898) →
   `Handler::register_keydown_event` which sets `m_subscribed_keys[key]=true`
   (Handler.cpp:105-116) — this is what makes the provider's `subscribed[b.key]`
   gate pass on the next poll.
5. The Lua callback itself runs on the program's processing thread. The Item
   Spawner mod wraps its body in `ExecuteInGameThread(...)` (main.lua:695 etc.);
   the game-thread identity gate (worker-3 seam) refuses delivery while the
   identity is unknown or the caller is another thread. That seam is outside
   this probe: the probe's callback is a native function that only writes a
   JSONL record, so rung-5's blocked game-thread delivery cannot mask the
   "pad reached the dispatch gate" signal this probe verifies.

## 5. Consequence for the first hardware test (design decision)

The provider API is sufficient for a standalone module — no Lua, no engine, no
game thread is needed to prove "pad → provider edge → Handler dispatch →
callback":

- hardware surface is fully injected via `PadBackend` (host: scripted; console:
  `make_native_ps5_pad_source`),
- dispatch is the real retained `RC::Input::Handler`
  (`init` → `register_keydown_event(Key::F7, cb)` → `process_event()` loop),
- evidence is a JSONL record written by the registered callback.

That standalone probe (`pad_probe/` in this directory) exercises the exact
objects the composed runtime links (PS5PadSource.cpp, Handler.cpp) and only
defers to a real `PS5Pad.ini` + `ConfigureRetainedPS5PadInput` run inside the
composed runtime, which remains the second hardware step after the probe
passes. Rationale for probe-before-composed: the composed runtime start
**throws** if the pad provider fails to open (ue4ss_backend.cpp:26 +
NativePadInput.cpp:19-23), so the first hardware contact with `scePad*` must
not be inside the full activated-runtime start where a pad failure is
indistinguishable from every other startup failure.
