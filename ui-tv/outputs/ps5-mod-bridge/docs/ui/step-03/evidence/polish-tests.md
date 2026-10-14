# Polish test baseline and final receipt

`tests/ui_polish_tests.cjs` exercises real browser assets with owned local API replies. All actions use the standard Gamepad API d-pad/X/O; DOM access observes state. No mouse clicks, programmatic focus, keyboard text entry, account sign-in, real Nexus calls or console calls are used. The valid sign-in URL is fulfilled by an owned browser route rather than the remote website.

The initial 13 test groups cover loading for games/mods/files, plain connection errors with reachable More info/read-only Retry, preferences across reload/new page, hidden diagnostic bytes with accessible original details, malformed/blocked storage, legacy service diagnostics, official sign-in URL validation, honest free-download handoff/local-only filters, and expired-account wording. The first four groups run at 1920×1080 and 3840×2160; saved screenshots use 1080p.

Agreed root-owned seams: `[data-catalog-loading="games|mods|files"]`, `[data-error-action="retry|more"]`, existing `#details-dialog`, optional `#continue-game`, existing catalog controls and account/source-link actions. Public preferences use `modbridge.catalog.preferences.v1` with `{version:1,last_game:{domain,name}|null,filters:{games,mods},sort:{games,mods}}`; tests exclude secrets and console target references from that record.

The initial full baseline recorded 1 passing group and 12 failures. Eleven failures reproduced intended production gaps. The remaining failure was the test trying to release the old document's gamepad button after expected sign-in navigation; that harness race was corrected, and the focused official-origin check then passed. Malformed/blocked storage browsing also already passed. No production behavior was changed by the test worker.

Evidence: `baseline-red.log` retains the original run, including the harness failure; `auth-baseline.log` records the corrected passing official-origin check. Focused reports use their own names and do not overwrite the final full report.

Independent source review added eight focused groups, bringing the suite to 21: catalog recovery after a background jobs error, technical names/Unicode preservation, accurate verified-account empty state, empty/failure/retry distinctions for mods/files, late-response cancellation, initial games/direct-lookup loading overlap, technical breadcrumbs, and a semantic matrix across known Nexus/host error codes. Focused red receipts reproduce missing catalog-owned Retry, missing original technical names in More info, false sign-in guidance for an empty verified catalog, lost files skeleton after games completion, raw breadcrumb names and generic recovery for several known errors. The empty-vs-failed mods/files and late-response cancellation groups already pass. Receipts use separate `focused-review-red`, `additional-review-red` and `breadcrumb-review-red` names so they do not overwrite root's full-run receipt.

The unavailable-title fixture now uses the complete real envelope. This avoids showing a malformed-fixture error in otherwise normal screenshots. The late catalog-response assertion scopes its loading check to the catalog, allowing unrelated background progress to exist.

The independent screenshot review found one additional recovery defect after the 21-group run: a mid-session Nexus credential failure left the old verified header with only Disconnect. A focused controller regression reproduced the missing Sign in action before the root-owned fix (`expiry-review-red`). The fix invalidates account verification without clearing picked downloads. Its focused green receipt proves a retained basket, disabled download submission and actual controller Sign in activation into the owned official-destination fixture, with exactly one explicit `/api/auth/start` POST.

The last focused controller review found that repeated background credential failures rebuilt the Sign in button and lost header focus. `header-focus-red` reproduced the loss after a real second jobs poll. The root-owned one-line fix keeps dialog scope intact and searches the visible document for the stable replacement control. `header-focus-green` proves Sign in retains focus across that actual poll, and X still reaches only the owned auth fixture.

The final frozen-source full run passed **23/23 groups, exit 0**. `full-final-23.log` and `test-results.json` are the authoritative final receipts. The first four behavior groups run at both 1920×1080 and 3840×2160; the additional groups run at 1080p, including a semantic matrix of 33 error codes. The test uses no console or external provider network access. `full-green-candidate.log` and `full-final.log` are the earlier 21- and 22-group receipts, not the final source verification.

`final-manifest.json` binds the tested source hashes and all 27 screenshots, each verified as 1920×1080. Visual review covered loading, errors, mod overview/files, selected downloads, console game, remembered filters/Continue, Activity, Installed, sign-in and empty states. The empty-files screenshot uses controller focus on Refresh games to bring its actual empty result into view. The final expired-account and repeated-poll screenshots show a clear focused Sign in action. No remaining UI review holds were found in this bounded slice; real integration dependencies are recorded separately in `nexus-audit.md` and the final review note.

Run with the existing Playwright installation:

```text
NODE_PATH=/WORKSPACE/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules node tests/ui_polish_tests.cjs
```

Use `UI_TEST_FILTER` for a focused test name. Root owns app/index/style implementation and combined regression verification. This worker may update only the new polish test and this evidence directory during the red/green iteration.

Use `UI_EVIDENCE_NAME` to save a focused report under a separate basename. A controller sign-in action intentionally navigates away; its test does not try to release a button on the destroyed old document.
