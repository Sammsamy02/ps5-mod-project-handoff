# Genuine target corlib cross-AOT handoff

Standalone artifact generation passed. The frozen object is genuine ELF64 little-endian x86-64 relocatable output assembled by the public Prospero compiler from a real Mono cross-compiler configured with `--target=x86_64-sie-ps5`. No target execution, PRX build, packaging, or deployment was performed.

## Consume these artifacts

- `artifacts/mscorlib.full-interp.ps5.o`: 35,758,480 bytes; SHA256 `a9f0a8564ac09739d03b2efd4d7f06fa54c003d24a060f72a7966811251840de`.
- `artifacts/libmscorlib-full-interp-ps5.a`: 35,758,734 bytes; SHA256 `a17607545dcba32be9982e8b836b89a64f632970d0686f1f1ee4d8e4a86424f4`. Its only member extracts byte-for-byte to the object above.
- Use either the object or its archive, not both. The generated global `mono_aot_module_mscorlib_info` has one `R_X86_64_64` relocation to the real file-info record. There are no undefined ELF symbols in this corlib object; Mono resolves runtime calls through its AOT tables/GOT.
- Keep the exact managed corlib alongside the owner runtime. `managed-input/mscorlib.dll` SHA256 is `86364b7803c92d8c88b590031f015f80b10b00641e9d067e07a529a131d815e4`, GUID `207D1288-4485-488D-9288-CC8CBF474289`.

The embedding owner must register the linked static module before `mono_jit_init_version` loads corlib. Pinned `mono/mini/jit.h` declares `mono_aot_register_module(void **)`; pinned `aot-runtime.c:2516` and the generated iOS driver show that the exported symbol's **value** is passed, not its address:

```cpp
extern "C" void* mono_aot_module_mscorlib_info;
// Once in the owning runtime bootstrap, before mono_jit_init_version:
mono_aot_register_module(static_cast<void**>(mono_aot_module_mscorlib_info));
```

Registration belongs to the sole runtime owner. Retain the linked module for the runtime's lifetime. These files do not change the production owner. A separate target corlib `.so` and its dynamic loading are unnecessary for this static route.

## Source, compiler and ABI binding

The upstream Mono commit is `0cbf0e290c31adb476f9de0fa44b1d8829affa40` (Mono 6.12.0). The host source copy was taken from the completed `cap10-mono-build/runtime-work`, with copied compilation products removed before rebuilding. Archive comparison checks 56,725 Mono and 18,730 corefx upstream files. The only upstream-source deltas in this cross tree are the inherited strict-loader and host `_Bool` compatibility patches plus the narrow cross-target configure case. Generated `config.sub` separately accepts the real `ps5` tuple. See `SOURCE-INTEGRITY-CHECKED.json` and the two configure patches.

The rebuilt Darwin-host compiler SHA256 is `78c35e95f2c061c0d7bb1613adfef6cf80d8ee01f0ca04c07465979e6ef46f8c`. Its configuration defines `MONO_CROSS_COMPILE` and `TARGET_AMD64`; `TARGET_MACH` and `MONO_KEYWORD_THREAD` are undefined. The host executable is correctly Mach-O; the generated target object is ELF. No supplied platform macro, host-object relabeling, or `mtriple` workaround is used.

The public target compiler compiled actual `sizeof`, `__alignof__`, and `__builtin_offsetof` expressions using genuine configured target headers. The complete table contains 278 metadata and 91 mini constants. Its header SHA256 is `e779dee188838da2381b0d21f9bc932bce96cca826606c1656012822a6ca34a0`. The final audit recompiles the same probes against the actual native target source and final config SHA256 `9f0324038b43cb4e01a20bb00b3e8c8e2a8083d9a5895aa8cbe5c6d032c34ee6`; all 369 constants match. The eglib config SHA256 is `987d4a65f6e7a26e43ef368ea742544e426a0037ad8568018780b183bf4b5bca`. Later ABI-affecting changes require this audit again.

Actual object metadata, read with a layout compiled by the target compiler, reports AOT file version 172, flags 290 (`FULL_AOT | SAFEPOINTS | INTERP`), double/long alignment 8, card-table shift 9 and mask 8388607. Primary AOTID is `65F000F7-EE1A-0ADA-DFFB-4278E266C4D9`. The embedded runtime-version string is empty (upstream default); compatibility is pinned externally by this source/compiler/ABI/corlib evidence, not by an invented runtime-version binding.

## Verification and scope

All native cross build subdirectories passed with `make -j1`. `--aot=full,interp,static,asmonly` exited zero, followed by public target assembly exit zero. Mono reports 35,820/35,828 methods, exactly the count in the separate successful host static-owner fixture. A preserved diagnostic run identifies an `Unsafe.As<T>` gsharedvt failure and two disabled COM error-info wrappers; upstream logging does not name the other five non-emitted entries. This is not a claim that every corlib method or target runtime path has executed.

TLS evidence combines actual configuration, pinned source and five selected real getter/trampoline disassemblies. `method-to-ir.c:1690` refuses fast inline TLS for AOT and emits runtime getter calls. `tramp-amd64.c:437` likewise emits the runtime LMF getter call. The selected generated functions contain zero FS/GS operands. The complete raw `.text` disassembly also decodes embedded data: its only three pre-`jit_code_end` segment-looking decodes occur in the calendrical helper's constant pool after its return at `0x40ff56`; remaining raw matches lie in tables. This is not a formal control-flow proof. See `ARTIFACT-VERIFICATION.json` and `TLS-DISASSEMBLY.log`.

Native runtime linking, production-owner target registration, and eventual target execution remain separate integration gates. The artifact does not prove those gates. No production, SDK, main-tree, or frozen #7 file was modified by this work.

## Reproduce in a fresh owned directory

The tools preserve existing receipts/outputs; do not rerun over this frozen directory. Start from the same immutable source/host bootstrap and target config identities. Clone the stable host runtime-work into a fresh `cross-work` (`cp -cR` on this Mac), then use the same scripts with their directory-relative roots:

1. `tools/prepare_cross.py` removes copied products, applies the narrow tuple patches and snapshots target config. Review hashes against this receipt before continuing; current upstream capability changes can legitimately change the initial snapshot.
2. `tools/build_cross.py configure` runs local autoconf 2.69 and real `--build/--host=x86_64-apple-darwin24 --target=x86_64-sie-ps5 --with-cross-offsets=... --with-tls=pthread`; exact flags/environment are in `evidence/CONFIGURE.json`.
3. Generate the complete ABI table with `tools/generate_offsets.py`, then `tools/build_cross.py build` (`-j1`). Historical failed generator/build attempts are preserved; the final generator fixes duplicate primitive sizes, omitted fields, and names containing double underscores.
4. `tools/aot_corlib.py` uses the hash-bound input and default single-thread AOT, then public `prospero-clang -c` with the pinned LLVM_CONFIG. Exact commands are in `AOT-ARTIFACT.json`.
5. Generate the full disassembly with pinned `llvm-objdump -d --no-show-raw-insn`, then `tools/verify_artifact.py` and `tools/archive_corlib.py`. `tools/audit_final_target_layout.py` pins the final native headers and compares all 369 constants. `tools/audit_skipped_methods.py` is the optional preserved diagnostic AOT run and does not replace the primary artifact.

The AOTID and upstream constant-pool padding need not be deterministic between runs. This handoff freezes the exact verified artifact hashes, not a promise of byte-identical recompilation.
