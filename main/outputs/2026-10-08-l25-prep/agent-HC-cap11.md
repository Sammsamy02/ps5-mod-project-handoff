# agent-HC — CAP11 ON-SCREEN WINDOW RENDERER design (report)

2026-10-08, design-only packet (Mac on battery: reads + docs only; no
builds, no console, no network, no tree edits outside my two deliverables).
Deliverable written:
`outputs/2026-10-08-l25-prep/CAP11-OSD-DESIGN.md`.

## Context read first (as tasked)

claude2-menu-session-20261010.log (DRY-4/5/6 + the full Item Spawner menu
build + navigation keybinds; NUL-interleaved — read via strings),
TownfallItemSpawner main.lua:488-746 (the proven UMG construction path incl.
AddToViewport(9999), canvas slot setters, input lock), FS API-PARITY-TABLE,
GL PAK-RUNTIME-MOUNT-DESIGN (present-hook boundary), CLAUDE-CAPABILITY-
STATUS (8-row table + #3 hook-kind inventory), GB ueselftest report, GY
CARRIER-DRIVE (sustained-driver question), keymap/MAPPING.md (chord
schema/containment law), PS5PadSource.cpp + pinned SDL_ps5joystick.h,
upstream vendored UE4SS GUI module + CppUserModBase/GUITab + the GUI-tab
docs guide, mod-ladder STATIC-SCAN (MiniMap/ModMenu UMG call census),
OTHER-ENGINES-RESEARCH (REFramework/BepInEx), CAP10-CSHARP-DESIGN.

## Recommended path (one paragraph)

**(a) ENGINE-UI TOOLKIT for v1**: an immediate-mode-flavored Lua toolkit
(BeginWindow/Text/Button/EndWindow diffed into a retained UMG tree) built
entirely on the console-proven reflected surface — widgetLib:Create [PROVEN
DRY-4], StaticConstructObject of UserWidget/WidgetTree/CanvasPanel/Border/
VerticalBox/TextBlock [PROVEN], SetText(FText) [PROVEN DRY-6], canvas slot
SetAnchors/SetPosition/SetSize [PROVEN-aggregate — the spawner menu rendered
at intended geometry], AddToViewport/RemoveFromParent + input lock [PROVEN].
v1 toolkit law: "Button" = TextBlock row + Border highlight (UButton style-
asset risk deferred, marked). Input = BOTH focus models: list-nav (proven
today) + a virtual stick cursor whose axes ALREADY EXIST in the
console-proven ScePad read (PS5_PadData.leftStick/rightStick are read-but-
discarded by PS5PadSource::poll — runtime change, no new console capability)
with toolkit-owned hit-testing as the guaranteed floor and an engine-mouse
compat layer (bShowMouseCursor + SetMouseLocation + InputKey, all reflected,
existence to probe) for unmodified PC UMG mods. Text entry: EditableTextBox
via the same construct path with the ENGINE invoking the PS5 system IME
(existence + plugin-compiled-in are the honest unknowns, S3 tests), falling
back to a self-drawn soft keyboard (all primitives proven). **(b) ImGui
draw-list → engine-texture translation is the designated fidelity upgrade**
(needed for ImGui-native code: UE4SS C++ GUI tabs post-#10, REFramework UI);
its honest unknowns: PS5 GNM dynamic-texture CPU-upload semantics + one new
pinned native seam — wrong for v1, right for v2. **(c) homebrew
compositor/video-out: declined-by-design** — no public layer API while the
game owns the video out; upstream's DX11/GLFW GUI backends can't exist on
PS5 (and the port composes no GUI rendering unit today — verified against
the l24 LINK-COMMAND.json and typescan-r4 candidate tree).

## The ONE next capability to prove

**The live-update family: per-row highlight updates (SetVisibility /
SetColorAndOpacity / SetRenderOpacity — BOUND-UNTESTED, first receipted in
S2) plus the game-thread loop (LoopInGameThreadWithDelay /
ExecuteInGameThreadAfterFrames — FS verdict 3, probe P7 = GY's sustained-
driver question).** Event-driven menus work today on the proven carrier pump
+ LoopAsync; that family is what turns them into live windows. Everything
else in v1 is already console-proven or a one-line existence probe (S0:
UMG.Button/Image/EditableTextBox/ScrollBox; SetMouseLocation/InputKey).

## Notable honest findings

- Upstream vendored UE4SS 3.0.1 has NO Lua ImGui binding (grep-verified) —
  real Lua GUI mods build UMG via reflected calls (exactly our route (a));
  so Lua-ecosystem compatibility is about INPUT (cursor/keybinds), not
  render API. The "bound GUI surface" in FS's table is only the dump
  globals' backends.
- The DRY runs' "no-viewport" marker was the dry-run ladder's own stop
  point; the spawner's real path DID AddToViewport and the menu was visible
  and navigated (F7 6/6) — menu-up is the anchor receipt for (a).
- No present-time hook dependency: UMG ticks itself once in the viewport
  tree; per GL's boundary and the #3 inventory, v1 needs no instruction
  patching and no new pointer hooks beyond the proven carriers.

Deliverables: CAP11-OSD-DESIGN.md (§1 render paths a/b/c + recommendation,
§2 input cursor+keyboard, §3 mod-facing API + shim, §4 dependency table vs
caps 1-8 (+ #10/GY cross-refs), §5 S0-S3 console ladder with 3-restart law +
screenshot law, §6 unlocks) and this report. Read-only everywhere else.
