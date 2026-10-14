# l21 upstream engine-version audit

The banner does not identify the source age. The selected compile receipts explicitly define version **3.0.1** while setting the Git SHA to **e3ba1016562d6c0868c410d0a71e88bfcdbf691b**. Official GitHub compares that September 29, 2026 revision as **1,152 commits ahead** of stable v3.0.1 (`d935b5b23bac03b65c14ae38382b02007204cc2e`, released February 14, 2024). [Pinned comparison](https://github.com/UE4SS-RE/RE-UE4SS/compare/v3.0.1...e3ba1016562d6c0868c410d0a71e88bfcdbf691b).

The local public superproject gitlink and previously retained, authenticated UEPseudo source receipt agree on **38e7171e9e8c4a871ff84765848f0290ca34a44e**. This audit used that existing private snapshot; it made no private repository requests or authentication changes. Public PR diffs and metadata are saved beside this report. The initial engine-support PRs were [#503, UE 5.4](https://github.com/UE4SS-RE/RE-UE4SS/pull/503), [#708, UE 5.5/optionals](https://github.com/UE4SS-RE/RE-UE4SS/pull/708), and [#977, UE 5.6](https://github.com/UE4SS-RE/RE-UE4SS/pull/977). The private core introduction history is unavailable locally, so current code presence is distinguished from a proven exact historical introduction commit.

**Concrete gap:** later [#1015](https://github.com/UE4SS-RE/RE-UE4SS/pull/1015/files) added UTF-8 and ANSI property registration as well as implementations. The selected build retains their implementations and constructors but omits both entries from the property-pusher map. The canonical retained public source also omits them. All 374 selected translation units were searched for those registrations with no match. That prevents calling these functions through the ordinary registered reflected-property path; it is not proof that the original menu exercises such a property.

All **374 selected translation-unit source hashes match** the build receipt. Every catalog line citation also matches a selected-source manifest pin. Source presence does not prove PS5 execution or the correct native ABI. No build or console action was performed.

## Actual code/API adaptations

### 54-text — 5.4+ — present

FText supports the 0x10-byte newer layout with dynamic active size; copy/destruction use engine FTextProperty operations instead of assuming the older shared-reference representation.

Selected evidence: [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FText.hpp:17](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FText.hpp:17); [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/FText.cpp:68](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/FText.cpp:68); [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/TypeChecker.cpp:538](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/TypeChecker.cpp:538).

Origin: Retained UEPseudo snapshot at pinned revision; exact introduction commit unknown because local archive has no Git history. [source 1](https://github.com/Re-UE4SS/UEPseudo/blob/38e7171e9e8c4a871ff84765848f0290ca34a44e/include/Unreal/FText.hpp) [source 2](https://github.com/Re-UE4SS/UEPseudo/blob/38e7171e9e8c4a871ff84765848f0290ca34a44e/src/FText.cpp).

Limit: Engine value operations, reflected size and a target FText roundtrip remain unproved by this source-only audit.

### 54-optional — 5.4+ — present

OptionalProperty discovery scans field classes and WorldPartitionRuntimeCellData. Its layout operations distinguish intrusive unset state from a separate is-set flag and initialize/destroy values accordingly.

Selected evidence: [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/TypeChecker.cpp:615](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/TypeChecker.cpp:615); [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/TypeChecker.cpp:660](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/TypeChecker.cpp:660); [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Property/FOptionalProperty.hpp:52](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Property/FOptionalProperty.hpp:52).

Origin: PR #708 explicitly adds TOptional/FOptionalProperty and moves the Unreal gitlink to 0a6f51de59e967199d64cc54c1c95c7b6af33b6b; exact private implementation diff unavailable locally. [source 1](https://github.com/UE4SS-RE/RE-UE4SS/pull/708) [source 2](https://github.com/Re-UE4SS/UEPseudo/blob/38e7171e9e8c4a871ff84765848f0290ca34a44e/include/Unreal/Property/FOptionalProperty.hpp).

Limit: ValueProperty is located after RepNotifyFunc + FName::StaticSize; correct native metadata is still required. Discovery is not a complete Lua optional-value API.

### 54-dynamicclass — 5.4+ — present

Initializer stops looking up UDynamicClass for 5.4 and above, where retained upstream says that class was removed.

Selected evidence: [outputs/2026-10-08-r5-thread-final-integration/thread/UnrealInitializer.cpp:1957](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-r5-thread-final-integration/thread/UnrealInitializer.cpp:1957).

Origin: Retained UEPseudo snapshot at pinned revision; exact introduction commit unknown because local archive has no Git history. [source 1](https://github.com/Re-UE4SS/UEPseudo/blob/38e7171e9e8c4a871ff84765848f0290ca34a44e/src/UnrealInitializer.cpp).

Limit: Selected PS5 initializer preserves this guard and also has native startup/profile/thread gates.

### 54-hitresult — 5.4+ — partial

LineTraceMod switches HitObjectHandle.Actor:Get() to HitObjectHandle.ReferenceObject:Get(); LuaUScriptStruct errors name the missing property.

Selected evidence: [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaUScriptStruct.cpp:183](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaUScriptStruct.cpp:183).

Origin: Exact public #503 diff, merged af9c6b66f26f414e281619e298db0da51978434b on 2024-05-17. Core support requires private UEPseudo #90. [source 1](https://github.com/UE4SS-RE/RE-UE4SS/pull/503/files).

Limit: The diagnostic is selected. LineTraceMod/shared Lua asset deployment is outside this C++ selection manifest and is unknown here.

### 55-optional-generator — 5.4/5.5 — present

SDK/UHT generation recursively emits TOptional<T> and optional default expressions, and skips unsupported optional initialization.

Selected evidence: [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/SDKGenerator/Common.cpp:675](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/SDKGenerator/Common.cpp:675); [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/SDKGenerator/UEHeaderGenerator.cpp:1155](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/SDKGenerator/UEHeaderGenerator.cpp:1155); [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/SDKGenerator/UEHeaderGenerator.cpp:3517](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/SDKGenerator/UEHeaderGenerator.cpp:3517).

Origin: Exact public #708 diff; merged f4a775ec48466ce2e8d31eb2ab88d5a4f204d3b0 on 2024-12-06. [source 1](https://github.com/UE4SS-RE/RE-UE4SS/pull/708/files).

Limit: Skipping optional initialization is deliberate in upstream. Generator support is not complete Lua get/set support.

### 55-enum-width — 5.5+ — present

UEnum::ECppForm and EEnumFlags use byte-sized types/accessors. Retained source documents narrowing from four bytes to one in 5.5.

Selected evidence: [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/CoreUObject/UObject/Class.hpp:482](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/CoreUObject/UObject/Class.hpp:482); [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UnrealFlags.hpp:214](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UnrealFlags.hpp:214); [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_SrcWrapper_UEnum.hpp:118](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_SrcWrapper_UEnum.hpp:118).

Origin: Retained UEPseudo snapshot at pinned revision; exact introduction commit unknown because local archive has no Git history. [source 1](https://github.com/Re-UE4SS/UEPseudo/blob/38e7171e9e8c4a871ff84765848f0290ca34a44e/include/Unreal/CoreUObject/UObject/Class.hpp) [source 2](https://github.com/Re-UE4SS/UEPseudo/blob/38e7171e9e8c4a871ff84765848f0290ca34a44e/include/Unreal/UnrealFlags.hpp).

Limit: Typed reads are present; native offsets remain separate evidence. This changes access width, not just an offset table.

### 56-construct — 5.6+ — present_with_limit

StaticConstructObject builds a distinct Above55 parameter structure with a 0x30-byte callback slot, SerialNumber, conditional remote-object fields and moved SubobjectOverrides.

Selected evidence: [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/UObjectGlobals.cpp:105](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/UObjectGlobals.cpp:105); [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UObjectGlobals.hpp:328](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UObjectGlobals.hpp:328); [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Hooks/Internal/DetourSubclasses.hpp:310](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Hooks/Internal/DetourSubclasses.hpp:310).

Origin: #977 requires private UEPseudo #148. The retained core has this concrete version branch; exact historical private introduction commit unknown. [source 1](https://github.com/Re-UE4SS/UEPseudo/blob/38e7171e9e8c4a871ff84765848f0290ca34a44e/src/UObjectGlobals.cpp) [source 2](https://github.com/Re-UE4SS/UEPseudo/blob/38e7171e9e8c4a871ff84765848f0290ca34a44e/include/Unreal/UObjectGlobals.hpp) [source 3](https://github.com/UE4SS-RE/RE-UE4SS/pull/977).

Limit: The upstream comment explicitly calls the new TFunction unsupported: its slot is zeroed padding, not callable support. UE_WITH_REMOTE_OBJECT_HANDLE defaults to 0. Native ABI/remote-handle mode are unproved.

### 56-alignment — 5.6+ — present

UStruct::GetMinAlignment reads int16 through GetMinAlignment506 instead of the older int32 accessor width.

Selected evidence: [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/UStruct.cpp:212](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/UStruct.cpp:212); [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_SrcWrapper_UStruct.hpp:118](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_SrcWrapper_UStruct.hpp:118).

Origin: Retained UEPseudo snapshot at pinned revision; exact introduction commit unknown because local archive has no Git history. [source 1](https://github.com/Re-UE4SS/UEPseudo/blob/38e7171e9e8c4a871ff84765848f0290ca34a44e/src/UStruct.cpp).

Limit: The common accessor always takes the narrow path, rather than dynamically choosing int32 for older versions. Its width matches the retained 5.6 model, but its native offset is separate.

### 56-string-discovery — 5.6+ — present

TypeChecker discovers UTF-8/ANSI string-property classes directly and within array/set/map members; VerseEnum supplies a UTF-8 metadata probe.

Selected evidence: [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/TypeChecker.cpp:361](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/TypeChecker.cpp:361); [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/TypeChecker.cpp:547](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/TypeChecker.cpp:547); [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/TypeChecker.cpp:665](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/TypeChecker.cpp:665).

Origin: Retained UEPseudo snapshot at pinned revision; exact introduction commit unknown because local archive has no Git history. [source 1](https://github.com/Re-UE4SS/UEPseudo/blob/38e7171e9e8c4a871ff84765848f0290ca34a44e/src/TypeChecker.cpp).

Limit: The separate VerseClass ANSI fallback is gated at 5.7. Finding a property class does not connect a Lua value pusher or prove a target invocation.

### 56-string-lua — 5.6 follow-up — partial_missing_registration

FUtf8String/FAnsiString constructors, property-pusher function bodies and SDK/UHT type handling exist, but both property-pusher registration entries from #1015 are missing.

Selected evidence: [outputs/2026-10-08-r5-thread-source-integration/thread/LuaMod.cpp:7167](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-r5-thread-source-integration/thread/LuaMod.cpp:7167); [outputs/2026-10-08-r5-thread-source-integration/thread/LuaMod.cpp:7182](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-r5-thread-source-integration/thread/LuaMod.cpp:7182); [outputs/2026-10-08-menu-complete-build/candidate/LuaUObject.cpp:1904](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-menu-complete-build/candidate/LuaUObject.cpp:1904); [outputs/2026-10-08-menu-complete-build/candidate/LuaUObject.cpp:1946](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-menu-complete-build/candidate/LuaUObject.cpp:1946); [outputs/2026-10-07-l19-local-profile-r4/units/UE4SSProgram.cpp:1804](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l19-local-profile-r4/units/UE4SSProgram.cpp:1804); [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/SDKGenerator/Common.cpp:687](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/SDKGenerator/Common.cpp:687).

Origin: Exact public #1015 diff adds both registrations; merged 800347c544458fdc98fd4ff7a0379875032e0434 on 2025-11-06. Initial #977 explicitly excluded UTF-8 string support. [source 1](https://github.com/UE4SS-RE/RE-UE4SS/pull/1015/files) [source 2](https://github.com/UE4SS-RE/RE-UE4SS/blob/e3ba1016562d6c0868c410d0a71e88bfcdbf691b/UE4SS/src/UE4SSProgram.cpp).

Limit: Selected Program and retained canonical e3ba1016 Program both omit the two entries. Bodies alone are insufficient for ordinary reflected property routing. This is inherited at the selected upstream revision, not shown to be PS5-only. No current menu-causality claim.

### gap-optional-lua — 5.4+ — absent_in_selected_surface

A dedicated Lua OptionalProperty value pusher and registration are absent from the selected LuaUObject/Program surface.

Selected evidence: [outputs/2026-10-07-l19-local-profile-r4/units/UE4SSProgram.cpp:1783](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l19-local-profile-r4/units/UE4SSProgram.cpp:1783); [outputs/2026-10-08-menu-complete-build/candidate/LuaUObject.cpp:338](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-menu-complete-build/candidate/LuaUObject.cpp:338).

Origin: #708 implements core/generator support, not a dedicated Lua optional pusher; selected-file absence checked directly. [source 1](https://github.com/UE4SS-RE/RE-UE4SS/pull/708/files).

Limit: Scoped negative search; the unsupported-property route exists. This is a broader API limit and is not evidence of the current menu failure or a port regression.

### gap-native-provider — 5.4/5.5/5.6 — unknown_native_coverage

PS5 chooses NativeProvidedVirtualVC rather than upstream UnrealVirtual504/505/506; native offsets and dispatch require independent proof.

Selected evidence: [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/VersionedContainer/Container.cpp:282](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/VersionedContainer/Container.cpp:282); [.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/VersionedContainer/Container.cpp:300](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/VersionedContainer/Container.cpp:300).

Origin: Selected __PROSPERO__ branches compared with retained canonical source. [source 1](https://github.com/Re-UE4SS/UEPseudo/blob/38e7171e9e8c4a871ff84765848f0290ca34a44e/src/VersionedContainer/Container.cpp).

Limit: This is the intentional port route. Merely retaining upstream generated tables does not establish native compatibility.

## Actionable interpretation

1. Do not replace this port merely because it prints 3.0.1; substantive later engine adaptations are selected.
2. Review the missing UTF-8/ANSI pusher registrations against the actual mod property types before proposing a change. Their absence is concrete, but no current failure is attributed to it.
3. Keep construction callback support and remote-object-handle mode explicit: upstream padding does not implement the new callback, and the default mode is zero.
4. Continue the native layout/ABI audit. `__PROSPERO__` uses NativeProvidedVirtualVC, so Windows-generated layout tables cannot settle it.
5. Treat missing Lua optional-value routing and untracked shared Lua helper assets as separate API/asset coverage limits.

The catalog is a focused inventory of checked version-sensitive algorithms and APIs, not an exhaustive claim that every engine source change or game customization is covered. Historical private PRs #90/#148 are referenced by public PRs; this report does not claim their exact diffs were read.

Machine-readable evidence: [catalog](version-sensitive-changes.json), [TSV](version-sensitive-changes.tsv), [provenance](provenance.json), [selected TU hashes](selected-tu-hash-verification.json), [negative registration evidence](negative-evidence.json).
