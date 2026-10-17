# CAP11 — ON-SCREEN WINDOW RENDERER (PS5 UE4SS port, design only)

Agent HC (ZCode), 2026-10-08 l25-prep packet. User-added capability **#11**
(priority FIRST among the new three, after B->C->A): make mod windows that use
ImGui and similar systems (UE4SS GUI, REFramework UI, BepInEx config windows)
appear on PS5 — drawing through **the game engine's own rendering or public
homebrew interfaces only** — with controller-driven cursor/navigation and the
PS5 on-screen keyboard for text entry.

**DESIGN ONLY.** Mac on battery: no builds, no console contact, no network, no
code to tree, no mods edited. Nothing here authorizes a console action; every
console step below is a proposal behind Claude review + explicit user OK under
the permanent PS5 rules (AGENTS.md) and the 3-restart law.

Evidence labels (per task):
- **[PROVEN]** — console receipt in this worktree (path cited).
- **[BOUND-UNTESTED]** — bound in the composed runtime, never console-fired
  (API-PARITY-TABLE.md vocabulary).
- **[TRAINING-KNOWLEDGE]** — from my training data; must be pinned from a
  primary source before code depends on it.
- **[INFERRED]** — my reasoning on local evidence.

---

## 0. The ground we stand on (one paragraph)

The engine's own UI stack already works through reflected calls on this
console, end to end. In the menu-up session
(`outputs/2026-10-08-l25-prep/evidence/claude2-menu-session-20261010.log`,
NUL-interleaved — `strings` it) the Item Spawner mod built a REAL in-game UMG
menu purely from Lua: `widgetLib:Create(pc, UserWidget, pc)` PASS (DRY-4),
`StaticConstructObject(TextBlock)` PASS (DRY-5, and ~30 more `[l22] widget/
object constructed` lines for WidgetTree/CanvasPanel/Border/VerticalBox/
TextBlock during the real menu build), `SetText(FText("test"))` PASS (DRY-6,
`[LFB-M] ftext-fwd ok=1`), then the full menu ("TOWNFALL : ITEM SPAWNER",
sections, rows) **appeared on screen** and was navigated by keybind callbacks
(key=37/38/39/40/13/118) — `CLAUDE-MENU-UP-SPAWN-BLOCKED.md` §1. All of it
dispatches through the port's reflected UFunction call path
(`calling_context->ProcessEvent(func, dynamic_unreal_function_data.data())`,
candidate `src/LuaUObject.cpp:365`; the task's LuaUObject.cpp:2413 cite is the
coherent-port member of the same dispatch family — API-PARITY-TABLE §2a).
**A mod GUI already exists on PS5.** Capability #11 is about generalizing it:
a reusable toolkit, controller cursor/keyboard input, and a compatibility
layer so existing PC GUI-mod code runs unmodified.

---

## 1. RENDER PATH OPTIONS

### 1(a) ENGINE-UI TOOLKIT — a game-agnostic ImGui-like layer ON TOP of UMG/Slate — **RECOMMENDED for v1**

**Shape.** A Lua-implemented retained-mode widget toolkit with an
immediate-mode-flavored API (shaped like Dear ImGui so mod authors feel at
home, but diffed into a retained UMG tree):

```
-- per "frame" (or per input event; see driver note):
OSD.BeginWindow("MyMod", x, y, w, h)     -- reuses or creates one UserWidget+CanvasPanel+Border frame
OSD.Text("Hello")                        -- TextBlock :SetText (create once, update after)
if OSD.Button("Spawn Axe") then ... end  -- text row + Border highlight + focus index (NOT UButton; below)
OSD.Separator(); OSD.TextWrapped(...)    -- TextBlock rows
OSD.EndWindow()
```

`Begin/End` runs inside `ExecuteInGameThread`; the toolkit diffs the issued
widget list against its retained tree — new widgets are
`StaticConstructObject`'d once and parented with `AddChild` + slot layout;
existing widgets get `SetText`/`SetVisibility`/`SetRenderOpacity`/slot
`SetPosition` updates; removed widgets get `RemoveFromParent`. This is
exactly the spawner's `buildUI`/`refreshText` pattern
(`.../TownfallItemSpawner/Scripts/main.lua:488-633`) generalized.

**Proven primitive inventory (each console-receipted, spawner session):**

| Primitive | Call | Status |
|---|---|---|
| Class/module resolution | `StaticFindObject("/Script/UMG.Default__WidgetBlueprintLibrary")`, `/Script/UMG.UserWidget` | [PROVEN] claude2 log [ST] sfo lines + spawner :499/:506 |
| Widget factory | `widgetLib:Create(pc, userWidgetClass, pc)` | [PROVEN] DRY-4 |
| Fallback factory | `StaticConstructObject(class, outer, FName(...))` | [PROVEN] DRY-5 + ~30 `[l22]` lines |
| Tree assembly | `WidgetTree` construct + `root.WidgetTree=` + `tree.RootWidget=` property writes | [PROVEN] (menu appeared; writes rode the successful buildUI pcall, spawner :524-528) |
| Layout containers | CanvasPanel / Border(+SetContent) / VerticalBox + `AddChild` | [PROVEN] aggregate (menu rendered with intended structure) |
| Canvas slot geometry | `SetAnchors/SetAlignment/SetPosition/SetSize` on the AddChild slot | [PROVEN-aggregate] menu rendered at intended top-right 570x500 geometry; no per-call oracle — S1 adds one |
| Text | `TextBlock:SetText(FText(...))`, `Font.Size`, `SetShadowColorAndOpacity`, `ShadowOffset`, `SetAutoWrapText` | [PROVEN] (DRY-6 + full menu text) |
| Colors | `Border:SetBrushColor`, text SetColorAndOpacity | [PROVEN-aggregate] (menu colors seen on screen) |
| Show/hide | `root:AddToViewport(9999)`, `SetVisibility(1)`, `RemoveFromParent` | [PROVEN] (menu opens/closes F7 6/6 — CLAUDE-MENU-UP §1) |
| Input lock while open | `pc:SetIgnoreLookInput/SetIgnoreMoveInput` | [PROVEN] spawner :446-452, menu coexisted with gameplay |
| Game-thread dispatch | `ExecuteInGameThread` queued→drained same-second in-world | [PROVEN] CARRIER-DRIVE §a/§b |
| Sustained scheduling | `LoopAsync` (mod async thread) | [PROVEN] 18s cadence, CARRIER-DRIVE |

**What needs one more reflected capability (not yet console-fired):**

1. **Per-row highlight state** — `SetVisibility`/`SetRenderOpacity`/
   `SetColorAndOpacity` per row. ModMenu/MiniMap use them ×90/×27
   (STATIC-SCAN.md :275) — [BOUND-UNTESTED] individually; same reflected-call
   machinery as proven SetText, low risk, verified in S1/S2.
2. **The live-update driver** — today's console proof updates UI only on
   keybind events (menu-granularity). Live-updating windows need the
   game-thread loop family: `LoopInGameThreadWithDelay` /
   `ExecuteInGameThreadAfterFrames` — [BOUND-UNTESTED], FS verdict #3, probe
   P7. Mitigation already proven: the R3 FuncPtr-marshal carriers ARE hooked
   to genuine per-frame engine events (`/Script/UMG.UserWidget:Tick` is one
   of the three armed bases — CARRIER-DRIVE) and drain the game-thread queue
   per fire, so event-driven UI (update-on-input) works TODAY; a menu-style
   mod needs no P7 at all.
3. **Widget classes beyond the proven seven** — Button, Image, ScrollBox,
   SizeBox, Spacer, HorizontalBox, ProgressBar, EditableTextBox: all stock
   `/Script/UMG` classes [TRAINING-KNOWLEDGE for UE 5.6 shipping builds];
   each needs a one-line `StaticFindObject` existence probe before use
   (S0/S1 marker, rides any already-planned console window).
4. **UButton caution [INFERRED + TRAINING-KNOWLEDGE]**: a
   `StaticConstructObject`'d UButton may render blank without a
   widget-style asset, and its click path needs input routing we don't own.
   **v1 toolkit law: "Button" = TextBlock row + Border highlight + our own
   activation logic** (the spawner's proven pattern). UButton is a later
   probe, not a dependency.

**Game-agnostic law honored**: engine base classes only (`/Script/UMG.*`,
`/Script/Engine.PlayerController`), the single per-title constant being the
modbridge root path (GB's ueselftest A1/A2 precedent,
`ueselftest/CALL-SHEET.md`). No game asset, no pak content, no font import —
engine default font renders TextBlocks.

**Why (a) wins for v1**: every primitive is already console-proven or one
cheap probe away; compositing, DPI scaling, resolution, HDR, and frame pacing
come free because the ENGINE draws it (UMG→Slate→GNM); it is game-agnostic;
it cannot corrupt the frame (no GPU/texture seam touched); failure modes are
the already-understood reflected-call failure modes (pcall-able, loggable).

### 1(b) IMGUI DRAW-LIST TRANSLATION — the fidelity upgrade path (v2)

Run Dear ImGui core **headless** (MIT; logic is portable C++ — the task's
"Mac-side logic" note means we author/validate the translation on the Mac,
then it rides our PRX), feed it our input state, then get its `ImDrawData`
(vertex/index buffers + per-command-list texture/clip rects) onto the screen
through the engine. Two candidate seams:

- **b1 — engine texture upload [the realistic form]**: raster ImGui's output
  into a CPU RGBA buffer, upload into a `UTexture2D` each frame at a pinned
  native engine call (`FUpdateTextureRegion2D`-family — [INFERRED]: such an
  upload path exists in stock UE [TRAINING-KNOWLEDGE], but the exact PS5
  GNM staging semantics — whether dynamic textures are CPU-writable or need
  the render-thread staging path — are **not locally pinned** and are the
  honest unknown), then display via `UImage:SetBrushResourceObject` +
  `AddChild` (ModMenu already calls SetBrushResourceObject ×7 —
  STATIC-SCAN :275 — [BOUND-UNTESTED]).
- **b2 — Slate draw elements (`FSlateDrawElement::MakeCustomVerts`)**: not a
  UFunction surface; needs native Slate paint code inside a custom widget
  class that cannot be fabricated at runtime through reflection. Dead for a
  reflected-only route; only viable if a compiled thunk ships in our PRX and
  registers a paintable widget through engine-native seams well beyond
  anything proven. [INFERRED] decline for v2 too.

**Risks / honest unknowns (why this is not v1)**: one new pinned-offset
native seam minimum (texture upload) with PS5-specific GNM staging unknowns;
per-frame upload bandwidth (a 1280x720 RGBA region ≈ 3.7 MB/frame — 30 Hz ≈
110 MB/s of copies unless dirty-rect partial uploads are wired); ImGui's own
font atlas must ship (content, though MIT-licensed); input must be
translated into ImGui's IO model (we own that — cursor/keys from §2).
**Payoff**: pixel-fidelity ImGui for C++ mods recompiled via #10's transpile
path, and it is exactly the seam a future REFramework port needs
(OTHER-ENGINES-RESEARCH §1c: "REFramework's ImGui renders through
D3D11/12/Vulkan present hooks; PS5 is GNM/sceVideoOut — no existing ImGui
backend", already pointing at "drive the GAME's own UI" as the mitigation).

### 1(c) HOMEBREW COMPOSITOR / VIDEO-OUT — **declined-by-design** (documented)

Public homebrew interfaces for pixels on PS5: `sceVideoOut` flip/registration
(belongs to the game process that owns the video out), and the system
compositor, which composites the game's output plus SYSTEM overlays only —
there is no public API for a third-party process to add a compositor layer
[TRAINING-KNOWLEDGE]. In-game overlay menus in the PS4/PS5 homebrew scene
work by interposing the game's own present/flip and drawing into the game's
framebuffer (GPU command insertion — GNM on PS5), or by being separate
homebrew apps entirely (system-card style), not by compositor admission
[TRAINING-KNOWLEDGE]. While a game runs: the game owns the video-out handle
and the GNM context; interposing GNM submission is deep, undocumented, and
outside "the game engine's own rendering or public homebrew interfaces".
**Verdict: does not apply while a game owns the video out; declined.**
(This is also why upstream UE4SS's own GUI backends — DX11 swapchain hook /
GLFW3+OpenGL3 external window, `src/GUI/DX11.cpp`, `src/GUI/GLFW3_OpenGL3.cpp`
— cannot be composed on PS5 at all; the port's composed runtime indeed has no
GUI rendering unit [LOCAL: l24 LINK-COMMAND.json has no GUI objects;
typescan-r4/candidate/src/ has no GUI dir].)

### 1(d) RECOMMENDATION

**(a) for v1; (b) as the fidelity upgrade path; (c) declined.** Reasons in
one line each: (a) is the only route whose whole primitive set is already
console-proven inside this project, is game-agnostic by construction, rides
the engine's own render pipe (free compositing/DPI/safety), and fails safe
through pcall-able reflected calls; (b) is the only route that can ever
satisfy ImGui-NATIVE code unmodified (UE4SS C++ GUI tabs, REFramework UI) but
buys a new native GPU-adjacent seam with honest unknowns — exactly the kind
of thing this project proves one rung at a time, post-v1; (c) has no public
interface while the game owns the output.

---

## 2. INPUT

### 2.1 Controller cursor + navigation (BOTH focus models — mods use either)

**Focus model A — list navigation (proven today).** UP/DOWN/LEFT/RIGHT/
RETURN keybinds drive a highlight index; activation is mod logic. This is
the spawner's model, console-proven through the full loop (pad chord →
`[l22] keybind callback entry key=38/40/13` → menu state changes;
zzdiag2-diag.jsonl `pad_l3r3_edge`→`pad_l3r3_f7_dispatched`). Keep as the
default: zero new surface.

**Focus model B — virtual cursor (the new work).** Mods built for mouse
hover/click (or ImGui ports) need a moving cursor with hit-testing. Design:

- **Axis source [BOUND-UNTESTED, high confidence]**: the pad provider
  already reads the full `PS5_PadData` — `leftStick {x,y}`, `rightStick
  {x,y}` (uint8_t), `analogButtons l2/r2`, touch[2] — from the pinned SDL
  header (`.../ue4ss-native-pad-provider-1/primary-sources/SDL/src/joystick/
  ps5/SDL_ps5joystick.h:58-102`); `PS5PadSource::poll` currently consumes
  only `buttons`+`timestamp` (`outputs/2026-10-08-l22-runtime/candidate/
  PS5PadSource.cpp:131-170`). The stick bytes are READ BUT DISCARDED today.
  Consuming them is a runtime-side change on the console-proven ScePad read
  — **no new console capability**; delivery to Lua (a polled
  `GetCursorDelta()`-style native or a pad-axis diagnostic/marker first) is
  the [BOUND-UNTESTED] half.
- **Analog→cursor mapping**: center = 127/128; deadzone 0.15 of half-range
  (≈19 units) with rescale-to-full (standard gamepad-cursor practice
  [TRAINING-KNOWLEDGE]); response curve exponent ≈2.2 for fine aim near
  center; velocity = normalized deflection × max_speed ≈ 1600 px/s
  (viewport-scaled via `GetViewportSize` — ModMenu already calls it ×2,
  STATIC-SCAN :287); clamp to viewport with edge margin. Right stick or
  d-pad can drive coarse jumps; optional stick-acceleration on hold.
- **Cursor rendering**: a small Border/Image widget moved per update via
  slot `SetPosition` (proven-aggregate family) — OR engine cursor (below).
- **Hit-testing + click, two routes**:
  1. **Toolkit-owned (v1, guaranteed floor)**: the toolkit knows every
     widget's canvas rect (it issued the geometry), so hover/activation is
     pure Lua rect math — cursor position vs. widget rects, CROSS =
     activate. No engine input state involved. Works for OUR toolkit's
     windows and any mod built on it.
  2. **Engine-mouse compat layer (for UNMODIFIED PC UMG mods)**:
     `pc.bShowMouseCursor = true` (property Set — [BOUND-UNTESTED], GB
     Phase G write-restore family), then drive the engine's own cursor with
     `APlayerController::SetMouseLocation` (BlueprintCallable in stock UE
     [TRAINING-KNOWLEDGE — verify by FindFunction probe]) so Slate's own
     hover/hit-testing runs, and synthesize clicks through
     `APlayerController::InputKey(LeftMouseButton, ...)` (BlueprintCallable
     in stock UE [TRAINING-KNOWLEDGE — same probe]). If either function is
     absent/compiled-out, degrade to route 1 and log. This is the
     translation layer promised in §3.
- **Chord budget**: menu open/close stays on the universal `L3+R3` row;
     cursor-mode toggle and any new binds must fit the 10-slot antichain
     ceiling + containment law (keymap/MAPPING.md §3) — TOUCH_PAD as the
     cursor/click modifier is the natural pick (free-button pool).

### 2.2 Text entry / the PS5 on-screen keyboard

Honest layered design — try the engine, fall back to our own keyboard:

1. **Engine text widget route (preferred v1)**: construct
   `/Script/UMG.EditableTextBox` via the proven construct path, `SetText`
   initial value, focus it (`UWidget::SetFocus` is BlueprintCallable in
   stock UE [TRAINING-KNOWLEDGE — probe]), poll `GetText()` per event/tick
   (no delegate needed — dynamic multicast delegates are NOT in the port's
   bound surface for Lua binding; polling avoids them entirely
   [LOCAL: API-PARITY-TABLE §2 — no delegate-bind member]). On consoles,
   stock UE shows the platform on-screen keyboard when a text field takes
   focus in keyboard-less input modes [TRAINING-KNOWLEDGE]; if Townfall's
   build carries that path, **the PS5 system IME dialog is invoked BY THE
   ENGINE** — the only system-UI route that satisfies "engine's own
   interfaces" cleanly. Honest unknowns: (i) EditableTextBox + friends must
   exist in this title's `/Script/UMG` pool — one StaticFindObject probe,
   [BOUND-UNTESTED]; (ii) the on-screen-keyboard plugin path must be
   compiled into this shipping build (console titles usually ship it for
   cert text-entry, but a game with no text fields can strip it
   [TRAINING-KNOWLEDGE]) — S3 is exactly this test; (iii) input-mode
   switching (`SetInputMode_GameAndUIEx` — ModMenu calls it, STATIC-SCAN
   :287, [BOUND-UNTESTED]) must cooperate.
2. **Own soft keyboard (guaranteed floor)**: if the engine route fails, the
   toolkit draws a QWERTY grid of TextBlock keys navigated by the cursor or
   d-pad (both focus models work — it's just our toolkit), assembling the
   string with `SetText` on a preview row, committing on RETURN. Engine
   never involved; ~zero new risk; every primitive proven. This is the
   v1 shipping answer if S3's engine route fails.
3. **Direct system IME from the payload (declined for v1)**: calling
   sceImeDialog ourselves — the dialog is a system-software service
   composited by the system UI [TRAINING-KNOWLEDGE]; a payload would need
   libSceIme resolved in the game process, correct dialog-ownership
   semantics against the running title, and it competes with the engine's
   own IME usage. Not a public-homebrew-interface-while-game-owns-output
   story, unproven, and unnecessary given routes 1-2. Revisit only if both
   engine and soft-keyboard routes are ever simultaneously insufficient.

---

## 3. THE MOD-FACING API (what PC GUI mods expect, and the shim)

**What the ecosystem actually calls (pinned from the vendored upstream +
local scans):**

- **Lua mods (the loadable class on PS5 today): the vendored UE4SS 3.0.1 has
  NO Lua ImGui binding** — grep of `src/` + `deps/` shows ImGui used only by
  the native C++ GUI module; zero `register_function` sites reference ImGui
  (LOCAL: vendored `ue4ss-unreal-native-build-1/source-dependencies/ue4ss/
  UE4SS/src/`). Real Lua "GUI mods" — TownfallItemSpawner, ModMenu v0.66,
  MiniMap — construct UMG directly via the reflected API (STATIC-SCAN §)
  exactly like §1(a). **So for the Lua ecosystem, the render path is already
  the engine's own UI; the port's "bound GUI surface" is only the dump
  globals routing to GUI::Dumpers backends (FS §1a) — there is no ImGui Lua
  surface to be compatible WITH.**
- **C++ mods**: `register_tab(name, render_fn)` + `UE4SS_ENABLE_IMGUI()` in
  `on_ui_init()` — the render callback calls ImGui directly against UE4SS's
  shared context (LOCAL: vendored `docs/guides/creating-gui-tabs-with-c++-
  mod.md`; `include/Mod/CppUserModBase.hpp:60`, `GUITab.hpp`). These are
  Windows DLLs — unloadable on PS5 regardless of GUI; they become relevant
  only via #10's recompile/transpile path, at which point their ImGui calls
  map onto route (b). Upstream's GUI render modes (ExternalThread /
  EngineTick / GameViewportClientTick — `src/UE4SSProgram.cpp:238-242`)
  confirm the "render inside the engine's tick" shape we mirror with UMG.
- **REFramework UI / BepInEx config windows**: ImGui-via-present-hooks and
  Unity IMGUI OnGUI respectively (LOCAL: OTHER-ENGINES-RESEARCH §1c, §2) —
  both arrive through future ports (#10 for BepInEx) and consume §1(b)/(a)
  respectively.

**The translation layer (lives in OUR runtime; mods are never edited):**

1. **Virtual-cursor service** (§2.1 route 2): makes an unmodified UMG mod's
   `bShowMouseCursor`/hover/click assumptions work with a stick-driven
   engine cursor. Port-only superset, additive, like the three `__tostring`
   supersets (FS §3).
2. **Keybind remap**: existing PS5Pad.ini chord→key layer (MAPPING.md) —
   already the shipping answer; unchanged.
3. **`OSD` Lua library (port-only, optional)**: the §1(a) toolkit exposed as
   a resident Lua module under the mod root (UEHelpers-style require), so
   console-native mod menus are ~20 lines instead of the spawner's ~200
   bespoke UI lines. Explicitly a superset — no upstream parity claim (there
   is no upstream object). When route (b) lands, an ImGui-shaped facade over
   the same call sites can be added for code shaped like community
   ImGui-Lua bindings.
4. **Soft-keyboard service** (§2.2 route 2) as a shared OSD component.

---

## 4. DEPENDENCIES ON CAPABILITIES 1-8 (explicit)

| Cap | Needed? | How |
|---|---|---|
| #1 kernel reads | NO | Nothing here touches kernel memory. |
| #2 scanner | NO for (a) | (a) is pure reflected surface. (b) later needs per-version pins for the texture-upload native call → #2/#3 pin-table discipline then. |
| #3 code patching | NO for v1 (and lawfully so) | Present-time rendering is NOT relied on: UMG widgets tick/draw themselves once inside the viewport tree (engine-owned tick). The per-frame UPDATE driver is the R3 FuncPtr-marshal carrier pump (console-proven) + mod async thread (proven) — per GL's present-hook boundary law and the instruction-detour soft-refusals (CLAUDE-CAPABILITY-STATUS #3). Live-update loops want P7 (`LoopInGameThreadWithDelay`, BOUND-UNTESTED), not new hooks. |
| #4 early loading | Already satisfied | Mod load is console-proven; UI construction gates on world/PC availability (DRY-1 `pcs` pattern, spawner :493-497). No milestone-relative proof needed for UI specifically. |
| #5 debugger | NO | (Crash symbolization unaffected.) |
| #6 controller | YES — userspace half | The pad seam is THE input source: chords console-proven; the virtual cursor needs the stick-axis bytes the provider already reads (§2.1) — runtime change, no new capability; kernel route stays declined-by-design. |
| #7 file overlay | NO | No pak content: engine default fonts/styles only. (Custom-font mods later → #7/#8 territory, out of scope.) |
| #8 asset conversion | NO | Nothing converted; no textures in v1. |
| #10 C# runtime (sibling) | LATER | BepInEx config windows (ConfigurationManager OnGUI) would ride #10 + this capability's translation surface. Cross-ref CAP10-CSHARP-DESIGN. |
| GY carrier question | CROSS-REF | Sustained per-frame UI updates = exactly CARRIER-DRIVE's open item: async thread is the state-independent sustained driver (proven, 250ms-granular cb= counter landed); carrier cadence per-frame-rate is unmeasured; P7 is the missing rung for tight loops. Menu-granularity UI needs none of it. |

---

## 5. FIRST CONSOLE TEST (ladder, pass/fail, laws)

All steps ride ONE Lua mod staged the GB/ueselftest way (own
`ue4ss/Mods/osdtest/Scripts/main.lua`, every file < 4096 B, loadlist rows,
arm on a chord inside the MAPPING.md antichain — e.g. `L1+R1 = F12`,
generated PS5Pad.ini + config-row re-pin per GB's round-4 procedure).
Nothing here needs a PRX change for S1/S2 list-nav; the stick-cursor half of
S2 and any native axis plumbing would ride the next PRX window.

**S0 (ride-along probes, zero risk, first):** marker lines only, pcall'd, in
the existing zzdiag/osdtest chassis — existence probes:
`StaticFindObject("/Script/UMG.Button" | ".Image" | ".EditableTextBox" |
".ScrollBox" | ".SizeBox")`, `FindFirstOf("PlayerController")` members
`SetMouseLocation`/`InputKey` presence via reflected call attempt or class
walk. PASS = markers logged; FAIL of any single probe only narrows the S1/S3
palette (does not block).

**S1 — game-agnostic test window (ueselftest-style).** On chord: DRY-style
ladder then build one window: UserWidget→WidgetTree→CanvasPanel→Border
frame→VerticalBox→ title + 3 "button" rows (TextBlock + Border highlight) +
status line; `AddToViewport(9999)`; input lock on. PASS = (i) `[OSD]` marker
set in UE4SS log (construct + per-call pcall results), (ii) **window visible
on screen** (operator screenshot → archive law), (iii) window closes on
chord rerun, gameplay resumes clean (no input leak). FAIL = any crash, no
visibility, or stuck input lock.

**S2 — navigation + activation.** (i) List-nav: mapped UP/DOWN rows move
highlight (`SetVisibility`/`SetColorAndOpacity` swap — the [BOUND-UNTESTED]
family gets its receipt here), RETURN activates (status-line `SetText`
round-trip logged). (ii) Cursor: left stick moves a cursor widget (native
axis feed or temporary keybind-driven delta if the PRX half isn't in the
window yet), hover highlights row under cursor, CROSS-equivalent activates.
PASS = logged round-trips for both models; screenshot of cursor-on-row
highlight. FAIL = highlight does not move/activate or wrong row activates.

**S3 — text entry round-trip.** If S0 found EditableTextBox: construct,
`SetText("seed")`, focus, engine on-screen keyboard appears (operator
observes system IME) or typed via soft keyboard if not; commit; `GetText()`
read back. PASS = logged `GetText() == entered`; screenshot of the keyboard
overlay (engine route) or soft keyboard (fallback route). FAIL = text lost,
crash on focus, or engine IME never appears AND soft keyboard falls back
(record which route shipped — the fallback route passing still PASSES S3,
with the engine route honestly marked failed-unproven).

**Laws**: 3-restart acceptance for each Sx (3/3 across separate boots, any
variance ⇒ STOP+report); screenshots archived per the session discipline;
klog to NEW files; one user-launched test per boot; no rapid-fire launches
(PS5 rules); no game file touched; rollback = remove mod from loadlist.

---

## 6. UNLOCKS (what this capability buys)

**Engines/ecosystems:**
- **UE4SS Lua GUI mods — formalized**: the class of mods the spawner/
  ModMenu/MiniMap represent (in-game menus, spawn lists, toggles, teleports,
  trainer panels) stops being bespoke per-mod UI code; the OSD toolkit +
  cursor service make the next one cheap and game-agnostic. ModMenu RUN-7
  and MiniMap RUN-6 (needs P7 loop + P5 struct-OUT — cross-ref FS verdicts
  3/5) become the first real beneficiaries.
- **UE4SS C++ GUI-tab mods** (post-#10 recompile): their ImGui calls map to
  route (b) — the only path that runs ImGui-native code unmodified.
- **REFramework UI, once that port exists**: praydog's present-hook ImGui is
  route (b)'s motivating consumer (OTHER-ENGINES §1c names the gap; this
  design is the local mitigation ladder).
- **BepInEx config windows via #10 later**: ConfigurationManager-style
  OnGUI windows translate onto (a)-shaped retained UI (or (b)).

**Mod types unlocked on console (v1, route (a) alone):** item/actor spawner
menus with searchable lists, teleport/warp panels, cheat toggles (god mode,
speed), hotkey rebinding UIs, inventory editors, weather/time-of-day
controllers, per-mod settings menus with text fields (names, quantities,
search filters) — i.e., the standard trainer/config-menu class of PC mods,
controller-operable, no keyboard required except where the mod itself needs
strings (§2.2).

---

## 7. The one-line summary

Route (a) — an immediate-mode-flavored Lua toolkit over the console-proven
UMG reflected surface — ships capability #11 with zero new engine seams; the
single next capability to PROVE is the **per-row highlight update family +
the game-thread loop (P7)** that turns event-updated menus into live
windows, with route (b) (ImGui→engine-texture translation) reserved as the
fidelity upgrade that later satisfies ImGui-native code unmodified.
