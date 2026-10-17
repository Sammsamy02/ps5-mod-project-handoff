# Minimal C# execution proof, without an invented host

Design-only. `Probe.cs` is a source file intended for a library assembly, not a compiled assembly deliverable. `native_bridge_contract.h` specifies proposed future adapter entry points; none are claimed to exist. This does not port Mono/CoreCLR, implement Unity bindings, or run arbitrary PC plugins.

The host must execute managed IL or a declared equivalent inside Townfall's process, resolve the diagnostic library, and invoke `ModBridge.Cap10.Probe.Run(Int32)`. Select execution technology only after verifying official source/license/platform requirements; the old CAP10 document explicitly marks its platform assertions as training knowledge, so they are not implementation evidence. No JIT or executable-memory permission is presumed. A native rewrite returning the same marker is not C# execution proof.

## Proposed boundary

Only fixed-width signed 32-bit scalars and one synchronous output pointer cross. C# Cdecl and C `int32_t` are a desired target contract, not a verified target ABI. Verify calling convention and integer/pointer marshaling with the chosen host/toolchain. `__Internal` requires explicit host import registration or a verified symbol resolver; it must not silently bind to an unrelated symbol. A host lacking P/Invoke must provide equivalent explicit bindings or use a documented contract revision.

The runtime adapter reports ABI 1. Probe must check actual runtime initialization through the runtime's verified state seam (implementation absent), then synchronously log the request and echo cookie only if ready. The reply pointer is owned by the managed marshaler for the call, never retained. No engine object pointers, callbacks, strings, offsets or C++ objects cross. Exceptions are contained separately at native adapter and managed invocation boundaries. Missing/version-mismatched imports fail closed with one host diagnostic, without invoking further entry points.

## One-launch evidence, executed later by console owner

1. Record exact host/assembly/native artifact hashes, execution mode (interpreter/AOT etc.), runtime host license/commit, and game-process identity through the existing diagnostic logger. Confirm assembly was loaded by the selected host, not synthesized log output.
2. Host logs entry into the managed Run method with a fresh per-launch cookie. Native runtime probe logs the same cookie plus runtime-ready result; host records managed return status 0. The adapter checks real runtime readiness before echo; synthetic Mac echo tests do not count.
3. Invoke Run twice with distinct cookies to detect canned output and stale state. Execute `ThrowForHostBoundaryTest`; host records the managed exception type, then successfully invokes Run again. Exercise the absent/version-mismatched binding in isolated host validation before deployment, with no process crash.
4. Record game responsiveness, stable memory/GC behavior over repeated managed invocations and clean close. One launch is the current scope; three boots apply only to kernel work.

Failure is useful evidence, never a success verdict. This diagnostic intentionally proves only managed execution and a diagnostic runtime call. Full Unity proof still requires real dependency loading, game type bindings/IL2CPP integration where applicable, object lifetimes, callbacks and actual plugin behavior.
