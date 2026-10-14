# Mbed TLS 3.6.7 SDK compile feasibility spike

Development-only compile/link evidence for the native HTTPS route. The final stage built all three default Mbed TLS static archives and linked an SDK smoke ELF; it was never dispatched or executed. No product files were changed. Source archives, objects and `.gitignore` are isolated under `.superpowers/native-dependency-spike/`; its local ignore file contains `*`. No console, account or credential was contacted. Earlier-stage limitations below are historical and are superseded by the final static-build/link sections.

## Pin and source

- Upstream tag: `mbedtls-3.6.7`, peeled commit `068ff080b369adfac81509f9b57b2afabaf82dc5`.
- Fetched bounded source archive: `https://codeload.github.com/Mbed-TLS/mbedtls/tar.gz/068ff080b369adfac81509f9b57b2afabaf82dc5` (5,548 KiB).
- Archive SHA-256: `ca6bd316bbec49ef20088f39b8755fcaec0b7e45781506de55b17cd191ae6937`.
- Pinned upstream release page: https://github.com/Mbed-TLS/mbedtls/releases/tag/mbedtls-3.6.7

This codeload commit archive omits submodule/generated content as GitHub source snapshots can; the translation units needed for this narrow spike were present. Use the upstream official Mbed TLS release tarball for a complete dependency build.

## Toolchain and exact compile checks

Used cached SDK v0.43 at `outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk` and cached LLVM `21.1.8` at `outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin`. The SDK wrapper requires `LLVM_CONFIG` set to that cached `llvm-config`; without it, the cached wrapper's relative compiler path is stale. No cache/toolchain files were changed.

Both commands succeeded with no diagnostics, `-std=c99 -Os -Wall -Wextra -Werror`, SDK wrapper target `x86_64-sie-ps5`, SDK sysroot and target includes:

```sh
LLVM_CONFIG="$PWD/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config" \
  outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang \
  -std=c99 -c .superpowers/native-dependency-spike/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5/library/entropy_poll.c \
  -o .superpowers/native-dependency-spike/obj/entropy_poll.o \
  -I.superpowers/native-dependency-spike/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5/include \
  -I.superpowers/native-dependency-spike/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5/library \
  -Os -Wall -Wextra -Werror

LLVM_CONFIG="$PWD/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config" \
  outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang \
  -std=c99 -c .superpowers/native-dependency-spike/public_headers.c \
  -o .superpowers/native-dependency-spike/obj/public_headers.o \
  -I.superpowers/native-dependency-spike/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5/include \
  -Os -Wall -Wextra -Werror
```

The first translation unit is Mbed TLS's actual pinned `library/entropy_poll.c`, compiled against the SDK target headers. The second includes public `mbedtls/ssl.h` and `mbedtls/x509_crt.h` and calls `mbedtls_ssl_init` and `mbedtls_x509_crt_init` to validate public-header/API compilation. The public fixture source is retained in the ignored scratch directory.

## Entropy branch and imports

The compiler defines `__FreeBSD__` and `__FreeBSD_version 1104001`; the SDK target headers define FreeBSD version `1104001` in `sys/param.h`, provide `KERN_ARND` in `sys/sysctl.h`, and declare standard BSD `sysctl()`. The SDK's `sys/random.h` is a FreeBSD 11.4 kernel-oriented header and provides no userland `getrandom()` declaration. Mbed TLS 3.6.7's FreeBSD code only selects `getrandom()` for FreeBSD 12 or later. With the SDK macros/headers, preprocessing showed the compiled platform entropy path calls `sysctl(name, 2, buf, &len, NULL, 0)` using `CTL_KERN` and `KERN_ARND`; the `/dev/urandom` stdio fallback is excluded because `KERN_ARND` is defined. The selected API is standard BSD `sysctl`/KERN_ARND, not an application-issued raw syscall.

`llvm-nm -u entropy_poll.o` reports exactly `U sysctl`. The public-header fixture reports `U mbedtls_ssl_init` and `U mbedtls_x509_crt_init`. Object SHA-256 values:

- `entropy_poll.o`: `bf21d24fd56ff7067110595092fc72596855319cfebd8785356be1416b0548bb`
- `public_headers.o`: `54e0afd75e106e091141d30f4fe243edf0ddfdf51c2a9fb8d93336be375afe4d`

## Initial compile-only evidence boundary

At this initial stage, pinned Mbed TLS entropy-source C and TLS/X.509 public headers compile for the SDK v0.43 target, and identifies `sysctl` as the entropy object's unresolved external. It does not show that the SDK linker resolves `sysctl`, that the runtime permits `KERN_ARND`, or that the call returns strong random bytes on firmware 13.60. At that stage no link, TLS handshake, DNS/network request, archive build, or target execution had been performed. Next useful checkpoint is a separate minimal owned test for the BSD `sysctl(KERN_ARND)` result on-target before relying on it for TLS.

## Full static-library build attempt (September 30, 2026)

I attempted the pinned Mbed TLS tree's `make lib` target with SDK `prospero-clang`, `-Os`, four jobs, and direct LLVM 21.1.8 `llvm-ar`/`llvm-ranlib`:

```sh
LLVM_CONFIG="$PWD/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config" \
  make -C .superpowers/native-dependency-spike/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5 \
  CC="$PWD/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang" \
  AR="$PWD/.superpowers/native-dependency-spike/tools/LLVM-21.1.8-macOS-ARM64/bin/llvm-ar" \
  RANLIB="$PWD/.superpowers/native-dependency-spike/tools/LLVM-21.1.8-macOS-ARM64/bin/llvm-ranlib" \
  CFLAGS='-Os' lib -j4
```

The cached LLVM 21.1.8 archive was `outputs/ps5-mod-bridge/native-probe/build/deps/llvm.tar.xz` (1.4 GiB), SHA-256 `b95bdd32a33a81ee4d40363aaeb26728a26783fcef26a4d80f65457433ea4669`. After confirming that hash, I selectively extracted only `LLVM-21.1.8-macOS-ARM64/bin/llvm-ar` and `llvm-ranlib` into ignored spike scratch; the latter is the archive's symlink to `llvm-ar`. Both report LLVM 21.1.8. The shared tool cache was not changed.

**First build failure:** Make began `aesni.c`, `aes.c`, `aesce.c`, and `aria.c`; the `aesni.c` compile stopped with `fatal error: 'cpuid.h' file not found`. This is the actual first blocker from the cached SDK cross-build. I did not inject a host `cpuid.h`, alter Mbed TLS platform feature configuration, or try an unverified target CPU detection replacement. The target sysroot path is selected by the SDK wrapper and is unchanged. No static libraries were completed and no link was attempted.

This narrowed the feasibility claim at that stage: standalone entropy polling and public TLS/X.509 headers compile, but a default-config full Mbed TLS library build does not complete unmodified with the current SDK sysroot. Resolving `cpuid.h` requires a separately reviewed, target-appropriate feature/build decision. The previously identified entropy `sysctl(KERN_ARND)` symbol remains a compile-only observation; no runtime entropy test has been made.

## Root-cause confirmation and successful static build

The `cpuid.h` failure came from an incomplete selective compiler cache, not an absent PS5 SDK header: `prospero-clang -print-resource-dir` identified the missing `.../lib/clang/21` resource tree, and `prospero-clang -E -v -x c /dev/null` listed only SDK target include directories. After checking the cached LLVM archive SHA-256 against `b95bdd32a33a81ee4d40363aaeb26728a26783fcef26a4d80f65457433ea4669`, I selectively extracted the bounded `LLVM-21.1.8-macOS-ARM64/lib/clang/21/include/` resource subtree (297 files, 15 MiB) into ignored scratch. Its `cpuid.h` came from that archive and has SHA-256 `af95bc7da3f66d3a62a5c75462f347f6330da5af6d173a5572042c8f68316b4d`. No host SDK headers were added. The retry changed only CFLAGS by adding explicit `-resource-dir <scratch>/LLVM-21.1.8-macOS-ARM64/lib/clang/21`; Mbed TLS source/configuration was unchanged.

The GitHub-generated source archive omitted Mbed TLS's `framework` git submodule. The first retry's cleanup command therefore stopped with `framework/exported.make not found`; it did not compile anything. GitHub's primary Contents API for Mbed TLS commit `068ff080b369adfac81509f9b57b2afabaf82dc5` identifies exact submodule gitlink `Mbed-TLS/mbedtls-framework@dde0c4a0e448a0552f18817dcea633bb851fd288`. I fetched that pinned gitlink as a 1.0 MiB codeload archive (SHA-256 `3b0d4864fa877dc9165b90ff3d5b68505c6176c0b22d317d99fe8c89a993e9fd`) and populated the matching scratch `framework/` submodule directory. This was a source-completeness correction, not a source patch.

With the resource dir and exact submodule supplied, the command shown in the previous section succeeded through all C sources and archives:

```sh
LLVM_CONFIG="$PWD/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config" \
  make -C .superpowers/native-dependency-spike/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5 \
  CC="$PWD/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang" \
  AR="$PWD/.superpowers/native-dependency-spike/tools/LLVM-21.1.8-macOS-ARM64/bin/llvm-ar" \
  RANLIB="$PWD/.superpowers/native-dependency-spike/tools/LLVM-21.1.8-macOS-ARM64/bin/llvm-ranlib" \
  CFLAGS="-Os -resource-dir $PWD/.superpowers/native-dependency-spike/tools/resource-root/LLVM-21.1.8-macOS-ARM64/lib/clang/21" \
  lib -j4
```

Artifacts (default upstream Mbed TLS config; no library feature switches):

- `libmbedcrypto.a`, 1.1 MiB, SHA-256 `7ccb34e79140f97cd3adea55688bae495c05f116b4e7639fb61eb5bd3ea09599`
- `libmbedx509.a`, 127 KiB, SHA-256 `e9f437c9e22d1df05ee8475ff3ab8ea9ec2b566c4594c9bb5f33fea64d9345f3`
- `libmbedtls.a`, 579 KiB, SHA-256 `c7e0c6fd536607a45bd5b2d399446bfa5816410d1bcbde79f53b8fd82285d7ea`

The source's effective entropy macros included `MBEDTLS_ENTROPY_C`, `__FreeBSD__=9`, and SDK `__FreeBSD_version=1104001`; `MBEDTLS_NO_PLATFORM_ENTROPY` was not set. The compiled entropy object has unresolved `sysctl` as its sole external symbol, confirming the selected KERN_ARND path, without `/dev/urandom` fallback in this build. The `sysctl` import's runtime resolution and successful entropy remain unknown.

## Static link smoke artifact (not run)

I linked a small C `main` that initializes an Mbed TLS SSL config and X.509 certificate context, calls `mbedtls_entropy_func` for 32 bytes, frees its contexts, and returns status. It used SDK `prospero-clang` with the default startup/runtime and static libraries in upstream link order: `-lmbedtls -lmbedx509 -lmbedcrypto`. The link succeeded. Resulting ignored scratch ELF is 884 KiB, SHA-256 `7881ba55e149a645f200d453d8510eb371839a4ede932f51b0f45b46edc844c1`; it was never dispatched or executed.

`llvm-readelf -d` reports needed SDK libraries `libkernel_web.sprx`, `libSceLibcInternal.sprx`, and `libSceNet.sprx`. Its dynamic symbol table still shows `sysctl` as undefined, along with ordinary libc/POSIX imports including `fopen`, `fread`, `fclose`, `clock_gettime`, `getpid`, `calloc`, `free`, `memcpy`, `memset`, and `__error`; the complete selected-symbol inspection also showed `puts`, `printf`, `vsnprintf`, `snprintf`, `opendir`, `readdir`, `stat`, `closedir`, `__inet_pton`, `fgets`, `fwrite`, `__udivti3`, `putchar`, `fseek`, `ftell`, `setbuf`, `ferror`, `rename`, `remove`, `rand`, `strlen`, `memcmp`, `memmove`, `strcmp`, `time`, `memchr`, `strstr`, `strchr`, and `__isthreaded`. A successful SDK link is not evidence those imports resolve or that `sysctl(KERN_ARND)` succeeds at target runtime.

This advances the evidence to source compilation and static archive creation plus SDK ELF linking for default Mbed TLS 3.6.7. It does not validate a TLS handshake, certificate verification, DNS, curl integration, target imports, runtime entropy, or PS5 execution.

Primary source references: https://github.com/Mbed-TLS/mbedtls/blob/mbedtls-3.6.7/README.md ; https://github.com/Mbed-TLS/mbedtls-framework/tree/dde0c4a0e448a0552f18817dcea633bb851fd288 ; https://api.github.com/repos/Mbed-TLS/mbedtls/contents/framework?ref=068ff080b369adfac81509f9b57b2afabaf82dc5

## Independent review and next runtime diagnostic

The independent dependency review confirmed the exact archives, all source/framework files, compiler resource/tool provenance, x86_64 static-library members, target-only include closure, real smoke ELF/imports and selected sysctl entropy path. The upstream sysctl polling loop assumes successful calls return a positive byte count. Before relying on it, an owned build-identity-bearing diagnostic must report raw sysctl return/errno/returned length without exposing random bytes, then bounded entropy/DRBG results. A successful zero-length response is an untested target assumption, not a demonstrated failure. BIND_NOW means full imported-symbol resolution precedes main, so import failure must be distinguished from entropy API failure. This smoke ELF only reports exit status and remains compile/link evidence, not the reviewed runtime diagnostic. See [independent audit](2026-09-30-native-dependency-review.md).
