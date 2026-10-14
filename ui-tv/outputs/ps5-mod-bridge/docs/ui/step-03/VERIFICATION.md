# Final UI verification — 2026-10-13

All tests use owned local data. Controller suites drive the real browser UI with standard Gamepad d-pad/X/O at 1920×1080 and 3840×2160. Screenshots are 1920×1080. The installer module suite uses Tab/Enter for the same focusable controls; Gamepad integration is proved separately. No real account, provider download or PS5 was accessed.

| Suite | Result | Evidence |
|---|---|---|
| General controller navigation | 18/18 | `final-controller/RESULTS.json` |
| Controller text entry | 10/10 | `final-keyboard/RESULTS.json` |
| Full-app installer controller | 34/34 | `final-installer-controller/RESULTS.json` |
| Polish, recovery, preferences, loading/error races | 23/23 groups | `evidence/test-results.json` |
| Installer service/mock | 38/38 | `final-service.log` |
| Installer presentation/real CSRF seam | 37/37 | `final-installer-module/test-results.json` |
| Generic selection | 14/14 | `legacy/final-generic_selection_ui_tests.log` |
| Retained downloads | 14/14 | `legacy/final-retained_downloads_ui_tests.log` |
| Service records and polling focus | 12/12 | `legacy/final-service_records_ui_tests.log` |
| Selection basket | 8/8 | `legacy/final-selection_basket_ui_tests.log` |
| Saved selection/restore | 22/22 | `legacy/final-selection_restore_ui_tests.log` |
| Inventory paging | 23/23 | `legacy/final-ui_paging_tests.log` |
| Delayed focus ownership | 2/2 | `legacy/final-ui_focus_race_tests.log` |
| Original UI/account/download flow | Pass | `legacy/final-ui_tests.log` |
| Static UI delivery | 9/9 | `final-asset-delivery.log` |
| Existing pure native asset checks | 3/3 | `final-native-assets.log` |

The controller/legacy receipt covers 157 cases plus the original UI flow, with matching complete web source hashes before and after. See `FINAL-CONTROLLER-RECEIPT.json`, `legacy/FINAL-RECEIPT.json`, and `FINAL-CONTROLLER-REVIEW.md`. Final polish evidence includes the mid-session Nexus expiry regression: selected downloads and request identity remain intact, submission is disabled until sign-in, and controller Sign in starts exactly the existing official auth operation. Historical pre-fix receipts remain in `pre-auth-refresh/` and red logs; they are not presented as final results.

Independent final source review found no important actionable defects, then separately reviewed the bounded account-expiry fix. See `../FINAL-REVIEW.md`. Final source app SHA-256 is `324d255d890f66a91c5b2a865ba552000a4d21fabfcf5db6df88f49dd97090e1`. `final-asset-identity.json` records the actual three-asset bundle: **195,104 bytes**, within **262,144 bytes**. This proves static delivery, not native console compilation or execution.

The normal `make test-ui` cannot be claimed as passing: the existing libarchive manifest uses local URLs while its verifier requires commit-pinned HTTPS URLs. This was reproduced before UI work and recorded in Step 1; dependency metadata and build outputs are unchanged.

The default installer is explicit practice data. Real installer operation, game-running enforcement, runtime mod compatibility, real management mutations, physical PS5 controls/rendering, Nexus registration/sign-in, NXM return, full search, image transport and cache reuse still require their respective runtime/provider integrations. These limits are visible or documented rather than replaced with successful mock claims. No game/mod/loader/plugin/loadlist/pak file was edited.
