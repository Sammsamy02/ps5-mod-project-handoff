# Nexus, errors and polish — bounded UI audit

Read-only production audit on the isolated UI branch. Only this new note was written; no app, provider, host, build, main checkout or console access. Recommendations retain existing routes and behavior unless explicitly identified below.

## Recommended next small steps

| Order | Change and user benefit | Existing data/seam | Proof |
|---|---|---|---|
| 1 | Translate legacy errors and hide diagnostics behind More info, so someone knows what happened and what to do. | `endpoint`, `loadAccount`, `renderAccount`, `renderActivity`, `renderInstalled`, `renderTitles`; existing error `code`, validated analysis reasons, service records. | Each known error has plain copy/action; no hashes, paths, IDs or raw codes outside details; poll preserves focus. |
| 2 | Show distinct loading, empty and failed views, so slow Nexus responses do not look like “no mods”. | Existing request counters already reject stale responses; add view loading/error state around `loadGames`/`chooseGame`/`chooseMod`. | Slow first request shows skeleton; actual empty reply shows empty state; failure shows Retry; switching games ignores old replies. |
| 3 | Remember local list filters/sort and the last Nexus game; make a small mod overview above its file list. | `state.filters`, `state.sort`, `saveNavigation`; existing mod `name`, `author`, `summary`, `description`, `version`, `source_url`. | Reload/reopen restores nonsecret catalog preferences; controller edits filters; description is text, not injected HTML; More info retains original details. |
| 4 | Explain own-account sign-in and the incomplete free-download handoff honestly. | Existing account/auth routes and `needs_nexus_authorization` job state. | Reconnect preserves picks; official page opens; no automatic success after opening a website; no grant/credential enters rendered diagnostics or browser storage. |

Use the existing config routes and official URL builders. A root-owned small helper in `app.js` can serve the first copy pass; a new cross-screen framework is unnecessary.

## Exact visible wording

| Existing state/code | Main copy | Action using an existing seam |
|---|---|---|
| No account | “Sign in to Nexus to browse and download mods.” | “Sign in to Nexus” → configured `authStart`. |
| `reconnect_required` / expired account | “Your Nexus sign-in expired. Sign in again to continue.” | “Sign in again” → `authStart`; retain selected files. |
| `network` / failed fetch | “Couldn’t connect to Nexus. Check your internet connection, then try again.” | “Try again” repeats the failed read; do not auto-loop. |
| Browser reports offline | “No internet connection. Reconnect, then try again.” | “Try again”; being online does not by itself prove Nexus is reachable. |
| `timeout` | “Nexus is taking longer than expected. Try again.” | Retry failed read. |
| `rate_limit` | “Please wait before checking Nexus again.” | Deliberate later Retry; do not invent a countdown. |
| `authorization_cancelled` | “Sign-in cancelled. You can sign in whenever you’re ready.” | “Sign in” / Back. |
| `authorization` | “Nexus couldn’t finish sign-in. Start sign-in again.” | “Sign in again”. |
| `truncated` | “The download stopped before it finished. Download it again.” | Reopen the file/authorize a new download; see retry limits below. |
| `content_hidden` | “Nexus is hiding this mod with your account’s content settings.” | Back / More info. Keep provider-side filtering; do not expose hidden records. |
| `job_limit` | “The download queue is full. Finish your current downloads, then try again.” | Retry pending selection; preserve remaining picks. |
| `storage_unavailable` | “The app couldn’t save this download. Restart the app, then try again.” | Back; internal storage location belongs in More info. |
| `stale_target_ref` | “The game list changed. Choose your game again.” | Existing refresh/reselect flow. |
| `needs_nexus_authorization` | “This download needs approval on Nexus.” | “Open Nexus file page”; current handoff limitation must remain visible. |
| Missing capability file | “Mods are not available yet for this game.” | Browse other mods / More info. Never infer support from file extensions. |
| Unverified compatibility | “Not tested in this game yet.” | More info; do not present analysis success as working gameplay. |

For the current unwired free-download return, add: “Returning the approved download to ModBridge is not available yet.” Once a real callback exists, replace that sentence with: “On Nexus, choose Mod Manager Download, then return here.” That later wording must not promise a callback before it works.

Account statuses already carry `status`, `credentials_present`, `live_service`, optional `error.code` and, only after live verification, `account.display_name`, `account.is_premium`, `account.is_supporter`. The host currently replaces account error messages with generic wording, but preserves the code. The UI can use that code without changing the host. When `nexus_auth_start` is false, say “Nexus sign-in is not available in this app yet”; do not offer a dead button.

## Hide details without removing evidence

- Activity: keep download label, friendly status, friendly size and next action visible. Move archive SHA, raw IDs, execution context, raw reasons and full analysis report into More info. A computed SHA alone is not comparison with an expected Nexus hash; avoid “download verified” unless that comparison exists.
- Saved downloads: “Download saved” and readable mod/file names when available. Existing retained records contain only identity/hash/byte fields, so a fallback should be “Saved download”, with identity in More info. Never invent a mod name. Do not add an install action without a trusted archive resolver.
- Installed/service records: “Saved mod records” and “We haven’t checked whether the game is open.” Put raw `title_id`, runtime state, load-list hash and blocker codes in More info. A historical `mods-started`/PID record does not prove the game is running or every mod worked.
- Titles: display the observed game name, or “Game name unavailable”; keep the title ID, version/fingerprint and source information in More info. Preserve snapshot selection/reselection rules.
- Filters: “Find in this list”, “Showing 4 of 20”, and “These are the latest mods. Open a Nexus mod link to find another.” This is a local list filter, not complete Nexus search.
- Mod overview: name, author, version, summary and an “Open on Nexus” action above Available files. Long/raw description stays readable text in More info. Existing description may contain markup; never insert it as trusted HTML.

Useful analysis summaries: `missing_dependency` → “This mod needs another mod before it can work”; `inventory_incomplete` → “Some of this download couldn’t be checked”; `platform_acceptance_missing` → “We haven’t tested this mod in the game yet”; `windows_runtime_unavailable` → “This mod needs a Windows feature that this game cannot use yet”; `managed_runtime_unavailable` → “This mod needs an extra mod system that isn’t available for this game yet.” Keep the original reason in details.

Persist only a bounded catalog preference record: selected Nexus domain, game/mod filter text and sort order. Do not persist credentials, signed/NXM links, private archive paths or stale console target references as preferences. Current navigation is session-only; filters/sort are memory-only, and choosing a game resets the mod filter. A separate owned preferences key can satisfy “remembers the last game and filters” without altering existing request/selection receipts.

## Concrete integration dependencies

| Feature | What exists | What is genuinely missing |
|---|---|---|
| Official account login | Configured public-client PKCE flow, state check, private token refresh, live user verification, auth start/disconnect; no bundled borrowed identity. | A valid registered client/callback and live user proof remain explicitly untested in README. The desktop callback is loopback; native preview does not provide the account bridge. Keep own-account configuration, not a replacement login form. |
| Free-account manager downloads | Real client validates NXM file/account/expiry; `POST jobs` accepts optional `nxm`; matching `needs_nexus_authorization` can resume with original key/target. | Browser/platform NXM callback is unwired. Public jobs omit original `idempotency_key`; UI discards admitted intent, so durable same-job continuation needs a private mapping or an agreed host continuation contract. Do not invent a resume endpoint or a PS5 protocol handler. |
| Retry actual failed jobs | UI can retry failed admission with the same selection; host preserves request identity. | Reposting a completed failed job’s original key returns the old failure. Only `needs_nexus_authorization` with a new matching grant requeues. A new download attempt needs a new intent; a dedicated same-job retry would require host support. |
| Full Nexus search/filtering | Generic games, latest mods, direct mod lookup and files; local name/author/summary filtering. | No server-side search/pagination route or advertised capability. Add no fabricated `/search` call or fake result count. Runtime/provider team must define it. |
| Mod images | No image URL survives the current provider adapter; native CSP explicitly has `img-src 'none'`. | Validated image fields, bounded acquisition/cache, CSP allowance and an agreed serving path. Use a truthful placeholder meanwhile; do not generate an image URL from IDs or bypass the host with arbitrary remote image fetching. |
| Download cache | Native-side retained-download capability/records are supported by the browser. | Desktop host does not advertise/serve retained downloads; there is no cache lookup/reuse transaction or archive-path resolver in these UI APIs. A saved-record card must not claim cache reuse or installation readiness. |
| Accurate wait times | Nexus errors internally have `retry_after`; client collects rate-limit headers. | `safe_error` publishes only code/message, so no cooldown/ETA value reaches the UI. Avoid guessed timings or rapid retries. |

## Official verification, 2026-10-13

Nexus’s current API documentation presents v3 and identifies v1 integrations as legacy; that does not justify replacing this working adapter during a wording/layout slice. Search/image integration must be based on an agreed, tested provider contract. [Official API documentation](https://api-docs.nexusmods.com/)

Nexus’s policy allows personal API-key use for testing/personal use and requires registration for wider public distribution; it also requires app identity headers. This is a release/configuration dependency, not a reason to interrupt the current local UI work. [API acceptable-use policy](https://help.nexusmods.com/article/114-api-acceptable-use-policy)

The official Vortex guide confirms that manager downloads depend on an NXM application association and a signed-in account. This supports the identified callback dependency; a web page opening alone does not complete it. [Official Vortex download guide](https://github.com/Nexus-Mods/Vortex/wiki/MODDINGWIKI-Users-General-Downloading-from-Nexus-Mods)

The official Nexus client repository describes registered-app SSO and throttling. Preserve user-initiated requests and response-driven pacing; do not copy its library-specific quota numbers into UI. [Official Nexus client](https://github.com/Nexus-Mods/node-nexus-api)

The official authentication discovery URL could not be fetched by the research tool. No new claim about public-client PKCE registration eligibility or live PS5 sign-in has been inferred from that failed fetch. No account was signed in and no download/provider API operation was performed during this audit.
