# UI work ownership — 2026-10-13

GPT works on branch `codex/modbridge-tv-ui` in `/WORKSPACE/.codex/worktrees/translation-core/modbridge-tv-ui-20261013`, based on `53e1e9bdc5a5788621c0b8096a73cc321f0c98a8`. The original UI files matched HEAD before isolation. This sparse worktree has a clean index; the task's app integration could not create/register a worktree for its projectless context, so an isolated Git worktree was used.

Claude's `/WORKSPACE/.codex/worktrees/translation-core/what` checkout is read-only to GPT. No runtime, native-host, installer, game, mod or console files are changed. Review and cherry-pick each completed step after resolving any later Claude changes; do not copy over a live checkout.

Step 1 ownership:
- Root: `web/index.html`, `web/style.css`, controller/focus/card-navigation sections of `web/app.js`, this note and step change note.
- Controller test worker: new `tests/ui_controller_tests.cjs` and `docs/ui/step-01/owned-controller-tests/` only.
- Read-only audits: existing UI/screens, installer seams and contract.

Screens: Games, game mod list, mod file list, selection basket, Activity, Installed and More info. Preserve existing account, title inventory/paging, direct mod lookup, analysis downloads and read-only service records. The first step changes presentation/navigation only. Browser gamepad simulation proves web behavior, not physical PS5 rendering or controller mapping.

Future slices will list their files before implementation. The UI uses the runtime team's four-call installer contract; broader management functions remain explicitly mock-only until agreed runtime support exists. No console access is permitted for this task.

Step 2 ownership and integration:
- Root: `web/index.html`, `web/app.js`, `web/style.css` integration and central-route wiring; existing browser fixtures' static asset allowlists only.
- Service worker: new `web/config.js`, `web/installer-service.js`, `web/installer-mock.js`, `tests/installer_service_tests.cjs`, service note.
- Installer UI worker: new `web/installer-ui.js`, `tests/ui_installer_tests.cjs`, UI note/screenshots.
- Controller worker: new `tests/ui_installer_controller_tests.cjs` and controller evidence.
- Static fixture allowlist updates: `tests/generic_selection_ui_tests.cjs`, `selection_restore_ui_tests.cjs`, `selection_basket_ui_tests.cjs`, `retained_downloads_ui_tests.cjs`, `service_records_ui_tests.cjs`, `ui_paging_tests.cjs`, `ui_controller_tests.cjs`, `ui_keyboard_tests.cjs`. Existing assertions stay intact in this integration step.
Screens: selected game's mods, PLAN, APPLY progress, RESULT, RESTORE confirmation, errors and More info. Existing legacy installed/service records remain visible and separate from explicit practice data.

Step 2 verification corrections: controller worker also owns `tests/ui_focus_race_tests.cjs` and the controller-only setup in `tests/generic_selection_ui_tests.cjs`, replacing brittle vertical-only navigation with actual directional reachability while preserving delayed-focus and request assertions.

Step 3 ownership (before implementation):
- Root: plain words/diagnostics, loading and preferences in `web/app.js`, accompanying `web/index.html` and scoped `web/style.css` presentation; copy-only expectations in legacy browser suites after the controller worker freezes its two test files.
- Polish test worker: new `tests/ui_polish_tests.cjs` and `docs/ui/step-03/evidence/` only; read-only Nexus audit note.
- Installer worker: final narrow review/fixes confined to its existing installer UI module/tests/note.
Screens: Games, game/mod/files, target picker, selection basket, Activity/saved downloads, Installed/legacy records, account/error banner and More info. Preserve public APIs and transport assertions. No search/image/backend endpoint is invented. Full Nexus search, image transport and NXM callbacks require the runtime/provider integration documented in the audit.

The static UI delivery adapters changed in step 1b (`companion/modbridge_host.py` static module allowlist and `native-host/web_assets.py` asset bundling). No native console server, runtime, game, mod, plugin or build output was changed. New installer JavaScript files are client adapters and mocks, not the real installer implementation.
