# Step 2 — review an install, then confirm

The player can review a readable plan, answer Yes/No questions, and confirm an install without technical details obscuring the decision.

Owned files: `web/installer-ui.js`, `tests/ui_installer_tests.cjs`, and this note/evidence folder. Root owns the app integration and styles; the service worker owns configuration, service adapter and mock. Claude's checkout and runtime files were not edited.

Changes:

- Added PLAN → explicit “Confirm and install” → RESULT, plus confirmed Restore. Cancel and Back discard a plan without applying it.
- Added plain errors and next actions for connection, Nexus sign-in, damaged download, unsupported mod type, refused install, running game and busy console. Technical details are behind More info. “Nothing was changed” requires explicit service evidence.
- Progress stays attached to its game when the player leaves the screen, and its focusable status region is reachable when they return. Estimates and percentages come from service status; the UI invents no completion time. Duplicate activation cannot submit another install. A confirmed result stays visible until acknowledged, even if the selected game is cleared or changes in the meantime. New archive offers are refused during RESULT; their caller can retry after acknowledgement.
- Added the per-game mod list, update notices, conflict explanation, session results and self-test. Preview controls demonstrate on/off and controller-friendly Move up/Move down. Real management controls remain unavailable because the shared four-call contract does not define those operations.
- The one-time setup message uses an owned browser preference. If storage is unavailable, it is remembered for the current page session.

The default sample game is fictional and visibly marked Preview. It changes no game or mod files. Observed games without capability information show “not available yet”; their capabilities are never invented. A supplied service is treated as real unless explicitly marked mock. Real service failures never fall back to preview success.

The service boundary remains exactly `plan`, `apply`, `restore`, `status`. PLAN identity, hash and answers are preserved through APPLY. Known running-game status blocks an operation; the service must also enforce this when it applies or restores. Optional extra status fields are not required by the real contract. A failed final status refresh preserves the confirmed result, clears stale mod records and offers Refresh. The UI publishes busy-state transitions so the app can keep its game picker attached to the accepted operation.

Verification: `tests/ui_installer_tests.cjs` passes **36/36** in Chrome against owned responses and the genuine mock/service modules, including a real-mode adapter with a changing request-token callback; see [results](test-results.json). Tab/Enter exercise the same focusable actions without a mouse. The app's separate controller suite tests Gamepad navigation. Neither suite proves physical PS5 rendering. These are isolated module screenshots using the shared stylesheet; rerun this suite after app styling changes to refresh them.

1080p screenshots: [game](screenshots/game-1080.png), [plan](screenshots/plan-1080.png), [progress](screenshots/progress-1080.png), [texture conversion](screenshots/converting-1080.png), [result](screenshots/result-1080.png), [restore](screenshots/restore-1080.png), [setup](screenshots/setup-1080.png), plus one image per required error in `screenshots/`.

No console access, uploads, launches or real external requests were made.
