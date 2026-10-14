# REPORT.md — worker-app-download-runtime

Origin: worker-app-download-runtime, 2026-10-04 (ZCode team, PS5 mod-manager).
Host-side only: no network, no console, no credentials, no nested subagents.
`worker-app-backend-integration`'s vendored tree was COPIED, never modified;
the original app tree was never written; all writes stayed in this worker dir
(plus `/tmp` probe scratch, process-local only).

## 1. Result

**The "download-runtime build whose retained profile isn't recorded" is no
longer a gap: the recipe was recoverable from receipts, and the build has been
rebuilt host-side, smoked, and its mod-service surface re-verified 42/42.**

```
python3 test/run_api_tests.py     (my vendored copy, download gate)
== PASS=42 FAIL=0 ==              (receipts/api-tests-download-runtime.log)
```

| Deliverable | Path | Status |
|---|---|---|
| Claim (first) | `CLAIM.json` | written before any work |
| Retained profile (argv + shas, never lost again) | `receipts/download-runtime-profile.json` | recipe + exact paths + pinned shas + historical receipt pointer |
| Build receipts | `receipts/host-build.json`, `ab-probe.json`, `api-tests-download-runtime.log` | 2 builds, live endpoint matrix, 42/42 |
| Rebuilt download-runtime binary | `build/download-runtime/desktop_fixture-download/modbridge-native-host-desktop-download` | build_id `d2303686…`, sha256 `a78c160e…`, 1,665,672 B |
| desktop_fixture A/B control | `build/desktop-fixture/desktop_fixture/modbridge-native-host-desktop` | build_id `017178e0…` |
| Documentation | `DOWNLOAD-RUNTIME.md` | lineage, recipe, honest table |
| Report (last) | this file | — |

## 2. What the download-runtime build IS (lineage answer)

`build.py --profile desktop_fixture --download-profile
<retained curl/MbedTLS profile> --curl-source <curl tree>`. Concretely:

- `--download-profile` = `.superpowers/native-dependency-spike/native-curl-profile-experiment-1`
- `--curl-source` = `.superpowers/native-dependency-spike/curl-release`
- Sets `MB_HOST_DOWNLOAD=1` in generated config.h → links the serialized
  curl 8.18 + Mbed TLS 3.6.7 exclusive owner, OAuth/session runtime, Nexus
  metadata client, credential-free download producer, and the modmgr routes;
  excludes legacy `entropy.c`/`drbg.c` (reported `not_selected`).
- Output dir/artifact gain the `-download` suffix; desktop compilers forced
  to `/usr/bin/clang`/`clang++`; profile MUST stay at its original absolute
  path (the retained compiler wrapper embeds absolute forced-include paths,
  re-validated per build).
- Historical app-host-5 build additionally enabled **jobs**
  (`--archive-profile` + `--job-directory` + `--oauth-config`,
  libarchive/xz/zlib composition) — documented, not exercised by my minimal
  rebuild (hence 1,665,672 vs 1,909,128 B).

**Why the profile wasn't retained:** it was never recorded in any *index* —
but nothing was actually lost. The recipe is self-describing
(`download/dependency_build.py` sha-pins every retained input and the
wrapper's absolute paths), the app tree's `download/README.md` documents the
recipe shape, and the complete 2026-10-02 receipt exists intact at
`.superpowers/native-dependency-spike/native-nexus-download-jobs-1/app-host-5/desktop_fixture-download/`
(artifact sha256 `7fc202ad…` re-verified against its own manifest this
session). worker-dll-mod-acquire's "not recorded in any manifest I could
grep" was accurate about manifests, and its pointer to
`.superpowers/native-dependency-spike/` was exactly right.

## 3. Verification evidence

1. **Build clean on first attempt** in a byte-copy of the integration tree;
   `dependency_build.py` revalidated ALL retained inputs (wrapper, archives,
   config headers, patch, notices, vendor trees) during the build — zero
   mismatches, so the retained profile is byte-intact.
2. **Full-binary live smoke**: `/version` 200; `/report` shows
   `download_runtime{selected:true, initialized:true, entropy_callbacks:4,
   entropy_bytes:128, clock_samples:11}` — the curl owner genuinely starts.
3. **A/B proof** (`ab-probe.json`, both binaries probed live):
   `/api/downloads` 503 vs 404 is the one live-observable route difference;
   modmgr routes compile only with the download gate; worker-8's
   mod-service routes refuse 503 in both (they are ungated by design and
   only the download build can ever pass their session admission).
4. **Mod-service surface survives**: 42/42 integrated-API checks
   (copied driver, only path constants retargeted) on the
   `-DMB_HOST_DOWNLOAD=1` compile, real fork/exec driver + pinned installer
   (`f2215f2f…` re-verified on my copy).

## 4. Honest table (summary — full version in DOWNLOAD-RUNTIME.md §4)

| Added by the download-runtime build | Still pending |
|---|---|
| curl/MbedTLS exclusive owner linked AND started (real entropy/clock evidence) | Live-cookie HTTP round trip (needs a configured session: fixture or real OAuth config + client driving capabilities→auth) |
| modmgr routes compiled in (env-gated, 404 fall-through preserved) | Jobs/archive/oauth composition build (inputs exist; documented argv) |
| mod-service API 42/42 on the download-gated compile | `/api/jobs`, `/api/downloads` exercised beyond 503-refusal |
| Retained profile permanently recorded (argv + shas + receipt pointer) | Real OAuth client_id/scope (user-only), console payload consumption, UE4SS execution |

`console_executed:false` and `provider_contact:false` in every receipt here.
Nothing in this work claims live Nexus access, entitlement, or console
execution.

## 5. Bounds

~48 tool calls (target 40-80). CLAIM.json first, REPORT.md last. Writes
confined to this worker dir (plus process-local /tmp probe bodies). The
app-backend-integration worker dir was only READ (vendored copy + test
driver + pinned binary copied out); the original app tree,
`.superpowers/` retained inputs, and all global settings untouched.
