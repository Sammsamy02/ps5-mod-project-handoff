# Native Windows-plugin translation: reusable contracts and test boundaries

Research checked September 29, 2026. No product code, dependency installation, executable probe or device contact. The objective remains automatic translation across native console games without per-mod patches; Stellar Blade is the first test, not the coverage boundary. This is research for an unapproved specification, not implementation approval.

## Main finding

A shared backend can separate **module execution**, **function-call transport**, and **game semantics**. Each has a falsifiable contract. A working Windows loader or correct cross-ABI call proves only its own layer; the remaining question is whether the native target supplies the same object, operation and hook effect. None of the eight primary sources below establishes universal automatic game correspondence.

## Module and call contracts

**1. PE module contract.** Inventory machine type, section layout/protection, preferred base and relocation types, ordinary and delay imports, named/ordinal exports and forwarded exports. Import services must preserve their actual behavior, not only return plausible values. Static TLS additionally needs initialization templates, zero fill, an assigned index, per-thread storage and ordered callbacks. Some accesses use thread-environment state directly, so implementing `TlsGetValue` alone does not cover static TLS. These are documented binary requirements in Microsoft's [PE specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).

Proposed descriptor: immutable input hash and architecture; required loader features; dependency versions; initialization/termination actions; resolved services; unsupported requirements. Ordinary image-base relocation must remain distinct from moving a game hook to a different build: the latter needs semantic correspondence, not the PE relocation delta.

**2. Call contract.** Microsoft's [x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170) uses positional integer/floating registers, caller-reserved shadow space and stack arguments. Aggregate parameters and results can introduce pointers; a hidden return-storage argument shifts other arguments. Varargs duplicate floating arguments into general registers. Stack alignment and preserved register/state rules also matter.

Every translated boundary therefore needs both source and target signatures, including implicit receiver/return storage, aggregate layout, variadic status, pointer direction/ownership and callback rules. The target ABI must be established from the exact available target toolchain/binary evidence; this memo does not assume a PS5 calling convention. Internal hook points can use compiler register allocation rather than an ordinary exported-function signature.

The upstream [libffi manual](https://raw.githubusercontent.com/libffi/libffi/master/doc/libffi.texi) requires a known function pointer, argument types/count and return type. It leaves higher-level conversion to its caller. Struct descriptions are ABI-specific; bit fields and variadic closures are missing features. Closures depend on platform support and executable-memory allocation, and cannot be freed while a callback can still execute. Thus libffi is a candidate transport dependency for known signatures, not a signature, ownership or game-function discovery engine. Its exact source revision and supported ABI combinations would need pinning and verification.

**3. Unwind contract.** Microsoft's [x64 exception documentation](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64?view=msvc-170) defines `.pdata`/`.xdata`, stack/register recovery and language-specific handler data. Dynamically generated nonleaf functions need registered function tables under Windows. A translated runtime must preserve the module's required exception/destructor behavior and unwind generated bridges. Raw Windows tables cannot simply be assumed meaningful to a different host unwinder. A bounded experiment can contain exceptions within one runtime and translate boundary errors explicitly; arbitrary cross-runtime exception propagation remains an additional requirement.

## Object identity and direct memory access

The following is a proposed bridge contract, not a capability supplied by the cited loader libraries:

- **Identity:** one stable guest representation for each live target object; repeated returns, pointer equality, nulls and stored handles behave consistently. Base-class/subobject and interior pointers preserve alias relationships. Address reuse must not silently revive an old object reference.
- **Layout/access:** guest field offset, width, representation and operation map to a verified target field or equivalent accessor. Include arrays/strides, bit fields, tagged pointers, vtable slots, receiver adjustment and RTTI operations that the module actually uses.
- **Lifetime/ownership:** specify borrowed versus owned objects, allocation/deallocation domain, refcount/GC participation, destruction notifications, retained pointers, callback cancellation and unload quiescence.
- **Consistency:** define when target changes become visible, when guest writes commit, reentrant callback behavior, permitted threads and atomic ordering. Copies synchronized only at API calls can become stale when the native engine mutates an object between direct guest loads.

Two reusable strategies merit comparison. **Guest-layout objects** retain the plugin's expected bytes and translate calls/field changes to native objects. Their identity table and synchronization must cover escaped/interior pointers and target-side changes. **Translated accesses** retain logical identities while rewriting guest loads, stores, dispatch and pointer operations to verified target operations. Their interception must cover every relevant path, including emitted code and bulk copies. Some objects with proved identical layouts could be shared directly, but compatibility must be established for the fields, lifetime and dispatch actually used.

The direct-access problem is concrete: [Improved Camera's hook source](https://github.com/ArranzCNL/ImprovedCameraSE-NG/blob/main/ImprovedCamera/source/skyrimse/Hooks.cpp) calls the original camera function, reads `tesCamera->currentState.get()->id`, hooks an internal location at relocation IDs 49852/50784 plus `0x1A6`, and generates assembly patches. A successful DLL initialization says nothing about which native camera object, field or instruction point preserves that behavior.

A proposed **hook contract** records the source operation and validated target point, expected instruction preconditions, live registers/flags, displaced instruction effects, callback timing, continuation and cleanup. Translating an internal patch must preserve these effects at the target point; copying its byte offset or assigning an ordinary FFI signature is insufficient evidence. Installation/removal also needs a proven thread/code-cache coordination method for the actual host.

## Recompilation, lifting and instrumentation

**Source recompilation** gives a compiler actual types and control flow, permitting common API adapters or automatic access rewriting. It requires complete source/dependencies and an adequate target toolchain. Recompiling source that still assumes PC field layouts, function IDs, injected instruction offsets or Windows middleware preserves those assumptions. Source-driven rewriting must still discover their target meaning; manually porting each plugin would fail the product requirement.

**Binary lifting** makes instruction effects available for transformation. [Remill](https://github.com/lifting-bits/remill) explicitly focuses on lifting machine instructions into LLVM bitcode, including x86/amd64. It is a library for other tools, not a complete native-game translator. Its documented dependency family includes LLVM/Clang and Intel XED. Proposed additional dependencies are executable/control-flow recovery, indirect-target handling, guest memory/TLS modeling, external-call bridges and semantic object mapping. Recovering the effect “load at pointer plus offset” does not determine that the load means “camera state ID.”

**Dynamic instrumentation** can intercept executed accesses and discover generated code, complementing static lifting. [DynamoRIO's code-manipulation documentation](https://dynamorio.org/API_BT.html) exposes instruction operands and code modification, while requiring original-context restoration for modifications/faulting instrumentation and translated source locations for fault handling. This supplies useful mechanisms and correctness obligations, not native PS5 support. A future port would additionally need compatible threading, code-cache memory, executable permissions and exception/signal facilities; none is established here. Observed execution also does not prove unexecuted paths are covered.

[Wine's official README](https://github.com/wine-mirror/wine) describes a Windows binary loader and Windows API implementations using Unix equivalents. That addresses Windows services. An engine-field load, internal function address or vtable call can bypass those services entirely. **Inference:** even perfect OS-call translation leaves a separate mapping problem whenever source and target game representations differ.

## Falsifiable proposed experiments

All execution below requires later approval. Begin with owned synthetic guest/host fixtures on a computer; use explicitly supplied mappings only as a labeled correctness oracle. They establish backend mechanics, not automatic discovery or console compatibility.

| Test | Input variation | Pass/fail observation |
| --- | --- | --- |
| Loader/TLS | Relocate a module; named, ordinal and delayed imports; TLS on existing/new threads; unload/reload. | Exact expected service/callback trace and independent initialized thread values; missing features produce explicit failure. |
| Calls/callbacks | Mixed scalar/floating parameters, stack overflow arguments, aggregate return, receiver, varargs and nested callbacks. | Values match native oracle; alignment/preserved-state checks hold; callbacks survive reentrancy and stop before reclamation. |
| Unwind | Throw inside guest logic and generated bridge; destructor counters; fault at rewritten access. | Expected cleanup and accurate recovered context; no accidental cross-runtime exception claim. |
| Identity/lifetime | Return one object through two APIs; base/interior aliases; deletion and same-address replacement; retained callback. | Equality/alias effects match oracle; invalid lifetime is detected where the contract can detect it; replacement has distinct logical identity. |
| Field semantics | Host reorders fields/vtable and changes array stride; direct guest reads/writes, bulk copy, atomic update; host mutation during callback. | Both chosen strategy and synchronization preserve expected state transitions, rather than only initial snapshots. |
| Code coverage | Indirect calls, trampoline emitted after startup, later instruction patch and cold path. | All tested relevant accesses/calls are translated; fault/state behavior matches; unsupported operations are identified. |
| Automatic mapping | Withhold mappings for independently built hosts with symbols removed, reordered fields and ambiguous functions. | Discover correct correspondence or report ambiguity; no per-fixture/per-mod branch. Measure wrong accepted mappings separately from unresolved cases. |

Next boundary: lawful source/target game evidence with exact build hashes and independently checked semantic oracles. Camera-state transitions and hook timing provide measurable behavior; an address similarity score alone does not. Game/build mappings can be reused across mods, but their automatic production must be tested on held-out modules/builds. Hardware execution, target ABI/runtime facilities and gameplay validation are separate gates after an approved probe. A successful fixture, Windows plugin load or one Stellar Blade mod cannot establish all-mod/all-title coverage.
