# Universal Nexus-to-PS5 mod translation: research proposal

Updated September 29, 2026. Proposal for review, not an approved implementation specification. No product code, payload build, console connection, mod installation or performance benchmark has been completed.

## Required outcome

One console application connects to Nexus through official authorization, identifies installed games or accepts a selection, and automatically translates and installs PC mods for native console games. Intended coverage spans the Nexus catalog: assets and pak files, scripts, managed assemblies, native plugins, UE4SS, Mod Engine 2 and other frameworks, including Skyrim first-person camera mods.

The user requires reusable automatic translation, optimization and no handcrafted fix or installer for each individual mod. **Stellar Blade is the first validation title, not the product boundary.** Reported firmware 13.6 is provisionally interpreted as 13.60. The working jailbreak/loader and exact Stellar Blade console update remain unknown.

These requirements are not established capabilities. Current evidence does not demonstrate an automatic translator that makes every arbitrary PC mod work with native PS5 games. Successful downloads, package conversion or one working Stellar Blade mod would not fulfill the complete objective.

## Three approaches

| Approach | Reusable machinery | Critical limitation |
| --- | --- | --- |
| **Shared translation pipeline with format, runtime and engine backends — recommended for investigation** | Common package/dependency model, asset transformations, script/API bridges, native ABI services, automatic build discovery and cached mappings. | Automatic discovery of equivalent game functions, layouts, hooks and asset semantics remains unproved. |
| Windows compatibility first | PE loading, Windows services and unchanged plugin logic where required interfaces can be provided. | A native console game is a different host binary. Windows API emulation does not locate its equivalent functions or repair direct game-object accesses. |
| Source-driven rebuilding/recooking | Recompile portable logic or generate target assets when suitable source inputs and toolchains exist. | Binary-only plugins and cooked packages may omit required inputs; this cannot cover the entire catalog alone. |

The recommended pipeline can use the other two approaches as shared backends. Windows translation stays within the project's scope; it is neither assumed to work nor discarded in favor of a selective installer.

## Shared system design

### Nexus and game identification

Use our own browsing interface alongside Nexus's real sign-in page. Retain the resulting authorization tokens locally and preserve file versions, dependencies, author information and license conditions. Official references include [OAuth/PKCE](https://github.com/Nexus-Mods/oauth2-demo-app), [websocket SSO](https://github.com/Nexus-Mods/sso-integration-demo) and [API documentation](https://api-docs.nexusmods.com/). Production registration and a usable console authorization callback still need verification.

Nexus's [download library](https://github.com/Nexus-Mods/node-nexus-api/blob/master/docs/classes/_nexus_.nexus.md#getdownloadurls) distinguishes Premium direct URLs from free-account website-generated download authorization. Follow those flows. Access to the entire catalog and compatibility with its contents are separate measurements.

Fingerprint title, edition, update, executable and relevant asset schemas. Distinguish PS4 games running on PS5 from native PS5 games. The first target is the user's installed Stellar Blade build; no build number is invented.

### Whole-package analysis and a common representation

Inspect file signatures, manifests, archive contents, source/bytecode, native imports and dependencies. Build one graph for mixed packages; a filename extension does not establish compatibility. Preserve installer choices and resource references.

The common translation representation records known format versions, resources, script/API requirements, native imports, relocations, identified hooks, type/layout requirements, load order and deployment requirements. Distinguish declarations, inferred candidates and validated mappings. Inventorying a DLL does not automatically recover its source or every behavior. Missing semantics remain a translation problem to solve, rather than fields filled with guesses.

### Reusable backends

| Family | Shared responsibility | Needed console evidence |
| --- | --- | --- |
| Assets/data | Decode containers/serializers, preserve references, transform platform payloads and rebuild packages. | Target schemas/customizations, sufficient input data, accepted output and correct gameplay effect. |
| Unreal/UE4SS scripts | Compatible Lua semantics and UE4SS APIs, reflection, game-thread dispatch and lifecycle services. | Object discovery, layouts, marshalling, ownership, events and hook cleanup. |
| Native Windows plugins | PE requirements, Windows services, calling conventions, exceptions/TLS and host interfaces; investigate object proxies or instruction rewriting. | Equivalent native game behavior, including direct object access, virtual calls, generated code and hooks. DLL loading alone is insufficient. |
| Bethesda scripts/extenders | Ordinary data/scripts and SKSE/F4SE services. | Equivalent console VM/engine services, object layouts and hook sites. |
| FROM SOFTWARE loaders | ME2/me3-style package ordering and file/archive redirection. | Native console archive/file services and lifecycle; native extensions have additional requirements. |
| Managed/Unity and other engines | Managed logic with compatible runtimes, engine/IL2CPP bindings and further runtime families. | Runtime availability, engine semantics, native dependencies and asset compatibility. |

Backends operate on classes of packages and APIs. An individual Nexus mod ID must not select a private production patch. Game/build mappings may be necessary, but the objective is to discover and validate them automatically and reuse them across mods. Hand-authored mappings used as experimental reference answers must be disclosed and do not count as automatic translation.

### Automatic game mapping: central research challenge

Investigate reflection, available symbols, strings/call graphs, instruction structure, cross-build matching and runtime observations to find equivalent functions and layouts. Cache validated results against the exact build. Ambiguous matches must not become unattended hooks.

Unreal reflection may expose enough information for some shared operations. Native mods can also read private fields, patch instructions inside functions, replace vtables and emit machine code. The translator must preserve those operations' meaning on the console target. Confidence scores need structural checks and behavioral validation.

Skyrim's [Improved Camera hooks](https://github.com/ArranzCNL/ImprovedCameraSE-NG/blob/main/ImprovedCamera/source/skyrimse/Hooks.cpp) are a concrete acceptance case: Address Library IDs, intra-function offsets, vtable replacement, trampolines and direct camera-state access. [Wine](https://github.com/wine-mirror/wine) supplies Windows compatibility machinery; [libffi](https://github.com/libffi/libffi/blob/master/doc/libffi.texi) handles known call signatures. Neither source establishes automatic correspondence with PlayStation game internals.

### Console host, deployment and verification

The [community SDK](https://github.com/ps5-payload-dev/sdk) and [PacBrew recipes](https://github.com/ps5-payload-dev/pacbrew-repo) provide candidates for C/C++, HTTPS, JSON, archives, Lua and controller/UI support. Recipes are not proof of execution under the user's loader on 13.60. Standalone app execution, filesystem access and in-game runtime execution require separate evidence.

Stage translation outputs with dependency/conflict checks, original-file hashes, an installation journal and restoration information. Check package structure before deployment and intended behavior in gameplay afterward. Removal must preserve subsequent user changes. Failed translation must explain the missing capability and must not be reported as success.

## Optimization

- Perform expensive decoding, matching and translation during preparation. Cache by source hash, target build and backend version; invalidate when inputs change.
- Stream downloads/large archives with bounded memory and resolve dependencies once per change.
- Keep network work outside gameplay. Resolve hooks and mappings at controlled lifecycle points rather than repeatedly scanning the entire executable.
- Batch engine calls on the correct game thread, bound event queues and release resources on unload and level transitions.
- Measure translation/startup time, memory, frame CPU cost and recovery on the console. Set budgets against the unmodified-game baseline; no performance numbers are asserted yet.

Computer tools can assist development and expensive experiments. The proposed shipped user workflow remains console-based. Moving computation to a computer does not itself solve native-game mapping.

## Stellar Blade validation and generalization

PC references show distinct backend requirements: a [Stellar Blade manager](https://github.com/StevenLiuMC/Stellar-Blade-Mod-Manager) routes files to `SB/Content/Paks/~Mods`; an [author's UE4SS-dependent mod](https://www.nexusmods.com/stellarblade/mods/332) uses a Windows proxy DLL/runtime under `SB/Binaries/Win64`; the [Custom Nanosuit System guide](https://github.com/Dekita/SB-CustomNanosuitSystem-Docs) uses Unreal asset tools. These are PC references, not console proof.

Validate the shared system using multiple independently authored packages and mixed dependencies. Test packages withheld from backend development. Mod-specific branching, hand-fixing every archive, manually rewriting individual scripts or maintaining mod-specific replacement scripts fails the automatic-translation requirement. Automatic transpilation and semantic-preserving code generation remain valid translation techniques.

Then test another game/build and a different runtime family, including the Skyrim native-plugin challenge. Record supported APIs/formats, manual inputs and failures. A finite corpus establishes measured coverage, not every future upload. The universal goal stays open until missing classes are resolved; it is not declared complete after the first demonstration.

## Findings affecting the next step

- [Relapse](https://github.com/ntfargo/Relapse-Exploit) lists support through 13.60. This is an upstream claim, not verification of the user's jailbreak or runtime privileges.
- UE4SS targets Win64; [Linux-host cross-compilation](https://github.com/UE4SS-RE/RE-UE4SS/blob/main/cmake/toolchains/xwin-clang-cl-toolchain.cmake) also produces Windows binaries. Its Unreal dependency is a [permissioned submodule](https://github.com/UE4SS-RE/RE-UE4SS/blob/main/.gitmodules), so console portability is not fully audited.
- [ME2](https://github.com/soulsmods/ModEngine2) is discontinued and points to [me3](https://github.com/garyttierney/me3). Their Windows/Proton routes do not establish native-console integration.
- Epic documents [platform-specific cooking](https://dev.epicgames.com/documentation/en-us/unreal-engine/cooking-content-in-unreal-engine) and [cooked-editor limitations](https://dev.epicgames.com/documentation/en-us/unreal-engine/working-with-cooked-content-in-the-unreal-engine). Changing a container is insufficient; required source data must be checked by format/payload category.
- The inspected [ps5debug-NG injection source](https://github.com/Pharaoh2k/ps5debug-NG/blob/master/debugger/source/proc_elf.c) supports a limited native ELF case. ELF and ELF_RPC both reach a routine that changes the thread's instruction pointer. Protocol documentation alone must not be used to assume a nonexecuting inspection loader. No runtime test or upstream repair was performed.

## Review and next decisions

Approve or amend this shared-translator direction before the written specification and implementation plan. The first specification should cover the reusable package/dependency representation and console capability probe. Automatic mapping, runtime backends and asset transformations retain separate research/implementation scopes. Sequencing work does not redefine the final product as an analyzer or installer.

Immediate hardware inputs: working jailbreak/loader and version, plus installed Stellar Blade title/update identity. A console endpoint is needed when an approved probe is ready. No device has been contacted.

Superpowers remains at architectural design review. The full goal is active and incomplete; no universal-compatibility or working-console claim is made.
