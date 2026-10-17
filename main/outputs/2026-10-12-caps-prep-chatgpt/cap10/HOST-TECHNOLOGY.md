# Real execution candidate: embedded Mono interpreter

Design-only host selection, Mac-only upstream source audit. Selected audit baseline: upstream Mono tag `mono-6.12.0.206`, commit `0cbf0e290c31adb476f9de0fa44b1d8829affa40`. This is a pinned investigation baseline, not a claim of current support, security fitness or console compatibility. Fourteen selected source/license files are retained under upstream/ with exact SHA-256 receipts. No runtime was installed, compiled, loaded or executed.

Mono has an actual native interpreter and embedding API, so it can address the absent-managed-host problem in principle. ILRuntime is implemented in C# and requires an already working managed environment, so it does not supply the initial native host. DotNetAnywhere is a smaller native C interpreter but its upstream is inactive; its legacy loader and platform assumptions add another port project. Neither alternative is claimed impossible; Mono has the concrete embedding interfaces needed for this diagnostic.

Primary references: [Mono embedding](https://www.mono-project.com/docs/advanced/embedding/), [Mono pinned license](https://github.com/mono/mono/blob/0cbf0e290c31adb476f9de0fa44b1d8829affa40/LICENSE), [interpreter source](https://github.com/mono/mono/blob/0cbf0e290c31adb476f9de0fa44b1d8829affa40/mono/mini/interp/interp.c), [driver source](https://github.com/mono/mono/blob/0cbf0e290c31adb476f9de0fa44b1d8829affa40/mono/mini/driver.c). Alternative primary sources: [ILRuntime repository](https://github.com/Ourpalm/ILRuntime), [DotNetAnywhere repository](https://github.com/chrisdunelm/DotNetAnywhere).

## Actual blockers discovered in source

`driver.c:2441-2446` selects interpreter execution mode; `:1981-1994` rejects builds lacking interpreter or architecture support. This is an actual runtime implementation, not a hypothetical transpiler.

The newer source audit in transition-audit/ distinguishes the actual Probe paths. Its two simple imports may use Mono's architecture-dependent native fast-call branch; the generic interpreter-to-native trampoline is conditional. Separately, current INTERP-mode exception startup requires five named AOT trampolines from matching mscorlib. An interpreter option does not establish a complete native-transition/exception implementation or freedom from executable-memory requirements. Target ABI, AOT loader and context/unwind behavior remain unproved.

Target platform port, threading/TLS, GC root registration, signals/unwind, memory protection, loader/library resolution, file access and libc integration remain absent. The selected headers are not a complete build checkout. Do not attempt to compile them alone. Corlib and dependencies also need a pinned reproducible licensed build. P/Invoke marshaling must be checked on target; source preflight cannot verify it.

## Future source integration boundary

A separate runtime-owned adapter (not added here) owns Mono initialization and one domain for the diagnostic. Keep it isolated from UE4SS C++ object ownership. Bind only the two declared diagnostic functions; it does not expose UObject pointers, invoke game methods or add game-specific fields. Native callbacks contain C++ exceptions. Managed invocation captures `MonoObject*` exception separately from a returned boxed Int32.

Proposed sequence, backed by actual public header names in HOST-PLAN.json:

1. Configure confined assembly/config roots through mono_set_dirs. Before mono_jit_init_version, select mono_jit_set_aot_mode(MONO_AOT_MODE_INTERP). This enables interpreter plus AOT-only execution; it does not force every method into the interpreter, and runtime invocation may prefer available AOT code. Target AOT/wrapper/trampoline support must be proved first. Do not pass --interpreter to mono_jit_parse_options: its pinned embedding parser does not handle that executable-driver option.
2. Attach the adapter-owned thread with mono_thread_attach. Do not call engine APIs from that worker; the readiness diagnostic must use a verified synchronized runtime state.
3. Open Probe.dll with mono_domain_assembly_open, get its image, class and Run method with explicit parameter count. Verify the actual method signature in the eventual assembly; matching the name/count alone is insufficient.
4. Resolve imports explicitly. The current __Internal declarations do not prove visibility from an injected module. Actual native-library.c performs ordinary process/library lookup and symbol probing; fallback callbacks alone do not enforce the proposed two-symbol allowlist. An assembly-scoped dllmap and dedicated virtual library are possible mechanisms only after their loader/probing/fallback behavior is verified. Reject unexpected assembly metadata/imports before execution and prove resolver confinement, symbol provenance, lifetime and error ownership. No resolver exists here.
5. Pass a NONNULL exception-output pointer to mono_runtime_invoke; check it before inspecting the result. Compare mono_object_get_class with mono_get_int32_class before unboxing successful Run results. Preserve/root any returned managed object or exception as required by the actual host transition/GC policy; the mere availability of these APIs is not rooting proof. Run distinct cookies and the controlled throw method, then a successful call again.
6. Keep GC handles/thread ownership and domain lifetime explicit. HOST-PLAN now includes actual mono_gchandle_new/get_target/free declarations for retained objects; validate initial rooting windows, moving-GC behavior and cleanup on every path. Avoid shutting down while code/callbacks remain active. No hot unload claim. Confirm clean game exit and bounded memory in the later console proof.

Licensing: the pinned upstream notice describes runtime/class libraries generally under MIT with listed BSD components and separate build/library exceptions. Preserve applicable notices/PATENTS and inspect the final selected dependency closure before redistribution. No console SDK or proprietary runtime is part of this selection.

## Runnable checks now

From repository root:

```sh
python3 -B outputs/2026-10-12-caps-prep-chatgpt/cap10/verify_host_sources.py
python3 -B outputs/2026-10-12-caps-prep-chatgpt/cap10/probe_preflight.py outputs/2026-10-12-caps-prep-chatgpt/cap10/Probe.cs outputs/2026-10-12-caps-prep-chatgpt/cap10/native_bridge_contract.h
python3 -B outputs/2026-10-12-caps-prep-chatgpt/cap09/test_prep_tools.py
```

These verify pinned source integrity, 17 real API declarations, bounded probe source signatures and refusal of mutated contracts. The separate assembly_preflight.py reads PE/CLI metadata only. None compiles C#, verifies IL behavior, implements the adapter or runs managed code. Console proof and full Unity plugin compatibility remain outstanding. TRANSITION-REQUIREMENTS.json adds14 source-backed prerequisite rows; its source hashes do not count as native or managed tests.
