# Step 3 — plain words, recovery and remembered browsing

User benefit: players can understand what happened, recover with the controller, and return to their last game and filters without technical clutter.

Changed `web/app.js`, `index.html`, `style.css`; narrowed the installer UI's path detector to preserve legitimate Unicode names. Added the polish suite and retained-feature presentation expectations. Test evidence paths are configurable so later checks preserve earlier receipts. Exact ownership and test adaptations are in `../OWNERSHIP.md` and `legacy-note.md`.

- Games, console selection, selected downloads, Activity and Installed use plain words. IDs, hashes, reports and paths move to More info; original public names remain available there. Raw provider error bodies and credentials are not rendered.
- Every known Nexus/host error has a next action. Catalog retry stays with its list when another background check fails. A retry repeats the intended read; existing download admission still preserves request identity.
- Loading, empty and failed catalogs are distinct. Independent list requests cannot clear each other's loading state. Back and stale-response checks still protect the current selection.
- Last game, local filters and sort persist as bounded public preferences. A new page offers Continue. Session navigation keeps its existing behavior; saved browsing never grants permission to modify a console game. Blocked or malformed storage leaves browsing usable.
- Mod downloads include a readable overview and an official Nexus-page link. Own-account sign-in uses the configured official origin. Free-account approval and the missing return handoff are explained honestly. Local filters are explicitly limited to loaded latest mods.
- Controller access to the top bar is reliable. Polls preserve More info focus, including after underlying records change while a dialog is open. Unicode names and standalone slash separators are preserved.

Behavior changes are presentation and recovery only: “Download for analysis” becomes “Download and check”; “Details” becomes “More info”; console target controls become game controls. Existing single/basket downloads, partial retry, saved selection, account disconnect, inventory/paging and confirmed/read-only records remain. No new provider, installer or native console endpoint was added.

Fresh verification: service **38/38**; installer screens **37/37**; standard Gamepad d-pad/X/O **18/18** general + **10/10** text entry + **34/34** installer, at 1080p and 4K. The polish suite and retained-feature final receipts are alongside this note. Static-delivery **9/9** and pure native-asset **3/3** pass; actual bundled UI is **195,104 bytes** within the existing **262,144-byte** bound. `FINAL-CONTROLLER-RECEIPT.json` binds the controller checks to unchanged source hashes. `../FINAL-REVIEW.md` records independent source review.

Screenshots: `final-controller/` (all main screens and More info), `final-keyboard/`, `final-installer-controller/` (plan, progress, result, restore, per-game controls and all seven errors), `evidence/` (loading, retry, remembered browsing, Nexus, plain records and additional error/empty states). All screenshot files are 1920×1080; 4K behavior is exercised by the controller tests.

Real integration is still needed for the installer service, trusted downloaded-archive handles, actual on/off/order mutations, full Nexus search/images, cache reuse and the free-account NXM return callback. These are not represented as working by a preview. See `nexus-audit.md` and the handoff. The normal `make test-ui` remains blocked by the pre-existing dependency URL/verifier mismatch; no dependency pins or build output were changed.

No console access, uploads, launches, real account sign-in, real downloads, game/mod writes or Claude-checkout edits occurred.
