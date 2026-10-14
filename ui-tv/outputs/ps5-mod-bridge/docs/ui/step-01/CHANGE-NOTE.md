# Step 1 — TV layout and controller navigation

User benefit: the app is readable from a sofa and the controller keeps a clear, usable focus while browsing or handling a slow response.

Changed `web/index.html`, `web/style.css` and navigation/focus/card portions of `web/app.js`; added `tests/ui_controller_tests.cjs`, `tests/ui_focus_race_tests.cjs` and this evidence folder. Work is isolated on `codex/modbridge-tv-ui`; Claude's checkout is unchanged.

- Text, controls and spacing scale together at 1920×1080 and 3840×2160; clear explicit controller focus highlight.
- Directional grid movement, enabled-action navigation, parent-card restoration on Back and recovery when controls are replaced/removed.
- Visible More info buttons on game/mod cards, plus controller scrolling through long details and return to the original trigger.
- Controller starts before network setup, so startup failure does not disable navigation.
- Fixed BrowserBack opening and immediately closing Details instead of navigating back.
- Slow mod/file responses no longer steal focus after the user moves to another control. Existing stale-request checks remain intact.

Preserved: browsing, local filtering/sorting, URL/domain lookup, account connection/disconnection, single/basket analysis downloads and retry identity, target inventory/paging, read-only installed/service records and existing shortcut support. No install behavior changed. On-screen text entry is the separate next step; physical PS5 mapping/rendering is not claimed.

Validation:
- Controller-only real Chromium suite: **18/18 pass**, across 1080p and 4K, using injected standard Gamepad d-pad + X/O. `owned-controller-tests/RESULTS.json` binds the tested assets.
- Delayed-response controller regressions: **2/2 pass**; both failed before the focus-ownership fix (`focus-race-red.log`).
- Existing `ui_tests.cjs`: pass before and after the change.
- Existing `ui_paging_tests.cjs`: **23/23 pass**.
- Independent source review: accepted after the late-response focus race was reproduced and fixed.
- `make test-ui` stops before compilation/UI tests because the existing dependency manifest contains local libarchive URLs and the existing verifier permits only commit-pinned HTTPS URLs. Dependency files are unchanged; this pre-existing build mismatch is not reported as a UI pass. Browser suites run independently with the bundled Node/Playwright and local Chrome. Additional retained-feature suites are included in the final integrated verification.

Screenshots: thirteen exact 1920×1080 PNGs in `owned-controller-tests/` cover Games, mods, files, game/mod/file More info, end of long details, selection basket, target picker, Activity, Installed, offline account and startup failure. Updated basket and target images were visually checked; no focused action is clipped.

Historical red logs and exhausted focus graphs are kept beside the final green receipt. No console calls, uploads, launches, runtime edits or game/mod file writes occurred.
