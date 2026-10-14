# curl 8.18.0 + Mbed TLS PS5 SDK compile/link spike

Development-only cross-build evidence for the ordinary SDK v0.43 native application. All fetched sources, staged dependency links, objects and executables are in ignored `.superpowers/native-dependency-spike/`. No SDK/shared tool cache or product file was modified. No install into a shared prefix, console dispatch, account request, credential use, TLS handshake, DNS lookup, or transfer occurred.

## Pins and sources

- curl tag: `curl-8_18_0`, verified peeled commit `2eebc58c4b8d68c98c8344381a9f6df4cca838fd`.
- Official curl release archive used for configure/build: `https://curl.se/download/curl-8.18.0.tar.xz`, SHA-256 `40df79166e74aa20149365e11ee4c798a46ad57c34e4f68fd13100e2c9a91946` (2.7 MiB). It has curl version `8.18.0`; its `configure.ac` and `lib/easy.c` match the codeload source at the pinned commit byte-for-byte.
- Exact GitHub codeload source at commit `2eebc58c4b8d68c98c8344381a9f6df4cca838fd`: SHA-256 `4741f67a4c6f158fb6d5c6c6cf90268f7767b9363b41edf9687a6a2bdb0b04e5` (3.3 MiB). Retained in ignored scratch as source-pin provenance; its snapshot lacks the generated `configure`, so the official release archive supplied generated build files.
- Mbed TLS: `mbedtls-3.6.7` peeled commit `068ff080b369adfac81509f9b57b2afabaf82dc5`, built static archives from the previous spike. Its framework gitlink is `dde0c4a0e448a0552f18817dcea633bb851fd288`.
- SDK v0.43: `ps5-payload-dev/sdk@d9c9519116944a7f1c22d262012c53b80b3520b7`.
- LLVM: verified cached release 21.1.8. The archive SHA-256 was checked as `b95bdd32a33a81ee4d40363aaeb26728a26783fcef26a4d80f65457433ea4669`. `llvm-ar`, `llvm-ranlib`, and its 297-file Clang resource include tree are isolated under spike scratch; `cpuid.h` SHA-256 is `af95bc7da3f66d3a62a5c75462f347f6330da5af6d173a5572042c8f68316b4d`.

The previous report [Mbed TLS compile spike](2026-09-30-native-mbedtls-compile-spike.md) records the Mbed TLS archive hashes: `libmbedcrypto.a` `7ccb34e79140f97cd3adea55688bae495c05f116b4e7639fb61eb5bd3ea09599`, `libmbedx509.a` `e9f437c9e22d1df05ee8475ff3ab8ea9ec2b566c4594c9bb5f33fea64d9345f3`, and `libmbedtls.a` `c7e0c6fd536607a45bd5b2d399446bfa5816410d1bcbde79f53b8fd82285d7ea`.

## Build configuration and outcome

The official curl release tarball includes a generated Autoconf `configure` script. CMake, Autoconf, Automake, and libtoolize executables are not installed in this environment; the generated configure script meant none were needed. The configure invocation ran with `--host=x86_64-pc-freebsd`, SDK `prospero-clang`, explicit Clang `-resource-dir`, and `CONFIG_SITE=/dev/null`. `PKG_CONFIG=/usr/bin/false`, empty `PKG_CONFIG_PATH`, and `PKG_CONFIG_LIBDIR=/dev/null` prevented host package discovery. Mbed TLS headers and archives were exposed only through scratch-local `mbedtls-prefix` symlinks. The SDK wrapper supplied target sysroot and import libraries; the only explicit added include/library roots were that Mbed TLS scratch prefix and LLVM's target-agnostic compiler resource headers.

The final configuration was static-only and Mbed TLS-backed: `--enable-static --disable-shared --with-mbedtls=<scratch>/mbedtls-prefix --without-openssl`. It disabled unused non-HTTP protocols (FTP, LDAP/LDAPS, RTSP, dict, telnet, TFTP, POP3, IMAP, SMB, SMTP, Gopher, MQTT, file, IPFS/IPNS, and WebSockets), unrelated optional dependencies (zlib, Brotli, Zstandard, libpsl, libidn2, HTTP/2, HTTP/3, libssh2), docs/manual, and compiled default CA bundle/path/embed. Proxy support stayed enabled. Configure reported:

- Cross compile: yes; host `x86_64-pc-freebsd`.
- SSL: enabled (Mbed TLS).
- Static libcurl yes; shared no.
- Protocols: HTTP and HTTPS only.
- HTTP/2 and HTTP/3, zlib, Brotli, Zstandard, IDN and libpsl: no.
- Resolver: POSIX threaded; IPv6 and Unix sockets enabled.
- CA bundle/path/embed: none configured.
- Link libraries: `-lmbedtls -lmbedx509 -lmbedcrypto`.

The exact configure arguments were:

```sh
--host=x86_64-pc-freebsd \
--prefix=<scratch>/curl-install \
--enable-static --disable-shared \
--with-mbedtls=<scratch>/mbedtls-prefix --without-openssl \
--without-zlib --without-brotli --without-zstd --without-libpsl \
--without-libidn2 --without-nghttp2 --without-nghttp3 --without-ngtcp2 \
--without-libssh2 --disable-docs --disable-manual \
--disable-file --disable-ipfs --disable-websockets \
--disable-ftp --disable-ldap --disable-ldaps --disable-rtsp --disable-dict \
--disable-telnet --disable-tftp --disable-pop3 --disable-imap --disable-smb \
--disable-smtp --disable-gopher --disable-mqtt \
--without-ca-bundle --without-ca-path --without-ca-embed
```

`make -C curl-build-http/lib AR=<scratch>/tools/LLVM-21.1.8-macOS-ARM64/bin/llvm-ar RANLIB=<scratch>/tools/LLVM-21.1.8-macOS-ARM64/bin/llvm-ranlib -j4 libcurl.la` succeeded. Static `libcurl.a` is 1.3 MiB, SHA-256 `47e707bb72da14bfcc7ac751070878ea97a2e92326ecae3700897adf54d640b7`. Generated `lib/curl_config.h` defines `USE_MBEDTLS`, `CURL_DISABLE_FILE`, `CURL_DISABLE_IPFS`, and `CURL_DISABLE_WEBSOCKETS`; CA bundle/path macros are undefined.

## Static link smoke artifact (not executed)

A C11 fixture calls `curl_easy_init`, sets URL to `https://example.invalid/`, explicitly sets `CURLOPT_SSL_VERIFYPEER=1`, `CURLOPT_SSL_VERIFYHOST=2`, and `CURLOPT_CAINFO="/data/modbridge/cacert.pem"`, cleans up, and never calls `curl_easy_perform`. It linked successfully through the ordinary SDK wrapper against static curl, Mbed TLS, and SDK pthread:

```sh
prospero-clang -std=c11 curl_link_probe.c -o curl_link_probe_http.elf \
  -I<curl-release>/include -I<scratch>/mbedtls-prefix/include -DCURL_STATICLIB \
  -L<scratch>/curl-build-http/lib/.libs -lcurl \
  -L<scratch>/mbedtls-prefix/lib -lmbedtls -lmbedx509 -lmbedcrypto -lpthread \
  -Os -Wall -Wextra -Werror
```

The ignored ELF is 1.4 MiB, SHA-256 `6428b076ad269f6acf14c293879f232388acbdddf32129e0453c39eb216a1638`. `llvm-readelf -d` shows SDK dependencies `libkernel_web.sprx`, `libSceLibcInternal.sprx`, and `libSceNet.sprx`. Its unresolved dynamic imports include `socket`, `connect`, `poll`, `pthread_*`, `sceNetPoolCreate/Destroy`, `sceNetResolverCreate/Destroy/StartAton/StartNtoa`, and `sysctl`. These show the target link shape and dependencies; they do not establish runtime import resolution, DNS behavior, or entropy behavior.

## Evidence boundary and next gate

This confirms a pinned-source cross-configuration, static curl + Mbed TLS archive build, and SDK ELF link with the intended HTTP/HTTPS-only protocol set. It does not establish peer/hostname verification in a handshake, CA file accessibility or provenance, trust-chain success, resolver/socket runtime behavior, HTTP redirects, Nexus API/account/download behavior, or firmware execution. The smoke fixture's CA path is a string only; no certificate bundle was downloaded or installed. Mbed TLS's `sysctl(KERN_ARND)` entropy import remains untested at runtime. The next concrete target gate is validation of expected SDK imports, especially `sysctl`, resolver calls, and pthread, followed by a controlled HTTPS test with an owned endpoint and a pinned CA fixture; only then should official Nexus endpoints/account flow be considered.

Primary source links: [curl 8.18.0 release](https://github.com/curl/curl/releases/tag/curl-8_18_0), [curl build/TLS options](https://github.com/curl/curl/blob/curl-8_18_0/docs/INSTALL.md), [Mbed TLS 3.6.7 release](https://github.com/Mbed-TLS/mbedtls/releases/tag/mbedtls-3.6.7), [SDK v0.43](https://github.com/ps5-payload-dev/sdk/tree/d9c9519116944a7f1c22d262012c53b80b3520b7).

## Cross-configure execution check (September 30, 2026)

A later audit of the first configure log found an important caveat: with only `--host=x86_64-pc-freebsd`, Autoconf linked a target `conftest` and attempted `./conftest` while deciding whether it was cross-compiling. macOS refused to execute the PS5 ELF with status 126, and configure then reported cross-compiling=yes. No conftest ran successfully, but the initial invocation did make an execution attempt.

I reran configure in a fresh scratch build directory with the same source, compiler, flags, dependencies, host tuple and feature options, adding the verified build tuple `--build=aarch64-apple-darwin24.6.0` (from the release's `config.guess`). Configure reported build `aarch64-apple-darwin24.6.0`, host `x86_64-pc-freebsd`, and cross-compiling=yes. Its log goes directly to the yes result and contains no `./conftest` execution command; forced cross mode prevented that attempt. The regenerated `lib/curl_config.h` is byte-identical to the configuration used for the existing static archive and link: SHA-256 `f882af6a7b84481109985bea7778a0e16e6937b7abc266b135a4de9c71bf690b`. Thus the recorded archive/ELF hashes remain valid for the corrected configure tuple; no redundant rebuild was needed. The earlier attempt is retained here as history, not as the recommended build procedure.
