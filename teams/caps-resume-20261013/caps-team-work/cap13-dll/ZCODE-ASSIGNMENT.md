# Capability 13 — downloaded PC DLL mods

**Owner: ZCode. Status: added to the roadmap; implementation not started.** Finish and prove capabilities #1–#12 first, then integrate/test #13. Mac preparation may run separately without delaying the current builds. GPT reviews ZCode's delivered work; Claude remains the integrator and console owner.

## Goal

Run downloaded PC DLL mods unchanged on the PS5 across supported games. Townfall is the first test game, not a special-case implementation. The long-term goal is broad DLL compatibility; publish actual supported interfaces and tested mod coverage rather than claiming every DLL works from a few successes. Mods must match their game's supported version.

This is a new compatibility layer on top of #1–#12. The current #9 native C++ host admits rebuilt modules using `mbm_mod_entry_v1`; it explicitly refuses original Windows PE / start_mod-only UE4SS binaries. #10 currently provides a minimal C# runtime proof. Neither alone proves downloaded PC DLL compatibility.

## Work assigned to ZCode

1. Choose a real, legally obtainable Townfall DLL mod whose initial requirements are UE4SS functions. Record its original download/source, version, license, hash, dependencies and required interfaces. Preserve the original file. If no suitable real Townfall DLL mod exists, report that evidence gap and use a clearly labeled Windows DLL fixture for preparation; do not call that a real-mod pass.
2. Add format-aware discovery for native Windows DLLs and managed C# assemblies. Identify architecture, runtime/framework and dependencies from the file contents. Keep the existing native PS5-module route intact.
3. Build the native Windows DLL compatibility route: real image loading, Windows calling conventions, imports/runtime services and the UE4SS binary interface required by the selected mods. Expand reusable support by interface/framework, without per-mod rewrites or hand-written game offsets. Merely renaming a DLL or rebuilding its source as a PS5 module does not meet #13.
4. Use #10 for compatible managed DLL mods, adding real mod discovery, dependency resolution, framework/API integration and lifecycle beyond the embedded Probe. Keep native and managed test results distinct; mixed/native dependencies need their own demonstrated support.
5. Expand toward the broad compatibility goal using actual mod dependency inventories. Begin with UE4SS-only functionality, then add the Windows/framework services that real mods require. Report unsupported requirements precisely. Do not implement a complete unused Windows surface before the first mod works.

## Dependencies and limits

| Capability | Contribution to #13 |
|---|---|
| #9 | Native mod services, lifecycle and engine bridge; additional Windows/UE4SS binary compatibility is still needed |
| #10 | Managed runtime for compatible C# DLLs; real mod/framework support extends its minimal proof |
| #11 / #6 | Mod windows and input, where a mod uses them |
| #2 / #4 / #5 | Engine/version admission, load timing and crash diagnosis |
| #7 / #8 / #12 | Pak, asset and shader support where required by the mod |
| #1 / #3 | Potential provider support for loading/executing foreign native code, subject to their actual proved capabilities |

All #1–#12 remain required in the overall project plan. A Windows DLL loader does not itself unlock protected game code. Treat DLLs that rewrite game instructions as unsupported while that operation remains unavailable; only change that status after a separately established, reviewed and tested capability exists. Do not infer it from kernel access or DLL mapping alone. Existing kernel authorization and three-restart proof requirements remain with #1/#3.

## Acceptance

- A real original Windows DLL mod loads, resolves its required interfaces, performs its intended visible action and shuts down cleanly. Input DLL bytes/hash remain unchanged. A rebuilt native module is a #9 result, not this proof.
- A real compatible C# DLL mod also performs its intended action through the managed route. The embedded Probe alone is insufficient.
- Dependency, initialization, callback and cleanup behavior is observed; unavailable imports/features produce a precise diagnostic instead of a fake successful no-op.
- A second supported game exercises the same generic runtime with appropriate mods for that game and no game-specific code or hand-written offsets. Reuse the existing blind-test plan; do not start separate game preparation early.
- Record startup and steady-state cost against the existing baseline. Discover/classify/cache stable module facts once; preserve correct dynamic state and avoid introducing recurring scans or stutter.

Work in a separate folder. Hand Claude a reviewed patch, dependency/import contracts, build/package instructions, evidence, a support matrix and a console test card. Only Claude merges, builds the combined mod loader and contacts the PS5. This assignment adds no implementation or console authorization.

Technical references: [Microsoft PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format), [Microsoft x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170), [Mono embedding](https://www.mono-project.com/docs/advanced/embedding/), [Mono native interop](https://www.mono-project.com/docs/advanced/pinvoke/). Local baseline: `../cap10/MAC-INTEGRATION-HANDOFF.md` and the current #9 `NativeModAbi.h` / `CppMod.cpp` in Claude's read-only candidate tree.
