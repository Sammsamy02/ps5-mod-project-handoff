# Independent static-thunk prototype review

The isolated design is a useful verified prototype. Its indexed thunk retains the registration's original function and callback node, forwards the original context/frame/result, avoids frame identity guesses, and prevents a cached retired thunk from acquiring a later registration's callbacks. It is not ready for runtime promotion. Publication, teardown, capacity and borrowed-frame support need explicit admission rules in the next candidate.

The frozen source selection and final receipt match their supplied hashes. All 426 selected files and five external evidence pins match; 23 protected original sources and their caches remain byte-identical. No selected source, artifact, mod, settings or console was changed by this review.

Independent verification used the exact prototype bodies with the worker's explicitly modeled host storage seam. Two fatal ASan/UBSan controls passed: a pre callback adding a post callback invokes that post in the same dispatch, while its newly added pre callback waits for the next Pre invocation; a filtered pre callback marked by reentrant retirement retains its capture after the last active invocation. Explicit quiescent cleanup of the retained data releases that capture. A load-only same-function publication control reported a TSan eight-byte race between Retire's Func store and an engine-style GetFuncPtr load. The first dispatching publication control observed no race and is preserved as inconclusive, not counted as a passing publication proof.

Five fresh SDK nm captures equal the sealed captures. The three whole-target object, source, dependency and recipe hashes match; recorded flags are the accepted R4 recipe except the candidate include and source/output/dependency paths. Each actual depfile includes the accepted owner header, unchanged UFunctionStructs.hpp and the candidate helper. The three helper functions have exactly one strong provider apiece, eight private thunk functions exist, and no private helper remains unresolved. The undefined-symbol delta versus the two baseline TUs is exactly recursive_mutex constructor/destructor/lock/unlock and __cxa_rethrow. This is a three-object comparison, not composition-wide import admission or native ABI execution.

## Findings and required next-candidate checks

1. **Publication is outside the registry mutex's protection.** UFunction.cpp:29-37 performs ordinary Func access; ScriptHookThunks.cpp:111 and :132 publish and restore it under the registry mutex, while the engine's field load does not use that mutex. The same-field TSan control confirms the host-model race. The worker's passing thread control caches a thunk before the writer starts and installs six other functions, so it certifies registry/dispatch behavior in that narrower domain. Before promotion, enforce the supported native thread/publication contract and test wrong-thread or nonquiescent access refusal. An aligned eight-byte field alone does not establish that contract.

2. **Quiescent retirement can retain callback captures.** UFunctionStructs.cpp:90 skips an instance-filtered callback before the unregistration check at :91. Retirement marks callbacks while execution is active and permanently extracts the node (ScriptHookThunks.cpp:130-134); the last dispatch only resets bIsMidExecution (:24-31). The independent filtered-capture control proves that such captures remain owned after active_calls reaches zero. Permanent node retention is disclosed and supports late thunk safety; capture release, CustomData lifetime and module unload still need a separate enforced cleanup/quiescence rule. Test reentrant retirement with filtered, exceptional and nested callbacks, and release captures only after admitted invocations finish while retaining late-thunk forwarding identity.

3. **The snapshot policy wording overstates the implemented delay.** UFunctionStructs.cpp:80/:108 and the report say additions start on the next dispatch. Snapshots occur separately at :82 and :110. The independent control confirms a pre-added post callback can run in the current dispatch. The worker has acknowledged this correction and will preserve the sealed report, issuing an addendum with the next candidate. State the per-phase policy and retain a cross-phase test.

4. **Historical queries still install a thunk and consume permanent capacity.** UFunction.cpp:110-122 calls GetFunctionHookData for UnregisterHook and GetFunctionHookCallbackData. In the absence of a map entry, Install publishes a new thunk (:87-119), including for a missing callback ID. This is inherited behavior and is explicitly disclosed, but query/removal admission needs a deliberate policy before promotion. Test absent queries/removals for no Func store, no node/slot creation and no capacity consumption if the next candidate makes them observational.

The existing prototype report correctly restricts installation to GNatives-null borrowed-frame mode. Later nonnull GNatives forwards the immutable original without callbacks. Existing-map admission precedes the GNatives check, so adding callbacks after that change can succeed but those callbacks are suppressed by dispatch. This does not certify Locals/GetParams, synthetic frames, every native entry point, Lua userdata destruction or module unload. Instance-specific post callbacks historically lack a FireOnInstance filter; the prototype preserves that behavior rather than proving new per-instance post semantics.

Capacity eight and permanent no-reuse remain demonstrator choices. A release candidate must provide bounded carrier/callback allocation policy, capacity refusal tests and actual allocator/code/import/package budget evidence. The protected PC Lua mod and saved layout/settings copies stayed unchanged. No native menu/spawn success or deployment approval follows from this review.

## Receipts

- SOURCE-PINS.json: 431 exact frozen pins.
- CONTROLS.json: two policy observations pass; first publication diagnostic inconclusive.
- PUBLICATION-CONTROL.v2.json: expected same-field race reproduced, raw stderr retained.
- TARGET-SYMBOL-AUDIT.json: five fresh exact captures, five external additions, eight thunks, three unique strong helper providers.
- TARGET-SOURCE-AUDIT.json: three actual target recipe/dependency checks and 23 protected source/cache pairs.

The review is complete for the sealed prototype. The next independent review should assess the separately frozen R5 candidate against these findings; no additional baseline tests or full build were run here.
