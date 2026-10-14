# Native diagnostic candidate

This independent, freestanding C11 diagnostic cross-builds into `build/native-probe.elf`. It advances native-host build evidence for the universal translator. It does not implement mod translation, a console manager, runtime backends or game attachment. The ELF has never been executed on a console; no firmware or game support is validated.

The candidate callable entry uses the community SDK LP64 argument layout. It requests only `getpid`, `getuid`, `geteuid`, `getpagesize` and `write` through the caller-supplied resolver. It formats a fixed-size report and writes at most 4096 bytes to inherited fd 1 in at most eight attempts, then returns and optionally stores a result in the supplied result slot. Missing getters preserve other observations. Missing resolver/output or failed/unfinished output returns failure. A write or resolver supplied by an external loader can block; there is no latency guarantee.

SDK source review pinned post-release `f7fd02e6e195902a449b5e664917be8c01888b34`; the v0.43 archive's release commit is `d9c9519116944a7f1c22d262012c53b80b3520b7`. Relevant bytes match both. See [SDK provenance correction](../../2026-09-30-native-sdk-host-route.md#release-identity-correction-and-impact); original URLs, hashes and artifact identities are retained.

The probe does not link the default SDK CRT. That startup calls a patch routine that changes process permissions/credential bits. Sources, caller pointers and runtime compatibility need review before any later console experiment. No deployment target exists.

## Local checks

From this directory on Darwin ARM64, using an already prepared local toolchain:

```sh
make test
make inspect
```

`make test` compiles and runs only the desktop fixture with Apple Clang, AddressSanitizer and UndefinedBehaviorSanitizer, plus Python rejection tests. Fixtures are labeled `desktop_fixture`; they never imply a console run. `make inspect` cross-builds with pinned LLVM 21.1.8 and the local GPL-derived `elf_x86_64.x`, hashes the local tools and unchanged vendor sources, audits ELF structure and saves disassembly. It never executes the ELF. All compiler/linker inputs are explicit freestanding objects; no libc, default libraries, CRT, import stubs, constructors or direct system-call instructions are permitted. Set `PYTHON=/absolute/path/to/python3` on either Make command to select an existing runtime.

The verifier accepts this restricted ELF subset. It checks executable program-header ranges as well as sections, dynamic tables from both sources, entry function identity, imports, callbacks and relocations. Every nonempty load starts on a 16 KiB boundary, has a bounded uint64 address and page-rounded end, and occupies pages disjoint from the other loads. The conservative encoding scan can reject non-instruction bytes that resemble system calls. These checks and source review provide static evidence, not a general proof of safe execution or loader compatibility.

Output files are local and ignored by Git:

| File | Meaning |
|---|---|
| `build/native-probe.elf` | Native candidate; runtime execution unmeasured |
| `build/static-report.json` | Static properties and artifact SHA-256 |
| `build/readelf.txt` | Independent LLVM header/segment/dynamic/symbol inspection |
| `build/disassembly.txt` | Independent LLVM disassembly |
| `build/test-report` | Desktop fixture executable; not a PS5 payload |

Every runtime/translation capability in emitted JSON remains `unknown` with null evidence. Firmware, loader/version, game/update remain null. Getter observations do not create capability validation records and should not be used to satisfy the core planner's capability requirements. A real compatible invocation would use `native_entry_unverified` as its evidence origin, preserving that limited scope.

Entry result codes are 0 for report emission completed, 2 for missing arguments/resolver, 3 for missing/failed/incomplete output, and 4 for report capacity failure. Getter resolution failure is recorded per observation when output is available. A numeric getter failure value is preserved as `unavailable`; an unresolved getter has a null value. This code cannot validate arbitrary caller pointers or recover from an invalid callback.

## Bounded dependency setup

The current local environment was checked as macOS 15.7.8 / Darwin ARM64 with more than 12 GiB free. No global tools were installed. On another supported Mac, check that same host architecture and free-space requirement before downloading. Python 3.9+, Make, Apple Clang and curl are prerequisites. The source manifest pins official archive URLs, release metadata digests, lengths, included source files and observed extracted tool hashes.

Only download beneath this probe's ignored build directory:

```sh
mkdir -p build/deps
curl -fL --retry 2 -o build/deps/sdk.zip https://github.com/ps5-payload-dev/sdk/releases/download/v0.43/ps5-payload-sdk.zip
curl -fL --retry 2 -o build/deps/llvm.tar.xz https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.8/LLVM-21.1.8-macOS-ARM64.tar.xz
python3 prepare_deps.py
```

`prepare_deps.py` performs no network contact. It checks Darwin ARM64 and free space, verifies both archive hashes and sizes before opening them, compares the SDK release header/script to the pinned source copies, validates every archive path/link and rejects special tar members. It selectively extracts compiler, ELF linker, inspection tools and license files through a private local staging directory. The inspected binaries require no in-tree support libraries. It publishes the extraction only after validation; no system paths are touched. Existing selected tools are checked rather than overwritten. Symlinks may not escape the package or project. A failed extraction preserves source files and removes only its own staging directory.

The selected compiler/linker/inspection binaries were inspected with macOS `otool`; their dynamic dependencies are system libraries under `/usr/lib`. Hash checks run before their build/inspection invocation. Review those dependency paths again if changing the pinned toolchain. The 1.51 GB archives and roughly 382 MB selective extraction stay in `build/deps`; no persistent PATH or shell-profile changes are required.

`make clean` deletes only listed build products and keeps the dependency cache. After preserving any wanted local reports, removing this probe's known `build/deps` directory discards the cache. It can be prepared again from the same verified archives. Never use a home-directory cleanup or global installation command for this subsystem.

## Unmeasured runtime boundary

The user reports firmware 13.60, Payload Manager 0.5.2, console ps5-host-not-configured.invalid and Stellar Blade update 01.014.001. A bounded manager `/version` GET returned 0.5.2. The underlying ELF loader identity/version, firmware identity and game fingerprints remain unavailable. The candidate SDK entry ABI, resolver handle/symbol conventions, callable stack restoration, inherited output and getter results are unmeasured on the console. Compiler `__PROSPERO__` and LP64 macros are build observations, not console ABI observations.

The user subsequently identified WebKit Autoloader v0.5.0. Its official source pins the downstream `itsPLK/ps5-elfldr` release v0.26-bb1e117, whose listener is **loopback-only**. This narrows the release-source contract; the installed/cache bytes and running loader are still unverified. `../../2026-09-30-webkit-autoloader-identification.md` records exact pins, fork differences and evidence. It confirms the getpid-slot mismatch also exists in that fork. Direct desktop-to-console 9021 access is not supplied by this chain.

The September 30 source review in `outputs/2026-09-30-loader-abi-review.md` identifies known candidate incompatibilities with upstream elfldr v0.26. Its first argument slot points at a getpid syscall stub rather than the true resolver assumed here; an owned getpid-like fixture returns code 3 without output. The manager's launcher also closes its socket without capturing stdout, and spawned-process return behavior is unmeasured. **Do not treat this artifact as ready for that loader.** The layout correction below resolves the static overlap; resolver and report/return transport still need correction before hardware execution.

The pinned upstream script remains unchanged in `third_party/elf_x86_64.x`. The build uses a local derived script that emits separate RX, R and RW loads starting at `0`, `0x4000` and `0x8000`. Their page-rounded ranges are disjoint. This corrects the earlier static page overlap; actual loader mapping remains unmeasured. Source review also found that elfldr v0.26 supplies a `getpid` stub in the first argument slot, while this candidate expects a name resolver. Payload Manager's launch path does not collect stdout, and spawned-process termination remains unresolved. The candidate is not ready for hardware execution; no resolver shim, direct syscall, transport or termination change accompanies this layout correction.

Filesystem, TLS, controller/display UI, C++ standard-library behavior, modules, Lua/UE4SS/Windows runtimes, executable-memory behavior, game introspection and asset translation remain unknown. The program does not open files, sockets or modules, change privileges, patch the kernel, inject/hook code, scan another process or write game files. Any future hardware experiment requires a separate concrete environment and authorized scope; this project supplies no execution command.

## Development receiving client

`diagnostic.py` adds artifact inspection, strict report parsing and a **numeric loopback fixture sender**. It checks one immutable file snapshot against caller-supplied size/hash and the restricted ELF verifier, excludes trailers outside the loader-consumed prefix, and bounds report output to 4096 bytes. A single socket deadline covers connect, short writes, write-half-close and reading to EOF. It never retries. Non-loopback addresses and hostnames are rejected before any socket; no remote invocation command or UI route exists.

Tests use the saved ELF only as data. Owned Python socket peers read that data and send synthetic JSON; they do not load or execute it. A parsed result is `report_received_unverified`: there is no nonce, artifact binding, authentication or firmware/loader attestation. The tool is development infrastructure and is not an accepted PC dependency of the finished console-only app. Existing native ABI blockers still prevent a hardware-ready payload.

Run `make test` here to include these tests. Python 3.9+ is supported; local checks use both the existing Python 3.9.6 and bundled Python 3.12.14. Filesystem preflight and local report parsing are byte-bounded but not forcibly time-preempted. A separately reviewed on-console receiver/bootstrap is needed to collect stdout from the exact bundled loader.

## Sources and licensing

This probe and SDK-derived layout/entry material use GPL-3.0-or-later. The original public header and linker script retain their upstream notices; full GPL text is in `third_party/SDK-GPL-3.0.txt`. LLVM's Apache-2.0 license with LLVM exceptions is in `third_party/LLVM-LICENSE.txt`; it licenses the build tools separately. Source/asset identities and exact hashes are in `third_party/manifest.json`.

Primary source references:

- [SDK v0.43 argument layout](https://github.com/ps5-payload-dev/sdk/blob/v0.43/crt/payload.h), [public payload API](https://github.com/ps5-payload-dev/sdk/blob/v0.43/include/ps5/payload.h)
- [SDK entry assembly](https://github.com/ps5-payload-dev/sdk/blob/f7fd02e6e195902a449b5e664917be8c01888b34/crt/_start.S), [startup](https://github.com/ps5-payload-dev/sdk/blob/f7fd02e6e195902a449b5e664917be8c01888b34/crt/crt.c), [automatic patch](https://github.com/ps5-payload-dev/sdk/blob/f7fd02e6e195902a449b5e664917be8c01888b34/crt/patch.c)
- [SDK kernel symbol stubs](https://github.com/ps5-payload-dev/sdk/blob/f7fd02e6e195902a449b5e664917be8c01888b34/sce_stubs/libkernel_web.c), [linker script](https://github.com/ps5-payload-dev/sdk/blob/f7fd02e6e195902a449b5e664917be8c01888b34/host/elf_x86_64.x)
- [SDK fixed release metadata](https://api.github.com/repos/ps5-payload-dev/sdk/releases/tags/v0.43), [LLVM fixed release metadata](https://api.github.com/repos/llvm/llvm-project/releases/tags/llvmorg-21.1.8)
