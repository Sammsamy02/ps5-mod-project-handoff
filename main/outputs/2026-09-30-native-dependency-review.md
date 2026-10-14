# Independent native dependency evidence review — 2026-09-30

Final consolidation: **all actionable factual/documentary findings are ADDRESSED; the local dependency quality/evidence review is CLEAR.** Neither corrected curl/libarchive configure log attempts `./conftest`; generated target headers and recorded compiler/link flags match the audited builds. The Mbed TLS/route stage language, libarchive ELF-hash typo, and test-target wording are corrected. Local compile/archive/link evidence remains substantiated, with no dependency target-runtime claim. Earlier findings below remain as review history; the final appendix records their resolution.

The Mbed TLS source compilation, static archives, and ordinary SDK smoke-ELF link evidence check out. No host-header or host-library leakage was observed. This audit establishes local build/artifact facts only: no TLS handshake, entropy result, dependency import resolution, DNS, account access, or PS5 execution occurred in this review.

Reviewed `outputs/2026-09-30-native-network-archive-route.md`, `outputs/2026-09-30-native-mbedtls-compile-spike.md`, and existing isolated `.superpowers/native-dependency-spike/` inputs/artifacts. Production source, SDK/cache, original reports, receipts, and curl-spike work were not modified. No libraries were rebuilt, target binaries executed, console/accounts contacted, staging/commits performed, or subagents spawned. Only this owned report was written after the completed read-only audit.

## Verdict and findings

**Quality/evidence verdict: local Mbed TLS compile/link evidence is substantiated; target-runtime claims remain unverified.** No provenance mismatch or demonstrated host-header/library leakage was found. The route report is a reasonable earlier source assessment, with later Mbed TLS build results recorded in the compile-spike report.

1. **Clarify stale stage claims in the spike report.** Its line 3 says “No object was linked”; the earlier Evidence boundary at line 50 says “No link.” The later Static link smoke artifact section and actual retained ELF prove that local linking subsequently succeeded. Label these assertions as initial-stage history and summarize current evidence at the top. Similarly, the route report's no-cross-build assertion at line 27 should be qualified as the state at that earlier assessment. This is an evidence-presentation correction, not a build failure; root owns updates to those reports.
2. **Keep the smoke ELF separate from an observable target diagnostic.** `linked_probe.c` initializes Mbed TLS entropy, SSL-config, and X.509 contexts, requests 32 bytes with `mbedtls_entropy_func`, frees the contexts, and returns only a boolean exit status. Its optimized `main` retains those calls, but the program has no build-identity/status report or independent process-exit receipt. A future owned checkpoint should expose matching artifact/build identity, raw `sysctl` return/errno/returned length, then Mbed TLS entropy/DRBG results, without exposing random bytes.
3. **Verify positive progress of target sysctl results.** The pinned upstream `sysctl_arnd_wrapper` subtracts the returned length and retries until complete; a successful call returning zero bytes would make no progress and loop indefinitely. This is an unverified target assumption, not a demonstrated failure. Check raw returned lengths before relying on the unmodified polling loop; do not infer a bounded runtime entropy test from the current smoke program.
4. **Account for the entire eagerly resolved import closure.** The smoke ELF has `BIND_NOW`/`NOW PIE`; all its imports matter before `main`, including `__udivti3` and `sysctl`. Distinguish loader/import failure from an entropy API result. After import/entropy/DRBG evidence, the next meaningful controlled TLS checkpoint is a successful trusted chain plus rejection of a wrong hostname and an untrusted chain. None of these outcomes proves Nexus account/download behavior or settles production CA provenance/update policy.

## Exact pins and source provenance

The reported Mbed TLS tag/peeled commit is `mbedtls-3.6.7` / `068ff080b369adfac81509f9b57b2afabaf82dc5`; framework gitlink is `dde0c4a0e448a0552f18817dcea633bb851fd288`. The audit independently compared every regular file in the retained archives with its extracted source destination: 948 Mbed TLS files and 1,091 framework files, zero mismatches. This includes the upstream configuration; no source/configuration patch was found. Remote signed-tag status and gitlink selection were documented by the producing reports; this local audit did not independently repeat their network queries.

| Retained input/artifact | Exact bytes | Independently recomputed SHA-256 |
| --- | --- | --- |
| Mbed TLS commit tar.gz | 5681475 | `ca6bd316bbec49ef20088f39b8755fcaec0b7e45781506de55b17cd191ae6937` |
| Framework commit tar.gz | 1045083 | `3b0d4864fa877dc9165b90ff3d5b68505c6176c0b22d317d99fe8c89a993e9fd` |
| Cached LLVM 21.1.8 archive | 1503047352 | `b95bdd32a33a81ee4d40363aaeb26728a26783fcef26a4d80f65457433ea4669` |
| Scratch LLVM `llvm-ar` | 37214928 | `c2640fc0a747413fe27818531d4a22f979d91b7e299723a68225917c42574b50` |
| Scratch resource `cpuid.h` | 12681 | `af95bc7da3f66d3a62a5c75462f347f6330da5af6d173a5572042c8f68316b4d` |
| Initial entropy object, `obj/entropy_poll.o` | 1760 | `bf21d24fd56ff7067110595092fc72596855319cfebd8785356be1416b0548bb` |
| Final library `entropy_poll.o` | 1760 | `bf21d24fd56ff7067110595092fc72596855319cfebd8785356be1416b0548bb` |
| Public-header object | 1344 | `54e0afd75e106e091141d30f4fe243edf0ddfdf51c2a9fb8d93336be375afe4d` |
| `libmbedcrypto.a` | 1177678 | `7ccb34e79140f97cd3adea55688bae495c05f116b4e7639fb61eb5bd3ea09599` |
| `libmbedtls.a` | 592520 | `c7e0c6fd536607a45bd5b2d399446bfa5816410d1bcbde79f53b8fd82285d7ea` |
| `libmbedx509.a` | 129682 | `e9f437c9e22d1df05ee8475ff3ab8ea9ec2b566c4594c9bb5f33fea64d9345f3` |
| `linked_probe.elf` | 904904 | `7881ba55e149a645f200d453d8510eb371839a4ede932f51b0f45b46edc844c1` |

The scratch `llvm-ar` and `cpuid.h` bytes independently match those exact members of the hash-verified LLVM archive. `llvm-ranlib` is the archive's symlink to `llvm-ar`, and the scratch symlink matches. The restored resource include subtree contains 297 regular files. It is compiler resource content from the pinned distribution, not an injected macOS SDK header. The unchanged SDK v0.43 wrapper and pinned compiler paths were already verified in the independent native-core review.

## Header closure and entropy selection

Read-only `prospero-clang -E -v -x c /dev/null` with the documented scratch `-resource-dir` selected target `x86_64-sie-ps5` and only these system include roots: SDK `target/include`, scratch LLVM `lib/clang/21/include`, and SDK `target/include_common`. Representative `-M` closures for `entropy_poll.c` (74 dependencies), `aesni.c` (173), `ssl_tls.c` (111), and `x509_crt.c` (117) contained only the pinned source, scratch resource tree, and SDK paths. There was no `/usr/include`, Homebrew, or macOS SDK header leakage. These are independently recomputed closure observations under the documented flags, not a claim that a complete historical per-command build log exists.

`-E -dM` on pinned `entropy_poll.c` confirms `__FreeBSD__=9`, `__FreeBSD_version=1104001`, `KERN_ARND=37`, `HAVE_SYSCTL_ARND`, and `MBEDTLS_ENTROPY_C`; `HAVE_GETRANDOM` and `MBEDTLS_NO_PLATFORM_ENTROPY` were absent. Both retained entropy objects have exactly `U sysctl`. The public-header object has exactly `U mbedtls_ssl_init` and `U mbedtls_x509_crt_init`.

The optimized linked platform entropy function contains the `CTL_KERN=1`/`KERN_ARND=37` selector and calls the imported sysctl pointer; its loop uses the returned length. No `/dev/urandom` stdio fallback is compiled in this function. Other retained Mbed TLS functions still account for file/stdio imports in the full smoke ELF; those imports do not imply that the entropy function fell back to a file. `rand` appearing in the broader import closure does not establish a secure entropy source.

## Real smoke ELF and imports

All static archive members are x86_64 ELF objects: crypto 86, TLS 18, X.509 9 (113 total), zero AArch64 members. The linked ELF is ordinary SDK ET_DYN x86_64: entry 0, three 16 KiB-aligned loads, first load RWE, valid file/memory/table/prefix bounds. ELF header end=64, program-table end=288, section-table offset=903496, section count=22, section-table end/loader prefix=904904, maximum load file end=770124.

Dynamic dependencies are exactly `libkernel_web.sprx`, `libSceLibcInternal.sprx`, and `libSceNet.sprx`. No host shared library dependency was found. The 39 undefined dynamic symbols exactly match the report's union of listed imports:

```text
__error __inet_pton __isthreaded __udivti3 calloc clock_gettime closedir
fclose ferror fgets fopen fread free fseek ftell fwrite getpid memchr
memcmp memcpy memmove memset opendir printf putchar puts rand readdir
remove rename setbuf snprintf stat strchr strcmp strlen strstr sysctl
time vsnprintf
```

Optimized `main` calls entropy-init at `0x9324`, SSL-config-init at `0x9334`, X.509-init at `0x9344`, and `mbedtls_entropy_func` with length 32 at `0x9356`, then retains all frees and tests the entropy return value before returning. The call was not optimized away. This is a genuine local link smoke artifact; there is no target execution evidence for it.

## Read-only inspection commands

All successful checks used Python standard-library file/hash/archive reads, target compiler preprocessing/dependency generation to stdout, or LLVM ELF inspection. They made no target calls and wrote no build outputs. Commands were run from `/WORKSPACE/.codex/worktrees/translation-core/what` with `LLVM_CONFIG` set to the cached LLVM 21.1.8 `llvm-config` where the SDK wrapper required it.

- `hashlib.sha256` streaming over the archives and listed artifacts; `tarfile.open` compared all source regular-file bytes against extracted destinations and checked selected LLVM archive members/symlinks.
- SDK `prospero-clang -E -v -x c /dev/null -resource-dir <scratch LLVM resource dir>`.
- SDK `prospero-clang -M <pinned library source>` with the documented scratch resource dir and pinned `include`/`library` search roots, for the four representative sources listed above.
- SDK `prospero-clang -E -dM <pinned library/entropy_poll.c>` with the same roots.
- Pinned `llvm-nm -u` for both entropy objects and the public-header object.
- Pinned `llvm-readelf -h` for the three static archives; `llvm-readelf -d` and `llvm-nm --dynamic --undefined-only` for `linked_probe.elf`.
- Pinned `llvm-objdump -d --disassemble-symbols=main,mbedtls_platform_entropy_poll <scratch linked_probe.elf>`.
- Existing native-host `build.py`'s read-only `elf_layout` on the scratch smoke ELF.

An initial attempt to read nonexistent `link_smoke.c` was corrected to the actual retained `linked_probe.c`; an initial search for nonexistent `native-probe/build.py` did not affect evidence or files. All substantive hash/source/closure/object/ELF checks above exited 0. No repeated tests or library builds were necessary.

## Scoped curl evidence review

Subsequently reviewed `outputs/2026-09-30-native-curl-compile-spike.md`, its exact retained release/codeload sources, `curl-build-http/config.log`, generated configuration, libtool archive metadata, stored dependency closures, static archive, smoke source, and final ELF. This was read-only: no configure/build/probe rerun or target contact occurred. Archive-spike work remained untouched.

**Verdict: curl compile/configuration/archive/link evidence is substantiated. One reproducibility correction is required in its narrative; no artifact identity mismatch or observed host-header/library leakage was found.**

The final configure log shows an initial attempt to run the target `./conftest` while detecting cross compilation (`config.log` lines 149–155). macOS rejected it with “cannot execute binary file”, exit 126; configure then set cross-compiling=yes. No target program successfully executed. However, `--host` alone did not prevent this attempt. The retained generated configure explicitly sets `cross_compiling=maybe` when host is supplied without build; differing explicit build/host aliases set it to yes immediately (`configure` lines 1727–1731). Future commands should include the observed build alias `--build=aarch64-apple-darwin24.6.0` as well as `--host=x86_64-pc-freebsd`, and the report should disclose the failed initial probe. This requires no rebuild merely to establish the existing artifact's identity.

| Curl input/artifact | Exact bytes | Independently recomputed SHA-256 |
| --- | --- | --- |
| Official `curl-8.18.0.tar.xz` | 2801444 | `40df79166e74aa20149365e11ee4c798a46ad57c34e4f68fd13100e2c9a91946` |
| Pinned commit `2eebc58c4b8d68c98c8344381a9f6df4cca838fd` codeload tar.gz | 3479442 | `4741f67a4c6f158fb6d5c6c6cf90268f7767b9363b41edf9687a6a2bdb0b04e5` |
| Final HTTP-only `libcurl.a` | 1352420 | `47e707bb72da14bfcc7ac751070878ea97a2e92326ecae3700897adf54d640b7` |
| `curl_link_probe_http.elf` | 1459832 | `6428b076ad269f6acf14c293879f232388acbdddf32129e0453c39eb216a1638` |

All 4,138 extracted release regular files and 4,201 extracted codeload regular files independently match their respective archives. `configure.ac` and `lib/easy.c` match across release and pinned commit sources as reported. These file comparisons and archive hashes establish the retained local inputs; this scoped audit did not repeat upstream remote tag verification.

The recorded configure arguments match the report. Generated `curl_config.h` selects Mbed TLS and POSIX threads, IPv6 and Unix sockets, disables the reported non-HTTP protocols including file/IPFS/WebSockets, and leaves compiled CA bundle/path unset. Proxy support remains enabled. `config.log` final compiler/resource/include/link settings and libtool `dependency_libs` point only to the SDK and scratch Mbed TLS prefix; the 256 retained `.Plo` dependency receipts contain 24,762 absolute path references and none outside scratch/SDK. All 178 static-archive members are x86_64 ELF. The package-discovery environment values themselves are not fully serialized into the config log, so the strongest independent no-leak evidence here is the actual recorded flags, dependency closures, and final linked dependencies.

The ordinary SDK smoke ELF passes the existing read-only ELF bounds inspector: ET_DYN x86_64, entry 0, three 16 KiB-aligned loads, first load RWE, 22 sections, section-table offset 1,458,424, table end/loader prefix 1,459,832, maximum load file end 1,247,172. Its needed libraries are exactly `libkernel_web.sprx`, `libSceLibcInternal.sprx`, and `libSceNet.sprx`, with `BIND_NOW`/`NOW PIE`. Its 114 undefined dynamic imports include ordinary socket/poll APIs, `pthread_create/detach/join` and mutex APIs, SDK `sceNetPool*`/`sceNetResolver*`, `sysctl`, and `__udivti3`. No host shared-library dependency appears.

Optimized `main` retains `curl_easy_init`, option setting (URL; verify-peer=1; verify-host=2; CAINFO), `curl_easy_cleanup`, and `curl_global_cleanup`. The retained source and body never call `curl_easy_perform`. Therefore neither the `.invalid` URL nor the CAINFO string establishes a transfer, CA-file access, entropy success, DNS, or TLS verification result. The configured POSIX threaded resolver can require worker threads even for synchronous easy requests; the native-core scalar single-thread TLS receipt below does not validate those pthread creation/resolver paths. A controlled HTTPS checkpoint must measure those dependencies and positive/negative certificate cases independently.

Additional read-only commands/checks: standard-library archive/file hashing/comparison; inspection of `config.log`, generated `curl_config.h`, libtool metadata and `.Plo` files; pinned `llvm-readelf -h/-d`, `llvm-nm --dynamic --undefined-only`, `llvm-objdump -d --disassemble-symbols=main`; existing `elf_layout`. The first dependency glob used `.Po` and found none; the correct stored libtool receipts use `.Plo` and were audited. All substantive curl checks exited 0. No broad rebuild or target execution was needed.

## Separate native-core receipt audit

At root's request, read the six completed `.superpowers/native-observations/native-core-{upload,readback,run-1,run-2,shutdown-1,shutdown-2}.json` records and subsequently supplied `native-core-launch-1.json`. These are root-produced observation receipts; this reviewer made no console call and did not alter the records. This section concerns shared DDS/hash/TLS-language/exception fixtures, not Mbed TLS encrypted transport.

Upload and stored read-back agree on exact payload path, 1,490,568 bytes, and reviewed development artifact SHA-256 `39ca575e447399aeb3cbd4e5d831bfbd77177527414cab064601e771a5de07d3`. The first launch receipt preserves the manager loadpayload route for that exact percent-encoded path and HTTP 200/OK, correctly labeled transport acknowledgment only. The first run's response byte counts and hashes recompute correctly from the recorded bodies.

Both runs' `/report` and `/version` match development build `c088db7ccd5bbcf16276fd25c74912f62230e06c86015549fca206fda3ed6462` and native origin `ps5_sdk_entry`. In both, `/core` equals `/report.core`; all seven named booleans are true, status passed, scope owned built-in fixtures, and installation unsupported. PIDs differ (290 and 291). Both shutdown receipts contain HTTP 200/shutdown_requested, endpoint-closed observations, and captured manager process lists that independently omit their respective reported PID. The second run also records the manager observing PID 291 while live. After final shutdown, recorded manager version is responsive at 0.5.2 and FTP is still connected with PID 267.

Evidence limitations: the second launch has no preserved dispatch acknowledgment and explicitly records the immediate readiness race; later matching native reports and the distinct PID support a fresh instance without reconstructing a missing command receipt. Endpoint closure plus absence from the manager snapshot is observed cleanup/process absence, not `waitpid`, exit-status, or independent operating-system exit attestation. Request receipts do not serialize every sent Host/Origin/shutdown header, though the reviewed server enforces those gates. Firmware/game identity remains not queried. The passing native core reports prove only the retained used single-thread allocator/EH/TLS and owned DDS/hash fixture paths in this reviewed development artifact; they establish neither the Mbed TLS/curl smoke ELFs' runtime behavior nor worker-thread TLS, arbitrary retained imports' semantics, installation, target writers, or game acceptance.

## Scoped initial libarchive evidence review

At root's additional request, reviewed `outputs/2026-09-30-native-libarchive-compile-spike.md` and the already produced `libarchive-build-1` configuration/closure/archive plus `libarchive_probe.c/.elf`. No libraries/tests were rebuilt or executed. **The initial codec-free build/link evidence is substantiated, with two factual/reproducibility corrections:**

- The report's initial linked-ELF SHA is mistyped: it says `aef7f0811ce9a4419b8cde128e6337030bfc73f22d42748887476f1ee9bd992`; the actual retained 287,032-byte ELF hashes to **`aef7f0811ce9a441b9e8cde128e6337030bfc73f22d42748887476f1ee9bd992`**. The fixture and static archive hashes match their reported values. This typo was reported to root for correction; no artifact mismatch was inferred from it.
- As with initial curl configuration, the retained libarchive configure log attempts `./conftest`, is rejected by macOS with exit 126, then selects cross-compiling=yes. The logged configure line numbers are 4940 (attempt), 4944 (exit), and 4959 (cross result). Explicit differing `--build`/`--host` should prevent the initial target-execution attempt. No successful target execution occurred. Forced-cross follow-up configurations are root/worker-owned and were not claimed reviewed at this initial stage.

| Libarchive input/artifact | Exact bytes | Independently recomputed SHA-256 |
| --- | --- | --- |
| Commit codeload tar.gz | 6134289 | `042f0efe7147063ff9ba10f1a38ed080e949bcbd04bdbf3592b8846dd11b1da2` |
| Official release tar.xz | 6012112 | `d3a8ba457ae25c27c84fd2830a2efdcc5b1d40bf585d4eb0d35f47e99e5d4774` |
| Initial `libarchive-build-1/.libs/libarchive.a` | 1516894 | `b771b3d2f53d4a0e27c05f0a794a90227c0d6c6faa18f7f9e5cbc71eceaaacb7` |
| `libarchive_probe.c` | 557 | `eb03e9ffd7d9cac59d470996f23e1828b2a18d08d1ca421ed8d62c4ed1152e78` |
| Initial `libarchive_probe.elf` | 287032 | `aef7f0811ce9a441b9e8cde128e6337030bfc73f22d42748887476f1ee9bd992` |

All 1,341 extracted codeload regular files and 1,550 extracted official-release regular files independently match their respective archives. The three cited parser/ZIP/public-header files match across release and pinned commit sources. All 123 archive members are x86_64 ELF; 136 stored `.Plo` dependency closures contain 12,227 absolute references and none outside scratch/SDK. Configured compiler/resource/ar/ranlib paths match the audited tooling. `CPPFLAGS`, `LDFLAGS`, and `LIBS` are empty; optional codec/crypto macros inspected are disabled. This matches the reported codec-free configuration and does not establish support for compressed ZIP or 7z.

The ELF's dynamic dependencies are only the three ordinary SDK libraries; its 67 undefined imports include pthread mutex operations, `sysctl`, ordinary file/stat/memory/string/locale/time APIs. Its ordinary SDK bounds are valid: ET_DYN x86_64, entry 0, three 16 KiB-aligned loads with first load RWE, 22 sections, table/prefix end 287,032 and maximum load file end 245,800. `BIND_NOW` again makes import resolution a separate target-startup requirement.

Optimized `main` retains the reported archive allocation/ZIP-registration/memory-open/free calls. The four-byte fixture contains only the ZIP end-of-central-directory signature, not a complete empty ZIP record. The report correctly describes a signature and does not claim runtime success; the source variable name `owned_empty_zip` must not be interpreted as fixture acceptance. A subsequent runtime checkpoint should use a complete independently owned valid archive, enumerate/read known entries, and compare exact bytes before expanding to compression codecs, path/link policy, extraction, or installation.

Checks used only standard-library archive/file reads, config/closure reads, pinned `llvm-readelf`, `llvm-nm`, `llvm-objdump --disassemble-symbols=main`, and the existing read-only `elf_layout`. An initial guessed `libarchive-build` path contained no completed archive; the actual report artifacts are under `libarchive-build-1` and were checked there. All substantive checks exited 0; no new build/test/device operation was necessary.

## Final forced-cross configuration consolidation

Independently inspected the producer's fresh corrected configure logs and generated headers, without rerunning configure, rebuilding, testing, or contacting any device. Both corrected command lines explicitly include `--build=aarch64-apple-darwin24.6.0` and `--host=x86_64-pc-freebsd`. Both logs go directly from the cross-compiling check to yes; neither contains any `./conftest` reference or execution attempt. Thus the original target-probe reproducibility findings are **ADDRESSED** in the documented corrected procedure. Original failed probes remain transparently recorded as history.

| Corrected receipt | Independently recomputed SHA-256 |
| --- | --- |
| `curl-build-http-corrected/config.log` | `287505a10ea011272f2c8c3025f290960db9224ca8729d9e85c312f79888434e` |
| Original and corrected `lib/curl_config.h` (25,246 bytes, byte-identical) | `f882af6a7b84481109985bea7778a0e16e6937b7abc266b135a4de9c71bf690b` |
| `libarchive-build-corrected/config.log` | `298c4b16cc6a79bd51aded6903697ede2d44c0472c8a0975e5b9043a1410cfbe` |
| Original and corrected libarchive `config.h` (47,728 bytes, byte-identical) | `d669a98a796c6fa9b7a29069272fb8808ec5845bd0ecaaaef1af042be0db3c74` |

For each project the original and corrected config logs also have identical recorded `CC`, `CFLAGS`, `CPPFLAGS`, `LDFLAGS`, `LIBS`, `AR`, and `RANLIB`. The additional build tuple therefore fixes configure's execution policy without changing the audited target configuration/compiler/link inputs; no redundant archive/ELF rebuild was necessary. The previously audited artifact identities remain the local built artifact identities, not claims that another artifact was produced by the configure-only follow-up.

Both spike reports append the failed initial probe, explicit corrected tuples, no-probe outcome, unchanged header identity, and no-rebuild explanation. Their evidence boundaries remain appropriately compile/link-only. A final direct report read confirms libarchive now states the independently verified ELF SHA `aef7f0811ce9a441b9e8cde128e6337030bfc73f22d42748887476f1ee9bd992` and exact 287,032-byte size. Its scope wording now says only `libarchive.la` was built, test targets were not built, and configure was not given an explicit test-disable option. Those two documentary findings are **ADDRESSED**.

The final report read also confirms Mbed TLS's opening now describes successful local static-library creation and linking without dispatch/execution, explicitly marking prior limitations as historical; its initial compile-only section uses past-stage language. The route report now marks the no-cross-build claim as an initial assessment and distinguishes the later native-core used-path evidence from unverified dependency runtime. That original stage-language finding is **ADDRESSED**. No actionable finding remains pending in this review. These final checks read exact files and compared stored header/config data only; no rebuild, test execution, or device/account contact occurred.
