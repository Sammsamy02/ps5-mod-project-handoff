# Backend research checkpoint

September 29, 2026. Source audits for the universal automatic translation objective. Stellar Blade is the first hardware test; no per-mod patch table is proposed. Console execution and translation have not been demonstrated.

## Stellar Blade: useful evidence, unresolved console bridge

The PC UE4SS fork's [Build 6](https://github.com/Chrisr0/RE-UE4SS/releases/tag/3.1.0-6) is pinned at `47477f84a9fc268e787dadc3197d730711a322f1`. Its public changes combine common resolver/error-handling work with literal title signatures, a UE4.26 override and ten inserted actor virtual-function slots in [VTableLayout.ini](https://github.com/Chrisr0/RE-UE4SS/blob/3.1.0-6/assets/VTableLayout.ini). Its permissioned Unreal submodule was not audited. This is a reference case for automatic signature/layout discovery, not a portable console solution.

The modding team's [installation guide](https://github.com/Stellar-Blade-Modding-Team/Stellar-Blade-Modding-Guide/wiki/Installing-Mods) routes matching pak/utoc/ucas containers, while [packaging](https://github.com/Stellar-Blade-Modding-Team/Stellar-Blade-Modding-Guide/wiki/Packaging-Mods) cooks Windows content. Those are separate operations. [retoc](https://github.com/trumank/retoc) and [UAssetAPI](https://github.com/atenfyr/UAssetAPI) offer reusable container/serializer machinery; their documented operations do not establish PS5 payload conversion or target-engine acceptance.

The fork's [Blueprint loader](https://github.com/Chrisr0/RE-UE4SS/blob/3.1.0-6/assets/Mods/BPModLoaderMod/Scripts/main.lua) needs reflected objects, asset lookup, spawning, thread dispatch, input and lifecycle hooks. Lua execution alone does not supply these services. A common backend must inventory API requirements and prove the corresponding target services, including missing/disabled hooks.

Suggested held-out categories: texture/audio replacements, mesh/material/physics packages, Blueprint/data edits, Lua gameplay/camera behavior and mixed Lua/JSON/asset packages. Tests should span independently authored packages and another game/runtime after Stellar Blade. Manual per-mod repairs do not count as automatic coverage.

## Automatic mapping: candidate discovery versus semantic validation

[Ghidra Version Tracking](https://github.com/NationalSecurityAgency/ghidra/blob/master/Ghidra/Features/VersionTracking/src/main/java/ghidra/feature/vt/gui/actions/AutoVersionTrackingTask.java), [BinDiff](https://www.zynamics.com/bindiff/manual/) and [Diaphora](https://github.com/joxeankoret/diaphora) supply reusable correspondence tools. Their matches are candidates for validation; similarity can conceal changed operands, layouts and effects. Unreal [patternsleuth](https://github.com/trumank/patternsleuth) provides engine address-discovery machinery, without establishing arbitrary native-console mappings.

The proposed mapping graph tracks functions, types, fields, virtual slots and hook sites separately. Exact source/target build identities, structural checks and scoped behavior evidence attach to each correspondence. Matching a function does not map an offset inside it. Private field accesses and generated code require their own contracts. Ambiguous, absent and contradictory matches stay unresolved.

Owned reference programs can test candidate accuracy, false positives, alias/lifetime behavior and changed hook placement without game data. Evaluation truth is withheld from the resolver. These tests measure shared algorithms; later console gameplay remains a separate requirement.

## Native plugins: three distinct contracts

[Microsoft's PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format) and [x64 ABI](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170) establish loader, TLS, call and unwind obligations. [libffi](https://github.com/libffi/libffi/blob/master/doc/libffi.texi) helps transport known signatures; it does not discover them or translate private game types.

Separate module execution, call transport and game-object semantics. The object contract covers identity, interior aliases, fields/vtables, allocation/lifetime, callbacks and concurrent updates. Guest-layout objects and translated memory accesses are candidate strategies. [Remill](https://github.com/lifting-bits/remill) lifts instructions; [DynamoRIO](https://dynamorio.org/API_BT.html) supplies instrumentation mechanisms. Neither automatically identifies the equivalent native game's object semantics, nor establishes PS5 support.

Skyrim [Improved Camera](https://github.com/ArranzCNL/ImprovedCameraSE-NG/blob/main/ImprovedCamera/source/skyrimse/Hooks.cpp) combines direct camera-state access with internal instruction hooks and emitted code. It remains a required acceptance challenge for the shared native backend. A Windows API shim alone does not cover operations that bypass those APIs.

## Inputs still needed

The working console loader/version; exact installed Stellar Blade title/update; accessible expected-PC and console build evidence; appropriate mod/dependency corpus and source access; and independently checked mapping/behavior oracles. No console endpoint has been contacted, no credentials requested or used, and no restricted engine source retrieved.

The shared desktop foundation is being built separately. Its success must not be promoted into a claim that these unresolved backends are implemented.
