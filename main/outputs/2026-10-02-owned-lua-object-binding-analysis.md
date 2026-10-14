# Owned Lua object-reference binding checkpoint

The shared Lua binding now handles typed object-reference property reads and assignments, provider-backed `IsValid`, null chains and instance/generation identity equality. It preserves the earlier Int32 method and masked Boolean property APIs. This advances a common requirement found in the retained UE4SS mods without editing their Lua sources or adding game/mod branches.

`native-host/lua/owned_binding.c/.h` operates over caller-owned IDs and storage. A reference property returns another locked receiver or canonical null userdata. Writes accept only that instance's receiver or Lua nil. Each trusted provider transaction revalidates current identities, property generation and permissions before mutation. The adapter rejects foreign identities, stale targets, malformed descriptors/results and arbitrary userdata fields. Null validity and chaining make no provider calls. `IsValid` is reserved and cannot be overwritten by a colliding property.

The property/null behavior was grounded in retained official UE4SS source at `e3ba1016562d6c0868c410d0a71e88bfcdbf691b`, especially [object property conversion](https://github.com/UE4SS-RE/RE-UE4SS/blob/e3ba1016562d6c0868c410d0a71e88bfcdbf691b/UE4SS/src/LuaType/LuaUObject.cpp#L410) and [UObject validity/null chaining](https://github.com/UE4SS-RE/RE-UE4SS/blob/e3ba1016562d6c0868c410d0a71e88bfcdbf691b/UE4SS/include/LuaType/LuaUObject.hpp#L761). Owned ID/generation checks are this component's narrower contract; they do not implement Unreal GC membership or engine object validity.

## Focused evidence

| Check | Result |
| --- | --- |
| Initial behavioral RED using the frozen earlier Boolean adapter and a refusing reference stub | 14 checks, 8 expected new-behavior failures; legacy 9,353 checks passed. Exact initial sources preserved separately. |
| Final real LuaRaw 5.4.7 host run | 86 reference checks and 9,353 Boolean/Int32 regression checks passed. |
| Final ASan/UBSan adapter/test run | Same passing counts. Retained Lua archive is uninstrumented; Apple LeakSanitizer unavailable. The bounded Lua allocator separately returned to zero at close. |
| Same product source cross-compiled and linked with retained PS5 SDK/LuaRaw | Two x86-64 test ELFs with validated dynamic layout/BIND_NOW and 33 imports within reviewed membership. Reference ELF retains all seven public C APIs. Never executed on PS5. |
| Independent binding/coverage reviews and root final identity check | No material finding. Final inputs match receipts; 254 paused title-work files remain unchanged. |

Final evidence is under ignored `.superpowers/native-dependency-spike/owned-lua-reference-binding-1`: `host-final-2/receipt.json`, `host-sanitize-2/receipt.json`, `native-final-1/receipt.json`, `root-final-verification.json`, `review-1.json` and `final-coverage-review.md`. The native reference ELF SHA256 is `60055c1bb98cdee0318bb635b38aa112338d6b629c3881067ebeb8b61016d2bf`. The verifier takes explicit read-only dependency paths and a fresh private output directory; the committed tests include no ignored source code.

The first sanitizer attempt stopped before tests because Apple ASan does not support `detect_leaks=1`. The retained failure and same-binary retry identify the runner configuration issue. Final receipts explicitly use `detect_leaks=0`; the binding did not change for this correction.

## Remaining runtime and application work

This is an owned-data component, not a PS5 Unreal engine provider. Engine object/name discovery, GC-aware lifetimes, real reflection/function argument conversion, game-thread execution, controller input delivery, UMG, traces and actor spawning remain unfinished. The unchanged Item Spawner's last observed F7 action still stops at missing `ExecuteInGameThread`; this component result does not supersede that observation. Windows UE4SS/DLL binary translation remains unimplemented.

The Nexus app's committed selection → authorized download → private archive analysis path is separate and does not yet connect this binding to a game. Automatic prerequisite installation, game deployment and a fully operational on-console mod store remain unfinished. No console, live account, game file or system operation was performed in this task. No mod or universal compatibility is claimed from these component tests. The full goal remains active.
