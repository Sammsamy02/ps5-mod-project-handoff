# Stellar Blade PC evidence for reusable Unreal backends

Research date: September 29, 2026. Written research only. The architecture, specification and implementation-plan approvals remain pending. No packages, game assets, restricted Unreal source, credentials or sexual mod media were downloaded; no dependencies, implementation, compilation, console connection or probe was performed. The objective remains automatic, optimized translation across Nexus without individual-mod hardcoding. Stellar Blade is the first native PS5 validation title. Reported firmware is 13.60; loader and exact installed title/update remain unknown. Every tool/runtime finding below concerns PC tooling, not demonstrated console support.

## 1. Archive routing is a separate backend responsibility

The modding team's [Installing Mods](https://github.com/Stellar-Blade-Modding-Team/Stellar-Blade-Modding-Guide/wiki/Installing-Mods) page identifies a matching `.pak/.utoc/.ucas` set produced under `WindowsNoEditor`, consistent `_P` naming, and placement under `SB/Content/Paks/~mods`. It warns that replacements of the same asset conflict. These are deployment and precedence conventions. Renaming or copying the three files does not change the cooked payload.

The team's [Creating Uproject](https://github.com/Stellar-Blade-Modding-Team/Stellar-Blade-Modding-Guide/wiki/Creating-Uproject) guide uses UE4.26, Generate Chunks, Use IoStore and cooking settings. [Packaging Mods](https://github.com/Stellar-Blade-Modding-Team/Stellar-Blade-Modding-Guide/wiki/Packaging-Mods), edited July 28, 2025, describes `PrimaryAssetLabel`, distinct chunk IDs, explicit asset selection and Windows packaging. That page says its then-current workflow used engine packaging while awaiting retoc support. Later tool forks must therefore be assessed separately; the guide's statement is not evidence that all current retoc builds fail.

**Reusable requirements:** inspect signatures and manifests rather than extensions alone; keep companion containers atomic; resolve mount points, package IDs, precedence and chunk collisions; distinguish replacement assets from new assets; journal installation/removal. A valid PC installation path must not be reused as an assumed console mount rule.

## 2. Assets require serialization and platform payload work

Epic's [UE4.27 packaging documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/packaging-projects?application_version=4.27) separates cooking content for a target platform from bundling it. Its [cooking documentation](https://dev.epicgames.com/documentation/unreal-engine/cooking-content-in-unreal-engine) explains that textures/audio can need different platform formats. The [cooked-content limitations](https://dev.epicgames.com/documentation/unreal-engine/working-with-cooked-content-in-the-unreal-engine?lang=en-US) describe read-only cooked assets, split `.uasset/.uexp/.ubulk` files, reference/path constraints and known Blueprint limitations. These documents do not establish recovery of all missing editor/source data.

Primary public tool candidates:

- [retoc upstream](https://github.com/trumank/retoc): IoStore manifest/list/verify/raw packing and Zen↔Legacy conversion. Its README requires matching container/header overrides when repacking certain games, especially before UE5.0, and notes missing dependency information before UE5.3. Conversion of shader-library containers is not compilation of shader programs for another platform.
- [retoc-sb](https://github.com/tuokri/retoc-sb/blob/master/README.md): a Stellar Blade fork incorporating BlafKing's changes and exposing a library interface. Its documented functions are container and Zen/Legacy transformations; no PS5 payload-cooking claim is documented.
- [UAssetAPI](https://github.com/atenfyr/UAssetAPI/blob/master/README.md): .NET asset read/write, unversioned `.usmap` interpretation, raw Kismet bytecode, and binary-equality verification. This is useful serializer evidence; round-trip equality does not establish cross-platform semantics.
- [FModel](https://github.com/4sval/FModel/blob/dev/README.md): archive explorer using CUE4Parse, with preview/conversion features. Inspection/export capability must not be mistaken for an automatic cooker. FModel is GPL-3; UAssetAPI states MIT, so component-selection/license review is distinct from format feasibility.

The modding team's [repository inventory](https://github.com/Stellar-Blade-Modding-Team/Stellar-Blade-Modding-Guide) currently contains separately named demo, PC v1.1.0 and v1.4.1 `.usmap`/`.idmap` files. Only their public metadata was inspected. That establishes build-labelled mapping artifacts, not their applicability to an unknown PS5 update.

**Reusable requirements:** preserve package/import/export graphs, soft references, bulk-data linkage, custom versions and unversioned schemas; identify mesh/skeleton/physics dependencies and texture/audio/shader payload formats. Classify transformations as container-only, serializer-only, recoverable payload conversion, or source/toolchain-dependent recooking. Test target acceptance and gameplay behavior separately. Automatically identify the correct target schema and reject ambiguous matches; do not silently apply a PC mapping.

## 3. The PC UE4SS fork mixes shared machinery with a title profile

GitHub's current latest-release response identifies [Build 6, tag `3.1.0-6`](https://github.com/Chrisr0/RE-UE4SS/releases/tag/3.1.0-6), published July 2, 2025, at commit `47477f84a9fc268e787dadc3197d730711a322f1`. Its [pinned README](https://github.com/Chrisr0/RE-UE4SS/tree/3.1.0-6) supports Win64 installation/builds and says AOB updates may be necessary.

The public [comparison against its merged upstream parent](https://github.com/Chrisr0/RE-UE4SS/compare/3d146b0dce6c3985f0d2e4adacea5eb0c3192b50...3.1.0-6) has eight commits and thirteen changed files:

- **Literal resolver/profile data:** three added signature scripts for `GUObjectArray`, `GNatives` and `FText_Constructor`, using byte patterns and fixed instruction-relative displacement calculations; an engine override selecting 4.26; and an ordered 758-line virtual-function layout.
- **Layout customization:** [VTableLayout.ini](https://github.com/Chrisr0/RE-UE4SS/blob/3.1.0-6/assets/VTableLayout.ini) differs from its included UE4.26 template by ten `Unk0`–`Unk9` entries inserted in the `AActor` order. This fixes ordinal/layout assumptions rather than discovering them automatically.
- **Shared resolver change:** [Signatures.cpp](https://github.com/Chrisr0/RE-UE4SS/blob/3.1.0-6/UE4SS/src/Signatures.cpp) returns unsuccessful Lua scanning when an FText constructor call faults. The public delta also changes SDK-generator symbol validation and adds a game-specific Windows save-backup path.
- **Unaudited engine bridge:** [`.gitmodules`](https://github.com/Chrisr0/RE-UE4SS/blob/3.1.0-6/.gitmodules) switches the Unreal bridge to `Chrisr0/UEPseudo`, pinned at `4f723a02ef4890ff0a538b6662f207e4ebfbb18a`. The permissioned submodule was not accessed. Its internal fixes, offsets and ABI remain unknown.

**Conclusion:** the inspected public delta is neither purely generic resolver improvement nor a list of fixed absolute process addresses. It combines reusable UE4SS services with hardcoded PC title signatures/layout constants and an uninspected engine dependency. These constants are useful experimental reference answers, but copying them into production does not fulfill automatic discovery. Additional offsets inside UEPseudo cannot be ruled out.

The release notes disable `HookBeginPlay`, document an update-related FText-signature workaround, and say some mods require FText. Treat successful startup with that capability absent as partial capability evidence, not successful translation of dependent mods.

## 4. Lua/Blueprint APIs and CNS are additional requirements

The fork's [BPModLoaderMod source](https://github.com/Chrisr0/RE-UE4SS/blob/3.1.0-6/assets/Mods/BPModLoaderMod/Scripts/main.lua) concretely uses `require`, game-directory enumeration, asset registry lookup, `FName`, object validity, `UWorld:SpawnActor`, reflected function calls, game-thread execution, keybinds, load-map hooks and BeginPlay hooks. It reads load order and loads configurable classes. A compatible Lua interpreter alone supplies none of those game bindings. Controller input, reload/unload, stale objects and level transitions need target services.

Dekita's [CNS installation guide](https://github.com/Dekita/SB-CustomNanosuitSystem-Docs/blob/main/guides/cns-mod-setup.md) combines Windows UE4SS/proxy files, Lua modules, a LogicMods container set, JSON and cosmetic containers. [Outfit setup](https://github.com/Dekita/SB-CustomNanosuitSystem-Docs/blob/main/guides/cns-outfit-setup.md) requires unique asset directories, resolved redirectors and appropriate packaged dependencies. [Basic JSON](https://github.com/Dekita/SB-CustomNanosuitSystem-Docs/blob/main/guides/cns-json-setup.md) describes `/Game/...Asset.Asset` soft paths, DLC requirements, variants and animation/physics references. [Advanced JSON](https://github.com/Dekita/SB-CustomNanosuitSystem-Docs/blob/main/guides/cns-json-advanced.md) specifies material indices/layers, scalar/vector/texture parameters and shape keys. [Animation JSON](https://github.com/Dekita/SB-CustomNanosuitSystem-Docs/blob/main/guides/cns-json-animations.md) references AnimationSequence assets and linked animation IDs.

CNS is a representative mixed dependency package. Its configuration expresses mod-specific content; understanding a reusable configuration schema is permissible. A translator-maintained private patch keyed to each Nexus mod is not.

## 5. Proposed category corpus and measurable acceptance

Select independently authored, non-explicit inputs and retain withheld packages. None was downloaded or executed here.

| Input category | Required evidence from the shared backend |
| --- | --- |
| Texture/UI or audio replacement | Payload format identified; container/reference round trip; target renders/plays correctly; asset conflicts detected. |
| Mesh/material/physics replacement | Skeleton, weights, material slots and physics dependencies preserved; movement and transitions remain correct. |
| Cooked Blueprint/data edit | Schemas/imports and Kismet/reflected calls preserved; new actors and lifecycle behavior work. |
| Lua gameplay/camera/UI mod | API inventory, target object/function discovery, marshalling, correct-thread callbacks, input adaptation and cleanup pass. |
| CNS mixed package | Lua+Blueprint+JSON+asset dependencies resolve together; variants, parameter changes, animation references and legitimate DLC checks behave correctly. |

Across categories: no branch selected by individual mod ID; no manually rewritten replacement script; no unnoticed missing API; exact target-build cache invalidation; clean uninstall/recovery; and measured preparation/startup/frame CPU/memory relative to the unmodified game. Cache container indices, schemas and validated mappings; resolve functions once per controlled lifecycle and batch game-thread work. Successful finite tests establish measured coverage, not universal compatibility.

Unresolved prerequisites: exact console build and execution environment; target mount/override rules; engine customizations, schemas and payload codecs; shader/asset source availability and usable recooking route; Unreal bridge source scope; native ABI/allocator/reflection/resolvers/vtables; runtime injection/lifecycle, hooks and input; actual mod-source/API dependencies. This research closes none of those with a console-support claim.
