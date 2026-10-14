# Native Nexus prerequisite discovery

The authenticated resident catalog can now read the prerequisites declared for
a mod. It uses the existing account, transfer and crypto owners. No archive
filename, mod description or file category is treated as a dependency decision.

## Behavior

GET `/api/games/{domain}/mods/{id}/requirements` accepts a current local session,
a safe game domain and a canonical positive mod ID up to INT32_MAX. The source
mod metadata is read first. Hidden, unpublished or disallowed adult sources are
refused before any requirements query. The source read and requirements read
share one operation deadline and the existing account/token publication gates.

The typed client constructs one fixed read-only GraphQL query. A separate,
additive private POST entry point accepts only that canonical body and the exact
Nexus GraphQL URL. Existing metadata GET, OAuth and download layouts remain
unchanged. The response passes the owning core decoder and source-bound catalog
validation before a normalized `requirements` object reaches the local caller.

Internal Nexus mod identity, external requirement metadata and DLC identity are
kept distinct. Every relation reports `satisfaction: "unresolved"`. The report
explicitly provides no exact file/version constraints or installation authority.
Complete empty sets are accepted. Missing collections, malformed rows, GraphQL
errors, count mismatches and truncated sets publish no accepted prefix.

The query/schema follows the official
[node-nexus-api source](https://github.com/Nexus-Mods/node-nexus-api/blob/da9b8eda8a3f381f04d87c7290595066ccbdbc5d/src/Nexus.ts)
and its pinned
[requirements types](https://github.com/Nexus-Mods/node-nexus-api/blob/da9b8eda8a3f381f04d87c7290595066ccbdbc5d/src/types.ts).
Only forward requirements are requested. Reverse dependants are excluded.
Pagination and live OAuth scope behavior are not established by these fixtures;
an incomplete forward set is refused rather than treated as complete.

## Evidence

The strict core/native-bridge controls pass 37 tests: 14 requirements controls,
two native ABI controls and 21 existing metadata cases. The fixed writer and
decoder handle typed identities, explicit empty sets, resource limits, invalid
UTF-8/duplicate keys, malformed collections and GraphQL errors without returning
a partial result.

The real transport passes 29 admission controls and 13 runtime controls against
nine owned TLS requests. These cover the fixed query, credentials/headers,
existing GET behavior, consumer refusal, cancellation, deadline reuse, redirect
refusal and terminal-owner cleanup. Earlier verifier setup failures were fixed
in test inputs; they did not open sockets or require product changes.

The actual resident app passes 29 owned HTTPS checks through a test-only socket
redirect. It performs owned PKCE sign-in and preserves the configured URL, SNI
and certificate validation. Its 26 provider requests include nine canonical
requirements POSTs. Complete/empty sets, a second game domain and INT32_MAX,
partial/error/null/malformed bodies, hidden sources, invalid sessions/IDs,
timeouts, four existing catalog routes and private-directory cleanup pass. No
archive is transferred. The pinned previous application instead returns 400
for the new local route and makes no requirements POST; seven historical RED
checks pass.

Current ordinary-host and PS5 SDK builds compile/link with zero diagnostics.
Both manifests record 136 current source inputs, including the fixed query
header. The native artifact audit verifies 53 current objects, 16523 header
records and 243 retained archive inputs, with no host headers. Its 137 imports
match the previous build, and its aligned 336-byte dynamic table ends with
DT_NULL. Existing archive/OAuth/metadata and preparation/generation bodies remain
linked through the selected crypto owner. The SDK artifact was not executed.

Host artifact: 2197208 bytes,
`7db757e346041b50d6ae868306d274f9e3f7b344bc8a341b449292358a7f226d`.
SDK artifact: 2695648 bytes,
`44afe34808d74e67ea770eda522c1eed6580fdff202107decc3a87cef7b3dffb`.

Receipts beneath `.superpowers/native-dependency-spike/`:

* `native-nexus-requirements-core-1/green-2/receipt.json`
* `native-nexus-requirements-1/transport-1/tls-green-3/receipt.json`
* `native-nexus-requirements-1/integration-freeze-1/report.md`
* `native-nexus-requirements-1/red-1/receipt.json`
* `native-nexus-requirements-1/green-1/receipt.json`
* `native-nexus-requirements-1/native-artifact-audit-2/receipt.json`

The paused 254-file baseline matches all saved input identities.

Reproduction:
`outputs/ps5-mod-bridge/native-host/tests/nexus_requirements_verify.py`.

## Remaining full-goal work

This is prerequisite discovery. Exact prerequisite file selection, recursive
dependency planning, installed-state evidence and installation remain unfinished.
The existing private PAK, INI and IoStore preparation paths do not establish
Windows DLL execution, Unreal reflection/game-thread calls or UE4SS gameplay.
The final PS5 account/store UI and console publication remain separate full-goal
work. The full goal remains active.

All verification in this phase uses owned local fixtures and saved inputs. No
console, production account or game/system files are touched. Paused title/UI
work and user files remain outside the phase. Previously stopped native
placement, game-memory-read and curl-upgrade actions are not retried.
