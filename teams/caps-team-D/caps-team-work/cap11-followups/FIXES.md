# Fixes folded in after the independent review (REVIEW.md: PASS-WITH-FIXES, 0 blockers)

Post-images after the fixes: NativeGuiHost.hpp `7ee5124f`, NativeModHostCore.hpp `691e36e0` (unchanged by the fixes),
NativeGuiEngineBackend.inc `87167192`, NativeModEngineServices.inc `07279af6` (unchanged by the fixes).
All evidence in tests/output/ was produced on these exact files (see tests/output/patched/inputs.log and
tests/output/target/stage.txt).

| # | Severity | Finding | Done |
|---|---|---|---|
| 1 | SHOULD-FIX | A build that fails after the render target is made leaves a new 2200x1728 target behind on every 2 s retry (up to ~0.9 GB before GC). | **Fixed.** `build_surfaces` is now a thin wrapper over `build_steps`: (a) a retry after a part-way failure reuses the render target it already made (same size); a fresh build (first open, map change, layout change) still makes a new one as R15e did; (b) after 3 part-way failures in a row the GUI stops trying (fail closed: nothing shown, no input block), one warning `[GUI] surfaces could not be built 3 times in a row; ...`, fresh tries on the next open or a new player. Lexical checks in check_backend_structure.py (40 items). |
| 2 | SHOULD-FIX | The sharp-font preload is unbounded (a mod merging CJK into the default font could make an 8192² atlas and a 256 MB allocation on the game thread). | **Fixed.** `bake_font` counts every codepoint of every font source in the atlas and estimates the texels; above `Config::max_bake_codepoints` (1024; stock font 224) or `max_bake_texels` (4 M) the extra bake is skipped with one warning and the old (R15e) text path stays. The host refuses to upload an atlas above `max_atlas_texels` (4096²) and turns the GUI off for the session (fail closed, before runs/RGBA/PNG are built); `create_atlas` has the same cap. Host-tested: skip path (warning names the bound, GUI still works, old 3.08 ratio) and cap path (nothing built or uploaded, input not blocked). |
| 3 | NIT | `CurrentRasterizerDensity` set before `GetTexDataAsAlpha8`, so a grow-triggered rebuild adds an unused 13 px bake per density. | **Fixed.** `GetFontBaked(size, density)` takes the density explicitly; the font's current density is switched only after the rebuild. Host test: no `13@2` bake (bakes after a 4K open: `13@1 20@2`). |
| 4 | NIT | `retire_surfaces` cleared the brushes before collapsing the widget (an image with no resource paints white). | **Fixed.** Collapse first; clear the two brushes only if the collapse call was made. |
| 5 | NIT | Backend contract comment still said the input call happens only after a successful prepare(). | **Fixed.** "blocked=true only after a successful prepare(); blocked=false at any time". |
| 6 | NIT | FrameGate's 4 ms VMT floor is measured against the last run of either pump: at a hand-back after >250 ms of VMT silence, one ENGINE_TICK can be skipped. | **Not changed, on purpose.** The floor rule is Team D's console-proven F5 design and is pinned by an existing cap09 test ("VMT pump keeps a 4 ms floor": a VMT call 2 ms after a marshal frame is skipped). Measuring it against VMT runs only would allow two frames within a few ms at every hand-back. The loss is at most one ENGINE_TICK when the VMT pump comes back after a map load. New host test: over a 1 s VMT silence, all 62 ENGINE_TICKs delivered, 95 marshal-driven frames in the gap, none doubled. Revisit if a mod ever needs every ENGINE_TICK across map loads. |
| 7 | NIT | Recorded test inputs were older than the shipped .inc. | **Fixed.** Everything was re-run on the final files: host suites (live 13/13, patched 14/14), target compile, gates. |
| 8 | NIT | At a fractional scale (1.333 at 1440p) glyphs land on 1/3-pixel offsets: density is 1:1 but text is slightly soft. 4K (2.0) and 1080p (1.0) are exact. | **Documented** in NOTES.md (no fix needed). Also documented: the default font has a designed +1 px per 13 px vertical glyph offset, so text sits a constant 0.08 px off the vertical grid (not visible). |

Test gaps the reviewer listed, now covered in tests/test_followups.cpp (49 checks):
- 1080p then 4K: the grown atlas frees the replaced texture that already carried our texture id (TexList = 1).
- Pixel alignment at scale 2: every glyph quad lands on whole pixels horizontally (x 1.0000).
- FU5 hand-over after the VMT pump is quiet for 1 s.
- Bake bound and atlas cap (review #2).
- Still only lexical for the engine-side code (the .inc needs the PS5 platform profile): FU1 guard, failure limit, render-target reuse, layout re-check, retire order. The target compile proves it builds; the console launch proves it runs.
