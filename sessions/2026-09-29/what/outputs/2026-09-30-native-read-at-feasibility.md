# Native bounded read-at feasibility — 2026-09-30

**Recommendation: use unchanged SDK `open`/`fstat`/`pread`/`close` imports for a generic physical-file provider.** Local C11 compilation and ELF linking succeeded with the existing SDK v0.43 and LLVM 21.1.8. A separate desktop owned-file fixture passed 66 checks under AddressSanitizer and UndefinedBehaviorSanitizer, including an actual read above 4 GiB. This establishes a source/build route and desktop algorithm evidence. PS5 import resolution, file API behavior, permissions, filesystem semantics and runtime bounds remain unverified.

This is an authorized throwaway spike, not a production provider or approved container ABI. Work began at `6ad311cdbf61a2a56d149da93ba2fb3f52cef0b5`. Only new ignored `.superpowers/native-read-at-spike/` files and this new report were written. No SDK/cache/product/index/commit changes, dependency downloads, native ELF execution, device/FTP/account access, game data or proprietary executable reads occurred. The desktop program created, altered and removed only its owned scratch tempfile; it read its own scratch directory metadata solely for a regular-file rejection test. No parser, codec, writer, installation or game-acceptance claim follows.

## Exact API and type evidence

The inspected extraction is `outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk`, verified against the existing 8,814,217-byte release ZIP, SHA-256 `a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb`. Source pin: `ps5-payload-dev/sdk@d9c9519116944a7f1c22d262012c53b80b3520b7`.

| Interface | Exact target declaration / observation |
| --- | --- |
| `open` | `int open(const char *, int, ...);` in `fcntl.h` / `sys/fcntl.h` |
| `fstat` | `int fstat(int, struct stat *);` in `sys/stat.h` |
| `pread` | `ssize_t pread(int, void *, size_t, off_t);` in `unistd.h`; visible with `_POSIX_C_SOURCE=200809L` |
| `close` | `int close(int);` in `unistd.h` |
| errno access | Compiled ordinary errno accesses produce the `__error` import |
| Read-only / path flags | `O_RDONLY=0`, `O_NONBLOCK=0x0004`, `O_NOFOLLOW=0x0100`, `O_CLOEXEC=0x00100000`; use target macros |
| `off_t` | `sys/_types.h` defines `__off_t` as `__int64_t`; `sys/types.h` aliases it to `off_t` |
| x86-64 sizes | `off_t`, `size_t`, `ssize_t`, `time_t` are 8 bytes; `off_t`, `ssize_t`, `time_t` are signed |
| File identity fields | SDK `dev_t` and `ino_t` are unsigned 32-bit; no assumed 64-bit inode ABI |
| `struct stat` | Target compilation gives 120 bytes; `st_dev` offset 0, `st_ino` 4, `st_mode` 8, `st_mtim` 40, `st_ctim` 56, `st_size` 72 |

The type measurements came from compile-time assertions and the emitted constant array in `type_probe.native.ll`, never from target execution. Desktop Darwin `struct stat` is 144 bytes and uses different timestamp names; its native struct must not be passed into the PS5 ABI. The target includes closure contains 25 source/header inputs, all either owned probe files or exact ZIP-matched SDK members. The effective include search contains SDK `target/include` and `target/include_common`; nonexistent compiler-resource and `/usr/local/include` directories were ignored. No host header was consumed by the target compilation.

`llvm-nm` verified `open`, `pread`, `fstat`, `close` and `__error` exports in the ZIP-matched SDK `libkernel_web.so`. These are link stubs, not the Sony runtime implementations. The pinned [SDK export source](https://github.com/ps5-payload-dev/sdk/blob/d9c9519116944a7f1c22d262012c53b80b3520b7/sce_stubs/libkernel_web.c) confirms the names.

The ordinary BSD contract is that `pread` reads at an explicit position without moving the descriptor cursor, returns a byte count, returns zero at EOF, and reports failures through `-1`/errno. `EINTR`, short counts, filesystem errors and filesystem-specific offset limits need explicit handling. These are design expectations from [FreeBSD 11.4 read documentation](https://github.com/freebsd/freebsd-src/blob/releng/11.4/lib/libc/sys/read.2), not demonstrated PS5 behavior. `fstat` observes the already-open file and exposes device, inode, size and modification/status-change times. [FreeBSD 11.4 stat documentation](https://github.com/freebsd/freebsd-src/blob/releng/11.4/lib/libc/sys/stat.2) describes those fields.

## Throwaway implementation and desktop results

`read_at.c` uses a 96-byte caller-owned provider and no allocation. Its caller supplies the destination; every request is capped at 65,536 bytes. It checks destination capacity, `offset <= size`, and `requested <= size - offset` before addition or conversion. The physical-file provider rejects a logical size or offset above `INT64_MAX` before an `off_t` cast. A generic logical uint64 ABI can remain wider; this physical provider must report its representability limit.

Each read captures `fstat` identity before reading and again before reporting success. The observation includes device, inode, exact size, full mode, and mtime/ctime seconds and nanoseconds; atime is deliberately excluded because reads may update it. Detected changes invalidate the provider. Exact requested bytes plus both successful identity checks are required for OK. All positive short-read continuations and EINTR retries count against a finite per-operation attempt limit, including fstat attempts. A zero result before the checked range is complete is `short_eof`; other failed reads are `io_error` with captured errno. Range, unsupported physical offset, call-limit, changed-source and impossible returned-count conditions remain separate. Partial bytes on any non-OK result are diagnostic only and must be discarded by a consumer.

The desktop run's 66 checks include setup/cleanup assertions. Substantive observations were:

- Read exactly 64 KiB at offset 65,530; compare all bytes against the generated original; preserve the descriptor cursor at 9; leave 16-byte guards on both sides untouched.
- Read the last byte, accept an empty read at exact EOF, and reject beyond-EOF, oversized, undersized-capacity and UINT64 wrapping-end cases before I/O. Signed-offset extremes were synthetic geometry tests; no near-`INT64_MAX` file was created.
- Use a sparse owned file with size 4,294,967,420 and read its generated byte at offset 4,294,967,419, verifying actual desktop `fstat`/`pread` above the 32-bit range without allocating that logical size.
- Inject EINTR followed by a positive short read and continuation; inject repeated EINTR, premature zero/short EOF, EIO, an impossible count, and fstat EINTR. Retries have measured finite attempt counts, and original read errno survives the completion stat.
- Demonstrate that consuming the last available attempt on a successful read, without a completion identity check, returns `call_limit` rather than OK.
- Actually truncate through a separate owned writer before reading, and truncate during a controlled read hook; pre-read and completion fstat respectively detect the changes. A real explicit mtime change at unchanged size is also detected.
- Actually reject a directory and classify a closed-descriptor fstat failure as I/O with EBADF. Physical EIO and EINTR were injected, not produced by a damaged disk or target signal.

The final desktop command exited 0 with `RESULT: 66 checks passed; desktop owned bytes only; off_t=8 ssize_t=8 stat=144 provider=96`; sanitizer stderr was empty and the owned tempfile was removed. Leak detection was disabled because this macOS ASan runtime rejects `detect_leaks=1` before main. ASan and UBSan remained enabled. An earlier compile attempt exposed Darwin's strict-POSIX visibility rules; a syntax-only comparison confirmed that adding desktop-only `_DARWIN_C_SOURCE=1` restores `st_mtimespec` and `O_NOFOLLOW`. Neither correction changed target compilation or SDK files. Initial failures and diagnostic comparisons are retained in scratch.

## Native compile/link, provenance and runtime limits

The unchanged `prospero-clang` wrapper compiled C11 with `_POSIX_C_SOURCE=200809L`, `-Wall -Wextra -Werror -Wconversion -Wsign-conversion -Os -g`, and used the normal startup, libraries and linker script. `LLVM_CONFIG` selected the existing cached LLVM 21.1.8. The compile and link commands exited 0 without diagnostics; no `lseek`+`read` fallback, C++ runtime, archive library, custom syscall, resolver shim or SDK modification was needed.

Final native artifact: **120,904 bytes**, SHA-256 `2c1f6a329de1d4218804e2a62f8045d4919b3970b8a00d1bc4683285557f7c72`. It is FreeBSD ELF64 x86-64 ET_DYN, entry 0 at the retained `_start` symbol. Its three loads are RWE/RW/RW with 16 KiB alignment. Largest load file end is 98,332; section-derived loader prefix and section table end are 120,904; all file-backed loads and tables fit. The ordinary SDK first load is RWE, so this is not a W^X proof.

Direct dynamic undefined symbols are exactly `__error`, `_exit`, `close`, `fstat`, `open`, `pread`. `DT_NEEDED` lists `libkernel_web.sprx`, `libSceLibcInternal.sprx`, `libSceNet.sprx`. Dynamic flags contain `BIND_NOW` and `NOW PIE`. The unchanged driver selects `-z now` and `--build-id=uuid`; the artifact digest identifies this particular link, not reproducible bytes across repeated links.

The retained SDK startup includes `__crt_start`, `__kernel_init`, `__patch_init` and `__rtld_init`. Its [CRT](https://github.com/ps5-payload-dev/sdk/blob/d9c9519116944a7f1c22d262012c53b80b3520b7/crt/crt.c) opens/initializes the payload and its dependencies before calling main. The [payload runtime linker](https://github.com/ps5-payload-dev/sdk/blob/d9c9519116944a7f1c22d262012c53b80b3520b7/crt/rtld_payload.c) eagerly applies relocations and fails unresolved strong symbols. Thus absent imports can prevent main; a local successful link cannot distinguish a future startup failure from a file API failure. The six direct imports do not enumerate the SDK's internal symbol lookups or the target libraries' transitive runtime closure; [SDK runtime initialization](https://github.com/ps5-payload-dev/sdk/blob/d9c9519116944a7f1c22d262012c53b80b3520b7/crt/rtld.c) resolves additional helpers internally. These paths remain unchanged and unexecuted.

Five cached LLVM tool hashes were reverified:

| Tool | SHA-256 |
| --- | --- |
| clang | `8501bb9d3deb0250f23422a759b94651c89479937a0af29a7c7f0b98eaf3ca5a` |
| ld.lld | `0c4381c70e4229b9988e120710de371c031cedd1d6da8f415a9aa7bac21a3620` |
| llvm-config | `39616d289c2c1da34a279ccd6e9478bff10b40ae7cd30901302dd8ffad6cede5` |
| llvm-readelf | `73efdbad7294e02ed570d7156d1d1c8d77ed922c2df281435a7a1f8ad90eeb30` |
| llvm-nm | `a011020cab7ca848ebb10948c636cde63a83cf81e81156285f96f577ea70e426` |

Probe implementation SHA-256 is `1767bf84187ccb74c0d7226e24e70c380f03b25236e9ee20fa266cfcea6801d4`; header is `ed5ba767012c209eb88109188816060a241b60e47d2ce106fd5b094f54c08e27`; native fixture is `d27cb71919c2646a210d71605030ce8f93ce94f00a95361db28b9af203365bcf`. Receipts also pin the desktop fixture, type probe and runner. All 17 selected wrapper/link-script/startup/library inputs and all target-header dependencies were byte-matched to the pinned SDK ZIP. Full commands, per-input hashes, source hashes, import/section/symbol output and layout are retained in the new ignored scratch directory.

## Constraints for the production design

1. Keep registration/opening outside the core read callback. Resolve approved logical paths to owned regular-file descriptors, check read-only access and descriptor lifetime, and freeze the logical binding for the session. `O_NOFOLLOW` protects the final component only; it does not replace a controlled resolver. Closing/reusing the descriptor while inspection is active violates the provider contract. The spike's argv-open fixture is a link harness, not an approved product path resolver.
2. Keep uint64 logical sizes and checked subtraction throughout the core; report `offset_unsupported` for this physical backend when its signed offset/type or filesystem cannot represent a request. Do not narrow through `long`, `int`, 32-bit file lengths or casts before validation. Reject non-regular and negative-size metadata. Preserve native errno separately from parser findings.
3. Reserve global callback/byte/work budgets before reads. Count provider fstat/pread attempts, including retries, against a finite native budget as well. Ordinary success in this probe costs three primitive attempts; an empty read costs two. Open/setup and close/teardown need their own finite policies; do not blindly retry close after EINTR. Aggregate memory must include caller buffers, all providers, caches, parser workspaces and report output. A 96-byte provider and one caller-owned 64 KiB destination do not establish the proposed whole-inspection 8 MiB bound.
4. State stability as **`checked_only`**. Read-only open handles and matching device/inode/size/mtime/ctime observations do not prove a coherent immutable snapshot, detect every same-size write/revert, guarantee timestamp granularity, or bind current pathname replacements. Recheck between phases and at completion; detected changes invalidate reports/cursors/cache entries. Registry identity, handle/session identity, and complete participating-source identity must accompany those observations. Whole-file or range digests of externally mutable content do not upgrade this guarantee.
5. Keep unavailable sources, short EOF, I/O error, source change, physical offset unsupported, provider contract failures, cancellation, deadline and resource exhaustion separate in the eventual versioned ABI. This throwaway API demonstrates only a subset and has no ABI-version negotiation, frozen registry, cumulative page budget, cache, exception boundary or production status integration.
6. Add cooperative cancellation/deadline inputs and check them between primitive calls and phases. **A finite call budget cannot give a hard timeout**: ordinary synchronous open, fstat, pread or close can block inside a single call. `O_NONBLOCK` does not demonstrate a hard deadline on regular-file I/O. The spike has no deadline implementation and makes no preemption guarantee. Trusted callbacks can likewise fail to return; count checks and exception containment cannot recover arbitrary memory corruption or dangling contexts.
7. A later owned on-console checkpoint must identify the exact artifact and distinguish startup/import failure from observed fstat/pread results. It must verify target stat layout/field behavior, offsets above 32 bits, exact and short reads, errors, identity changes and process lifecycle using only owned fixtures. This scratch ELF is deliberately not a runtime diagnostic and was never launched. The current DDS C ABI and its 8 MiB input cap remain untouched.

Evidence files: [native receipt](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-read-at-spike/native-receipt.json), [desktop receipt](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-read-at-spike/desktop-receipt.json), [SDK input provenance](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-read-at-spike/input-provenance.json), [link inputs](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-read-at-spike/native-sdk-link-inputs.json), [layout](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-read-at-spike/native-layout.json), [owned fixture log](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-read-at-spike/desktop-owned-fixture.stdout.txt). Reproduce locally with `python3 .superpowers/native-read-at-spike/run_spike.py native` or `desktop`; the native mode compiles/links/inspects only and never executes the ELF.
