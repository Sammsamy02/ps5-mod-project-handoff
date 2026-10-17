# libarchive 3.8.7 PS5 SDK compile/link spike

This isolated development spike tested whether pinned libarchive C sources can be cross-built and linked with SDK v0.43. All source/build files, tools, and outputs are confined to ignored `.superpowers/native-dependency-spike/`. No SDK/cache changes, product edits, installs, console use, account activity, or runtime execution occurred.

## Source and tool pins

- Upstream tag `v3.8.7`, peeled commit `ded82291ab41d5e355831b96b0e1ff49e24d8939`; GitHub identifies the tag as verified-signed and describes this as a security/bugfix release: [v3.8.7 release](https://github.com/libarchive/libarchive/releases/tag/v3.8.7).
- Exact codeload snapshot URL `https://codeload.github.com/libarchive/libarchive/tar.gz/ded82291ab41d5e355831b96b0e1ff49e24d8939`, SHA-256 `042f0efe7147063ff9ba10f1a38ed080e949bcbd04bdbf3592b8846dd11b1da2`.
- The official release distribution `https://libarchive.org/downloads/libarchive-3.8.7.tar.xz` was used for generated configure/build files; SHA-256 `d3a8ba457ae25c27c84fd2830a2efdcc5b1d40bf585d4eb0d35f47e99e5d4774`, matching upstream's official [checksum listing](https://libarchive.org/downloads/libarchive-3.8.7.tar.xz.sums.txt). Pinned `archive_read.c`, `archive_read_support_format_zip.c`, and `archive.h` were byte-identical between this tarball and the exact codeload snapshot.
- SDK v0.43: `ps5-payload-dev/sdk@d9c9519116944a7f1c22d262012c53b80b3520b7`; cached `prospero-clang` wrapper.
- LLVM 21.1.8 cached toolchain was previously verified from the existing official archive (SHA-256 `b95bdd32a33a81ee4d40363aaeb26728a26783fcef26a4d80f65457433ea4669`). Scratch contains the `llvm-ar`/`llvm-ranlib` tools and full Clang 21 resource-header subtree.

## Cross-build configuration and result

This environment has no CMake, Autoconf, or Automake executable. The official release tarball provides a generated Autoconf `configure`. Configure ran in scratch with `--host=x86_64-pc-freebsd`, SDK `prospero-clang`, `LLVM_CONFIG` set to cached LLVM's `llvm-config`, explicit `AR`/`RANLIB` from the verified LLVM tool archive, `-Os` and explicit scratch `-resource-dir`, `CONFIG_SITE=/dev/null`, `PKG_CONFIG=/usr/bin/false`, empty `PKG_CONFIG_PATH`, and `PKG_CONFIG_LIBDIR=/dev/null`. Preprocessor search output showed only the SDK target `target/include` and `target/include_common`, in addition to compiler builtin headers from the explicitly selected LLVM resource tree.

The configure options were:

```sh
--host=x86_64-pc-freebsd \
--prefix=<scratch>/libarchive-install \
--enable-static --disable-shared \
--disable-bsdtar --disable-bsdcat --disable-bsdcpio --disable-bsdunzip \
--disable-xattr --disable-acl \
--without-zlib --without-bz2lib --without-lzma --without-lz4 --without-zstd \
--without-iconv --without-expat --without-openssl --without-nettle \
--without-lzo2 --without-libb2
```

Configure reported cross-compiling=yes for `x86_64-pc-freebsd`; no external codec/library package was used. Generated `config.h` leaves `HAVE_ZLIB_H`, `HAVE_BZLIB_H`, `HAVE_LZMA_H`, `HAVE_LZ4_H`, and `HAVE_ZSTD_H` undefined. libarchive's source does still compile reader/writer translation units with optional codec paths disabled; that does not imply the disabled codecs work. Command-line tools, ACL, xattr, crypto integrations, and optional codecs were excluded by configuration. Only the `libarchive.la` make target was built, so test targets were not built; configure was not given an explicit test-disable option.

`LLVM_CONFIG=<absolute cached llvm-config> make -j4 V=1 libarchive.la` succeeded. The resulting static archive `.libs/libarchive.a` is 1.4 MiB, SHA-256 `b771b3d2f53d4a0e27c05f0a794a90227c0d6c6faa18f7f9e5cbc71eceaaacb7`.

## SDK link probe (not executed)

A scratch C11 fixture calls `archive_read_new`, registers ZIP format support, opens a four-byte owned ZIP end-of-central-directory signature in memory, and frees the reader. It was linked with SDK `prospero-clang`, `-Os -Wall -Wextra -Werror`, the source public headers, and the static archive alone. Link succeeded without adding an external codec or host library. Fixture SHA-256: `eb03e9ffd7d9cac59d470996f23e1828b2a18d08d1ca421ed8d62c4ed1152e78`; linked ELF SHA-256: `aef7f0811ce9a441b9e8cde128e6337030bfc73f22d42748887476f1ee9bd992` (287,032 bytes, approximately 280 KiB). It was never run.

`llvm-readelf -d` lists SDK dependencies `libkernel_web.sprx`, `libSceLibcInternal.sprx`, and `libSceNet.sprx`. The ELF has ordinary undefined imports including libc file/stat/memory/string/time functions, `pthread_mutex_lock`/`pthread_mutex_unlock`, and `sysctl`. These identify the link shape only; symbol resolution at firmware runtime is not demonstrated.

## Evidence boundary

This establishes that the pinned libarchive 3.8.7 C library can be cross-compiled with a codec-free static configuration and linked into an SDK v0.43 ELF using ZIP-reader APIs. It does not establish runtime API behavior, import resolution on firmware, secure extraction, path/link handling, encrypted ZIP support, deflate or any other compression codec, 7z, Nexus package coverage, memory limits, or target execution. This baseline is not sufficient for typical compressed ZIPs: ZIP deflate requires a supported inflater, such as zlib, in a subsequent separate pinned codec build. Enable codecs only after representative package fixtures establish the required format/filter matrix. No extraction operation or install path was built.

The first make call used a nonexistent `libarchive/libarchive.la` target; the right top-level target is `libarchive.la`. The first actual compile invocation then inherited an incorrectly resolved relative `LLVM_CONFIG` from the scratch working directory and the SDK wrapper attempted `/llvm-clang`. Supplying the absolute cached `llvm-config` path fixed the invocation; the unchanged configured sources then compiled and linked. No source patch or build-feature workaround was made.

Primary references: [libarchive 3.8.7 release](https://github.com/libarchive/libarchive/releases/tag/v3.8.7), [official release download and checksums](https://libarchive.org/downloads/), [tagged upstream configure options](https://github.com/libarchive/libarchive/blob/v3.8.7/configure.ac), [tagged upstream CMake codec options](https://github.com/libarchive/libarchive/blob/v3.8.7/CMakeLists.txt), and [SDK v0.43 source](https://github.com/ps5-payload-dev/sdk/tree/d9c9519116944a7f1c22d262012c53b80b3520b7).

## Cross-configure execution check (September 30, 2026)

A later audit of the first configure log found that with only `--host=x86_64-pc-freebsd`, Autoconf linked a target `conftest` and attempted `./conftest` while deciding whether it was cross-compiling. macOS refused to execute the PS5 ELF with status 126, after which configure reported cross-compiling=yes. No test binary successfully ran, but the initial invocation did make an execution attempt.

I reran the same configure in a fresh scratch build directory with `--build=aarch64-apple-darwin24.6.0` (the release's `config.guess`) and the existing `--host=x86_64-pc-freebsd`. Configure reported those exact build/host tuples and cross-compiling=yes before later checks. The corrected log has no `./conftest` execution command. Its generated `config.h` is byte-identical to the config used by the built archive/link (SHA-256 `d669a98a796c6fa9b7a29069272fb8808ec5845bd0ecaaaef1af042be0db3c74`), so no rebuild was needed. The previous configure attempt remains documented as history; builds should always pass both tuples.
