# cap11 NOTES — UE4SS GUI windows (mod tabs) on the PS5 (Mac-only work)

Status: **PARTIAL** — built on cap09's native mod host; GUI host, batcher,
atlas, PNG writer and math hooks implemented and host-tested (9 suites, all
PASS, `tests/output/`); the runtime backend is staged but never target-compiled
or run. Every claim is **Mac-only** unless it cites a console log line.

## 0. Fixer update (2026-10-13, see ../FIXES.md)

- **F1 fixed**: walking/looking is blocked only once the player controller exists (so the first open works) and
  unblocked only if a block was really applied (UE's ignore-input is a counter; an unmatched "unblock" could cancel
  a cutscene's block).
- **F2 fixed**: after a map change the surfaces are rebuilt AND the font atlas is uploaded again; the block is
  re-applied to the new player.
- First target compile (Mac-only) fixed 4 build problems the host tests could not see (SSE header, ImVec2
  operators, `TObjectPtr`, one `powf` import). 0 new imports for the whole #11 composition.
- Pad: only one free button pair (`L1+R2`) exists with the 7 current Lua mods; the #11 launch uses it for F10.

## 1. The 76 "gaps" of ChatGPT's coverage gate

ChatGPT's gate (re-run here on the reproduced inventory) refuses 76 Dear ImGui
functions used by EventViewerMod + KismetDebuggerMod. They are now covered by
**real Dear ImGui 1.92.1** (the same pinned files the runtime unit already
includes), linked in the host AND in an SDK-built mod module that shares the
host context and allocator (the upstream `UE4SS_ENABLE_IMGUI` model):

- `tests/output/COVERAGE-HOST.json`: **76/76 exercised across the module
  boundary** in `test_gui_sdk_mods` (counts per API from the probe's own log
  line), **0/76 console-proven** (pending the #11 launch).
- No ImGui assert on either side; the frame image produced through the CPU
  clipper equals a scissor reference raster (0.000% pixels differ).

What this does **not** do: make EventViewerMod / KismetDebuggerMod work. Their
GUI calls are covered, but they also need global ProcessEvent/ProcessInternal
hooks, GNatives writes and game-thread pausing, which #9 refuses (code detours /
no two-source values). No other real UE4SS GUI mod exists locally; the launch
uses the generic NativeGuiProbe.

## 2. How the window gets on screen (and why this route)

- One host-owned ImGui context (`NativeGuiHost.hpp`), created with the host
  allocator; error recovery on, recoverable errors go to an error callback
  (logged), `IM_ASSERT` never aborts (`mbm_imconfig.h`).
- Each mod tab runs on the game thread inside BeginTabItem/EndTabItem, inside a
  cap09 `CallScope` (counted for revoke→drain) and an ImGui error-recovery
  bracket (a mod that forgets End/Pop is repaired; host test proves it). A tab
  that faults disables its mod; other tabs keep rendering. A tab may
  unregister itself from inside its render call (no deadlock — the seam
  NATIVE-GUI-SEAM.md flagged).
- `ImDrawData` → `ImGuiCanvasBatcher.hpp`: ImGui's clip rectangles are applied
  on the CPU (Canvas K2_DrawTriangle has no scissor) — 120,000 random samples,
  0 mismatches.
- Output uses only reflected engine functions in the families the shipping
  Lua mods already use on this console (minimap: `CreateCanvasRenderTarget2D`,
  `BeginDraw/EndDrawCanvasToRenderTarget`, UMG Image `SetBrushResourceObject`,
  `AddToViewport`; console-proven context: `l25-prep/evidence/launch-l26-R11-launch14-20261013/ue4ss.decoded.log`
  line 885 `[l22] Lua mod started: TownfallMiniMap` + the user's "EVERYTHING IS
  PERFECT" for launch 14; earlier sessions' UMG notes are context only).
  New in this route: `K2_DrawTriangle` with a TArray of `CanvasUVTri`,
  `ClearRenderTarget2D`, `ImportBufferAsTexture2D`. All 55 names the backend
  resolves are present in the test game's own executable
  (`tests/output/engine_names_check.log`, source 1); the run-time reflection
  lookup is source 2 and fails closed.
- Font atlas: PNG (stored deflate, `StoredPng.hpp`, verified by an independent
  Python decoder) → `ImportBufferAsTexture2D` → real UTexture2D with alpha.
  Fallback: the atlas as 1,734 untextured runs drawn into a render target
  (`ImGuiAtlasRuns.hpp`, exact reconstruction proven).
- The panel is drawn **opaque** (window background alpha 1, render target
  cleared opaque) so the result does not depend on render-target alpha
  conventions; text is blended over it.
- `CanvasUVTri` element layout (UE5: double vectors) and every parameter are
  read from reflection once per surface build (cached; rebuilt after a map
  change, when the viewport widget is destroyed).
- Input: pad chords → UE4SS key events (existing pad mapping) → thread-safe
  queue → ImGui gamepad navigation press/release pairs on the game thread.
  While open: `SetIgnoreMoveInput/LookInput(true)` once the player exists; the matching `false` on close only if applied (tested, F1).
  Keys: F10 open/close, PAGE_UP/PAGE_DOWN tabs, d-pad + CROSS (pinned rows).
  Pad buttons are non-exclusive: the game also sees the d-pad (same as the
  spawner menu today).

## 3. Integration facts found (Mac-only)

- The `cppmod` unit already includes the retained `imgui.h` (via
  UE4SSProgram.hpp → GUI/GUI.hpp; unit dependency file), so the GUI config is
  injected by `#define IMGUI_USER_CONFIG` at the top of CppMod.cpp; ImGui is
  compiled into that unit only (unity block). The integrator must confirm with
  `nm` that no other object references `ImGui::` (none links ImGui today).
- New include-graph files must live under the l26 runtime folder (build driver
  check), hence the copied pinned ImGui .cpp/.h files.
- **Zero new imports**: Dear ImGui would add `acosf atan2f atof cosf logf
  powf sinf strncmp strncpy sqrtf`; `mbm_imgui_math.h` + the unity wrapper
  route them to already-imported `cos/acos/fmod/pow/strtod` or local code
  (accuracy: sqrt 2e-16 rel, log 2e-16, atan2 2.4e-7, ceil exact).

## 4. Open risks (first console contact answers them)

1. `K2_DrawTriangle` with an imported texture: the texture/alpha path is
   unverified on this engine build; fallback atlas route exists.
2. Cost while open: one ProcessEvent with ~2–5k triangles per frame (+5 small
   calls). Closed: no engine call at all. Measure `[PERF]` while open.
3. `sceKernelLoadStartModule` of mod PRXs (shared with #9, the #9 launch
   answers it first).
4. Text input (`InputText`) has no on-screen keyboard: text fields can be
   navigated, not typed into (USB keyboard keys are not routed in v1).
5. ImGui `Image()` with mod textures is skipped (counted): only the atlas
   texture can be drawn in v1.

## 5. User note (Leon textures / brightness)

Not a GUI issue; see cap09/NOTES.md §5 (points to pak materials/shaders, #8/#12).
