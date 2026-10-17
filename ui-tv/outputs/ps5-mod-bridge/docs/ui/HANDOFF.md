# PS5 Mod Bridge UI handoff

Worktree: `/WORKSPACE/.codex/worktrees/translation-core/modbridge-tv-ui-20261013`

App: `/WORKSPACE/.codex/worktrees/translation-core/modbridge-tv-ui-20261013/outputs/ps5-mod-bridge`

Branch: `codex/modbridge-tv-ui`, based on `53e1e9bdc5a5788621c0b8096a73cc321f0c98a8`.

Claude's `what` checkout and its build/frozen/runtime folders were not edited by this work. Review/cherry-pick these steps in order, resolving any overlapping later Claude edits. No push or PR was created. The manual sparse Git worktree could not be registered with the projectless chat; it remains available at the path above.

| Step | Commit | Change note | User benefit |
|---|---|---|---|
| 1 | `4bba9985` | `step-01/CHANGE-NOTE.md` | Readable TV layout and reliable d-pad focus/back. |
| 1b | `70fd89e8` | `step-01b/CHANGE-NOTE.md` | Controller text entry and delivery through existing static asset interfaces. |
| 2 | `00f98dc7` | `step-02/CHANGE-NOTE.md` | Review a plan, answer Yes/No, confirm, see progress/result, and confirm Restore. |
| 3 | Commit containing this note | `step-03/CHANGE-NOTE.md` | Plain errors/details, remembered browsing, loading states and stable polling focus. |

The UI preserves the existing app and adds explicit practice installer screens. The default mock is clearly marked and changes no game/mod files. Per-game preview demonstrates on/off, controller reordering, conflicts, updates, session results, self-test and one-time setup. Real installed/service records remain separate.

## Runtime integration needed

1. Keep `web/config.js` as the route/config source. Default `installer.mode` is `mock`; switch to `real` only when the actual service is available. The adapter uses exactly plan/apply/restore/status. The app supplies a current CSRF-token callback. Real failures never become successful previews.
2. Bind observed console title IDs independently from Nexus game names. A missing capability file means unavailable. The runtime must enforce game closure during apply/restore and report failures accurately; supply plain descriptions, warnings and questions for meaningful plan review; optional progress/status fields must not become required mock metadata.
3. Provide trusted `{game_title_id, archive_path, label}` to `ModBridgeInstallerUI.offerArchive`. Existing retained records expose download IDs, not archive paths: the browser must not guess or write a file. Offer returns false during pending work or an unacknowledged RESULT; the caller should retain and retry after acknowledgment. `selectGame` queues changes until RESULT is acknowledged.
4. On/off and load order changes are preview-only. Agree runtime management operations before enabling real controls; do not silently add writes to the shared four-call contract. Session/self-test/setup/progress values require real runtime evidence.
5. Preserve official own-account Nexus authentication and request pacing. Free-account NXM return needs a real platform association/callback and private continuation identity. Opening the official Nexus page alone does not finish a download. Full search, images and cache reuse need agreed provider data/routes; the current UI clearly identifies these limits. See `step-03/nexus-audit.md` for exact seams.
6. Fix the existing libarchive dependency manifest/verifier mismatch before claiming a new normal host build: the manifest contains local URLs while the verifier requires commit-pinned HTTPS URLs. UI work did not change these files or bypass verification. Browser and static asset checks run independently.

No physical PS5 rendering/controller mapping, console installs, runtime compatibility, real Nexus login/download or universal mod support is claimed. All work and tests stayed on the Mac with owned fixtures. Keep modded games offline.

## Evidence and commands

Read `step-03/VERIFICATION.md` and `FINAL-REVIEW.md` for final results and source hashes. Each step includes short change notes, screenshots and red/green evidence. Final controller screenshots are under `step-03/final-controller`, `final-keyboard`, and `final-installer-controller`; polish screenshots are under `step-03/evidence`.

Use bundled Node `/WORKSPACE/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node` with `NODE_PATH=/WORKSPACE/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules`; local Chrome is used headlessly. Browser suites live in `tests/`. Set `UI_EVIDENCE_ROOT` to a new folder when rerunning controller, keyboard or installer suites so historical receipts remain unchanged. Run pure asset checks with `python3 -m unittest discover -s tests -p ui_asset_delivery_tests.py -v` and `python3 -m unittest discover -s native-host/tests -p test_web_assets.py -v`.

No console permission or launch is needed to review these UI changes. The runtime team owns later real-service and console validation.
