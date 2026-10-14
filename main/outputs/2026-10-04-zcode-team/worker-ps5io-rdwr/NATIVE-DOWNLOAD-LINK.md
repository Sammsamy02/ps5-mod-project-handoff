# NATIVE-DOWNLOAD-LINK.md — E2E RUNBOOK V2 §1A, executed and reproducible

Origin: worker-ps5io-rdwr, 2026-10-04 (ZCode team, PS5 mod-manager).
Closes the §1A gap of `worker-e2e-final/E2E-RUNBOOK-V2.md`: the console
payload now links the native download closure, and the F1 `ps5_io` RDWR
extension it depends on is in, host-tested and console-compiled.

**Executed result (this run, receipts in `receipts/native-link/`):**

- ELF: `native-link/modbridge-installer.download.elf` — 1197880 bytes,
  sha256 `fbd355568ccc615f…`
- 123 dynamic imports, **0 missing** (every import defined by a pinned SDK
  target lib; `receipts/native-link/console-import-providers.json`)
- all four retained API symbols statically present
  (`mb_curl_runtime_start_v1`, `mb_curl_runtime_close_v1`,
  `mb_curl_download_v1`, `mb_curl_ps5_entropy_read32_v1`)
- `curl_global_init`/`getentropy` are NOT imports: curl/MbedTLS are the
  static pinned archives; entropy rides `sysctl(KERN_ARND)` via
  `ps5_entropy.c` (import `sysctl` -> libkernel*)
- host stub `owned/mb_curl_stub_host.c` is NOT in the link (asserted)

## 1. Exact input list

Recorded with sha256 in `receipts/native-link/native-link.json`
(`verify.inputs`) and `receipts/native-link/verify-only.json`; the script
re-verifies every one before any compile and aborts on mismatch:

| Role | Input |
|---|---|
| Extended adapter (F1) | `worker-ps5io-rdwr/src/ps5_io.{c,h}` (vendored copy + `patches.json`) |
| Download TU (one recorded patch) | `worker-e2e-final/owned/modinst_download.c` -> staged copy, console branch `PS5IO_WRONLY` -> `PS5IO_RDWR` (anchor unique, sha-recorded) |
| Composed core (patched by e2e-final) | `worker-e2e-final/composition/vendored/modinst_core.c` (+ `modforge_activation.c`, `ps5_prepare_bridge.c`, `archive/intake.c`, `archive/private/posix_compat.c`) |
| Owned adapters | `worker-e2e-final/owned/{ps5_io_dir.c, modinst_archive_prepare.c, e2e_status.c, modinst_download.h}` |
| Download closure TUs | `ps5-mod-bridge/native-host/download/{owner.c, curl_download.c, ps5_entropy.c}` (+ `owner.h`, `curl_download.h`, `runtime.h`) |
| Real sink | `ps5-mod-bridge/native-host/archive/private/file_sink.{c,h}` |
| Dependency recipes | `download/dependency_build.py`, e2e-final vendored `archive/dependency_build.py`, `archive/link_layout.py` |
| Pinned closures (sha-verified by the recipes) | patched `libcurl.a` + `libmbedtls.a` + `libmbedx509.a` + `libmbedcrypto.a` (DL profile `native-curl-profile-experiment-1`, curl source `curl-release`, config id `86adf346…`); `libarchive.a` -> `liblzma.a` -> `libz.a` (archive profile) |
| Toolchain | pinned SDK `sdk-v0.43/ps5-payload-sdk` `bin/prospero-clang` + pinned LLVM 21.1.8 `llvm-config`/`llvm-nm`/`llvm-readobj`, resource dir per composition |

## 2. Command sequence (`native_download_link.py`)

```
python3 native_download_link.py --verify-only   # phase 1 only, receipt only
python3 native_download_link.py                 # full §1A relink
```

1. **Verify** — sha256 of all 26 inputs; extended adapter identity vs
   `patches.json`; pinned SDK `target/include/sys/fcntl.h` must still define
   `O_RDWR 0x0002`; the `modinst_download.c` console-branch anchor must
   appear exactly once; stub exclusion asserted. Any failure aborts before
   compiling.
2. **Stage** — fresh `native-link/` dir; staged `modinst_download.c` with the
   single RDWR patch; staged extended `ps5_io.{c,h}` (upstreams untouched).
3. **Prepare closures** — `prepare_download("native", …)` and
   `prepare_archive("native", …)` re-run their full pin verification
   (archives, wrapper, configs, LLVM tools, sdk.zip) into fresh outputs.
4. **Compile** — 14 console TUs with `prospero-clang` (core flags mirror the
   e2e composition; the four download TUs replicate the retained wrapper
   contract: `-DMBEDTLS_CONFIG_FILE="tls_config.h"`, forced includes
   `common.h` + `tls_effective.h`, DL include dirs, `-I` modbridge include
   root; `-nostdinc` NOT imposed — SDK sysroot provides `<fcntl.h>` etc.,
   documented deviation). `-DMI_HAS_DOWNLOAD=1` enables the dispatch hook.
5. **Link (the composition point)** — this is where the download TU joins
   `compose_installer`'s console payload: the e2e-final object set + the five
   download TUs + `libarchive -> liblzma -> libz -> libcurl(patched) ->
   libmbedtls -> libmbedx509 -> libmbedcrypto` + `-lkernel_web`, with
   `-Wl,--no-undefined,-z,now` (kept), `-Wl,--gc-sections`, the
   `link_layout` caller script and a map file.
6. **Verify ELF** — `llvm-readobj` (headers/PT_DYNAMIC, no stderr);
   `llvm-nm -D --undefined-only` import census; provider check over
   `$SDK/target/lib/*.so` (**abort on any missing import**); retained API
   symbols must be defined in the image.

### Link-order decision recorded during execution

First link attempt died loudly (by design, `--no-undefined` + duplicate
detection): `mbedtls_platform_gmtime_r` defined both by `owner.c` and by the
archive closure's `libmbedcrypto.a`. Resolution: ONE mbed tree — the archive
closure's `libmbedcrypto.a` is dropped and all mbed symbols resolve from the
download closure's pinned mbed libs (same MBED_PIN `068ff08…`, exactly the
retained consumer's link set). Evidence: first attempt's
`receipts/native-link/console-link.stderr` (preserved by rerun history in
this REPORT), final link clean.

## 3. Console-only prerequisites that remain (nothing here ran on a console)

- A **signed-in mod-provider user session owned by the DOWNLOAD OWNER
  process** (not this installer): URI/entitlement resolution happens under
  the owner's session (`mb_nexus_resolve_download_v1`); the installer only
  receives the resolved HTTPS URI in the job TSV. No credentials ever appear
  in code, job files, records or logs.
- **One-job-per-process owner semantics**: one `mb_curl_runtime_start_v1`
  per process, one job per `download --job` invocation, close always; no
  reset/retry/rebind exists in the ABI. A failed job must not be replayed
  blindly (RUNBOOK §1C).
- Deployment of `modbridge-installer.download.elf` as payload + the runtime
  ack component before stages 4/5 mean anything (RUNBOOK §0/§4-5).
