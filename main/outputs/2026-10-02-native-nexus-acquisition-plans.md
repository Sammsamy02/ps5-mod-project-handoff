# Native Nexus acquisition plans

The resident authenticated app now computes a bounded prerequisite closure and
selects live Nexus source file identities. It uses shared handlers for arbitrary
valid game domains. The result is acquisition metadata, with compatibility and
version satisfaction still unresolved.

## Behavior

POST `/api/acquisition-plan` takes exactly `game`, `mod_ids`, `file_choices` and
`file_policy`. It accepts one to eight unique positive INT32 root mod IDs,
optional explicit file choices, and either `explicit_only` or
`unique_current_primary`. Existing local session, CSRF, Origin and 4096-byte body
framing apply. Caller-supplied plans, provider URLs and account claims are not
inputs.

For each reachable source, the app reads current mod visibility before reading
requirements and files. One authenticated operation owns the entire discovery,
deadline, cancellation and final publication. Internal same-game dependencies
are fetched once; relation provenance remains in the plan. The closure includes
at most eight mod identities, with at most 128 relations and unresolved facts.

Explicit file choices must belong to the full live file list with unique IDs.
Automatic selection requires exactly one primary current MAIN file and typed,
consistent category hints across the list. It never selects by highest ID,
filename, version ordering or free-form notes. Missing, contradictory or
ambiguous hints require an explicit choice. A file version is a provider label,
not proof that a prerequisite version constraint is satisfied.

Hidden, external, DLC, cross-game, unqueryable, cyclic and oversized dependencies
remain unresolved, as do missing or unused explicit choices. The response reports
whether discovery is complete separately from acquisition readiness. Malformed
provider bodies, transport failure, cancellation and deadline failure publish no
plan. Every non-ready plan has an empty `selected_sources` list; it never exposes
a selected prefix that looks ready to download.

A ready plan returns deterministic prerequisite-first source identities. This
order is explicitly not game load order. Every plan reports
`download_authorized: false`, `installation_supported: false`, unresolved
compatibility/version satisfaction, and no exact prerequisite file/version
constraints. The download resolver must independently authorize each transfer.

The file hints and requirement schema follow the pinned official
[Nexus source types](https://github.com/Nexus-Mods/node-nexus-api/blob/da9b8eda8a3f381f04d87c7290595066ccbdbc5d/src/types.ts).
Fixtures establish the implemented contract; live OAuth scope and production
GraphQL behavior remain untested in this phase.

## Evidence

The focused real decoder/selector suite passes 35 tests: 14 new hint/selection
controls and 21 existing metadata cases. The previous decoder lacks the typed
hints and selector; retained RED controls capture those failures. The focused
auth driver passes strict and sanitizer builds, including source gating, fixed
requirements construction and cancellation after staging with full output
revocation. HTTP framing passes 17 focused controls.

The actual resident app passes all 55 acquisition-plan checks against an owned
HTTPS provider after owned PKCE sign-in. Its 121 provider requests include 40
fixed requirements POSTs. The checks cover diamond dependency deduplication,
prerequisite-first ordering, root/file permutations, explicit historical files,
unknown/contradictory/ambiguous categories, hidden sources, cycles, external/DLC
and other-game relations, closure bounds, malformed responses, auth/framing and
one cumulative timeout. A separate current-app requirements regression passes
all 29 existing checks. Neither suite transfers an archive.

The first graph run passed 54 of 55 checks because its contradictory-category
fixture used the unrecognized value `OPTIONAL`; the app correctly treated that
as unknown and withheld every selected source. The fixture now uses the pinned
recognized value `OPTION` with a conflicting category ID. The failed receipt is
preserved, and the corrected rerun uses unchanged app sources and artifact.
The pinned prior app returns 400 for a valid acquisition-plan POST and performs
no metadata discovery after sign-in, establishing the historical route failure.

Current host and PS5 SDK artifacts compile/link with zero diagnostics. Their
manifests bind 136 current source inputs and 52/53 objects respectively. The
native audit checks all 53 objects, 16524 header records and 243 retained archive
inputs with no host headers. Both new planner and selector bodies are retained;
the selected crypto owner remains unique, and all 137 imports match the previous
artifact. The aligned 336-byte dynamic table has 21 entries and ends with the
sole terminal DT_NULL. The native artifact was not executed.

Host artifact: 2309048 bytes,
`a56486fd9b861f47b49ace574f8ddf30041c100bfaf54cac959226cab31146eb`.
SDK artifact: 2824312 bytes,
`966aaa2a1f75cb490656d3df99c98416f84576c99ea73561603e3d50a94289dd`.

Receipts beneath `.superpowers/native-dependency-spike/`:

* `native-nexus-file-selection-1/implementation-report-1.json`
* `native-nexus-acquisition-plans-1/auth-controls-1/receipt.json`
* `native-nexus-acquisition-plans-1/http-controls-1/green/receipt.json`
* `native-nexus-acquisition-plans-1/red-1/receipt.json`
* `native-nexus-acquisition-plans-1/green-1/receipt.json`
* `native-nexus-acquisition-plans-1/green-2/receipt.json`
* `native-nexus-acquisition-plans-1/requirements-regression-1/receipt.json`
* `native-nexus-acquisition-plans-1/native-artifact-audit-1/receipt.json`

The paused 254-file baseline matches its saved identities. Reproduction uses
`outputs/ps5-mod-bridge/native-host/tests/nexus_acquisition_plan_verify.py`.

## Remaining full-goal work

The next concrete step is connecting an internally recomputed ready plan to the
existing download queue with atomic admission and replay. The queue currently
has eight global process-lifetime source slots and advances one job per HTTP
request. It needs explicit draining for unattended progress. Its worker already
performs fresh authorization, but the native queue cannot yet supply or resume
free-account website grants.

Successful archive preparation does not establish installed-state evidence,
target placement, Unreal reflection/game-thread calls, UE4SS Lua gameplay or
Windows DLL gameplay. The final PS5 account/store UI and console publication
remain unfinished full-goal work. The full goal stays active.

This phase uses owned local fixtures and saved inputs. It does not contact the
console or production account, write game/system files, or retry previously
stopped native placement, game-memory-read or curl-upgrade actions. Paused title
and UI work remains outside its scope.
