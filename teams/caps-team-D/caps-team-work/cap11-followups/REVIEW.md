# Independent review: cap11 follow-ups (one reviewer agent, Mac-only, read-only)

Reviewed: PATCH/diffs/ALL.diff and PATCH/post at backend sha 471e4631 / host 6ec23d08 / core 691e36e0 / services 07279af6
(the pre-fix post-images). The reviewer's report follows verbatim. Disposition of every finding: FIXES.md.

---

**Verdict: PASS-WITH-FIXES.** No blockers. Two should-fix items, both about memory under rare conditions, are listed first. All six claims check out against the code and the pinned ImGui 1.92.1 sources.

Hard rules: no game or mod names in the code. 0 new undefined symbols. The import check is sound: the baseline unit is compiled with `--baseline-plain`, so without the patch overlay, and the delta is taken against both that unit and R15e's runtime-l26-A.o. Field list is identical (49 fields). no-detour and fs_throw both pass on the 4 changed files.

Mac-only reviewer check: I wrote a scratch host test (scratchpad/review/rv.cpp) using the real GuiHost and ImGui 1.92.1, built with ASan/UBSan, one compile at a time. It runs scale 1 → 2 → 1.333 → 2, with PushFont(nullptr,16/40), SetWindowFontScale(0.8) and non-Latin text in a tab. Result: 0 IM_ASSERTs and a clean ASan/UBSan run. The old texture is freed after one frame (TexList=1).

## Findings

**1. SHOULD-FIX — NativeGuiEngineBackend.inc:483-543 (build_surfaces).** A build that fails after `make_crt` (line 512) leaves a brand-new render target behind on every retry.
- At 4K the target is now 2200x1728. That is about 30 MB at the UTextureRenderTarget2D default format (RGBA16F), 4x the old size.
- Retries come every 2 s, but UE frees unreferenced objects only at the next GC (about 60 s by default).
- If one later step fails every time on a new game while the window is open, up to about 30 targets (about 0.9 GB) can build up.
- Fix:
  - (a) Reuse the target: `if (!(valid(m_target) && m_rt_w == rt_w && m_rt_h == rt_h)) { m_target = make_crt(rt_w, rt_h); m_rt_w = rt_w; m_rt_h = rt_h; }`, setting m_rt_w/h right after creation instead of at the end.
  - (b) After 3 part-way failures in a row, stop retrying until the next open or player change, with one warning (fail closed).

**2. SHOULD-FIX — NativeGuiHost.hpp:557-561 (bake_font preload loop) and inc:639 (create_atlas).** The preload is unbounded: every codepoint of every source of the default font is rasterised at size x density.
- With the stock font (224 glyphs) this is fine: atlas 512x128 → 512x512 at 4K, runs 1734 → 18343.
- Before the patch, a mod that merged a large range (for example CJK) into the default font in on_ui_init never had those glyphs rasterised, because the atlas was never rebuilt after start().
- Now the first open rasterises them at 40 px, plus 13@1 and 13@2 through ImFontAtlasBuildMain's legacy preload. ChineseFull is about 20k glyphs x about 1600 texels ≈ 34M texels, so the atlas can reach 8192². create_atlas then allocates w*h*4 (up to 256 MB) plus alpha, runs and PNG on the game thread. That means a multi-second hitch, or running out of memory.
- Fix: count the codepoints in `font->Sources` first. If the count is over a bound (for example 1024 codepoints), or the projected texels are over about 4M, skip the extra bake, log one warning and keep the old behaviour. Also have create_atlas return 0 when w*h > 4096² (GUI off for the session = fail closed).

**3. NIT — NativeGuiHost.hpp:548 vs 564.** `CurrentRasterizerDensity` is set before `GetTexDataAsAlpha8`. As a result BuildMain's legacy preload (imgui_draw.cpp:3380, 3426 `GetFontBaked(LegacySize)` → density -1 → CurrentRasterizerDensity, 5294-5295) adds an unused 13@density bake for each density.
- My run: bakes 20@2, 13@2, 20@1.33, 13@1.33; atlas 512x128 → 512x512 → 512x1024; runs 8459 → 18343 → 23075.
- Fix: GetFontBaked already takes the density explicitly. Call GetTexDataAsAlpha8 first, then set CurrentRasterizerDensity (keep the restore-on-failure).

**4. NIT — inc:583-589 (retire_surfaces).** The brushes are cleared before the widget is collapsed. A UImage with a null resource paints a solid white box. If the SetVisibility call ever failed, a white panel would stay on screen.
- Fix: collapse first, and clear the brushes only if that call succeeded.
- For the launch card: retired widgets stay in the viewport, collapsed and holding no render target or atlas, until the next map change. That is one small widget tree per resolution or DPI change.

**5. NIT — NativeGuiHost.hpp:96-100.** The Backend contract still says set_gameplay_input_blocked runs "only after a successful prepare()". FU1 now calls it with `false` after a failed prepare. RuntimeGuiBackend handles this, because release_block re-checks m_blocked_pc.
- Fix: change the comment to "true only after a successful prepare(); false at any time".

**6. NIT — NativeModHostCore.hpp:180-181 (FrameGate::due).** The VMT pump's 4 ms floor is measured against the last run of *either* pump. If the VMT pump was silent for more than 250 ms (startup, or a map load while the marshal carrier keeps firing), a marshal run less than 4 ms before the next VMT call makes that VMT call skip. One ENGINE_TICK is then lost; before the patch, ENGINE_TICK was never gated.
- Fix: measure the VMT floor against the last VMT run only.

**7. NIT — evidence.** tests/output/patched/SUMMARY.txt records NativeGuiEngineBackend.inc sha c7207188. PATCH/post holds 471e4631, edited at 17:29:39, after that suite ran (the target compile at 17:31 and the gates at 17:45 did use 471e4631).
- I reran check_backend_structure.py (34/34) and api_usage_gui.py (28/28) read-only on 471e4631: both PASS.
- Rerun `run.sh patched` so the recorded input matches the file that ships.

**8. NIT — fractional DPI.** At scale 1.333 (1440p), ImGui places text on whole units, so glyph quads land on 1/3-pixel offsets (my run: 497 of 648 vertices off the pixel grid). Density is 1:1, but text is slightly soft. 4K (2.0) and 1080p (1.0) are exact. No fix needed; worth telling the user.

## Answers

**a) Font bake.** The approach is correct and safe.
- The atlas is locked only from NewFrame to EndFrame (imgui.cpp:8697-8698 sets it, 5880 clears it), so baking between frames is legal.
- `GetFontBaked(20,d)` creates the bake (imgui_draw.cpp:5310-5338). FindGlyph rasterises the glyphs, and each upload sets TexIsBuilt=false (2955). GetTexDataAsAlpha8 then rebuilds through Build → BuildMain (2969-2973, 3360-3382).
- Inside the frame, UpdateCurrentFontSize computes 13·1.5 = 19.5 → IM_ROUND → 20 and looks up density = CurrentRasterizerDensity. ImGui does not touch that value without RendererHasTextures (imgui.cpp:8838-8841), so the 20@density bake is an exact hit. This is confirmed by test and by my run.
- While locked, ImGui refuses to load glyphs (4417).
- SetTexID writes `TexRef._TexData`, which is the new texture (imgui.h:3614; BuildSetTexture 3938-3944, which also retargets the draw lists).
- The grown-out texture is marked WantDestroyNextFrame (3965-3970). Because the patch clears its TexID, UpdateNewFrame frees it on the next frame (2798-2818).
- The only reachable assert is "dynamic font size with a locked atlas" (5333). It needs a font with zero bakes, which is pre-existing (a mod font added after the last bake).
- Mods' own ImGui copies run the same GetFontBaked code. ImFont* pointers and LegacySize are untouched.

**b) Scaling.** Consistent, with no double scaling.
- ImGui emits vertices and ClipRect in DisplaySize units. DisplayFramebufferScale only feeds draw_data/viewport FramebufferScale (imgui.cpp:5679) and the nav-windowing move speed, which these pad keys never trigger.
- The batcher clips in units and multiplies positions once, at emit. RT pixels = units x scale.
- Minor: the AA fringe stays 1 unit (imgui.cpp:5324), so shape edges are 2 px soft at 4K; text is not affected.

**c) Engine side.** I found no crash path.
- Every raw pointer is re-checked against GUObjectArray before use.
- `SetBrushResourceObject(nullptr)` is accepted by UImage/FSlateBrush.
- The re-check costs 2 reflected calls every 120 frames.
- Logs are bounded: at most 8 refusals per session, one scale warning, one release line per block.
- The throttle is correct: 500 ms when there is no player, 2 s after a part-way failure, reset on success so a map change rebuilds at once.
- Open issues on this side: #1 and #4.

**d) Input.** The counter stays balanced on every path I traced: open, failed prepare (released once), rebuild on the same player (short-circuit at inc:745, no double increment), player change, map change (the dead player's counters go with it), failed rebuild, close, and atlas failure → set_open(false).

**e) FrameGate.** No regression apart from #6.
- One gate; tasks and ticks run once per engine frame. dt is the time since the last frame that ran (it was the time since the last pump call). The first dt is 0, and the GUI clamps it to 1/60. ENGINE_TICK is still delivered only by the VMT pump.
- pump_seen and vmt_seen are still set on every call, so HookAdmission is unchanged.
- Mod stop waits on in_flight, not on queued tasks.
- A nested pump call is already blocked by NativeLuaActionDispatchScope (LuaMod.cpp:7172/7273).
- For the launch card: NativeLifecycleProbe's "[NPROBE] game-thread tick n=1800" line will now appear about every 30 s at 60 fps. HARVEST-cap09-launch-F shows about 1800 per second before the patch.

**f) Tests.**
- The texel-ratio, FU1 and FU5 tests are meaningful.
- Gaps:
  - The TexID-clearing loop is never exercised with a TexID already set. The first bake happens before the first SetTexID, and the later rebakes don't grow the atlas. Add a 1 → 2 case (1080p then 4K); it passes in my run.
  - Pixel alignment is not asserted.
  - The FU5 handover after the VMT pump is silent for more than 250 ms is not tested.
  - No mod font or large range is tested (#2).
  - The engine-side FU1/FU3/throttle logic is checked only lexically; the FU1 host test uses a fake backend.
- Launch card should look for:
  - `[GUI] surfaces built panel=1100x864 rt=2200x1728 ... scale=2.000000`
  - `[GUI] font baked size=20 density=2.000000 atlas=512x512`
  - and none of: `[GUI] imgui assert`, `[GUI] refused`, `viewport scale unavailable`.
- Item 6 (include proof) is fine: 12 LIVE, 2 PATCH, 0 stale. The one "other" header (CppModModule.hpp) is the loader source the R15e build itself uses.
