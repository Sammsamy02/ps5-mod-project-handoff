# Native diagnostic build evidence

Current artifact update: the September 30 layout correction supersedes the initial ELF described below. The reviewed current ELF is 50,848 bytes with SHA-256 `33fba54b4e18d9467a3b295aca080a411729758e12ca91f6015c9dd74dd3be3f`, and disjoint RX/R/RW 16 KiB page ranges. See `outputs/2026-09-30-native-layout-report.md`. Independent review passed 16 native tests, a fresh cross-build/static inspection and eight additional layout boundary cases. Resolver/output/return issues remain unresolved; no native ELF has been executed. The initial 34,464-byte hash and overlapping layout below are retained as historical build/review evidence.

September 30, 2026. The approved standalone diagnostic now cross-builds on this Mac into an actual x86-64 ELF candidate. Twelve local tests pass, and independent static inspection confirms the restricted properties below. The ELF has not been executed on a console. Firmware 13.60, the caller/loader ABI, inherited output, game access and all translation/runtime capabilities remain unmeasured or unknown.

## Artifact and observed checks

Source is in `outputs/ps5-mod-bridge/native-probe/`. The independent local Makefile uses explicit freestanding objects and raw pinned Clang/LLD; it does not link SDK CRT, libc or import stubs. The artifact and inspection logs remain beneath that subsystem's ignored `build/` directory.

| Check | Observed result |
|---|---|
| Artifact | `native-probe/build/native-probe.elf`, 34,464 bytes |
| SHA-256 | `7aa8bab0d0a436db6d6274d4ac5effa5b1f4a8ed986b6bc10eb3b8cd8ea7e060` |
| Format | ELF64 little-endian, x86-64, position independent (`ET_DYN`) |
| Entry | `_start`, relative address 0, defined 18-byte function in executable `.text` |
| Load layout | Two file-backed `PT_LOAD` segments, alignment `0x4000` (16 KiB) |
| Interpreter / needed libraries / named undefined imports | None |
| Relocations / initialization or finalization callbacks and arrays | None; both section and dynamic/program-header tables checked |
| Default startup / patch / kernel helpers | None linked; symbol/disassembly review confirms custom entry only |
| Direct system-call instructions | None found by conservative executable-range encoding checks and disassembly review |
| Native execution | Unmeasured; no console connection or execution occurred |

A clean rebuild reproduced the same hash. `make test` passed all 12 tests, including a desktop C fixture under AddressSanitizer and UndefinedBehaviorSanitizer. Tests cover missing resolver/getters/output, unsigned IDs and signed numeric limits, preservation of failed values, bounded serialization/output, partial/zero/negative/invalid writes, entry result slots and untouched kernel-address/pipe fields. All fixture capabilities stay `unknown`, with null evidence. No desktop fixture is represented as console evidence.

ELF rejection tests cover Mach-O/truncation, machine/alignment/non-executable entry, undefined imports, helpers, constructors, dynamic callbacks and packed relocations, hidden program-header behavior, mismatched `_start`, and system-call encodings outside `.text`. Dependency tests cover mismatched/missing hashes and escaping file/symlink paths. Archive preparation tests cover traversal, escaping links, unselected unsafe members and special tar files. RED failures were observed before their implementations/fixes; the full local suite is now green.

Independent review reproduced three checker gaps, then confirmed the fixes with negative fixtures: program headers are checked despite misleading sections, dynamic startup/RELR entries are rejected, and the actual `_start` function must match the ELF entry. The reviewer inspected the current artifact independently and found no additional important issue after correction. Source review and a restricted verifier still cannot prove arbitrary loader/runtime behavior.

The unchanged broader project's current standard checks also passed: 77 core tests and 11 CLI tests. Those results do not validate this candidate on hardware or resolve separate platform/runtime prerequisites.

## Dependency/source evidence

Observed host: macOS 15.7.8 / Darwin ARM64. Apple Clang supplies only the desktop fixture compiler. The native cross compiler and ELF linker are official LLVM 21.1.8, source commit `2078da43e25a4623cab2d0d60decddf709aaea28`. The compiler emitted `__PROSPERO__`, `__x86_64__`, ELF, little-endian and LP64 size macros for the selected target. Those are compiler observations.

Both official release archive hashes and lengths were verified before extraction or tool execution:

| Archive | Bytes | SHA-256 |
|---|---:|---|
| [Community SDK v0.43](https://github.com/ps5-payload-dev/sdk/releases/download/v0.43/ps5-payload-sdk.zip) | 8,814,217 | `a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb` |
| [LLVM 21.1.8 macOS ARM64](https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.8/LLVM-21.1.8-macOS-ARM64.tar.xz) | 1,503,047,352 | `b95bdd32a33a81ee4d40363aaeb26728a26783fcef26a4d80f65457433ea4669` |

Expected archive digests are from the official [SDK fixed release metadata](https://api.github.com/repos/ps5-payload-dev/sdk/releases/tags/v0.43) and [LLVM fixed release metadata](https://api.github.com/repos/llvm/llvm-project/releases/tags/llvmorg-21.1.8). The release public argument header matches the [v0.43 source](https://github.com/ps5-payload-dev/sdk/blob/v0.43/include/ps5/payload.h), SHA-256 `752279ee971eee0dc1eda7286a0008caaf7cd34a99c7501d054d40866c0b8fdf`. Its LP64 layout matches the derived standalone header's measured offsets. The archive [linker script](https://github.com/ps5-payload-dev/sdk/blob/f7fd02e6e195902a449b5e664917be8c01888b34/host/elf_x86_64.x) matches SHA-256 `3bacf56a21602a1298752023a4c5b0c305954ddf6cdbce1464476fc9da9d9093`.

All dependencies stay under the probe's ignored build directory. The retained selective extraction is 10 files / 381,632,927 bytes, with compiler, linker, inspection aliases and a license. An initially selected unused library was removed after all selected tools were confirmed to use only macOS system dynamic dependencies under `/usr/lib`; the preparation script now omits libraries. No global dependency, PATH/profile change or home-directory tool installation occurred. `prepare_deps.py` reverified both archive hashes/lengths and the SDK source matches; `verify_dependencies.py` verified 13 pinned source/tool paths. A fresh project-local selective extraction reproduced all nine checked tool paths, left no staging leftovers, and was removed afterward; no native executable was launched from that reproduction. The source manifest preserves exact URLs, digests, Git blobs and tool versions. GPL-3.0-or-later notices/text for SDK-derived probe material and Apache-2.0/LLVM-exception text for tools are retained separately.

Provenance clarification: the historical SDK source-review pin `f7fd02e6e195902a449b5e664917be8c01888b34` is a post-release revision. SDK v0.43 resolves separately to `d9c9519116944a7f1c22d262012c53b80b3520b7`. The [SDK provenance correction](2026-09-30-native-sdk-host-route.md#release-identity-correction-and-impact) records byte-identical relevant header/linker/startup sources; original URLs, hashes and artifact identities remain unchanged.

The source rationale remains necessary: SDK [startup](https://github.com/ps5-payload-dev/sdk/blob/f7fd02e6e195902a449b5e664917be8c01888b34/crt/crt.c) invokes an [automatic patch](https://github.com/ps5-payload-dev/sdk/blob/f7fd02e6e195902a449b5e664917be8c01888b34/crt/patch.c) that changes process syscall permissions and credential bits. This candidate avoids that runtime entirely. The custom callable entry follows the [SDK assembly convention](https://github.com/ps5-payload-dev/sdk/blob/f7fd02e6e195902a449b5e664917be8c01888b34/crt/_start.S), while resolver handles/symbol names are candidate conventions from the SDK startup and [kernel stubs](https://github.com/ps5-payload-dev/sdk/blob/f7fd02e6e195902a449b5e664917be8c01888b34/sce_stubs/libkernel_web.c).

## Exact limits and remaining inputs

The ELF contains no operation that contacts a console, opens files/sockets/modules, changes privileges or memory permissions, modifies the kernel, injects/hooks executable code, scans another process, or writes game files. The only intended side effects on a later compatible invocation are inherited fd 1 output and the optional caller-provided result slot. Callback/pointer validity and actual behavior cannot be established offline. A supplied output function can block; eight attempts and 4096 bytes bound retries/volume, not latency.

The preserved SDK load layout is RWE at relative address 0 (`0x1664` bytes) and RW at `0x1668` (`0x2a78` bytes). Their page-rounded ranges overlap despite 16 KiB alignment. Loader mapping/permission behavior must be confirmed separately; this build does not prove a standard page-mapping loader preserves executable access. No permission-changing workaround is included.

The user has since supplied console ps5-host-not-configured.invalid, Payload Manager 0.5.2 and Stellar Blade update 01.014.001; firmware 13.60 remains a user report. A bounded manager `/version` GET returned 0.5.2. Required future evidence still includes the underlying ELF loader identity and entry ABI, its invocation/output convention and mapping behavior, verified firmware/runtime facts and actual game fingerprints. Inherited output may be redirected by that external loader; this program selects no transport or endpoint.

Follow-up source review: `outputs/2026-09-30-loader-abi-review.md` reproduces the first-slot getpid/resolver mismatch with an owned desktop fixture and identifies segment-permission overlap, discarded manager output and unmeasured spawned-process return behavior. The existing hash/static checks do not resolve those runtime issues. The artifact remains an unexecuted candidate and must be corrected before an upstream-v0.26-style loader trial.

Filesystem/TLS/UI, C++ runtime, native module loading, executable-memory behavior, Lua/UE4SS/Windows runtimes, in-game introspection and Unreal pak/asset translation stay unknown. The diagnostic has no integration that promotes its report to capability validation. It neither proves nor narrows the user's universal automatic translation requirement. It supplies the bounded native build evidence needed before broader host/backend work can be justified.
