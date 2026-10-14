# EXECUTE-ACQUISITION — exact controller sequence for ONE P3RFix acquisition (2026-10-04)

Status: **PREPARED, NOT EXECUTED.** Every argument below is cited to code actually
read in this workspace (`$W` = `/WORKSPACE/.codex/worktrees/translation-core/what`).
This runbook is proposal-only until the user gives GO. One candidate, one download.

## 0. Candidate and authority

- Candidate: **P3RFix** — Persona 3 Reload (UE4.27), `dinput8.dll` proxy + plugins
  (RETENTION-CRITERIA.md candidate #2, ranked #1/#2 for first acquisition; proxy-hook
  shape, small archive, source-available).
- Pipeline authority: `native_nexus_download_jobs` (status complete_committed,
  commit fcc16d5…) and `callable_archive_download_transport` — receipts cited in
  `worker-dll-mod-retention/ACQUISITION-PATH.md`.
- Interface authority (read this session):
  - HTTP surface: `$W/outputs/ps5-mod-bridge/native-host/http.c`
    (`/api/capabilities`, `/api/auth/start`, `/auth/callback?`, `/api/account`,
    `/api/games…`, `POST /api/jobs`, `GET /api/jobs/<id>`,
    `POST /api/jobs/<32hex>/resume`, `/api/downloads`, `POST /shutdown`).
  - Job intent schema: `jobs.h` lines 133–160 ("exactly kind/game/mod_id/file_id/
    idempotency_key"); kind value `download_analyze` (`jobs.cpp:437,498`).
  - Auth flow (PKCE, loopback redirect, process-lifetime credentials): `auth.h`.
  - Host CLI: `host.c:294–307` (`--port` 1..65535, `--lifetime` 1..3600).
  - Build CLI: `build.py:420–494` (flags and coupling rules).

## 1. CRITICAL: rebuild is required — the existing binary cannot reach real Nexus

The verified app `…/native-nexus-download-jobs-1/app-host-5/desktop_fixture-download/
modbridge-native-host-desktop-download` was built with the FIXTURE OAuth config
(`fixture-config-1/oauth-config.json`): client `owned_native_jobs_client`,
scope `fixture_scope`, fixture CA, and **both provider hosts pinned to 127.0.0.1**
(`users.nexusmods.com:443:127.0.0.1`, `api.nexusmods.com:443:127.0.0.1`).
Its run-3 receipt records `console_executed:false, live_account:false` — the
verified end-to-end pipeline ran against a local TLS fixture. A real acquisition
requires a fresh build with a REAL OAuth registration and real DNS (no resolves).
Do **not** reuse the fixture binary for the live run.

## 2. Only the user can provide (gates — controller must confirm each before Phase A)

1. **Registered Nexus OAuth client_id and scope** for this app (user owns the
   Nexus Mods API registration; users handle all sign-in/MFA per studio rules).
   Workers never receive or store client secrets; the config below embeds only
   the public client_id (process-lifetime credentials, `auth.h` header comment).
2. **Explicit GO for exactly one download** (P3RFix), plus the **mod page URL**
   (pins the real `domain`, `mod_id`) and **which file** (`file_id`, version).
   These IDs are never invented here; they are read from the page/catalog live.
3. **Premium status**: a free account is supported but takes the website-grant
   resume branch (Phase E3; verified behavior `free_user_requires_separate_grant`
   in run-3 receipt). User must be ready to click the site download when asked.
4. Browser available on this Mac (loopback callback must land on 127.0.0.1).
5. License/attribution fact for the manifest (repo license; note if ambiguous).

## 3. Controller-only prerequisites

1. **Retained download profile**: `build.py` requires `--download-profile`
   ("explicit retained matching curl/MbedTLS profile") and `--curl-source`.
   `--curl-source` is pinned: `$W/.superpowers/native-dependency-spike/curl-8.18.0.tar.xz`
   (exists). The exact `--download-profile` value used by the verified 2026-10-02
   build is NOT recoverable from the manifests I inspected (only post-build
   commands are recorded in `commands.json`); BEFORE Phase A the controller must
   locate the retained matching profile under
   `$W/.superpowers/native-dependency-spike/` (candidates: `curl-build-http*`,
   `dependency-owner-1`, retained profile JSON beside them). Do not guess a path.
2. **Private scratch + store directories** (must pre-exist, private 0700, owned by
   the current user, distinct; `build.py:166–173,468–469`, `host.c:378–404`):
   ```
   mkdir -p -m 700 "$W/.superpowers/native-observations/dll-mod-retention-2026-10-04-1/host/scratch"
   mkdir -p -m 700 "$W/.superpowers/native-observations/dll-mod-retention-2026-10-04-1/host/store"
   ```
3. **Real OAuth config** — write `$W/.superpowers/native-observations/
   dll-mod-retention-2026-10-04-1/oauth-config-real.json` (schema pinned by
   `oauth_config.py:10–11,46–113`; exactly these fields, ≤16 KiB):
   ```json
   {"client_id": "<USER-PROVIDED real client_id>",
    "scope": "<USER-PROVIDED approved scope>",
    "redirect_uri": "http://127.0.0.1:8085/auth/callback",
    "ca_pem_path": "/etc/ssl/cert.pem",
    "users_resolve": "",
    "api_resolve": "",
    "timeout_ms": 15000,
    "connect_timeout_ms": 5000}
   ```
   Rules enforced by the code: `client_id` `[A-Za-z0-9_-]{1,128}` and not
   `public_test`; redirect must be exactly `http://127.0.0.1:<port>/auth/callback`
   and match the run port; CA file 1 B..1 MiB certificate-only PEM (macOS system
   bundle `/etc/ssl/cert.pem` verified present, 333483 bytes); resolves empty for
   real DNS; `1 <= connect_timeout_ms <= timeout_ms <= 30000`.

## 4. Phase A — build the real app (one command; ~minutes)

```
cd "$W/outputs/ps5-mod-bridge/native-host" && python3 build.py \
  --profile desktop_fixture \
  --download-profile "<controller-located retained profile>" \
  --curl-source "$W/.superpowers/native-dependency-spike/curl-8.18.0.tar.xz" \
  --oauth-config  "$W/.superpowers/native-observations/dll-mod-retention-2026-10-04-1/oauth-config-real.json" \
  --archive-profile "$W/.superpowers/native-dependency-spike/native-libarchive-lzma-experiment-1/closure-3/descriptor.json" \
  --job-directory   "$W/.superpowers/native-observations/dll-mod-retention-2026-10-04-1/host/scratch" \
  --download-store-directory "$W/.superpowers/native-observations/dll-mod-retention-2026-10-04-1/host/store" \
  --output-root     "$W/.superpowers/native-observations/dll-mod-retention-2026-10-04-1/app"
```
Flag coupling enforced by `build.py:455–489`: jobs require archive-profile +
job-directory + download runtime + oauth-config; download requires curl-source.
Artifact: `…/app/desktop_fixture-download/modbridge-native-host-desktop-download`.
Record `sha256` of the artifact and of `config.h` (contains BUILD_ID) in the receipt.

## 5. Phase B — launch the host

```
"$W/.superpowers/native-observations/dll-mod-retention-2026-10-04-1/app/desktop_fixture-download/modbridge-native-host-desktop-download" --port 8085 --lifetime 3600
```
- `--port 8085` must equal the port in `redirect_uri` (checked at startup,
  `host.c:352–356`); `--lifetime 3600` is the CLI maximum (`host.c:302–304`).
- Startup fails (exit 5) if runtime/auth/jobs creation fails — treat any early
  exit as a stop condition, not a retry trigger.

## 6. Phase C — session and user sign-in (cookie jar one file, e.g. `$J=/tmp/p3rfix.cookies` — never echoed, deleted after)

1. **Controller:** bootstrap session (any authenticated GET issues the session):
   ```
   curl -s -c "$J" -b "$J" http://127.0.0.1:8085/api/capabilities
   ```
   Response contains `"csrf_token":"<43 chars>"` and capability flags
   (`http.c:395–419`); `Set-Cookie: mb_session=…; HttpOnly; SameSite=Lax`.
   Require `"jobs":true` and `"download_analyze":true` before continuing.
2. **Controller:** start the OAuth transaction:
   ```
   CSRF='<token from step 1>'
   curl -s -c "$J" -b "$J" -X POST \
     -H "Origin: http://127.0.0.1:8085" -H "X-CSRF-Token: $CSRF" \
     -H "Content-Type: application/json" -d '{}' \
     http://127.0.0.1:8085/api/auth/start
   ```
   Response: `{"authorization_url":"https://users.nexusmods.com/oauth/authorize?…"}`
   (`http.c:426–448`). Mutations require Origin + X-CSRF-Token + cookie
   (`http.c:288–293,331–333,429`).
3. **USER:** open the authorization URL in a browser **on this Mac**, sign in to
   Nexus Mods (user handles all credentials/MFA), and approve the authorization.
   Nexus redirects to `http://127.0.0.1:8085/auth/callback?code=…&state=…`; the
   host completes the PKCE exchange, verifies live userinfo, and answers 303 →
   `/app` (`http.c:461–476`, `auth.h` header notes). Do not close the tab early.
4. **Controller:** verify identity (live userinfo check, never token possession):
   ```
   curl -s -b "$J" http://127.0.0.1:8085/api/account
   ```
   Expect a connected/verified summary (`http.c:478–496`). Session lives 1800 s
   (`auth.h:9`); if it expires, return to step 1 — never replay a failed auth.

## 7. Phase D — catalog preflight (pins the real IDs; no blind IDs)

With the domain/mod_id/file_id from the user's page URL:
```
curl -s -b "$J" "http://127.0.0.1:8085/api/games/<domain>"            # domain sanity (optional)
curl -s -b "$J" "http://127.0.0.1:8085/api/games/<domain>/mods/<mod_id>"
curl -s -b "$J" "http://127.0.0.1:8085/api/games/<domain>/mods/<mod_id>/files"
```
Confirm the chosen `file_id` is visible in the files listing (hidden/unpublished
files are rejected server-side, `auth.h:228–233`). `<domain>` is lowercase
`[a-z0-9][a-z0-9_-]{0,79}` (`nexus_download.cpp:131–139`) — take it from the
page URL, e.g. the Persona 3 Reload section domain; do not assume a spelling.

## 8. Phase E — submit ONE job and drive it to a terminal state

1. **Controller:** submit (body schema is exactly five flat fields; extra fields,
   nesting, `target_ref`/`nxm` are refused — `jobs.h:133–160`):
   ```
   curl -s -c "$J" -b "$J" -X POST \
     -H "Origin: http://127.0.0.1:8085" -H "X-CSRF-Token: $CSRF" \
     -H "Content-Type: application/json" \
     -d '{"kind":"download_analyze","game":"<domain>","mod_id":<mod_id>,"file_id":<file_id>,"idempotency_key":"p3rfix-acquire-2026-10-04-1"}' \
     http://127.0.0.1:8085/api/jobs
   ```
   (`idempotency_key`: 8..80 ASCII 33..126; IDs positive; queue cap 8 records;
   archive cap 1 GiB — P3RFix-class archives are far below it.) Response: one
   queued job record with a 32-hex `id`.
2. **Controller:** poll until terminal — the host advances one queued job per
   loop pass automatically (`host.c:483–512`):
   ```
   curl -s -b "$J" http://127.0.0.1:8085/api/jobs/<job_id>
   ```
3. **Free-account branch** (state `needs_nexus_authorization`): **USER** clicks
   the download for the chosen file on the mod page in the browser and, when the
   site offers it, copies the `nxm://` link (or approves the redirect). Then:
   ```
   curl -s -b "$J" -X POST \
     -H "Origin: http://127.0.0.1:8085" -H "X-CSRF-Token: $CSRF" \
     -H "Content-Type: application/json" \
     -d '{"nxm":"nxm://<domain>/mods/<mod_id>/files/<file_id>?key=<grant>&expires=<exp>&user_id=<uid>"}' \
     http://127.0.0.1:8085/api/jobs/<job_id>/resume
   ```
   (`jobs.h:201–222`: exact `nxm://game/mods/ID/files/ID` shape, literal
   key/expires/user_id query names once each, key 1..1024 ASCII; only this
   cookie's `needs_nexus_authorization` record can resume; grant is volatile,
   erased after use; the archive transfer itself sends NO account headers.)
   Then resume polling per step 2.
4. **Terminal states:** success = downloaded/analyzed with retention saved
   (`metadata_state` "saved"); failure = stored terminal failure (failed scratch
   archives are removed — `jobs.h:146`). **Never resubmit an identical failed
   intent** without reconciliation; duplicate intents with the SAME key return
   the same record, conflicting keys are refused (INTENT_CONFLICT).

## 9. Phase F — evidence collection

1. Saved original (store layout pinned: `<store-root>/<job-id>.archive`,
   `download_store/store.cpp:426`):
   ```
   sha256 "$W/.superpowers/native-observations/dll-mod-retention-2026-10-04-1/host/store/<job_id>.archive"
   shasum -a 256 <same file>          # readback on disk (ACQUISITION-PATH step 6)
   curl -s -b "$J" http://127.0.0.1:8085/api/downloads   # public retained-download listing
   ```
2. Save the full `GET /api/jobs/<job_id>` record and the `/api/downloads` body as
   receipt artifacts (public records contain no credentials/URI — run-3 check
   `public_jobs_no_credentials_or_uri`).
3. **Controller:** stop the host cleanly:
   ```
   curl -s -X POST -H "X-Modbridge-Shutdown: 1" -H "Content-Length: 0" http://127.0.0.1:8085/shutdown
   ```
   (`http.c:845–847`; requires the shutdown header and a Content-Length header.)
4. Delete the cookie jar `$J`. Never print it.

## 10. Phase G — retention placement (per ACQUISITION-PATH.md steps 4–6)

```
RET="$W/.superpowers/native-observations/dll-mod-retention-2026-10-04-1/p3rfix"
mkdir -p -m 700 "$RET"
cp "$W/.superpowers/native-observations/dll-mod-retention-2026-10-04-1/host/store/<job_id>.archive" "$RET/original.zip"
shasum -a 256 "$RET/original.zip"    # must equal the store hash recorded above
```
Write `manifest.json` next to it (model: verified `app-host-5` manifest format):
`attributions[]` (project "P3RFix", source repo + Nexus page URL, version,
license — note ambiguity instead of guessing), `inputs{}` with per-file
`bytes`+`sha256` of `original.zip`, and a provenance block (game, engine UE4.27,
mod_id/file_id/version, downloaded_at_utc, account flow used — "OAuth PKCE
browser sign-in" or "OAuth + website grant resume", job receipt path, original
request URL). Write `receipt.json`: job id, transfer stats from the job record,
sha256 verified on disk, build BUILD_ID + artifact sha256, host/CSR F outcome.
The archive stays inert: nothing is extracted or executed (Phase G hash and the
Phase H census are the only touches).

## 11. Stop / refusal conditions (binding)

- Any user prerequisite in section 2 missing → no build, no run.
- Host exits non-zero at startup → stop; reconcile; no blind relaunch replay.
- HTTP 401/403 (session/CSRF/Origin refused) → re-bootstrap from Phase C step 1;
  never replay the rejected request unchanged.
- Terminal job failure, `content_hidden`, or entitlement refusal → record and
  stop; do not try alternate files without a new user GO.
- ONE download per this runbook. No batch, no second candidate, no retries that
  create duplicate transfers. Credentials/cookies/grants never appear in any
  command log, receipt, or these notes.
