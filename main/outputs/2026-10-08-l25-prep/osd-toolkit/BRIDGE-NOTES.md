# BRIDGE-NOTES — OSD toolkit (host prototype) to console

Agent HL, 2026-10-10 (l25-prep). Companion to `osd-toolkit/toolkit.lua` +
`host-test.lua` (S1..S5 = HOST scenarios; the console ladder S0..S3 is
CAP11-OSD-DESIGN.md §5 — distinct numbering, kept separate on purpose).
Mac-local deliverable only: nothing here authorizes a console action.

## 1. The reflected calls the toolkit issues (host: recorded by the stub)

| # | Family (toolkit call site) | Console status |
|---|---|---|
| 1 | `StaticFindObject("/Script/UMG.{UserWidget,WidgetTree,CanvasPanel,Border,VerticalBox,TextBlock}")` — 6 calls, once at first emit | [PROVEN] claude2 menu-session `[ST] sfo` lines + spawner :499/:506 |
| 2 | `StaticConstructObject(class, outer, FName(name))` — every widget (window shell, rows, cursor) | [PROVEN] DRY-5 + ~30 `[l22] widget/object constructed` lines. `widgetLib:Create(pc, class, pc)` (DRY-4) is the spawner's primary factory; the toolkit binds the game-agnostic StaticConstructObject shape so it needs no per-mod widget lib. Exact arg tail (flags) re-pinned from spawner :516 at staging time. |
| 3 | Property writes `root.WidgetTree = tree` / `tree.RootWidget = canvas` | [PROVEN] (menu rendered; writes rode the successful buildUI pcall, spawner :524-528) |
| 4 | `canvas:AddChild(w)` → slot; `slot:SetAnchors/SetAlignment/SetPosition/SetSize` | [PROVEN-aggregate] intended 570x500 top-right geometry rendered; no per-call oracle — console S1 adds it. Arg shapes (FAnchors struct vs numbers) pinned from spawner source at staging. |
| 5 | `TextBlock:SetText(FText(...))` (+ `Border:SetContent` for row/frame nesting) | SetText [PROVEN] DRY-6 + full menu text; SetContent [PROVEN-aggregate] |
| 6 | `Border:SetBrushColor(r,g,b,a)` — row highlight + cursor quad | [PROVEN-aggregate] for static colors; **per-row highlight updates are the [BOUND-UNTESTED] family console S2(i) receipts** (the single most-uncertain emit the toolkit depends on) |
| 7 | `root:AddToViewport(9999)` / `root:RemoveFromParent()` — show/hide/park | [PROVEN] menu opens/closes F7 6/6 (CLAUDE-MENU-UP §1) |
| 8 | `pc:SetIgnoreLookInput(true)` / `pc:SetIgnoreMoveInput(true)` — input lock while any window is open | [PROVEN] spawner :446-452; menu coexisted with gameplay |

Plus one acquisition: `FindFirstOf("PlayerController")` [PROVEN, DRY-1 pcs
pattern]. The design's "~6 calls" core is #1-#6; #7/#8 ride the same
reflected dispatch path (`calling_context->ProcessEvent`,
candidate `src/LuaUObject.cpp:365`).

## 2. Game-thread law

Every emit rides `ExecuteInGameThread` (toolkit's `game_thread()`; the
design's "Begin/End runs inside ExecuteInGameThread"). Console evidence
(CARRIER-DRIVE): queued closures drain SAME-SECOND **in-world** via the
R3 FuncPtr-marshal carriers (UserWidget:Tick / Actor:ReceiveTick /
AnimInstance:BlueprintUpdateAnimation Func-swaps; 74 armed at world load).
In the MAIN MENU state there are ZERO drains — 4 closures sat undrained
until world load. Consequence for the toolkit: build/update windows
in-world or on input events; a menu-up-session window needs the
event-driven path, not a game-thread loop.

## 3. The tick dependency (GY cross-ref — CARRIER-DRIVE.md)

- **Sustained driver = the mod async thread** (`start_async_thread`;
  `process_delayed_actions` + 5ms loop, LuaMod.cpp:9417-9497). A
  `LoopAsync(250, ...)` observer fired at exactly its 18s cadence all
  session. This is what drives `OSD.Tick` on console.
- **LoopAsync inversion (the load-bearing law): a callback returning TRUE
  is ERASED after one fire** (remove_if result law; v10 6s T-cycle with
  `return true` fired once; 18s loop with `return false` fired all
  session). **The toolkit's Tick therefore ALWAYS returns false** — wired
  as `LoopAsync(iv, function() return OSD.Tick(iv) end)`; host S2 asserts
  the return value explicitly.
- **P7 not required for v1 menus**: the toolkit's dirty-gate means idle
  frames emit NOTHING and dispatch NOTHING (host S4: ExecuteInGameThread
  dispatch count stays 1 across a 100-frame soak; total recorded calls
  constant). So a coarse async cadence (100-250 ms) + keybind-event-driven
  input suffices for spawner-class menus — exactly the event-driven mode
  that is proven TODAY. Tight per-frame game-thread loops
  (`LoopInGameThreadWithDelay` / `ExecuteInGameThreadAfterFrames` = P7,
  [BOUND-UNTESTED], FS verdict #3) become interesting only for
  live-updating windows (MiniMap-class).
- EngineTick detour is dead in this build (compiled out under R3 marshal);
  the VMT Tick pump was refused (KI-1). Neither is a toolkit dependency.

## 4. Input feed

- **List-nav (focus model A)** rides the console-proven keybind path:
  pad chord → `[l22] keybind callback entry key=38/40/13` → toolkit queue
  `press` events (UP/DOWN/LEFT/RIGHT/RETURN). Zero new surface. Chord
  budget: menu open/close stays on the universal L3+R3 row; nav keys map
  inside the 10-slot antichain ceiling + containment law (keymap/MAPPING
  §3).
- **Virtual cursor (focus model B)** needs the stick axis bytes
  `PS5PadSource::poll` ALREADY READS BUT DISCARDS (`PS5_PadData`
  `leftStick {x,y}` uint8, SDL_ps5joystick.h:58-102;
  PS5PadSource.cpp:131-170). Consuming them is a runtime-side change on
  the console-proven ScePad read — no new capability; the Lua DELIVERY
  (polled `GetCursorDelta`-style native, or an axis diagnostic marker
  first) is the [BOUND-UNTESTED] half. Interim per design S2(ii):
  keybind-driven deltas pushing synthetic `axis` events into the queue —
  the toolkit is input-source-agnostic by design (it only sees the queue).
- Mapping constants implemented + host-tested: uint8 center 127/128 →
  normalized [-1,1]; radial deadzone 0.15 of half-range with
  rescale-to-full; response exponent 2.2; max speed 1600 px/s
  (viewport-scale via `GetViewportSize` family — ModMenu already calls it
  ×2, STATIC-SCAN :287); clamp to viewport with 2 px margin; hit-testing
  is pure-Lua rect math over toolkit-issued geometry (design 2.1 route 1,
  the guaranteed floor). The engine-mouse compat layer
  (`bShowMouseCursor` + `SetMouseLocation` + `InputKey`) remains route 2
  for UNMODIFIED PC UMG mods, existence-probed in console S0.

## 5. Console staging shape (later, not authorized here)

Resident Lua module under the mod root, UEHelpers-style `require` — the
port's native module searcher resolves scripts_path-relative names with a
loadlist row (GB's ueselftest `part2..part12` precedent). Console
admission law: every file < 4096 B — `toolkit.lua` is a 27 KB host
prototype and MUST be split GB-style before staging. The host test's
`fresh()` wiring (stub env → `toolkit.new(env, …)`) is exactly the mod
`main.lua` wiring shape (`local OSD = require("osd").new(_G or env, opts)`).

## 6. What this prototype does NOT prove (honesty)

The stub's chainable cells make every reflected call succeed: no
marshaling/struct-layout questions are exercised (FAnchors/FMargin arg
shapes, FText wide-string marshaling — the ET seam family, slot return
types, FName pool admission). Those stay with the console S0..S3 ladder
(CAP11 §5) under the 3-restart law. What it DOES prove, so console time
is spent only on engine seams: the retained-tree diff logic (id-path
keying, create-once/update-on-change accounting, park-and-reuse), the
close/reopen identity law, the focus and cursor state machines, input
queue ordering semantics, and the zero-emission idle frame.
