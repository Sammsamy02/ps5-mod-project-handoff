# Step 1b — controller text entry and UI asset delivery

User benefit: searching and looking up a mod no longer requires a keyboard, and the added controls are delivered by both app hosts.

Added `web/controller-keyboard.js` and its script/input activation/style wiring. The controller edits a draft; Done applies it and sends ordinary input/change events, while O or Cancel keeps the original value. Shift, Space, Backspace, Clear and input length limits work. Mouse and hardware-keyboard inputs keep their existing behavior. The on-screen board provides English letters, digits and the displayed URL punctuation; it can preserve/delete existing Unicode but does not enter every writing system.

The UI-only static block in `companion/modbridge_host.py` serves the named added scripts. `native-host/web_assets.py` bundles the scripts actually referenced by the index, in source order, into the existing app.js; the emitted index keeps one script. Native HTTP routes and the three C asset symbols remain unchanged. Source web files are never rewritten by generation. See `../asset-delivery-note.md` for exact scope and limits.

Verification:
- **10/10 controller-only Chromium tests**, at 1080p and 4K; missing-keyboard red proof preserved. `RESULTS.json`, `keyboard-final.log` and three exact1080 screenshots are in this folder.
- **9/9 new asset-delivery tests**, including actual loopback desktop HTTP delivery and native bundle execution/order/unchanged-source/refusal checks.
- **3/3 existing pure native asset tests** pass. No native/console compile or launch was used.
- Independent keyboard and asset-builder source reviews accepted. Keyboard screenshot visually checked; controls and focus fit the viewport.

No game, mod, loader, plugin or loadlist files are edited. No console requests were made. The existing normal-build dependency-metadata mismatch remains documented in Step1; no dependency pins were changed.
