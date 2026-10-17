# Independent final UI review — 2026-10-13

No important actionable defects remain from this read-only review. The subsequent sign-in recovery delta's P2 focus finding was reproduced and resolved as recorded below. This is a source-review result, not proof of real PS5 or installer-backend operation.

Reviewed `codex/modbridge-tv-ui` from base `53e1e9bdc5a5788621c0b8096a73cc321f0c98a8` through commits `4bba9985`, `70fd89e8`, `00f98dc7` and the frozen, uncommitted Step 3 candidate. Read the ownership record, Step 1/1b/2 change notes, Step 3 plan, Nexus audit and legacy-test note. No applicable `AGENTS.md` was present in the checkout or its ancestor directories.

The review covered the complete added UI/service/mock/keyboard modules and the app, host-static and asset-bundler changes, particularly:

- Stale catalog responses, loading/error ownership, retry identity, target invalidation and focus replacement after polling or closing More info.
- Bounded public catalog preferences and explicit continuation without deriving a console target from Nexus browsing.
- Player-facing rendering, escaped original details, official Nexus origin/page construction and truthful image/search/NXM limitations.
- The four installer calls, accepted plan/hash/answer capture, pending-operation game locks, unavailable capabilities and absence of real-to-mock success fallback.
- Explicit mock-only on/off/order controls, module order, existing CSP and the unchanged three-asset native interface.
- Legacy test adaptations: transport bodies, CSRF, stale/request identity, malformed-data refusal and focus invariants remain asserted; technical evidence moved into matching More info dialogs.

Independent verification on this frozen candidate:

- `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -p ui_asset_delivery_tests.py -v` — **9/9 passed**.
- `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s native-host/tests -p test_web_assets.py -v` — **3/3 passed**.
- Inspected the actual source bundle: exactly `index.html`, `app.js`, `style.css`, totaling **194,910 bytes**, below the existing 256 KiB limit. All eight source hashes match `step-03/frozen-assets-before.json`.

Full browser suites were left to the separate final-verification workers. Physical controller mapping, PS5 rendering, real installation/restoration, live Nexus authentication and download continuation remain unproven. The documented pre-existing local-libarchive manifest/verifier mismatch still prevents claiming a normal `make test-ui` pass.

Only this review note was written. No production or test files were edited; no console, provider account, external download, native compile, upload, launch or game/mod filesystem operation was performed.

## Sign-in recovery delta review

Confirmed that the subsequent production delta consists exactly of the four-line known-auth-error branch in `web/app.js:179`. It lowers a previously verified account to unavailable and exposes the existing official Sign in action. It does not clear or rewrite the basket, pending download intent, target reference or idempotency key, and it sends no request. The added controller regression retains the selected download, disables admission while unverified, and explicitly activates the official sign-in route.

**Resolved [P2]: repeated background auth failures replaced the focused Sign in action (`web/app.js:179–181`).** With a verified account followed by repeated `/api/jobs` `reconnect_required` replies, the first in-memory browser probe observed `sign-in` → `game:orchard` on the next poll, as jobs reads increased from 2 to 3 with zero POSTs. The root corrected the focus replacement scope at `web/app.js:121` to search the document when no modal is open. Existing `visibleControl` checks continue to exclude hidden views, disabled controls and inert content; an open modal retains its own scope. The same bounded probe then **passed**, preserving `sign-in` across the next poll (reads 2 → 3), with zero POSTs and page errors. This probe uses native focus setup to isolate polling behavior and is not claimed as a gamepad-only test. No production or test file was changed by the reviewer.

Byte comparison confirms that these are the only production changes since the initial review. Current source `web/app.js` SHA-256 is `324d255d890f66a91c5b2a865ba552000a4d21fabfcf5db6df88f49dd97090e1`; the other seven source hashes are unchanged. Independent static inspection produces a **195,104-byte** three-asset bundle, still below 256 KiB: bundled `app.js` is 163,000 bytes with SHA-256 `1603ee66f82665117f05d5a1062c481ca99bc8081cfcc5af4d51a15e78c001b7`. Generated index/style identities are unchanged. The earlier 194,910-byte result and independent 9/9 + 3/3 runs apply to the pre-auth snapshot; refreshed whole-branch browser verification remains owned by the final-verification workers.
