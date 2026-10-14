# Technical audit of the PS5 Mod Translator Roadmap — 2026-09-30

The roadmap's shared engine/format/runtime modules are a useful architecture. Its “Supported” coverage and several automatic cross-platform steps are **desired outcomes, not demonstrated capabilities**. Current project evidence covers desktop archive analysis, planning, private scratch transactions, Nexus account/download handling and browser UI. Native execution, console translation, asset redirection, game injection and gameplay equivalence remain unverified.

Preserve the user's constraint: the finished application, conversion, matching, installation and runtime run entirely on PS5, without a PC, phone or AI service. A computer is allowed for one-time development/build work. Online/co-op/multiplayer mods are deliberately excluded. A mod that needs unavailable external execution or AI is Unsupported and never installed. This audit proposes shared backends and evidence gates within that constraint; it does not substitute a permanent desktop companion.

Scope: phases 2, 3, 5, 7, 8, 9 and 10, plus the address-mapping/coverage claims they depend on. Input: the pasted roadmap attachment. Public primary documentation/source was checked on the research date; sources are linked beside conclusions. No product-source edits, staging, commits, device contact, executable acquisition or mod execution occurred. Source scratch copies are under `/tmp/roadmap-technical-audit/`.

## Phase 2: reusable asset conversion, with exact target schemas

**Keep:** neutral representations, shared readers/writers, strict build profiles, conversion caching and round-trip validation. **Correct:** reading a container and retiling blocks does not establish a console asset writer.

Epic explicitly defines cooking as platform-specific conversion. A Pak/IoStore container can be readable while the contained serializers, bulk data, materials, shaders or audio remain incompatible. Epic's cooked-content workflow is Windows-scoped and cooked assets can be read-only and split across `.uasset`, `.uexp` and `.ubulk`; “unpack and recook everything” is therefore not a guaranteed fallback. [Cooking](https://dev.epicgames.com/documentation/en-us/unreal-engine/cooking-content-in-unreal-engine), [cooked-content limits](https://dev.epicgames.com/documentation/en-us/unreal-engine/working-with-cooked-content-in-the-unreal-engine).

Unity likewise requires AssetBundles built for the target platform. A parser for PC bundles is a reusable component, not a PS5 writer. [Unity platform restrictions](https://docs.unity.com/en-us/engine/6000.0/manual/assets-and-media/assets-managing-runtime/assetbundles-section/platforms).

BC compression and memory tiling are separate transformations. A DDS header supplies extent, mip information and pixel format, not an arbitrary console swizzle equation. AMD's public address library requires additional surface state, including swizzle mode, resource type, samples, pitch and alignment; mip tails have their own fields. This demonstrates that format/size alone is insufficient, **not** that this AMD implementation directly supports PS5. A particular game's original header might supply enough state; establish that from samples rather than assume it. [DDS header](https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-header), [AMD address-library inputs at pinned commit](https://github.com/GPUOpen-Drivers/pal/blob/c5e800072a32f68b6ccc4422936d96167c6e0728/src/core/imported/addrlib/inc/addrinterface.h#L738).

The texture intermediate form should preserve compressed blocks where possible, plus color space, channel meaning, mip/slice layout and opaque original metadata. Mesh intermediates need more than vertices/bones: preserve topology, skinning/bind transforms, material assignments, morph/LOD and required dependency references. A known writer must reconstruct target serialization; an intermediate form does not supply missing target information. Audio conversion similarly needs an implemented encoder and target container contract; copying a codec name does not supply either.

Replace “PC original must reproduce PS5 original” with two checks: untouched target parse/write round-trips preserve required bytes and metadata; paired PC/PS5 semantic content matches under declared decoding/normalization rules. Cross-platform cooks may differ in compression, ordering and platform metadata. Require exact byte equality only where the conversion is defined as lossless. Test target engine acceptance separately.

repak and retoc are useful shared container components. retoc documents container/header-version overrides and missing dependency information in some older versions. Their packing/conversion support does not compile platform shaders or establish a target game's mount policy. Encryption detection is feasible; guaranteed automatic extraction of a usable archive key from every executable is not established by these sources. Missing accessible key/codec/schema remains Unsupported for that backend. [repak](https://github.com/trumank/repak), [retoc](https://github.com/trumank/retoc).

## Phase 3: reflection reduces mapping work, but does not create universal UE4SS compatibility

**Keep:** portable Lua interpreter, shared reflected-object API and reusable Unreal resolvers. **Correct:** neither reflection nor a common compiler proves that one signature set covers an engine version.

UE4SS's own documentation says nondefault game configuration and compiler-generated code changes invalidate built-in AOBs. Its custom configurations include member-layout and vtable files as well as signatures. Even if every selected title used the same compiler family, compiler release/options, optimization, link-time optimization, engine forks and game edits remain relevant. Use engine-version patterns to generate candidates, then validate each exact build's invariants and object layouts. [UE4SS compatibility](https://docs.ue4ss.com/dev/guides/fixing-compatibility-problems-advanced.html), [custom layouts](https://docs.ue4ss.com/custom-game-configs.html).

Reflection represents members explicitly exposed to Unreal's property/function system. Names such as `None` and `ByteProperty` are useful anchors; finding them does not establish a valid global table, object lifecycle or correct engine-call ABI. A Lua script can also depend on game-specific objects, native modules, Windows APIs or a particular event order. Inventory its API/dependency requirements before declaring it compatible. [Epic reflected properties](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-uproperties).

Revised milestone: one identified Lua API subset works unchanged on an exact owned target build, with object lifetime, callbacks and failure behavior verified. LogicMods need their cooked assets and dependency graph accepted in addition to the script loader. Broader engine coverage remains a measured regression result.

## Phase 5: a shared graphics backend is substantial work beyond a presentation hook

**Keep:** a portable effect parser and a small shipped set of effects. **Correct:** interception of a common presentation function does not supply writable final-color resources, a graphics context, compatible pipelines or synchronized command submission.

ReShade separates its standalone FX compiler from its injected rendering runtime. Its public implementation contains API backends, resource and effect management, hook lifecycle and GUI/input integration. The compiler can be reused independently; the whole runtime is not provided by a Sony video-out hook. No reviewed primary source establishes a working PS5 presentation/GPU backend for this project. [ReShade upstream](https://github.com/crosire/reshade), [pinned source overview](https://github.com/crosire/reshade/blob/7bf9de8b33bcc76c3177007e65d73c72dd0f34c0/README.md).

SPIR-V is an intermediate language consumed under a client API's execution rules. It still needs target compilation, register allocation/scheduling and the execution environment's layout/binding rules. ReShade's SPIR-V generator emits SPIR-V and descriptor/builtin semantics; it does not emit a Sony shader container. LLVM's AMDGPU backend documents specific target triples, processors and code-object conventions. Selecting AMD machine code does not establish Sony graphics ABI, resource binding, shader-stage linkage or metadata. The roadmap skips that backend between SPIR-V and the proposed header writer. [SPIR-V specification](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#_introduction), [ReShade generator](https://github.com/crosire/reshade/blob/7bf9de8b33bcc76c3177007e65d73c72dd0f34c0/source/effect_codegen_spirv.cpp), [LLVM AMDGPU guide](https://llvm.org/docs/AMDGPUUsage.html).

Same-GPU hardware can support shared infrastructure; it does not make every game's render-target formats, HDR/SDR transfer functions, synchronization and command-state requirements equal. Revised milestone: one effect has a validated standalone target pipeline and resource contract, then works in two exact games with frame timing and color correctness measured. “Every game from one hook” remains unproven.

## Phase 7: managed execution and IL2CPP interop are separate ports

**Keep:** interpreter-only managed execution as a candidate. **Correct:** iOS interpreter availability is evidence that interpretation can avoid JIT dependence, not evidence of a PS5 runtime port or binary-compatible BepInEx surface. Microsoft's documentation describes platform-specific interpreter/AOT integration and performance tradeoffs. [Mono interpreter on iOS](https://learn.microsoft.com/en-us/dotnet/maui/macios/interpreter?view=net-maui-10.0).

Current Il2CppInterop describes a CoreCLR-to-IL2CPP bridge. Its pinned class injector uses Unity-version-specific native layouts, GC handles, delegate function pointers and generated `DynamicMethod` IL; its detour interface requires a platform detour provider. Replacing CoreCLR with a Mono interpreter needs a verified implementation of those paths, including native callbacks and lifetime rules. Metadata names do not supply all runtime registrations or callable native behavior. [Interop design](https://github.com/BepInEx/Il2CppInterop), [class injector](https://github.com/BepInEx/Il2CppInterop/blob/81a6f78c8b653e0da4a3420ac4cd00819e8b6292/Il2CppInterop.Runtime/Injection/ClassInjector.cs#L737), [detour interface](https://github.com/BepInEx/Il2CppInterop/blob/81a6f78c8b653e0da4a3420ac4cd00819e8b6292/Il2CppInterop.Runtime/Injection/DetourProvider.cs).

Native Harmony-style detours are not automatically the same patching contract as managed Harmony transpilers or class injection. Define and test the supported patch subset. Distinguish managed `.dll` assemblies from native Windows PE DLLs. “Most IL2CPP PC mods work” and “Mono mods need small tweaks” need a corpus; neither follows from metadata availability. Revised milestone: one exact plugin and all dependencies execute on PS5 using the declared interpreter/interop subset, with GC, callbacks, error paths and performance checked.

## Phase 8: same CPU is useful, but DLL record-and-port is an experimental compatibility subsystem

Microsoft's x64 ABI uses RCX/RDX/R8/R9 and shadow space; the current native diagnostic source assumes a different ordinary C entry convention. PE also carries imports, relocations, TLS and exception/unwind metadata. Matching x86-64 ISA enables reuse of compatible instructions; it does not supply the OS/runtime loader or the target game's ABI. [Microsoft calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170), [PE specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).

Mapping the PC executable as data and running DLL startup does not reproduce the game process, its initialized objects, TLS, threads, callbacks or subsequent gameplay states. A recorder sees effects on the paths it executes. Deferred hooks, configuration branches and state-dependent writes can escape that trace. A “tweak” label does not prove limited behavior.

Address correspondence is only one layer. Direct guest field accesses, vtables, allocations, exception handling and callback calls also need compatible objects or transformation. A calling-convention thunk alone cannot fix those. A bounded recorder/replayer can be researched against owned synthetic binaries and an explicit operation subset; broad closed-source DLL support must remain experimental until guest-memory/runtime assumptions and semantic translation are demonstrated. Prefer deterministic data/config transformations where they provide the same effect.

The Phase 6 matcher must keep candidates and contradictions, not translate a confidence score into install permission. BinDiff explicitly states that identical mnemonic similarity can conceal changed operands/immediates/addresses. Verify parameter roles, accessed members, ownership, thread requirements and the exact instruction context separately. A branch flip or call removal needs a verified behavior contract, not merely a matching basic block. [BinDiff confidence/similarity limits](https://www.zynamics.com/bindiff/manual/).

No AI is required for a bounded matcher/rule engine. Without adequate deterministic evidence, Unsupported is the correct console-only result. Any supported method must remain shared by operation/engine/build profile; a private rule selected by mod ID is not the requested architecture.

## Phase 9: depth and shader matching need semantics, not only shape

DXVK and vkd3d-proton supply substantial Direct3D-to-Vulkan translation infrastructure. Their documented target is Vulkan, not PS5 graphics. Reusing shader translators can produce a useful intermediate representation; the unresolved Phase 5 target compiler/runtime remains necessary. [DXVK](https://github.com/doitsujin/dxvk), [vkd3d-proton](https://github.com/HansKristian-Work/vkd3d-proton).

ReShade's depth example tracks resources, draw statistics, clear operations, backup copies and several aspect/resolution heuristics. Screen resolution plus draw count is not its complete model and cannot identify every game's scene-depth resource. Dynamic resolution, temporal jitter, reversed depth, resolves and resource lifetime need target evidence. [Pinned depth implementation](https://github.com/crosire/reshade/blob/7bf9de8b33bcc76c3177007e65d73c72dd0f34c0/examples/09-depth/generic_depth_addon.cpp).

Identical shader inputs/outputs and constant layout do not prove equal operations or resource meaning; many unrelated shaders share an interface. Match exact source/target variants with pipeline state, stage linkage, texture/buffer semantics, coordinate/depth conventions and specialization information. Reject ambiguous correspondence. Revised milestone: one documented shader transformation reaches the intended target variant and produces the intended result across representative scenes, followed by save/reload and performance checks.

## Phase 10: candidate scanning can be shared; “universal” swapping needs an archive/texture contract

Common compression support does not expose an unknown archive's record boundaries, indexes, checksums, resource references or texture layout. Decoding a plausible image is a candidate, not proof of a texture resource accepted by the game. A perceptual fingerprint is deliberately lossy, can match duplicates/atlases, and a replacement texture may look entirely different from the original it replaces. Prefer mapping PC-original identity/path to target-original identity, then apply the mod's replacement; record ambiguity instead of choosing by image similarity alone.

“In-place swap replaces the original” directly contradicts Phase 1's original-files-untouched promise unless “original” means a private copy. Amend it to replace a same-size region in a verified **private overlay copy**, rebuilding required indexes/checksums under a known format contract. The current scratch journal only validates private transaction behavior; it does not establish console game redirection. Retain original hashes and a separately verified overlay backend. Unknown archive/texture contracts remain Unsupported.

## Coverage and labels

“Phases 1–5 cover most mods by count” has no supplied corpus, denominator or demonstrated results. Define eligible offline mods, versions and complete dependency sets; report counts by engine, category and exact target build. Count a mixed package once as working only when every required component works. One successful game or texture does not establish majority coverage.

The four labels can remain user-facing: Supported requires an available verified backend/profile and all required console-only dependencies; Needs conversion requires an implemented conversion path, not a hypothetical port; Unsupported records the missing requirement and blocks installation; Failed records a deployed test failure. Keep planned coverage in a separate roadmap status. Boot success alone is insufficient for intended effect, saves, lifecycle and performance.

## Next feasible shared-format checkpoint

Proceed with a **read-only Unreal package/texture evidence backend and an owned DDS/BC intermediate-form round trip**. This complements the existing analyzer/planner without claiming a PS5 writer or injecting a game. Build/test on the permitted development computer; design the reusable library with bounded memory, no AI calls and no permanent external process so it can later be built into the PS5 app.

The backend should record container/package versions, references, custom-version/schema requirements, compression/encryption flags, texture format/mips/color space and bulk-data provenance. Preserve unknown bytes. Existing readers may supply verified portions; report every opaque part. Use source/target profiles by engine/format/exact build, with no mod-ID conditionals. A target encoder stays unavailable until its actual layout is evidenced.

Exact user-owned inputs needed for a real target adapter:

1. Confirmed native-PS5 title ID and update/build, plus hashes and sizes of the inspected files; the reported Stellar Blade `01.014.001` alone is not a content fingerprint.
2. A matched PC-original and PS5-original texture/package from the same semantic asset, with all companion bulk/package files and dependency/index metadata available through the user's existing authorized file access. Include at least multiple mip levels, color/normal-map examples and any streaming split relevant to the format.
3. One user-selected texture mod archive and, where available, its uncooked/source image, exact PC build requirement, dependencies and intended changed appearance. A self-authored sample is suitable for initial development fixtures.
4. Available engine/custom-version/schema metadata and any accessible codec/key material required to read those specific containers. Missing access remains an explicit blocker; this task does not provide acquisition or privilege-changing instructions.
5. Before deployment, an independently identified native loader plus verified standalone filesystem/reporting contract, and a separately verified own-game overlay route. Their existence cannot be inferred from Payload Manager's version, loader source compatibility or a scratch transaction test.

Initial acceptance: deterministic inventory; parser rejection of malformed/truncated inputs; lossless untouched-target round trip where specified; normalized source/target content comparison; explicit missing-schema/codec/layout reasons; originals untouched. Then implement one proved target texture writer and verify the changed appearance in the exact owned game. Expand the same format/backend tests across unrelated assets and titles before broad coverage claims.

This checkpoint preserves the console-only objective. It advances the reusable translation machinery while keeping UE4SS, managed interop, native DLL bridging and graphics backends as explicit later capability ports rather than silently classifying them Supported.
