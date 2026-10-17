# Genuine static corlib host proof

The current production `MonoOwner.cpp` passes with the genuine statically linked corlib module. The owner itself performs registration; the final harness has no registration call. The one final production run returned `REAL_MONO_OWNER_PRODUCTION_STATIC_CORLIB_HOST_PASS`, stage 7, two successful managed runs, one expected managed exception, and successful recovery and cleanup.

This is a Darwin x86_64 host proof using the previously built pinned Mono runtime, genuine configure-generated headers, the unchanged invocation boundary, and the real host observation implementation. It establishes the static registration integration and the executed Probe flow. It does not establish target runtime operation, target link compatibility, target admission, Townfall execution, or general C# plugin compatibility.

## Evidence that the static route executed

- `logs/PRODUCTION-STATIC-OWNER.stdout.log` begins `Mono: Found statically linked AOT module 'mscorlib'.` and reports AOT exception/trampoline functions and corlib methods resolved from that module.
- The owner prefix contains exactly `lib/mono/4.5/mscorlib.dll`, before and after execution. The static assembly/object live outside that prefix. No corlib AOT dylib exists there.
- `DYLD_PRINT_LIBRARIES=1` recorded the actual loaded host images. No `mscorlib.dll.dylib` load occurred. The executable links the static object and the exact frozen patched native Mono dylib.
- `MONO_PATH`, `MONO_ENV_OPTIONS`, `MONO_AOT_CACHE`, and inherited dyld overrides were absent during the actual owner run. `mono_set_dirs` supplies the actual owner prefixes.
- The final harness was compiled with `MODBRIDGE_CAP10_MONO_HOST_VALIDATION=1` and `MODBRIDGE_CAP10_STATIC_CORLIB=1`. No target admission macro or target platform macro was defined.
- No runtime assertion, fatal diagnostic, or native crash occurred. Expected logged rejections of path-prefixed owned-library aliases precede the exact owned provider's successful resolution; these are strict loader behavior, not a failed Probe.

Primary receipt: `evidence/PRODUCTION-STATIC-OWNER-CHECK.json`. The first historical proof used harness registration and an unchanged owner; its distinct receipt, executable, objects, harness, and logs remain under the `STATIC-OWNER` names. There was one owner execution for each source state, two executions total. No AOT rebuild was needed for the production integration check.

## Minimal integration and independent review

The pinned public header `<mono/mini/jit.h>` declares `mono_aot_register_module(void**)`. The upstream generated driver declares the generated module symbol as `extern void*`. The AOT compiler emits a pointer-sized global containing the address of its actual `mono_aot_file_info`; no private structure declaration is needed in embedding code.

```cpp
extern "C" void* mono_aot_module_mscorlib_info;

// Before mono_jit_init_version, after the owner's one-shot/existing-runtime guards:
mono_jit_set_aot_mode(MONO_AOT_MODE_INTERP);
mono_aot_register_module(static_cast<void**>(mono_aot_module_mscorlib_info));
```

Pass the symbol's value, not `&mono_aot_module_mscorlib_info`. Link the genuine generated object into the image that owns the runtime. The tested object uses Mach-O's leading underscore at the assembly level; the C/C++ identifier has no leading underscore. A target ELF object must be built and validated independently for that target.

The production owner now guards the declaration/call with `defined(__PROSPERO__) || defined(MODBRIDGE_CAP10_STATIC_CORLIB)`, making the target link require the actual generated corlib symbol. The call is after `MONO_AOT_MODE_INTERP` selection and before `mono_jit_init_version`. The existing claim/root-domain checks precede registration. The runtime lifecycle, provider lifecycle, and invocation boundary are unchanged.

Independent review found the change correct for this pinned runtime. Removing exactly these two added blocks reproduces the previous owner's SHA256, proving the reviewed change is limited to the declaration and registration call. See `evidence/PRODUCTION-OWNER-REGISTRATION.patch` and `evidence/PRODUCTION-OWNER-SOURCE-REVIEW.json`.

## Lifetime and matching configuration

`mono_aot_register_module` retains the assembly-name pointer and file-info pointer in `static_aot_modules`; it does not copy or take ownership of the linked image. Module loading copies the file-info structure but retains pointers into its code/data/tables. Keep the linked module image resident through all runtime activity and cleanup. The proof executable has process lifetime. The pinned code exposes no matching static-module unregister operation; cleanup destroys `aot_modules`, not the static registry. This work does not establish unloading the owner image or restarting Mono after cleanup. The production one-shot owner remains appropriate.

The unchanged owner selects `MONO_AOT_MODE_INTERP`, which sets AOT-only execution, enables the interpreter, and uses AOT trampolines. Therefore corlib was compiled with `full,interp`: pinned runtime validation specifically requires interpreter trampolines in `mscorlib`. `static` enables static registration/global tables; `asmonly` emits assembly for the platform assembler. The exact option parser recognizes all four flags and `outfile=`. No guessed cache values, fake runtime APIs, or internal ABI structures were used.

The runtime validates assembly GUID, AOT format version, GC identity, safepoints, alignment, trampoline count, and card-table facts. This successful host proof does not substitute for target ABI equality. The target worker owns that independent check.

Pinned source references, under `../cap10-mono-build/sources/mono-0cbf0e290c31adb476f9de0fa44b1d8829affa40/`:

| Source | Relevant lines |
|---|---|
| `mono/mini/jit.h` | 109–110: public registration declaration |
| `sdks/wasm/packager.cs` | 302–311: upstream generated symbol declaration and registration |
| `mono/mini/aot-compiler.c` | 8100–8130: option parsing; 11441–11445: emitted pointer; 11474–11491: symbol naming |
| `mono/mini/aot-compiler.c` | 12089–12147 and 13584: zero-default compilation thread setting; 13724–13732: full/interp flags |
| `mono/mini/aot-runtime.c` | 1857–1942: module compatibility; 2144–2159: static module lookup and trace; 2288–2293: pointer-bearing module data |
| `mono/mini/aot-runtime.c` | 2516–2555: registration contract and retained pointers; 2576–2580: cleanup |
| `mono/mini/driver.c` | 2941–2946: interpreter mode also uses AOT-only/trampolines |

## Exact identities

| Artifact | SHA256 |
|---|---|
| Pinned Mono source commit | `0cbf0e290c31adb476f9de0fa44b1d8829affa40` (Git commit) |
| Frozen compiler | `c301fbc2012d02f0f00c1625f6ffb366f46e8fa5cabfa24fd073138365d786fe` |
| Frozen strict native runtime dylib | `c564b99bf331c552323c57c4d70ce9a38d5f7e7c304fa0f343bfa1dbc3428f7c` |
| Genuine MDK corlib DLL | `86364b7803c92d8c88b590031f015f80b10b00641e9d067e07a529a131d815e4` |
| Generated Mach-O static object | `ba0de672ed0469e4737014418fef7a1079fc3d1c68f7706f1b00d8266d3ffeb0` |
| Production owner source | `a73847833fd1621a0304641e5f6add267787223474bea0d95b4c9d0848e4f732` |
| Final host executable | `373fd354e853514e1bfe3727e8047d7284b3ea6bb3229921b4102f519bbe9c24` |

The compiler reported 35,820/35,828 compiled methods, exactly the count in the earlier successful dynamic corlib build. This fixture does not claim every corlib method was AOT-compiled. The executed owner flow nevertheless passed in the selected AOT/interpreter mode. Build-time `MONO_PATH` points only at the owned genuine DLL so the uninstalled compiler can initialize; it is cleared for the actual owner.

The initial compiler attempt omitted that build-only path and stopped before AOT generation; its exact diagnostic and receipt are preserved. After fixing the compiler bootstrap environment, AOT and assembly succeeded. A reporting assertion initially overlooked the emitted `.align 3` between the module label and pointer. It was corrected and the existing successful outputs were finalized without rebuilding. Default single-thread AOT was used throughout; the prior unrelated `threads=2` failure remains documented in the frozen native-build experiment.

All writes were limited to this new experiment folder. Earlier source/runtime/dependency evidence was read-only. No target executable/PRX build, package deployment, console access, or network operation occurred in this task.
