# Export verification — 2026-10-14

Host checks were run against gitignored copies rendered from this export, after
restoring the authored Nexus client package accidentally flagged by the earlier
scan. No PS5 was contacted and no game was launched.

| Check | Result |
| --- | --- |
| Installer/service contract, `node --test tests/installer_service_tests.cjs` | 38 passed, 0 failed |
| Browser installer flows/errors, `node tests/ui_installer_tests.cjs` | 37 passed, 0 failed |
| Desktop/native static-asset delivery, Python `ui_asset_delivery_tests.py` | 9 passed, 0 failed |
| Installer core/planner/rollback/adapter modules, eight unittest modules | 191 passed, 0 failed |
| Python syntax, export tools + app companion + installer/compat source | 43 files parsed |
| TV UI browser JavaScript syntax | All six web modules checked with `node --check` |
| Export integrity/exclusions | Retained source hashes verified; no forbidden game/build/key/raw-log extensions, Epic copyright source, original user home paths, literal private network addresses, or GitHub/AWS/private-key patterns found in the final scan |
| Additional credential-like literal review | 33 candidate occurrences reviewed: explicit synthetic/invalid test keys or adversarial URL fixtures, no live credentials identified |

The installer tests emit pre-existing ResourceWarnings and intentional duplicate-ZIP
entry warnings; their run exited successfully. Disk-image case-sensitivity tests and
tests depending on the excluded real third-party mod were not run. Existing original
UI screenshots and historical verification documents remain alongside their source;
new browser screenshots/results from the export smoke check stayed in `.local/`.

This is not a full runtime rebuild, a regression run over every historical source
snapshot, live Nexus authentication/download proof, or PS5 target testing. The complete
build and console proof remain the successor's work. Old receipts with redacted paths
or source hashes must be regenerated rather than accepted as current release evidence.

Run `python3 tools/verify_export.py` to repeat source-manifest and exclusion checks.
The original worktrees were read, not edited or pushed. This repository has fresh
history so excluded content in old Git commits cannot be recovered from this upload.
