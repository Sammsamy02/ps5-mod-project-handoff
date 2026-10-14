# Keyboard and mouse on PS5: design (Team I2, 2026-10-13)

Everything in this document is **Mac-only**. Nothing has been run on the console.
Nothing in the runtime tree, the frozen builds or the build folders was changed.

## 1. In plain words

**The goal.** A PC mod says "when F8 is pressed, do this". On the PS5 today, F8 only
arrives when you press a controller combination (for example L3+R3 = F7), set in
`PS5Pad.ini`. We want a real USB keyboard and mouse plugged into the PS5 to work as
well, with the controller combinations still working exactly as before.

**What we found.** Townfall already supports a USB keyboard and mouse on its own.
Its program loads the PS5 keyboard and mouse libraries and calls their open and read
functions (section 2, facts F2 and F3). So the game already reads the keyboard and
turns key presses into Unreal "keys".

**What we recommend (option C).** Do not open the keyboard ourselves. Only one part
of a game can own it. If we take it, the game can lose it, and if the game has it, we
get nothing. This is the same problem the controller had, which needed a workaround.
Instead, once per frame, the runtime asks the game's own player object "is F8 down
right now?". It uses the engine's own function for this (`IsInputKeyDown`) and asks
only about keys that mods registered. When an answer changes from "no" to "yes", the
mod's key bind fires, the same way it fires today from a controller combination. The
controller and the keyboard work side by side.

**Why it is safe.**
- No new system functions (imports).
- No device access.
- No change to game code.
- Nothing is written specifically for Townfall.

If the engine function is missing, the keyboard part switches itself off and the
controller combinations keep working. With no keyboard plugged in, nothing happens
and nothing breaks.

**What is not known yet.** We do not know whether the engine in Townfall really
receives the USB keyboard. It is very likely, but not proven. A small read-only Lua
test mod can answer this in one launch, with no new build (section 6, launch K1).

**Next step.** First the Lua-only test launch K1. If it passes, the runtime change
gets one build and one capability launch, K2.

## 2. Facts used (all Mac-only)

| # | Fact | Source |
|---|---|---|
| F1 | The runtime PRX (R16) has 207 imports: libScePad 5, libSceUserService 3, libkernel 73, libc 126. It has **no** mouse, keyboard or IME imports. `sceKernelDlsym`, `sceKernelLoadStartModule` and `sceKernelGetModuleInfo` are already imported. | `outputs/2026-10-08-l26-build-variants/archive-r6/R16-cap07s2-pakmount/2026-10-08-l26-package-A/pass-1/EXACT-IMPORT-IDENTITIES.json` |
| F2 | Townfall's executable lists `libSceIme.prx`, `libSceImeDialog.prx` and `libSceMouse.prx` as needed libraries. It imports `sceMouseInit/Open/Read/Close`, `sceImeKeyboardOpen/Close/GetInfo/GetResourceId` and `sceImeUpdate`. It does **not** import `sceImeOpen` (so it uses keyboard input, not the on-screen text panel), and it does not import `libSceKeyboard`. | `evidence/townfall-input-imports.json`, made by `evidence/decode_input_imports.py` from the saved declared-import metadata (public NID hash) |
| F3 | Both `/<random>/common/lib/libSceIme.sprx` and `.../libSceMouse.sprx` are loaded in the running Townfall process. The path has a random sandbox folder name. | Kernel-log module list from an earlier session (Team C copy `caps-team-C/.../wrapper-ev/klog-20261012-162139.log`, lines 6387-6392). Used as design input only, not as a verdict for any launch. |
| F4 | The controller already has the "game owns the device" problem. `scePadOpen` returns `0x80920004` (ALREADY_OPENED) because the game holds the pad. The runtime works around it by borrowing the game's handle (`scePadGetHandle`). | Comments and evidence in `worker-pad-coexist/.../NativePadInput.cpp` and `2026-10-08-pad-recovery-candidate/PS5PadNative.cpp` (earlier sessions 317/341/355/370) |
| F5 | Listing modules from inside the game does not work. `sceKernelGetModuleList` gave 6 handles with empty names, and `sceKernelGetModuleInfo` returned rc `0x7FFFFFFF`. `dlopen(RTLD_NOLOAD)` is unavailable. VMMAP does work. | `outputs/2026-10-04-zcode-controller/console/ingame-v1-facts-2026-10-04.jsonl` (g3, g4, g5) |
| F6 | The key-bind handler takes one input source. On PS5 its name check is skipped (`UE4SSProgram.cpp` 931-941). An unavailable source is still attached and polled ("deferred attach"). `process_event` runs on the UE4SS update thread. A bind fires only when its modifier set matches the event's exactly. | R16 `Handler.cpp` (worker-pad-coexist) and `UE4SSProgram.cpp` |
| F7 | Game-thread work already happens every frame: the UEngine::Tick vtable pump drains the game-thread queue. In Lua, `ExecuteInGameThread` is PROVEN, and table arguments for struct parameters (`{X=..}`) are used by the console-proven spawner mod. Reading FKey `KeyName` is proven by the self-test (`ConsoleKeys`). | `VmtTickPumpCore.inc`, `API-PARITY-TABLE.md`, spawner `main.lua` 247-540, `ueselftest/part7-8.lua` |
| F8 | Public model of the keyboard and mouse API: the shadPS4 emulator source. It is reverse-engineered, models the **PS4**, and is a **single source**. It says: one keyboard open per process (a second open returns BUSY); key events arrive only through `sceImeUpdate` to the opener's callback; event ids are KeycodeDown 257, Up 258, Repeat 259, Connection 260, Disconnection 261; modifier status bits are 0x100-0x8000 in USB HID modifier order. For the mouse: `sceMouseOpen(user, type, index, param)` and `MouseData{timestamp, connected, buttons, x, y, wheel, tilt}`. | github.com/shadps4-emu/shadPS4 `src/core/libraries/ime/*`, `mouse/mouse.h` (read 2026-10-13; nothing copied) |
| F9 | Townfall runs UE 5.6 (cap02 pack name). `APlayerController::IsInputKeyDown(FKey)` is public engine API: Epic API docs (4.26/4.27) and the Blueprint node "Is Input Key Down". The EKeys names come from Epic's public API page. | dev.epicgames.com EKeys page; docs.unrealengine.com APlayerController pages |

No Sony SDK material was used. The SDL header in `refs/` is the public SDL 2.30.0
release.

## 3. The three options

### Option A: reuse the game's loaded keyboard and mouse libraries (no new import)

**How it would work.** The runtime finds `sceImeKeyboardOpen`, `sceMouseRead` and
the other functions inside the game's already-loaded libraries, then opens the
keyboard and mouse and reads events itself. There are two ways to find the
functions:
- (a) `sceKernelLoadStartModule(path)` on the already-loaded library, then
  `sceKernelDlsym` by name;
- (b) read the game's own resolved import slots. The game's relocation table gives
  the slot (source 1). At run time, check that all four slots point into one
  executable mapping outside the eboot (source 2).

**Risks.** These decide against option A for shipping:
1. **Exclusive open.** Under the public model (F8), the keyboard is one per process.
   - If the game opened it first (likely: F2), we get BUSY and nothing else.
   - If we open it first, the game may lose its keyboard, which changes the game.
   - It is the same trap as the pad (F4), and there is no public "borrow the
     keyboard handle" function among the game's imports.
2. **Event delivery.** Events arrive only through `sceImeUpdate` to the opener's
   callback. If we call it from our thread, it might also run the game's own IME
   callbacks on our thread. Unknown.
3. **Finding the functions.**
   - Module listing fails in-game (F5).
   - The library path has a random sandbox name (F3), and loading by bare name is
     unproven.
   - Route (b) needs a per-game slot offset (the profile generator could compute
     it) and assumes slots are bound at load time. Unproven.
4. **Key codes.** No public source says whether PS5 keyboard codes are USB HID
   usages. It needs a console capture first.
5. **Mouse.** It opens per index, and whether a second index sees the same mouse is
   unknown.

**Answers to the questions.**
- No keyboard plugged in: open may fail, or may succeed and report "connected"
  later. Unknown.
- Coexistence with the game: no (BUSY, or we take the keyboard from the game).

**Verdict.** Not for shipping. It is plan B only if launch K1 shows the engine does
not receive the keyboard, and even then it starts as a read-only diagnostic.

### Option B: new runtime imports

It has all of option A's device problems, plus:
- About 9 new imports, each needing provider evidence (207 -> 216). A wrong import
  identity has crashed a launch before: `scePadGetHandle` was published under
  libkernel (rev 3 note in `PS5PadNative.cpp`).
- **It is not fail-closed.** The runtime PRX would then depend on `libSceIme` and
  `libSceMouse`. On a game that does not load them, the loader must load them (a
  side effect), or the runtime does not start at all.

**Verdict.** No. Strictly worse than A.

### Option C: read Unreal's own key state (RECOMMENDED)

**How it works.**
- Once per game frame, on the game thread (the existing Tick pump, F7), for each key
  that has a `RegisterKeyBind`, the runtime calls the local player controller's own
  reflected `IsInputKeyDown(FKey)`.
- Modifiers come from `LeftShift`/`RightShift`, `LeftControl`/`RightControl` and
  `LeftAlt`/`RightAlt`.
- An up->down change becomes a normal UE4SS key event. It goes through a lock-free
  queue to the UE4SS update thread, where key binds already run.
- A "Merged" source holds today's pad source plus this source, so the handler still
  sees one source (F6).

**Why it is the safest choice.**
- **Data-only.**
  - No hook, no detour, no new import, no code patch.
  - The function is found by name at run time. Source 1 is Epic's public docs.
    Source 2 is the game's own reflection data at run time.
  - Its parameter layout (size, Key parameter, bool return) is checked by
    reflection. Any mismatch means "unsupported", and the source switches itself
    off.
- **No device access.** The game keeps the keyboard and mouse, so exclusive open is
  not our problem, and the game's own keyboard use is unchanged.
- **No keyboard plugged in.** The engine answers "up" for every key, so there are no
  events and no errors. This is host-tested (`no_keyboard_plugged_in`).
- **Fail closed, per key.** A key name the game does not have is skipped (it is
  looked up only, never added to the name table). The other keys still work.
- **Correct for new objects.** The controller is re-found when the cached one is no
  longer valid (cache rebuilt on miss). After a level change or a loading screen,
  keys that are already held never fire (host-tested).
- **Bounded cost.**
  - Only bound keys are asked about (Townfall's set is about 15), plus 6 modifier
    reads.
  - The budget is 0.25 ms per frame. Above it, the source samples every 2nd, then
    4th, then 8th frame, and counts each slowdown.
  - Above 64 bound keys, keys are sampled in turns (round-robin).
- **Generic.** It works at engine level (UE4 and UE5 both have `IsInputKeyDown`),
  with no game names and no offsets.

**Limits.** These must be stated to the user:
1. It sees only keys that reach the player's input. When a game menu takes all
   keyboard input (a "UI only" screen or a text box), keys may not arrive. The
   controller combinations still work there. PC UE4SS reads the Windows keyboard
   directly, so on PC this limit does not exist.
2. The engine must receive the keyboard at all. This is likely for Townfall (F2) but
   unproven, and launch K1 decides it.
3. The engine has F1-F12 only (no F13-F24), no media keys and no IME keys.
   Punctuation names follow US key positions.
4. A key bind fires on press only, with no auto-repeat. UE4SS on PC behaves the same.
5. Mouse movement and a pointer for UE4SS GUI windows (#11) are not covered. That is
   a later step on the same route (`GetMousePosition`).

## 4. Key mapping (PS5 / engine keys -> UE4SS `Key`)

- Full table: `prototype/src/KeyboardMouseMap.cpp`, with a readable dump in
  `prototype/out/keymap.tsv` (168 lines: 163 keys, 3 modifiers, 2 aliases).
- The table was generated from the runtime's real `KeyDef.hpp` enum, so every key a
  PC mod can bind appears exactly once.
- The gate `tests/check_table.py` checked 730 items and found 0 errors.

| Group | UE4SS `Key` | Engine key (option C) | USB HID usage (option A only) |
|---|---|---|---|
| Letters | `A`..`Z` | `A`..`Z` | 0x04..0x1D |
| Top-row digits | `ZERO`..`NINE` | `Zero`..`Nine` | 0x27, 0x1E..0x26 |
| F keys | `F1`..`F12` | `F1`..`F12` | 0x3A..0x45 |
| F13-F24 | `F13`..`F24` | none (unmapped) | 0x68..0x73 |
| Arrows | `UP_ARROW` ... | `Up` `Down` `Left` `Right` | 0x52 0x51 0x50 0x4F |
| Editing/navigation | `RETURN` `ESCAPE` `BACKSPACE` `TAB` `SPACE` `INS` `DEL` `HOME` `END` `PAGE_UP` `PAGE_DOWN` | `Enter` `Escape` `BackSpace` `Tab` `SpaceBar` `Insert` `Delete` `Home` `End` `PageUp` `PageDown` | 0x28 0x29 0x2A 0x2B 0x2C 0x49 0x4C 0x4A 0x4D 0x4B 0x4E |
| Locks/pause | `CAPS_LOCK` `NUM_LOCK` `SCROLL_LOCK` `PAUSE` | `CapsLock` `NumLock` `ScrollLock` `Pause` | 0x39 0x53 0x47 0x48 |
| Numpad | `NUM_ZERO`..`NUM_NINE`, `ADD` `SUBTRACT` `MULTIPLY` `DIVIDE` `DECIMAL` | `NumPadZero`..`NumPadNine`, `Add` `Subtract` `Multiply` `Divide` `Decimal` | 0x62, 0x59..0x61, 0x57 0x56 0x55 0x54 0x63 |
| Punctuation (US positions) | `OEM_ONE` `OEM_PLUS` `OEM_COMMA` `OEM_MINUS` `OEM_PERIOD` `OEM_TWO` `OEM_THREE` `OEM_FOUR` `OEM_FIVE` `OEM_SIX` `OEM_SEVEN` | `Semicolon` `Equals` `Comma` `Hyphen` `Period` `Slash` `Tilde` `LeftBracket` `Backslash` `RightBracket` `Apostrophe` | 0x33 0x2E 0x36 0x2D 0x37 0x38 0x35 0x2F 0x31 0x30 0x34 |
| Windows keys | `LEFT_WIN` `RIGHT_WIN` | `LeftCommand` `RightCommand` | 0xE3 0xE7 |
| Mouse | `LEFT_MOUSE_BUTTON` `RIGHT_MOUSE_BUTTON` `MIDDLE_MOUSE_BUTTON` `XBUTTON_ONE` `XBUTTON_TWO` | `LeftMouseButton` `RightMouseButton` `MiddleMouseButton` `ThumbMouseButton` `ThumbMouseButton2` | none (mouse API, F8) |
| Modifiers | `ModifierKey.SHIFT` / `CONTROL` / `ALT` | `LeftShift`\|`RightShift`, `LeftControl`\|`RightControl`, `LeftAlt`\|`RightAlt` | 0xE1\|0xE5, 0xE0\|0xE4, 0xE2\|0xE6 |
| Unmapped | IME keys, media/browser/launch keys, `CANCEL` `PRINT` `SLEEP` `OEM_EIGHT` `PACKET` ... | none | some have usages (see table) |

**Totals.** 100 of the 163 keys have an engine name and work with option C. 124 have
a HID usage (option A only).

**Sources.**
- Engine names: Epic's public EKeys page (source 1), then the live game's name table
  at run time (source 2).
- HID usages: USB-IF HID Usage Tables (source 1) and SDL 2.30.0 `SDL_scancode.h`
  (source 2, checked automatically).

**Unknown, said plainly.**
- Whether the PS5 keyboard API reports HID usage numbers. No public source; option A
  only.
- Whether the PS5 system keeps some keys for itself: Escape, the Windows key,
  PrintScreen, volume. To be checked in launch K1.
- Non-US layouts: punctuation follows key position, as UE does on PS5. Unverified.
- Whether `Gamepad_FaceButton_Bottom` is Cross on Townfall. It is the K1 positive
  control, so a wrong guess shows up as a missing control, never as a crash.

## 5. Design details (for the reviewer)

**Units.** All are in `prototype/`, and none is in the runtime tree.

- `KeyboardMouseMap` holds the table above, with lookups `fkey_for(Key)`,
  `key_for_fkey`, `key_for_hid`.
- `EngineKeySampler` runs on the game thread. Once per frame it:
  1. reads the published bind set;
  2. calls `KeyStateReader::begin_frame()` (Ready / NoController / Unsupported);
  3. reads the 6 modifier keys, then each bound key that has an engine name;
  4. on an up->down change, pushes `InputEvent{key, modifiers}` into an SPSC ring.
     It uses the runtime's own `RingBufferSPSC<InputEvent,256>`, which holds 255;
     overflow is counted.
- `EngineKeySource` is a `PlatformInputSource` on the update thread. It:
  - publishes the handler's subscribed keys as 4 atomic words;
  - drains the ring;
  - delivers events only when input is allowed;
  - reports available once the engine has answered once;
  - closes the channel on `deactivate`, after which the sampler stops calling the
    engine.
- `CompositeInputSource` ("Merged") polls every child on every call, so the pad keeps
  its own borrowed-handle retry. It:
  - merges the same key+modifiers from different children in one poll (pad L3+R3 =
    F7 together with keyboard F7 is one press);
  - isolates a throwing child;
  - never fails activation because one child is down.
- `KeyStateReader` is the engine boundary. The real one is **runtime work, not done
  here**. Steps:
  1. Local controller: use the runtime's existing player-controller lookup, cached;
     check it is valid every frame and re-find it when it is not. The
     `controller_generation` counter changes when the object changes.
  2. Look up the UFunction `IsInputKeyDown` on its class.
  3. Check by reflection: one struct parameter of the engine's `Key` struct, which
     has `FName KeyName`, plus a bool return value. Use the reflected offsets and
     `ParmsSize` only, never hand-written offsets.
  4. Fill a parameter buffer, then call through the runtime's existing UFunction
     call path (the same one Lua calls use).
  5. Resolve key names with a find-only FName lookup, and cache misses as "key not
     in this game".
  6. Any check fails: return `Unsupported`.

**Where it plugs in (runtime work).**
- Composition unit `NativePadInput.cpp`, `ConfigureRetainedPS5PadInput`: build
  `Merged{pad (only if PS5Pad.ini exists), engine keys}` and hand it over as today's
  `input`.
- With no `PS5Pad.ini`, `Merged{engine keys}` gives keyboard binds even on a game
  with no pad profile yet.
- The sampler gets one call per frame from the Tick pump drain (game thread).
  Teardown order: handler shutdown (closes the channel), then pump retirement, then
  unload.
- Switch: on by default, because it fails closed. An off switch can be one line in
  `UE4SS-settings.ini`; the integrator chooses.
- `PS5Pad.ini` is unchanged, so there is no MFA size re-pin.

**Log lines (bounded).**
- Once: `[kbm] engine-keys supported=<0|1> reason=<...> bound=<n>`.
- Every 60 s: `[kbm] SUM frames= sampled= reads= fails= events= drops= divisor=`.

**Gates.**
- No new import: the import count stays 207. `check_import_identity` /
  EXACT-IMPORT-IDENTITIES must show 207.
- No detour: the no-detour gate is unaffected (no code bytes, no new vtable slot).
- The fs-throw gate applies as usual.

## 6. Console test plan

The step-by-step version is in `LAUNCH-CARD.md`. Every console step needs the user's
OK for that step.

### Launch K1: "does the engine see the keyboard?"

Lua only. No build, no runtime change.

**What is deployed.** `probe/zzkbprobe/` (`Scripts/main.lua` 2193 B plus an empty
`enabled.txt`; read-only, binds nothing), as a new folder we own: exact mirror,
loadlist rows with the exact sizes, then the row-check. It can also ride along with the next capability launch that happens
anyway, because it changes nothing the game runs.

**What the user does.**
1. Plug the USB keyboard and mouse into the PS5 before starting the game.
2. In normal gameplay (not a menu), type F8, A, Space, Enter and Shift. Click the left
   and right mouse buttons. Press Cross a few times.
3. Unplug the keyboard and mouse. Press Cross again and use the L3+R3 mod menu once.
4. Optional: plug the keyboard back in and press F8. This tests hot-plug. It is a
   finding, not a pass/fail item.

**What it proves.**

| Result | Meaning |
|---|---|
| **K-PASS** | `[kb] DN/UP` lines appear for the keyboard keys and mouse buttons, plus the Cross control. Option C works on Townfall. |
| **K-PARTIAL** | Cross edges appear, but no keyboard edges. The engine does not get the keyboard: go to plan B, the option A diagnostic. |
| **K-NOPROBE** | `CAP fn=0`, or the controller is never found. This is a probe problem; fix it on the Mac. |
| **Controller unchanged** | Must hold in every case. L3+R3 works as in the last clean launch, with no new errors and no stutter. The probe adds about 300 cheap calls per second. |

**Also learned from K1.**
- Whether the PS5 system takes Escape, Windows, PrintScreen or volume keys: if these
  are tried, they show no edges.
- Behaviour with no keyboard: after the unplug, no edges and `fail=0`.

### Launch K2: the capability (one build, after K1 PASS)

**What is built.** The runtime change from section 5: the real `KeyStateReader`, the
Merged source and the sampler wiring. Before the build: one independent reviewer
(shipping runtime code), the gates, and the 207-import check. Then one build and one
launch.

**What is deployed.** `probe/zzkbbind/` (`Scripts/main.lua` 570 B plus an empty
`enabled.txt`). It binds F8, Shift+F8,
NUM_FIVE and the right mouse button, and prints `[kbb] FIRE <bind> n=<i>`.

**What it proves.**
1. A real key fires a normal `RegisterKeyBind`: F8 gives exactly one `FIRE F8` per
   press, and Shift+F8 gives only `FIRE SHIFT+F8`.
2. Keyboard F7 opens the same mod menu as L3+R3.
3. The controller is unchanged: every pad combination fires exactly as in the last
   clean launch, and the self-test count is not lower.
4. With the keyboard unplugged, `[kbm] SUM` shows events=0 and fails=0.
5. The one-time `[kbm] engine-keys supported=1` line appears.

**Rollback.** Last clean build.

## 7. Prototype and host tests (Mac-only results, this session)

Run `prototype/run.sh` (about 1 minute). Full log: `prototype/out/RUN-LOG.txt`. The
real R16 input code is compiled in read-only, from source pins: `Handler.cpp`
(worker-pad-coexist), `KeyDef.cpp/.hpp`, and `PS5PadSource.cpp` (l22 candidate) with
a fake pad backend.

| Step | Result |
|---|---|
| Table gate (real enum + KeyDef spellings + SDL HID + public EKeys) | PASS, 730 items checked, 0 errors |
| Our 3 sources at `-Wall -Wextra -Wpedantic -Werror` | clean |
| Unit + integration (ASan+UBSan), 21 tests | PASS, 1166 checks |
| Two real threads under ThreadSanitizer | PASS: 10000 presses shown, 10000 delivered, 0 dropped, nothing after shutdown |
| Lua probe host test (`zzkbprobe`) | PASS, 193 checks (size 2193 < 4096, ASCII) |
| Lua bind-check host test (`zzkbbind`) | PASS, 9 checks |

**Notable tests.**
- `controller_events_identical_with_and_without_keyboard_route`: the same pad script
  produces identical callbacks with and without the keyboard source, using the real
  PS5PadSource.
- `deferred_attach_pad_held_by_game`: the pad gives ALREADY_OPENED at start, startup
  does not abort, binds register, and keys still work.
- `pad_chord_and_real_key_same_poll_fire_once`.
- `unsupported_engine_fails_closed_pad_continues`.
- `held_at_start_is_not_a_press`.
- `over_budget_slows_down_and_caps`.
- `queue_overflow_drops_counted_no_crash`.
- `broken_child_never_silences_the_others`.

## 8. Risks (option C)

| Risk | Effect | Mitigation |
|---|---|---|
| The engine on this game does not receive the keyboard | No keyboard binds | K1 decides before any build. Pad combinations unaffected. Plan B = option A diagnostic. |
| Menus with UI-only input | Keys missing in those screens | Documented limit; pad combinations still work. |
| Cost per frame (stutter history) | Hitches | Only bound keys; 0.25 ms budget with automatic slowdown; bounded logs. |
| UFunction call from the Tick pump at a bad moment | Crash | Same thread and path as the PROVEN `ExecuteInGameThread`. Reflection checks before the first call; fail closed. |
| Key held across a level load | Phantom press | Baseline reset on controller change (tested). |
| Enhanced Input (UE5) changes key-state tracking | No edges | K1 measures it on a UE 5.6 game. |
| PS5 system keeps some keys | Those keys never fire | Documented; K1 lists them. |

## 9. Next steps

1. **Integrator:** decide K1, either its own Lua-only launch or riding along with the
   next launch. Follow `LAUNCH-CARD.md`: copy `probe/zzkbprobe/` and add its two loadlist
   rows with the exact sizes (main.lua 2193, enabled.txt 0).
2. **After K1 PASS (Mac):** write the real `KeyStateReader` and the sampler wiring as
   a staged runtime patch. Port `prototype/src/*` unchanged and add the reader and
   the `NativePadInput.cpp` composition change. Then host tests, ONE independent
   reviewer, gates (import count must stay 207), one build, launch K2.
3. **After K1 K-PARTIAL:** write the option A **diagnostic** only. It records
   whether `sceImeKeyboardOpen` returns BUSY (the game owns the keyboard), and the
   raw key codes for 5 known keys. That gives a second source for the HID question.
   Nothing ships from it without a new decision.
4. **Later:** mouse pointer and position for the UE4SS GUI windows (#11), via
   `GetMousePosition`, on the same route.
5. Because the route is engine-level, Stellar Blade (UE4) needs nothing new. Its
   blind test covers it automatically.

## 10. Files (all in this folder)

- `DESIGN.md` (this file) and `LAUNCH-CARD.md` (K1 and K2 steps).
- `prototype/include/kbm/*.hpp`, `prototype/src/*.cpp`: the prototype (map, engine-keys
  sampler and source, merged source).
- `prototype/tests/`: `test_kbm.cpp`, `test_threads.cpp`, `check_table.py`,
  `test_probe.lua`, `test_bind.lua`, and `shims/DynamicOutput/Output.hpp` (log
  stand-in).
- `prototype/run.sh`: runs everything.
- `prototype/out/`: `RUN-LOG.txt`, `keymap.tsv` and the per-test outputs.
- `probe/zzkbprobe/`: the K1 Lua probe in deploy layout (`Scripts/main.lua`,
  `enabled.txt`). Its log guide is `probe/EXPECTED-MARKERS-zzkbprobe.txt`.
- `probe/zzkbbind/`: the K2 bind check, in deploy layout.
- `evidence/`: `decode_input_imports.py` and `townfall-input-imports.json` (F2).
- `refs/sdl-2.30.0/`: the public SDL header (data only) and its `SOURCE.txt`.
