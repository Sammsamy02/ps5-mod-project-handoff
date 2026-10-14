# AUDIT.md — worker-8-app-backend: actual app/backend route + capability state

Origin: worker-8-app-backend (re-dispatch), 2026-10-04. Evidence gathered read-only
from the existing app tree; file:line cites are exact at read time. Existing files
were NOT modified.

## 0. Executive summary

The existing app backend (`outputs/ps5-mod-bridge/native-host/`) is a real,
working read/auth/download/jobs host with a **read-only mod-service bridge**.
It can sign in, browse, download+analyze+prepare (PAK/IoStore/INI), and report
durable downloads — but it has **no working install/uninstall/records-write
path of its own**. That vertical exists PROVEN in the team composition
(worker-7 -> installer-composition -> e2e-final -> zip-to-mfb), running as a
separate CLI (`modinst_composed`) with its own durable records.txt store. The
two halves are not connected anywhere in the app. This worker delivers the
connecting drop-in modules (records store mapping + job actions + capability
report + modforge-service/1 adapter) plus the patches that would wire them in.

## 1. HTTP surface actually implemented (host app)

All in `outputs/ps5-mod-bridge/native-host/http.c` (dispatch at
`http_handle_request` http.c:801-928):

| Route | Method | Backing | Evidence |
|---|---|---|---|
| `/api/capabilities` | GET | host_info + self-test flags | http.c:395, 841-875 |
| `/api/auth/start`, `/api/auth/disconnect` | POST | OAuth (when `MB_HOST_OAUTH_CONFIGURED`) | http.c:426-460, 833 |
| `/auth/callback?` | GET | OAuth code capture | http.c:461, 842 |
| `/api/account` | GET | auth context | http.c:478, 876 |
| `/api/games`, `/api/games/<d>/mods[/<id>[/files]]` | GET | catalog via auth context | http.c:495-590 (catalog_route 495) |
| `/api/jobs` (GET/POST), `/api/jobs/<id>` | GET | nexus download/analyze/prepare jobs | http.c:634-716 (collection 651-672), `/api/jobs/` prefix 107 |
| `/api/generations` (GET/POST) | | IoStore generation jobs | http.c:651, 825 |
| `/api/acquisition-plan`, `/api/acquisitions` | POST | prerequisite planner/acquisitions | http.c:517-533, 717-748, 817-823 |
| `/api/downloads` | GET | retained original archives (download store list) | http.c:753-768 |
| `/api/mod-service` | GET | service.describe passthrough | http.c:771-799 |
| `/api/mod-service/records` | GET | records.list passthrough | http.c:773, 790-791 |
| `/api/detected-titles[/pages...]` | GET/POST | title detection | http.c:881-923, app.js:357-455 |
| static `/`, `/app.js`, `/style.css` | GET | web prototype | http.c:845-924 |

**Mod-service routes are read-only and passthrough only**: `mod_service_response`
(http.c:771) forwards exactly two canned requests via `mb_mod_service_read`
(`mod_service/read_bridge.cpp:9-11`: `service.describe`, `records.list`). There
is **no route that submits a job, prepares, installs, uninstalls or cancels**.

**Route gap (UI contract break)**: `web/app.js:935` fetches `GET /api/installed`
and filters `transaction_confirmed === true` + sha256; `http.c` contains **zero**
occurrences of `api/installed` (grep count 0). The Installed tab silently
degrades to its empty state on every load. Honest fix is either a real records
projection or an explicit error — not fabricated data.

**Job model reality** (jobs.h:119-160 doc + host.c:483-511): serialized queue,
at most 8 records, no background thread, no installation; "Public reports
establish analysis or configured private format preparation only;
runtime/installation remain false" (jobs.h:153-154). `jobs.cpp` implements
download/analyze/prepare/acquisition/generation kinds — no install kind.

## 2. Mod-service bridge: contract present, backend optional

- Contract: `mod_service/vendor/modforge_service.h` (`MFS_CONTRACT
  "modforge-service/1"` line 51) + `vendor/SERVICE.md` — full method surface:
  `service.describe, capabilities.query, game.register, mod.register_download,
  mod.prepare, install.plan, install.apply, mod.uninstall, mod.forget,
  records.list, game.status, job.status, job.cancel, job.list` (SERVICE.md:87-196).
- Owner lifecycle: `mod_service/owner.h:30-46` — open validates that describe
  advertises MFS_CONTRACT + `mod.register_download` + `records.list` before the
  bridge is exposed (owner.cpp does this at startup; host.c:442-455).
- Registration writer: `mod_service/register_bridge.cpp:236-306`
  (`mb_mod_service_register_download`) validates retained download-store rows
  (`lookup` 224-238), calls backend `mod.register_download`, and requires the
  returned record to have `state=="downloaded"`, `title_id` PPSA/CUSA + digits
  (`title_valid` 41-46), and `source{provider,domain,mod_id,file_id}`
  (`record()` 214-221) — i.e. **any drop-in backend must emit records.list rows
  in exactly that shape or the app refuses it with 502**.
- State model per SERVICE.md:46-57: `downloaded -> prepared -> installed`,
  backward on uninstall; `running` is per-game, derived ONLY from the runtime
  status file (SERVICE.md:53, 190-191).
- `app_service_startup` checkpoint: service startup proven on host (9 service
  build tests, owner lifecycle 5, actual HTTP reopen) with a **reference
  archive**, `console_executed:false` — the linked backend was never a real
  installer; STARTUP.md:3-8 says "The ordinary app build leaves ModForge
  disabled."

## 3. Existing durable stores in the app

- Download store (`download_store/store.h`): open/publish/lookup/list/metadata
  (store.h:24-49); IDs are `dl-` + 64 hex (register_bridge.cpp:48-52
  `download_valid`); app route `/api/downloads` lists them.
- Nexus jobs (`jobs.h:9-18`): flat JSON intents, cookie-scoped records,
  archive cap 1 GiB, prepare cap 512 MiB (jobs.h:243-244); `native_nexus_download_jobs`
  checkpoint: 26 actual HTTP/TLS checks, 21 transfers, 65 job controls,
  `installation_supported:false`, `catalog_and_scan_jobs_explicit_config_only:true`.
- Provider client (`native_nexus_provider_client` checkpoint): retained API
  `mb_nexus_client_request_v1` / `mb_curl_provider_transfer_v1` /
  `mb_curl_download_v1`; host+native builds sha-pinned, `console_executed:false`.

## 4. The PROVEN install stack (team composition, not linked into the app)

Chain: worker-7-installer (modinst core + mfb) -> worker-installer-composition
(compose_installer.py, console payload 647928 B / 116 imports / 0 missing,
host binary 26/26) -> worker-e2e-final (gap-1 records wiring into console
payload; download stage to real curl-owner sink ABI, 31/31 e2e suite) ->
worker-e2e-rehearsal (61 PASS / 0 FAIL full staged rehearsal, GO conditions) ->
worker-zip-to-mfb (auto-bridge: downloaded zip -> kind-1 mfb; 75/75) ->
worker-ps5io-rdwr (F1 RDWR=0x0002 closed; native download ELF 1197880 B,
123 imports, 0 missing).

### 4.1 The records store (the interop contract this worker maps to)

`worker-7-installer/src/modinst.c`:
- File `<state_dir>/records.txt`, header `modforge-records\t1`, rows written by
  `records_save()` (modinst.c:340-362): `mod\t<id>\t<title>\t<state>\t<bundle|->`,
  `variant\t<mod>\t<group>\t<option>\t<bundle>\t<is_default>`,
  `choice\t<mod>\t<group>\t<option>`,
  `game\t<title>\t<engine>\t<sdk>\t<generation>\t<loadlist_sha>`;
  parsed by `records_load()` (modinst.c:296-338). Token rule: printable ASCII
  0x21-0x7e, 1..128 (is_token, modinst.c:218-226).
- States: registered=`downloaded` (modinst.c:1176), prepare sets `prepared`
  (modinst.c:1225-1227), install commits `installed` / uninstall back to
  `prepared` (modinst.c:844-846, 1142-1144). **Running is never stored** —
  derived from `<root>/runtime-status.txt` ack lines
  `state\t<seq>\t<state>\t<code>\t<detail>` (read_runtime_status,
  modinst.c:1240-1263; SERVICE.md:190-191 agrees).
- Transition rules proven by e2e-final suite: only `downloaded -> prepared` is
  defined; re-prepare idempotent; unknown/wrong state refuses exit 2 with
  quarantine retained (REPORT.md §2, checks B1-B4b).

### 4.2 The CLI action surface (what job actions delegate to)

Composed binary usage (verified by executing
`worker-zip-to-mfb/composition/build/host/modinst_composed` with no args):

```
register-mod --id ID --title TITLE [--bundle FILE | --variant GROUP/OPTION=FILE ...] [--defaults g=o,...]
prepare --title T --mods a,b [--choices g=o,...]
install --title T [--mods a,b] [--remove x,y] [--choices g=o,...]
        [--cancel-after N | --cancel-at pre-stage|post-stage|swap-journal|rename1]
records [--title T]
simulate-runtime --title T --rt-state activated|engine-ready|mods-started|failed|stopped
download --job JOB.tsv                (stage 1; console flavor refuses unless linked)
prepare-archive --title T --archive F --quarantine D [--mod ID] [--no-auto-bundle]
recover
```

Exit codes (modinst.c:54-60): 0 ok, 1 internal/io, 2 bad request,
3 not ready (blockers), 4 needs choices, 5 cancelled.
Cancel is cooperative at proven safe points with "previous root untouched"
(modinst.c:91-93, 1046-1089); installs are transactional root swaps with
journal + `.modforge-root` ownership marker (worker-7 REPORT; modinst.c:1035).

### 4.3 Readiness/limits (honesty constraints)

- worker-e2e-rehearsal READINESS §1-3: everything so far is HOST-side; console
  stage 1 needs the ps5io-rdwr relink + signed-in provider session; `lua_mod`
  installs files but stage 5 must end `running=0` without a real UE4SS ack.
- `native_nexus_download_jobs` / `app_service_startup` checkpoints:
  `installation_supported:false`, `console_executed:false`.
- App-side install/uninstall is therefore NOT proven on console; this worker's
  modules must keep `running` ack-gated and never claim installation support.

## 5. Gap list (what this worker delivers against)

1. No app route can drive prepare/install/uninstall/cancel (§1).
2. No modforge-service/1 backend exists in-tree; only the reference archive
   path (§2, STARTUP.md).
3. The proven records.txt store has no reader/writer module the app can link;
   the app's only records view is the passthrough GET (§1-2).
4. `/api/installed` route missing while web/app.js:935 calls it (§1).
5. No capability report exists that distinguishes linked-installer /
   download-linked / runtime-ack states honestly (jobs.h:153-154 hard-codes
   runtime/installation false for the download/analysis layer only).

## 6. Bounds kept

Read-only audit; no existing file modified (patches delivered separately in
PATCHES.md); no network, no console, no credentials touched.
