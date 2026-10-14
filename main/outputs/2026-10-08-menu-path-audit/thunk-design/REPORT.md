# Isolated static-thunk prototype

This is a local, non-promoted prototype. It fixes registration identity and retained callback-storage lifetime in meaningful host controls and compiles three actual SDK target TUs. It is not selected runtime source, a packaged runtime, native script-argument support, or console/menu evidence.

The original shared wrapper has two independent reproduced defects. It cannot identify its retained original with a null/foreign frame identity without Code-8 or another function's map entry; and reentrant UnregisterAllHooks erases the callback/map node still being used by FirePreCallbacks. Fatal ASan reports heap-use-after-free for the latter. A paired TSan test reports a data race between original registration and dispatch.

The prototype uses eight ordinary source-compiled thunks. Each index retains the exact UFunction identity, original function pointer and callback-data node. The dispatcher forwards Context/frame/result unchanged, independently of CurrentNativeFunction and Code. Retired slots forward their immutable original without callbacks. Slots are never reused; a loaded old thunk cannot acquire a new registration's callbacks. C++17 map extraction retains the old node, including in-flight references, after retirement. The original public class/header layout is unchanged.

One registry mutex protects installation, retirement and callback mutation. Callables are copied for admitted invocation and called outside the registry lock. Nested dispatch counts preserve bIsMidExecution until the last active call finishes. Callback additions start on the next dispatch rather than being added to an already traversed callback map; that policy change is explicit. Retirement preserves an intervening Func replacement. Capacity, missing/recursive original and GNatives-nonnull registration refuse before the first Func store.

| Verification | Actual coverage | Result |
|---|---|---|
| `RED-RECEIPT.json` | Exact original full wrapper/callbacks and actual install/UnregisterAllHooks methods, explicit host storage |5 failures/1 known-identity pass: null/foreign identity, late retired-null call, re-registration ABA, reentrant erase |
| `green-v3/GREEN-RECEIPT.json` | Exact final candidate callback/install/retirement and static dispatcher |16/16 fatal ASan/UBSan cases pass, including the five RED controls, capacity/no reuse, refusal before stores, intervening replacement, nested retirement, exception cleanup, later unsupported GNatives forwarding and concurrent registration |
| `thread-v2/THREAD-RECEIPT.json` | Original and final candidate source under10,000 dispatches/6 concurrent registrations |Original reports TSan race and aborts; candidate exits0 with no TSan finding |
| `target/TARGET-RECEIPT.json` | Three fresh whole selected SDK target TUs: UFunctionStructs.cpp, UFunction.cpp, ScriptHookThunks.cpp |3/3 compile; actual accepted owner/original headers and SDK flags retained |
| `target/SYMBOL-RECEIPT.json` | Actual SDK nm against three new objects and two actual R4 baseline objects |Eight static thunk symbols; Install, Retire and RegistryMutex providers resolved; no unresolved private helper import |

The external undefined-symbol delta versus those two baseline TUs is five standard C++ symbols: recursive_mutex constructor/destructor/lock/unlock and `__cxa_rethrow`. This is **not** a full-runtime import admission check or a claim that the packaged206-import set is unchanged. A later integration needs composition-wide provider/import checks and the unchanged package/budget gates.

Constraints before any integration:

- Capacity8 is a generic demonstrator setting, not a release budget or a game value. No reuse means retirement and reload consume capacity permanently. A release bound/configuration needs an independently bounded carrier-count policy and memory/code budgeting.
- Installation requires GNatives-null borrowed-frame mode. A later nonnull GNatives change forwards original only. No synthetic frame is allocated or clone branch enabled. Callback frame parameters are borrowed; this does not prove that every callback can interpret script arguments through Locals. The original marshal callback does not read argument payloads, but wider API support remains separate.
- Native evidence proves ordinary Func width8 and the Invoke register contract; the wider D8-call census explicitly leaves entry coverage incomplete. Thunks avoid guessed frame identity; they do not invent a blanket external FFrame/FNative ABI.
- Original/context/native object lifetime, user CustomData destruction and module unload still require quiescence. There is no unload barrier here. Late original forwarding requires this module/code to remain loaded.
- Existing public raw callback-data/map references are not a general concurrent borrow API. GetFunctionHookCallbackData and UnregisterHook still use the historical GetFunctionHookData query behavior, which can install when no map entry exists. That behavior needs explicit review before promotion. The searched selected source has no additional direct map consumers outside UFunction.cpp/UFunctionStructs.cpp, but external API consumers cannot be inferred absent.
- A callback admitted before retirement may complete afterward. Userdata must remain alive for that in-flight callback. The source-compiled thunk experiment does not certify LuaMod teardown.
- Callback-copy/snapshot allocations and permanent node retention add owned memory costs. These must be measured against the real allocator/module budget; no existing package or bank budget was changed.

Initial diagnostic artifacts are retained: the first GREEN run found a host-only missing return after renaming main to basic_case, subsequently fixed in the host harness; the earlier thread capture predates only the formatting correction. The v3 sanitizer, v2 thread and target receipts bind the final candidate source. The full refusal-only shared-wrapper candidate remains separate and intentionally fails valid modeled script-prefix delivery; it must not be mistaken for this per-registration design.

No R4 source/header/profile/configuration/artifact, original Lua or console state changed. Root must independently review before requesting a new runtime integration slice.
