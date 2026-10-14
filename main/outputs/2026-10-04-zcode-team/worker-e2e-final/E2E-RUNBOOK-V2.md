# E2E-RUNBOOK-V2.md — console acceptance run, staged procedure (wired edition)

Origin: worker-e2e-final, 2026-10-04 (ZCode team, PS5 mod-manager).
Supersedes the staged procedure in worker-installer-ps5io/CONSOLE-E2E-PLAN.md
and worker-installer-composition REPORT §7. It reflects the two closed gaps:

- **gap-1 (records wiring):** `prepare-archive --mod <id>` now performs the
  durable `downloaded -> prepared` transition in worker-7's records line
  store on success (idempotent re-run; unknown/installed mods refuse with the
  quarantine retained). The load-list sha256 stays the ONLY install/recovery
  commit truth (unchanged worker-7 semantics).
- **gap-2 (download hookup):** the composed binary has a `download --job
  JOB.tsv` command that runs the REAL exclusive curl-owner transport ABI v1
  (`mb_curl_runtime_start_v1` -> `mb_curl_download_v1` ->
  `mb_curl_runtime_close_v1`, receipt `callable_archive_download_transport`
  @ ba0c4f1) with explicit-config-only job files, streaming through the REAL
  file sink (sha256 + sync + readback) into `/data/modbridge/intake/<id>.zip`,
  then registers the mod `downloaded`. PROVEN host-side only, over the
  contract-faithful `file://` stub transport (31/31, see REPORT).

Everything below marked **[HOST-PROVEN]** is verified by
`test/smoke_host.sh` (26/26) + `test/smoke_e2e.sh` (31/31); everything marked
**[TARGET-OPEN]** still needs the console run to be considered true.

## 0. Payloads/modules that must be present (composition manifest)

| Component | Source | Status |
|---|---|---|
| Installer payload ELF (gap-1 wiring included) | this dir `composition/build/console/modbridge-installer.payload.elf` (664400 bytes, sha256 3860cd56…, 116 imports, 0 missing) | **[HOST-PROVEN]** core; console runtime behavior **[TARGET-OPEN]** |
| Native download closure (curl owner + sink + ps5 entropy) | `outputs/ps5-mod-bridge/native-host/download` (+ its dependency closure per `download/dependency_build.py`) | **[TARGET-OPEN]** — NOT yet linked into the console payload; see §1A |
| ps5_io RDWR flag extension | upstream ask to worker-installer-ps5io | **[TARGET-OPEN]** — required before §1A (sink demands an O_RDWR fd; see REPORT F1) |
| Runtime ack emissions (five mfa_status lines at module_start) | worker-runtime-ack/runtime-ack.diff (RuntimeAckSink wired into RuntimeOwner/UE4SS backend) | **[TARGET-OPEN]** — injected runtime must carry it or stage 5 honestly fails |
| UE4SS runtime component | deferred component (capability table `state/caps.tsv` gates `planned`/`integrating` frameworks) | **PLACEHOLDER** — mods with `lua_mod` roles install files and the records/install path treats them normally, but in-game startup needs UE4SS actually present and enumerable from the activation views. Without it, stage 5 must end `running=0` (honest fail), never fabricated |
| Saved corpus zips (first-run fixtures) | `.superpowers/native-observations/nexus-real-mods-2026-10-01/` (`minimap-v3.1.zip`, `infinite-stamina-v1.zip`, `leon-2k-v1.1.zip`) | present; read-only |

## 1. Stage 1 — download (the first REAL console download)

### 1A. One remaining controller link step [TARGET-OPEN]

The `download` command's call path is compiled and host-proven, but the
console payload link does not yet include the download closure. Controller
must relink the console flavor adding, from the pinned closure:
`owner.c`/`curl_download.c`/`ps5_entropy.c` + the pinned native curl/PSA
closure (`download/dependency_build.py`), keeping
`-Wl,--no-undefined,-z,now` so any missing symbol aborts loudly. The closure
retains exactly the symbols this command calls (receipt
`native_app_download_build/native_closure/retained_api_symbols`:
`mb_curl_runtime_start_v1`, `mb_curl_download_v1`,
`mb_curl_runtime_close_v1`, `mb_curl_ps5_entropy_read32_v1`). The host test
stub `owned/mb_curl_stub_host.c` must NOT be linked on console.

### 1B. What the first REAL run needs (user session state)

- A signed-in mod-provider user session owned by the DOWNLOAD OWNER process,
  not this installer: the provider URI resolution
  (`mb_nexus_resolve_download_v1`, live user, entitlement/grant, HTTPS link)
  runs under the owner's authenticated session. The installer receives only
  the resolved provider URI in the job file — **no credentials ever appear
  in code, job files, records or logs** (explicit-config-only; the transport
  result is scalar facts only).
- Job file on the console (TSV, unknown keys refused):
  `id`, `title`, `url` (the resolved provider URI), `intake` (default
  `/data/modbridge/intake`), optional `sha256` (expected content hash),
  `timeout_ms` (1..300000, default 300000), `connect_timeout_ms` (default
  10000), `max_bytes` (default 1 GiB sink cap).
- The exclusive curl/PSA owner: one start per process, one job per command
  invocation, close always; no reset/retry/rebind exists (by contract).

### 1C. Stage-1 procedure [HOST-PROVEN over the stub; TARGET-OPEN on console]

```
<service> --state /data/modbridge/state --roots /data/modbridge/roots \
    download --job /data/modbridge/jobs/<id>.tsv
```
Expected on success: `downloaded mod=<id> title=<T> state=downloaded
bytes=<n> sha256=<hex> intake=/data/modbridge/intake/<id>.zip`; the archive at
that path; sidecar `<state>/downloads/<id>.json` (content-addressed sha,
never the URL); records row `downloaded`; e2e-status.jsonl
`"stage":"stage-1-download","outcome":"ok"` plus an `end` line.
Failure (refused/timeout/cut/sha-mismatch): exit 1, honest status name, NO
intake file, NO `.tmp` leftover, NO record row — re-run fixes nothing until
the job/owner state changes; never replay a failed job blindly.

## 2. Stage 2 — prepare

- 2a archive flow [HOST-PROVEN]:
  `<service> prepare-archive --title <T> --archive /data/modbridge/intake/<id>.zip --quarantine /data/modbridge/intake/<id>.q --mod <id>`
  Success: quarantine members + `manifest-v1.bin`, journal retained, AND
  records row transitions to `prepared` (`records prepared mod=<id>`).
  Re-run: idempotent (`records unchanged`). `--mod` unknown or mod not in
  `downloaded` state: exit 2, quarantine retained.
- 2b bundle flow [HOST-PROVEN]: unchanged (`register-mod --bundle` / `prepare`).
- Expected: `prepared` distinct from `downloaded`; nothing under the mod root.

## 3. Stage 3 — install [HOST-PROVEN]
Unchanged from CONSOLE-E2E-PLAN.md stage 3; journal + load-list sha256 is the
commit truth; rollback probes (`--cancel-at rename1`, `--crash-at staged`,
`recover`) all still pass in the 26/26 suite.

## 4-5. Stages 4/5 — runtime ack [TARGET-OPEN]
Only the real injected runtime (runtime-ack.diff emissions) can produce
`running=1`; `simulate-runtime` covers the file format host-side only.
UE4SS component placeholder (§0) decides whether `mods-started` is reachable.

## 6. Stage 6 — uninstall [HOST-PROVEN]
Unchanged; removed mods drop back to `prepared`.

## Watch surfaces during the run
- `<state>/records.txt` — worker-7 line store (states: downloaded/prepared/
  installed; running is derived only from runtime-status.txt).
- `<state>/e2e-status.jsonl` — bounded stage events; a missing `end` line
  means the command died (never fabricated).
- `<state>/downloads/<id>.json` — content-addressed download truth.
- Console owner's own transport receipts (the installer never logs URLs).
