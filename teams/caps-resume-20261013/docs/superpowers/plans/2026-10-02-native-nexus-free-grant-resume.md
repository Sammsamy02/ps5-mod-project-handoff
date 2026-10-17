# Native free-account Nexus job resume

Continue the full active goal from 9679483. Queued acquisitions and unattended
draining are verified local progress. Add the missing free-account website grant
path to the same native job worker. The user is unavailable: keep this local,
preserve paused files, and do not contact the console or production account or
retry the specifically stopped actions. Runtime UE4SS/DLL support, persistent
installation, target discovery and the final PS5 store remain unfinished.

## Contract

Add an ABI-v1 resume parameter structure and `mb_nexus_jobs_resume_v1` without
changing existing parameter layouts or enum values. Borrow cookie, CSRF, Origin,
the exact 32 lowercase-hex job ID and a bounded intent. Expose exact POST
`/api/jobs/{id}/resume`, with existing 4096-byte JSON/framing/session rules and
202 for accepted resume. Only the same session owner's existing authorization-
required source job can be requeued. Keep its ID, kind, source, idempotency key
and global slot; resuming must work even with all eight slots occupied. Reject
queued or terminal jobs without overwriting a pending grant or redownloading.

The intent is exactly `{ "nxm": "nxm://..." }`. Parse a documented strict subset
of the Nexus website file grant: exact nxm scheme, validated game domain, exact
canonical positive signed-63 mod/file path, and exactly one key, expires and
user_id query field. Reject fragments, userinfo, ports, other paths/URI kinds,
duplicate or extra query/body fields, malformed percent encoding and out-of-
range numbers. Decode query values before validation; key is nonempty printable
ASCII, bounded by the existing resolver's 1024-byte limit. Match game/mod/file
to the retained source and reject expiry before admission using checked internal
time. Never treat the URI as a fetch target or caller entitlement. The strict
subset deliberately does not implement Vortex's collection/OAuth/other forms.

Stage the parsed grant, output and fresh bindings before a non-failing commit.
Recheck session, account, storage and expiry immediately before admission. A
failure must not retain the secret, change state/count or leave partial output.
Retain the grant only in volatile private queued-record storage. It must never
appear in public JSON, logs, disk receipts or archive URLs. The worker takes and
erases it once, passes a disjoint typed grant to the existing auth-owned resolver,
and erases the local copy before archive transfer. In particular, the typed
grant domain cannot alias request.domain because existing span validation rejects
that layout. Every failure, revocation and context cleanup clears the grant.

The existing resolver still obtains live account identity, binds user_id and
file scope, rechecks expiry, and obtains the fixed Nexus download-link route.
Wrong live user or grant expiry at worker time returns authorization-required
without a link/archive transfer; a new correct grant may resume that job later.
Use the existing credential-free archive sink and unchanged archive/preparation
handlers. No browser custom-scheme interception or production OAuth claim.

Primary protocol evidence is the retained pinned official Vortex NXMUrl and
node-nexus-api source in
`.superpowers/native-dependency-spike/native-nexus-acquisition-queue-1/nxm-primary-source-1`.

## Work

- [x] Capture actual historical 9679483 application RED for the new route before
  product edits; use owned PKCE/TLS fixtures only.
- [x] Implement jobs ABI, strict grant parsing, atomic resume and consume/wipe;
  focused controls for full capacity, ownership, malformed/scope/expiry grants,
  output/final admission, queued/terminal refusal and fresh resolver identity.
- [x] Add the exact bounded HTTP mutation route and focused framing controls.
- [x] Exercise actual owned HTTPS free-account authorization and same-ID resume
  with an unchanged real archive. Verify exact key/expiry link query, live user,
  credential-free archive headers and no extra transfer on rejected retries.
  Keep wrong-source/expired admission and wrong-user/worker-expiry controls
  focused; do not repeat prior dependency or oracle matrices unnecessarily.
- [x] Compile current full host/SDK source closure and audit retained ABI,
  source/header identity, imports and crypto ownership. Native ELF is unexecuted.
- [x] Save checkpoint and focused evidence, verify paused baseline unchanged,
  commit only owned files and update the full-goal progress ledger.
