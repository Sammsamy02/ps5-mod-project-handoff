# DOWNLOAD-RUNTIME.md — the download-runtime build, recovered and rebuilt

Worker: worker-app-download-runtime, 2026-10-04. Host-side only: no network,
no console, no credentials. `worker-app-backend-integration`'s vendored tree
was **copied, not modified**; the original app tree was never written.

## 1. The gap, closed

Two prior workers hit the same wall:

- worker-app-backend-integration REPORT §3: *"a real session needs the
  download-runtime build — the retained `--download-profile` inputs are NOT
  recorded in this workspace (worker-dll-mod-acquire noted the same)."*
- worker-dll-mod-acquire REPORT: *"The exact `--download-profile` value used
  by the verified 2026-10-02 build is not recorded in any manifest I could
  grep (commands.json holds only post-build commands). `--curl-source` is
  pinned."*

Both were right that no *index* recorded it — but the recipe was recoverable.
It was never lost; it was **undocumented**:

1. The app tree's own `native-host/download/README.md` names the recipe shape
   (`build.py --profile <p> --download-profile <retained curl/MbedTLS profile>
   --curl-source <curl tree>`) and the composition name
   `desktop_fixture-download`.
2. `native-host/download/dependency_build.py` hard-pins every retained input
   by sha256 (wrapper, archives, config headers, patch, notices) and
   re-validates the absolute profile path embedded in the retained compiler
   wrapper — a self-describing recipe.
3. The full historical build receipt **exists intact** at
   `.superpowers/native-dependency-spike/native-nexus-download-jobs-1/
   app-host-5/desktop_fixture-download/` (`manifest.json` 5.9 MB + all objects
   + `commands.json` argv + the binary itself). I re-hashed its artifact this
   session: sha256 `7fc202ad…` still matches its own manifest.

## 2. What the download-runtime build IS

`build.py` invoked **with** `--download-profile` (+ its mandatory
`--curl-source`). For `--profile desktop_fixture` that yields the
`desktop_fixture-download` composition:

- Generated `config.h` gets **`MB_HOST_DOWNLOAD 1`** — worker-8's
  `/api/mod-service*` routes (always compiled) now have their **session
  runtime linked**: serialized curl 8.18 + Mbed TLS 3.6.7 exclusive owner
  (`mb_curl_runtime_start/close_v1`), OAuth/session admission
  (`auth.cpp`, `oauth_code.cpp`), Nexus metadata client
  (`nexus_client.cpp`), credential-free archive producer
  (`nexus_download.cpp` + `download/owner.c`, `download/curl_download.c`,
  `archive/private/file_sink.c`), `nexus_bridge.cpp`, `src/nexus.cpp`.
- worker-app-backend-integration's **modmgr routes** (`/api/installed`,
  `/api/install`, enable/disable/delete) are compiled in (runtime env-gated
  by `MB_MODMGR_*`).
- Legacy `entropy.c`/`drbg.c` checkpoint objects are **excluded**; `/report`
  marks them `not_selected` and shows the live curl owner's scalar metadata.
- Link gains the prepared (init-failure-backported) `libcurl.a` + retained
  `libmbedtls/libmbedx509/libmbedcrypto.a` and
  `-framework SystemConfiguration -framework CoreFoundation`.
- With `--download-profile` the desktop compilers are forced to
  `/usr/bin/clang` / `/usr/bin/clang++`.
- The **jobs/archive composition** (historical app-host-5 also had it) adds
  `--archive-profile` (retained libarchive/xz/zlib descriptor),
  `--job-directory`, `--oauth-config` → `analysis.cpp`, `jobs.cpp`,
  `download_store/store.cpp`, intake/groups/fd_io, register_bridge,
  `/api/jobs`, `/api/downloads`. **Not required** for the mod-service
  session surface; not exercised in my rebuild.

### Retained profile (never lost again — see `receipts/download-runtime-profile.json`)

```
--download-profile .superpowers/native-dependency-spike/native-curl-profile-experiment-1
--curl-source      .superpowers/native-dependency-spike/curl-release
```
(config_id `86adf346…`, curl commit `2eebc58c…`, Mbed TLS `068ff080…`,
patch `5c0f9f33…`; wrapper embeds absolute paths, so the profile must stay
at its original location. All inputs sha-revalidated during my build.)

## 3. Rebuild + verification (this session)

- Vendored **byte copy** of the app-backend-integration repo into
  `vendored-app/repo` (their tree untouched; only their `native-host/build`
  artifacts excluded). Pinned installer copied alongside, sha re-verified
  `f2215f2f…` = CONSOLE-CONTRACT §7 pin.
- **Build succeeded first try**: `desktop_fixture-download` /
  `modbridge-native-host-desktop-download`, build_id `d2303686…`,
  sha256 `a78c160e…`, 1,665,672 B (smaller than historical 1,909,128 B
  because the historical build additionally enabled jobs/archive/oauth).
- **Live smoke of the full binary** (`receipts/ab-probe.json`): `/version`
  and `/api/capabilities` 200; `/report` shows
  `download_runtime{selected:true, initialized:true, entropy_callbacks:4,
  entropy_bytes:128, clock_samples:11}` — the curl owner genuinely started
  and drew real host entropy. No provider contact, `console_executed:false`.
- **Mod-service surface survives**: the 42-check integrated-API suite
  (copied driver; only path constants retargeted) re-run against my tree
  with the download gate (`-DMB_HOST_DOWNLOAD=1`) →
  **PASS=42 FAIL=0** (`receipts/api-tests-download-runtime.log`).

## 4. Honest table — download-runtime build vs desktop_fixture build

Both binaries built by me from the same tree, probed live (full endpoint
matrix in `receipts/ab-probe.json`):

| Surface | desktop_fixture (MB_HOST_DOWNLOAD=0) | download-runtime (=1) | Verdict |
|---|---|---|---|
| `/version`, `/api/capabilities`, `/report`, `/api/account` | 200 | 200 | both; `/report` differs: download build reports live curl-owner metadata, legacy entropy/drbg `not_selected` |
| worker-8 `/api/mod-service[·/records·/request]` | compiled, refuse **503** (no session owner) | compiled, refuse **503** (session owner linked but no session configured) | routes exist in both by design; only the download build can ever pass admission |
| worker-app-backend modmgr routes `/api/installed` etc. | **compiled out** | **compiled in** (env-gated; 404 fall-through without `MB_MODMGR_*`) | compile-time difference; proven by build flags + their 42/42 suite |
| `/api/downloads` | 404 (handler not compiled) | **503** (compiled; needs jobs store) | live-observable DIFF |
| `/api/jobs`, `/api/generations` | 404 | 404 | jobs composition NOT built here (needs `--archive-profile/--job-directory/--oauth-config`; retained inputs exist and are documented) |
| linked runtime | legacy `entropy.c`/`drbg.c` checkpoint | curl 8.18/Mbed TLS exclusive owner, OAuth/session, metadata client, download producer, modmgr routes | structural, sha-pinned in `dependencies-download/manifest.json` |

### What the download-runtime build adds — honestly

- **Adds (proven here):** the session runtime is linked and **starts** (real
  entropy, real clocks, clean lifecycle in `/report`); modmgr routes compiled
  in; the mod-service API surface re-verified 42/42 on the download-gated
  compile; full dependency identity (argv, object shas, include closures)
  recorded for reproducibility.
- **Does NOT add (not claimed):** a live-cookie HTTP round trip. That needs a
  configured session (real OAuth `client_id`/scope or the fixture OAuth
  config + a client driving `GET /api/capabilities` → cookie/CSRF →
  `/api/auth/*`). All session-refusing endpoints honestly returned 503/4xx;
  `provider_contact:false`; `console_executed:false` everywhere. The fixture
  OAuth config (`fixture-config-1/oauth-config.json`, loopback-pinned hosts)
  exists and is referenced in the receipts — driving a full cookie flow
  against it is the natural next step but belongs to a session-flow worker,
  not this build-recovery task.
- **Console-pending (unchanged):** UE4SS in-game execution, real PS5 ELF
  execution of the download composition, live Nexus entitlement, staged
  command consumption by a payload — none claimed by anyone in this lineage.

## 5. Files

| File | What |
|---|---|
| `receipts/download-runtime-profile.json` | **THE retained profile**: exact paths, argv recipe, pinned shas, historical receipt pointer (path + sha), my rebuild identity |
| `receipts/host-build.json` | my two builds + live smoke + regression summary |
| `receipts/ab-probe.json` | full endpoint matrix, both binaries |
| `receipts/api-tests-download-runtime.log` | PASS=42 FAIL=0 |
| `vendored-app/repo` | my byte-copy of the app tree (build root) |
| `vendored-app/modinst_composed` | pinned installer, sha-verified |
| `build/download-runtime/desktop_fixture-download/` | the rebuilt download-runtime binary + `dependencies-download/manifest.json` + `commands.json` |
| `build/desktop-fixture/desktop_fixture/` | A/B control binary |
| `test/run_api_tests.py`, `test/modmgr_http_tests.c` | regression driver (paths retargeted to my copy) |
