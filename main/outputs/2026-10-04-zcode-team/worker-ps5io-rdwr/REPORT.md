# REPORT.md — worker-ps5io-rdwr: F1 closed, §1A native download link delivered AND executed

Origin: worker-ps5io-rdwr, 2026-10-04 (ZCode team, PS5 mod-manager).
Everything here is host-side/local-toolchain work: no network, no console
execution, no file outside this output dir was modified (all upstream inputs
read-only; vendored copies + recorded patches only). Pinned SDK
(sdk-v0.43 prospero-clang) and pinned LLVM 21.1.8 exclusively.

## 1. Delivered

| Artifact | Path (here) | Status |
|---|---|---|
| Extended adapter (F1) | `src/ps5_io.{c,h}` + `patches.json` | vendored copy of worker-installer-ps5io + 6 recorded patches (3 c / 3 h), upstream sha e7b01ad5… untouched |
| Host suite (extended) | `test/test_ps5io_host.c`, `test/run_tests.sh` | original 74 checks + 17 new = **91 pass / 0 fail**, plain and ASan+UBSan |
| Console object build | `build_console.sh` -> `receipts/console-build.log` | compiles clean; undefined set unchanged (sceKernel* + memcpy/strlen/memset); provider check clean |
| §1A relink script | `native_download_link.py` | controller-runnable; `--verify-only` mode; 26-input sha verification + abort-on-missing import provider check |
| **Native download ELF** | `native-link/modbridge-installer.download.elf` | **1197880 bytes, sha256 fbd355568ccc615f…, 123 imports, 0 missing, retained API present** |
| §1A documentation | `NATIVE-DOWNLOAD-LINK.md` | exact inputs + hashes, command sequence, composition point, link-order decision |
| E2E regression proof | `receipts/{host-extended-build.json,smoke-host.log,smoke-e2e.log}` | e2e suites re-run against a host binary rebuilt with the EXTENDED ps5_io |

## 2. F1 — the RDWR extension (the SDK-cited bit value)

`archive/private/file_sink.c:31` admits only an fd whose `fcntl(F_GETFL)`
O_ACCMODE is exactly O_RDWR; the exported `ps5_io` flag set had no RDWR
member, so the download TU's console branch could only open WRONLY — the
sink would refuse it (FD_REFUSED) at stage 1.

- New adapter flag `PS5IO_RDWR = 64` (next free symbolic bit, keeping the
  adapter convention of NOT being the raw O_* value).
- It maps to the pinned SDK's own `O_RDWR`: **`target/include/sys/fcntl.h`
  of ps5-payload-sdk sdk-v0.43 defines `O_RDONLY 0x0000 / O_WRONLY 0x0001 /
  O_RDWR 0x0002 / O_ACCMODE 0x0003`** (BSD layout; receipt
  `receipts/sdk-oflags.txt`; `build_console.sh` re-greps it and aborts if
  the value ever changes). Host build maps to host `<fcntl.h>` O_RDWR (also
  0x2 on macOS).
- Validation extended in `ps5io_open` (both flavors, before any syscall):
  exactly one access mode among RDONLY/WRONLY/RDWR; no unknown bits; TRUNC
  or APPEND on RDONLY refused; DIRECTORY on a write-capable mode refused —
  all `-EINVAL`. Every previously valid caller combination stays valid.
- RDWR fds keep the `O_NOFOLLOW|O_CLOEXEC` hardening (not write-only), so a
  fresh sink tmp cannot be redirected through a planted symlink.
- One recorded patch over the immutable download TU (staged by the link
  script, never in place): console branch of `mi_open_sink_rdwr`
  `PS5IO_WRONLY|CREAT|TRUNC` -> `PS5IO_RDWR|CREAT|TRUNC` (anchor unique,
  sha-recorded in `receipts/native-link/native-link.json`).

Host evidence: the suite asserts the exact sink admission contract on a
PS5IO_RDWR fd (F_GETFL accmode == O_RDWR, not append, ELOOP on symlink
plant), write+positional-read on the same fd, TRUNC-free RDWR preserving
content, and -EINVAL refusal of all eight invalid combos; it also proves a
WRONLY fd FAILS the sink contract — i.e. the pre-F1 console branch could
never have passed stage 1.

## 3. §1A — native download link (executed, reproducible)

`native_download_link.py` performed the RUNBOOK §1A relink end to end:
verified 26 sha-recorded inputs, staged the RDWR-patched download TU,
re-ran the pinned dependency preparation (`prepare_download("native",…)` /
`prepare_archive("native",…)`, each with its own full pin verification),
compiled 14 console TUs, and linked with `-Wl,--no-undefined,-z,now` kept
(`-Wl,--gc-sections`, `-lkernel_web`, link_layout caller script). The
import provider check resolved all 123 dynamic imports against the pinned
SDK target libs (0 missing) and the four retained `mb_curl_*` API symbols
are statically present. The host stub `mb_curl_stub_host.c` was never in
the link (asserted). Full detail + sequence: `NATIVE-DOWNLOAD-LINK.md`.

Deviations, recorded honestly:
- The three download TUs + file_sink compile directly with prospero-clang,
  replicating the retained wrapper's forced includes instead of executing
  the wrapper (no `-nostdinc`; SDK sysroot supplies `<fcntl.h>`).
- ONE mbed tree in the link: the first attempt aborted on a genuine
  duplicate (`mbedtls_platform_gmtime_r` in owner.c AND the archive
  closure's libmbedcrypto.a); the archive closure's libmbedcrypto.a was
  dropped so all mbed symbols resolve from the download closure's pinned
  mbed libs (same MBED_PIN, the retained consumer's exact link set).
- Console closure TUs keep the e2e composition's documented relaxation
  (-Wall/-Wextra, no -Werror) for the adapted composition sources.

## 4. No-regression proof (host)

- Extended adapter suite: **91/0**, plain + ASan/UBSan
  (`receipts/host-test.log`, `receipts/host-test-asan.log`).
- The e2e-final host binary was REBUILT from its own recorded receipts with
  ONLY ps5_io.o replaced by the extended compile
  (`receipts/host-extended-build.json`): their suites re-run clean —
  **smoke_host 26/26** (`receipts/smoke-host.log`), **smoke_e2e 31/31**
  (`receipts/smoke-e2e.log`) via vendored copies of the two scripts with
  only BIN/REC redirected into this dir (`test/smoke-copies.json`).
- Upstream dirs untouched: worker-installer-ps5io sources still at their
  recorded shas; worker-e2e-final receipts/logs not overwritten.

## 5. Remaining console-only prerequisites (precise restatement)

1. **Signed-in owner session**: the provider URI/entitlement resolution
   (`mb_nexus_resolve_download_v1`) runs under the mod-provider user
   session owned by the DOWNLOAD OWNER process — not this installer, not
   this worktree. The installer sees only the resolved HTTPS URI inside the
   job TSV (`id`,`title`,`url`,`intake` required; optional `sha256`,
   `timeout_ms` 1..300000, `connect_timeout_ms`, `max_bytes`; unknown keys
   refused). No credentials exist in code, job files, records or logs.
2. **One-job-per-process semantics**: exactly one
   `mb_curl_runtime_start_v1` per process, one job per
   `download --job JOB.tsv` invocation, `mb_curl_runtime_close_v1` always;
   the ABI has no reset/retry/rebind. Failed jobs (refused/timeout/cut/
   sha-mismatch) leave NO intake file, NO `.tmp`, NO record row — re-running
   the same job changes nothing until the job/owner state changes; never
   replay a failed job blindly (RUNBOOK §1C).
3. Payload deployment + the runtime-ack component are still open for stages
   4/5; the UE4SS component remains a placeholder (RUNBOOK §0). Nothing in
   this report executed on a console.

## 6. Reproduce

```
sh test/run_tests.sh            # 91/0 plain + sanitizers
sh build_console.sh             # console objects + provider check + SDK citation
python3 native_download_link.py --verify-only   # 26-input sha receipt
python3 native_download_link.py                 # full §1A relink (fresh out dir)
```
