# REPORT — worker-dll-mod-acquire (2026-10-04)

Workspace: `/WORKSPACE/.codex/worktrees/translation-core/what` (all reads and
writes confined here; writes only under
`outputs/2026-10-04-zcode-team/worker-dll-mod-acquire/`).

## Delivered

1. **EXECUTE-ACQUISITION.md** — exact, controller-runnable sequence to acquire
   ONE P3RFix (dinput8 proxy shape) through the real Nexus client: rebuild the
   desktop host with a real OAuth config (fixture binary provably cannot reach
   real Nexus — its config pins both provider hosts to 127.0.0.1 with a fixture
   CA), launch with `--port 8085 --lifetime 3600`, bootstrap session via
   `GET /api/capabilities` (issues `mb_session` cookie + `csrf_token`), user
   PKCE sign-in through `POST /api/auth/start` → browser → loopback
   `/auth/callback`, verify via `GET /api/account`, pin real IDs via catalog
   reads, submit exactly `{"kind":"download_analyze","game":…,"mod_id":…,
   "file_id":…,"idempotency_key":…}` to `POST /api/jobs`, poll
   `GET /api/jobs/<id>`, free-account website-grant resume via
   `POST /api/jobs/<32hex>/resume` with `{"nxm":…}`, evidence from
   `host/store/<job_id>.archive` + `GET /api/downloads`, clean stop via
   `POST /shutdown` with `X-Modbridge-Shutdown: 1`, retention placement per
   ACQUISITION-PATH steps 4–6.
2. **INTAKE-VERIFICATION.md** — census invocation for
   `worker-dll-mod-retention/inspect_dll_census.py` (verified CLI:
   `inputs… -o out.json`, pure-stdlib in-memory), baseline delta procedure
   against `baseline-dll-census.json`, artifact checklist, acceptance gates.
3. **CLAIM.json** (written first), this REPORT.md (written last).

## Evidence base (all read this session)

- Prior designs: worker-dll-mod-retention (ACQUISITION-PATH/RETENTION-CRITERIA/
  INVENTORY); the blocked acquisition-prep and nexus-session-prep outputs in the
  session-CWD tree (their BLOCKED/PREREQS analysis was correct — their inputs
  genuinely were absent from THAT workspace; the real materials are in this one).
- Real interface code: `outputs/ps5-mod-bridge/native-host/{nexus_client.h/.cpp,
  nexus_download.h/.cpp, auth.h, http.c, host.c, jobs.h, build.py, oauth_config.py,
  download_store/store.cpp, download/verify.py}`.
- Verified receipts: `native-nexus-download-jobs-1/actual-http-tls-1/run-3/
  receipt.json` (all checks passed incl. real PKCE login, real archive transfer,
  hash unchanged, CSRF/cookie/origin refusals) and `fixture-config-1/oauth-config.json`.
- macOS CA bundle `/etc/ssl/cert.pem` verified present (333483 B, under the 1 MiB cap).

## User-only prerequisites (gates, none executable without them)

1. Real Nexus OAuth `client_id` + approved scope (user's own registration; user
   handles all sign-in/MFA; no secrets handled by workers).
2. Explicit GO for one download + the P3RFix page URL and file choice (pins
   domain/mod_id/file_id at run time — never invented here).
3. Premium-status awareness (free account needs the website-grant resume branch).
4. Browser on this Mac for the loopback callback; license fact for the manifest.

## Controller-side open item (stated honestly)

The exact `--download-profile` value used by the verified 2026-10-02 build is not
recorded in any manifest I could grep (commands.json holds only post-build
commands). `--curl-source` is pinned. The controller must locate the retained
matching curl/MbedTLS profile under `.superpowers/native-dependency-spike/`
before Phase A; the runbook flags this as a hard pre-step, not a guess.

## Risk note

The full pipeline is receipt-verified end-to-end against a local TLS fixture
(`live_account:false`). The first REAL-network run (real DNS, real CA trust,
real OAuth scope, real grant flow) is prepared but not yet verified; the runbook's
stop/refusal conditions cover first-run failure without replay.

## Bounds compliance

No network or console execution of the prepared sequence (only local read-only
inspection commands were run); no nested subagents; no credentials read, echoed,
or written (`work/nexus-test.crt/key` untouched); tool calls used ≈29 of ≤40;
writes confined to this directory.
