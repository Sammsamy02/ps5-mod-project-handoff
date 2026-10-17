# Native free-account website-grant resume

The native worker now accepts a file-specific Nexus website grant for a free-
account source job and resumes its original ID. This completes the local grant
path through source download and analysis; installation and engine runtime
support remain unresolved. Continue the full active goal from 9679483.

## Behavior

The additive `mb_nexus_jobs_resume_v1` API and exact POST
`/api/jobs/{32-lowercase-hex-id}/resume` accept an existing source job's
file-specific Nexus website grant. The intent is exactly `{ "nxm": "..." }`,
within the existing 4096-byte JSON frame and cookie/CSRF/Origin rules. Acceptance
requeues the same owned authorization-needed record, preserving source, kind,
idempotency key, job ID and slot. It works at the eight-record capacity limit.
It does not create a new source or acquisition.

The parser accepts a bounded mod/file NXM subset, with one literal key, expires
and user_id query name each. Values are percent decoded and validated against
the existing typed resolver limits. Scope must match the retained source and
expiry is checked using internal time. Collection/OAuth forms, arbitrary URLs,
changed sources, extra fields and malformed encodings are refused. Queued or
terminal records cannot have grants overwritten or be downloaded again through
this endpoint. Existing REFUSED maps to HTTP403, missing/foreign records404 and
malformed input400; accepted resume202.

Private staging, response serialization and final session/account/storage/time
checks precede commit. The grant remains only in volatile private queued storage.
The worker takes and erases it, supplies a disjoint typed grant to the existing
auth-owned resolver and clears its local copy before the credential-free archive
transfer. Grant and resolved provider URI are excluded from public job JSON and are not
persisted by the app.

The resolver obtains live account identity, rechecks file/user/expiry and uses
the fixed Nexus download-link route. Wrong live user and expiry remain
needs_nexus_authorization without an archive transfer, so a fresh grant can
resume the same job. A late expiry at link cleanup now retains that state too;
previously the worker's stage-specific classification made it a terminal failure.
No new downloader, account owner or per-mod conversion is introduced.

## Evidence

The actual ordinary-host app passes29 owned HTTPS checks. After free-account
PKCE sign-in, one retained source reaches authorization-needed without a link or
archive request. Wrong game/mod/file and expired admission preserve the same
record without provider work. Other-cookie and wrong-CSRF attempts are refused.
A wrong-user grant is admitted for fresh worker validation, then returns the
same job to authorization-needed after live account lookup, without a link or
archive request. A fresh corrected grant succeeds through another live lookup.

Exactly one fixed link lookup and one credential-free archive transfer occur.
The decoded key/expiry query matches the fixture contract; the grant is absent
from the archive URL/headers and public responses. The unchanged real Movement
archive is2805bytes with SHA256
`3850a2b51d96a0fb2055a26371b7a6b3dfd527a68f1e34ecf3cdd2c47293d6ad`.
The original sink and archive analyzer reach `analyzed` using the same ID and
single source slot. Repeated/terminal resumes are refused without redownload;
original submit idempotency remains intact. No generation is automatically
created. The eight owned TLS requests and application/provider listeners clean
up, leaving private scratch empty. Provider account data is fixture data, not a
production Nexus account verification.

Focused component controls pass46 assertions using actual jobs/auth/resolver,
query/decoder, sink, FD and archive code with lower token/provider/producer
substitutes and controlled clocks. They cover full capacity, strict URI/JSON
parsing, ownership, output rollback, final admission/expiry/re-entry, initial
worker expiry and late-link expiry with fresh-grant retry. Worker-time expiry is
component evidence; it is not claimed from the actual HTTPS fixture. The first
component run had only a fixture expectation error: it expected unreserved `~`
to be percent encoded. Correcting that expectation required no product change.
No sanitizer or prior graph/codec matrix is rerun in this phase.

Current host and PS5 SDK builds pass with zero diagnostics and unchanged inputs.
They bind136 sources and52/53 objects, respectively. The SDK audit retains the
new resume and existing APIs through one crypto/zeroize owner; all137 imports
match the previous app. Its aligned336-byte dynamic table has21 entries and one
terminal NULL. The ELF is unexecuted. Host artifact2348344bytes; SDK artifact
2878576bytes. A narrow source review finds no blocking issue.

Historical actual RED uses the retained 9679483 app after owned free-account
PKCE sign-in. Its authorization-needed source preserves ID/state and rejects
the complete new request with HTTP400 instead of202, with no new provider/link/
archive request. The first split-frame helper race is retained separately;
RED2 sends a complete frame and checks the real response Content-Length.

The focused HTTP path passes22 framing controls and enabled strict C syntax.

Receipts are beneath
`.superpowers/native-dependency-spike/native-nexus-free-grant-resume-1/`:

* `red-2/receipt.json`
* `component-green-2/receipt.json` and `implementation-report-1.json`
* `http-route-controls-1.json` and `jobs-review-1.json`
* `actual-tls-green-1/receipt.json`
* `build-host-1/receipt.json` and `build-native-1/receipt.json`
* `native-artifact-audit-1/receipt.json` and `root-final-verification.json`

Reproduction uses committed `native-host/tests/nexus_free_grant_resume_verify.py`
and `nexus_free_grant_resume_tls_verify.py`, with explicit owned fixture/dependency
paths. The paused254-file baseline retains its saved identities.

## Remaining full-goal work

The pinned official [Nexus client](https://github.com/Nexus-Mods/node-nexus-api/blob/da9b8eda8a3f381f04d87c7290595066ccbdbc5d/src/Nexus.ts)
documents free-account website keys and link query parameters. Official
[Vortex source](https://github.com/Nexus-Mods/Vortex/blob/032157db2284db108db38798e82b8b48bb13ac4a/src/renderer/src/extensions/nexus_integration/NXMUrl.ts)
establishes the mod/file key, expiry and user identity. This implementation
supports a strict subset; it does not establish PS5 browser scheme interception
or production OAuth registration. Every free source requires its own website
authorization; acquisition metadata cannot provide a group download grant.

Source acquisition and private byte preparation do not establish engine
compatibility or installation. Persistent game deployment, automatic target
discovery, a real Unreal reflection/game-thread provider, UE4SS Lua gameplay,
Windows DLL gameplay and the final PS5 account/store UI remain unfinished.
The full goal stays active. No console, production account or game/system write
occurs in this phase, and specifically stopped actions remain stopped.

The next eligible local compatibility task is archive-backed Lua entrypoint and
module intake using the existing bounded loader. The read-only audit identifies
a real gap: current analysis counts Lua filenames, while the earlier UEHelpers
trial used a private ZIP-to-C-header generator. Reuse unchanged source bytes and
existing archive readers, account for unhandled DLL/settings members, and retain
the observed Item Spawner game-thread failure. This adds reusable intake, without
claiming a game provider or retrying stopped placement/memory-read actions.
