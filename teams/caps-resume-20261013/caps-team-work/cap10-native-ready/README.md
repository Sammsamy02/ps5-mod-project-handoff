# Genuine wider native PAL — Mac readiness, 2026-10-13

The pinned Mono.Native source set now genuinely cross-compiles into a static native archive. A strict ordinary ELF link of **every archive member** with the real source-built Mono runtime succeeds. This closes the demonstrated native build and strong import gaps for this source set. It does not establish target execution, helper packaging, managed P/Invoke resolution, arbitrary C# or DLL compatibility, Unity, or production admission.

No console contact, game/PRX build, proprietary SDK/source, global installation, source replacement APIs, main/candidate edits, or frozen-folder writes occurred. All new work lives in this component.

## Stable artifacts and proof

| Result | Evidence |
|---|---|
| Genuine static PAL | `handoff/libmono-native.a`: 2,328,862 bytes; SHA256 `4efa694b678b6df329f91c1fdd4231177a8515d8761969cbaa3377f35d9b420f`; 42 ELF64 x86-64 members |
| Actual API census | 125 `SystemNative_*` definitions; all 125 retained as dynamic exports in the whole-archive link; `evidence/SYSTEM-NATIVE-EXPORTS.txt` and `NATIVE-LINK.json` |
| Strict ordinary closure link | `link-check/native-whole-archive-link-only.elf`, SHA256 `170a57a11a2652d16317ee11fcf10622c09f87c3a86b096bef85beacbd03d0d9`; `--no-undefined -z defs`, entry point zero, ET_DYN with PIE, no TEXTREL |
| Precise import closure | 296 dynamic UND entries: **290 strong imports** matched to public import libraries, **6 unresolved weak SDK hooks**; `NATIVE-IMPORT-PROVIDERS.json` |
| Actual input closure | 42 source units and 299 compiler-discovered dependency files hash-bound in `NATIVE-COMPILED-INPUTS.json`; no dependencies outside owned work, public SDK and pinned LLVM |
| Source/format audit | `NATIVE-AUDIT.json`: one compiled CoreFX source change, no native constructor/destructor arrays, no allocated absolute32/32S relocations; four allocated absolute64 pointers require normal relocations |
| Real target configuration | `NATIVE-LINE-IO-CONFIGURE.json` and log; no cached feature answers or invented OS macros |
| Exact changed-function semantics | Two host executables, five scenario groups each, both real Darwin stdio routes under ASAN/UBSan; 200 concurrent shared-stream lines per route; zero outstanding returned allocations |
| Demonstrated behavioral regression | Same harness against exact unpatched parent GetLine fails ownership accounting; `GETLINE-PARENT-REGRESSION.json` |

The ELF is a link-only image using `-nostartfiles`; it has no fixture main or replacement implementation. It is not a shared native helper or a console-loadable asset. No `.so` was produced. Public `.so` import libraries are symbol declarations, not implementations that were executed.

## Narrow source fixes

The inherited target runtime came from frozen `../cap10-port-closure/target-work`. Its seven original Mono port changes and runtime archive remain unchanged. Upstream identities are Mono `0cbf0e290c31adb476f9de0fa44b1d8829affa40`, CoreFX `c4eeab9fc2faa0195a812e552cd73ee298d39386`, public SDK v0.43 `d9c9519116944a7f1c22d262012c53b80b3520b7`, and LLVM 21.1.8. Parent source and SDK provenance receipts are hash-bound in `NATIVE-AUDIT.json`; exact tool paths remain in `LOCAL-INPUTS.json`.

Three **final patches**, bound by exact original/result hashes in `NATIVE-PATCH-BINDINGS.json`, are the reproduction inputs:

1. `patches/native-configure.ac.patch` requires a usable declaration before selecting `strcpy_s`, and probes both declarations and link symbols for line reading and stream locking. The public SDK exports `strcpy_s` without a supported declaration, so the genuine existing `strlcpy` fallback is selected.
2. `patches/native-config.h.in.patch` is the generated template from final `configure.ac`. It adds only the line-reader/stream-lock feature macros.
3. `patches/native-external-corefx-src-Native-Unix-System.Native-pal_io.c.patch` changes only `SystemNative_GetLine`. With usable `getline`, it frees any allocation after EOF/error while preserving errno. Without implemented `getline`, the declared and linkable `fgetln` route copies the borrowed byte buffer under `flockfile`, preserves the newline and embedded bytes, appends NUL, unlocks, and returns caller-owned malloc storage. No private FILE fields or platform ABI declarations are used.

The fgetln route follows the actual [FreeBSD fgetln contract](https://raw.githubusercontent.com/freebsd/freebsd-src/main/lib/libc/stdio/fgetln.3): the borrowed result includes any newline, is not NUL-terminated, and becomes invalid on subsequent stream I/O. The public SDK headers declare the exact APIs used, and target configure verifies their import symbols. `FGETLN-CONTRACT.json` binds those facts.

`patches/history/native-declaration-probe.patch` is an **historical subset** of the final configure patch. Do not apply both. The OpenSSH/NetBSD fallback under `upstream/` and `experiments/` was researched but never compiled or included. Its receipt explicitly marks it unused; the final implementation uses existing SDK stdio APIs.

Only `HAVE_STRCPY_S` changes an existing configuration answer. Eight new line-reader/stream-lock macros are added. The generated delta is `NATIVE-CONFIG-DELTA.patch`. These macros select PAL code and do not change runtime structure layouts. The inherited runtime archive is still SHA256 `38d0342104fe5c7741225c6d4e6937d1e60869ed3ab2c409a743994320e6cca9`.

## Coverage and precise limitations

The actual upstream Mono.Native source set compiles file/status/directory/memory/time/user/random/socket/name-resolution/kqueue-event and Brotli implementation units. An exported function count is not a claim that every operation succeeds on a console.

There are 136 `SystemNative_*` declarations in the headers corresponding to the selected source units. The 11 missing exports are the upstream `HAVE_TCP_VAR_H`-gated TCP/UDP/IP/ICMP/interface/route statistics functions, listed individually in `NATIVE-AUDIT.json`. They are optional diagnostics and enumeration, not missing ordinary file/socket implementations. This build does not provide fake diagnostic results.

The existing upstream source also deliberately reports unsupported operations where capabilities are absent: inotify watchers, domain-name retrieval, socket sendfile, pipe-size control, Linux SO_PEERCRED peer identity, MADV_DONTFORK advice, and non-Mach absolute time. Other flags/options can be rejected by the genuine PAL conversions. Public import availability does not establish console permissions, filesystem paths, socket behavior, credentials, clocks, entropy, GC cooperation, or actual runtime behavior.

GSS, BTLS, MonoPosixHelper and the rest of the full CoreFX native library family are not supplied by this archive. The generated config's wider dllmaps are not evidence that these helpers exist. Brotli C code is compiled, but no general managed compression/DLL compatibility claim follows from that fact.

The six unresolved weak SDK hooks are `__dlopen`, `__dlsym`, `__dladdr`, `__dlclose`, `__dlerror`, and `kernel_mprotect`. The actual linked SDK dl wrappers check missing hooks and return NULL/-1. The SDK `mprotect` PROT_EXEC path calls `kernel_mprotect` without a NULL check. This is retained SDK behavior, not a new implementation; `SDK-WEAK-HOOKS-DISASSEMBLY.txt` records the evidence. Loader integration and any executable-memory path must account for it. All 290 strong imports resolve through `libkernel_web.sprx`, `libSceLibcInternal.sprx`, and `libSceNet.sprx`. No fake Mono import library is linked.

## Actual loader, initialization and lifetime contract

The matching generated config still dllmaps `System.Native` to `$mono_libdir/libmono-native.so`. A static archive does not satisfy this loader mapping by itself. Claude must choose and verify a real target helper package or a reviewed owned static-provider mapping. This component changes neither the owner/provider nor the runtime loader.

In the pinned managed source, `Interop.Sys`'s static constructor P/Invokes `mono_pal_init` from `System.Native`. The actual native function atomically registers `Interop/Sys::Read` to the genuine `ves_icall_Interop_Sys_Read`, which performs the real PAL read with Mono GC-safe transitions and last-error handling. `mono_native_initialize` separately registers a test counter icall; it is not general runtime initialization. The platform identity remains honestly `MONO_NATIVE_PLATFORM_TYPE_UNKNOWN` for this unassigned public port.

The archive has no native constructor/destructor arrays. Its global platform identity, atomic once state and counter require normal initialized data/BSS semantics. Native code/data must remain alive until managed/runtime cleanup and all calls and registered icalls have stopped, because registration stores native function pointers. No unload/reload lifecycle was executed here.

## Reproduction and handoff

Carry `tools/`, final `patches/`, `evidence/NATIVE-PATCH-BINDINGS.json`, and `LOCAL-INPUTS.json` into a **fresh sibling component**. Do not overwrite this completed `target-work` or any frozen source. The staging utility clones the genuine frozen baseline using APFS copy-on-write, checks exact source/patch hashes, applies the final three patches, and runs the existing local Autoheader/Autoconf. Its fresh-copy recipe was executed and regenerated identical sources/configure; the temporary verification clone was removed after recording hashes.

```sh
python3 tools/stage_native.py
python3 tools/build_target.py configure --label NATIVE-LINE-IO
python3 tools/native_preflight.py
python3 tools/build_target.py build --part native --label NATIVE-LINE-IO
python3 tools/test_getline_host.py
python3 tools/link_native.py
python3 tools/audit_native.py
```

All target compiler commands are serial (`make -j1`). The preflight refuses inherited frozen-path bindings and captures `make -n` before compilation. Configuration is necessary after cloning because generated Makefiles/config.status/libtool embed absolute source/build paths. Staging alone does not make those generated files safe to build.

`HANDOFF-MANIFEST.json` selects the native archive, exact final patches, config/dllmap, tooling, host test, command logs, input/export/provider/format audits and provenance. Source/runtime proof ends at compile, host semantics and link. Actual target P/Invoke, initialization, cleanup, helper loading, engine mount, managed exception recovery and game continuation remain console integration tests. #13 DLL discovery/compatibility remains separate.
