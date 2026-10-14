# Owned Lua object-property bindings

The next bounded runtime task adds the property chain required by unchanged UE4SS Lua sources. It promotes the existing Int32 method and Boolean property adapter as a reusable C leaf, then adds typed object-reference reads/writes, `IsValid` and owned identity equality. This is an offline implementation task already authorized by the user's overnight instruction; no console or live-account operation is part of it.

Pinned upstream `e3ba1016562d6c0868c410d0a71e88bfcdbf691b` constructs null UObject userdata for object properties, permits UObject/nil assignment, and supports null property chaining. Its UObject validity also depends on engine membership and unreachable state. Our provider contract represents that dependency explicitly; it does not supply an engine provider.

## Contract

- Product files: `native-host/lua/owned_binding.c/.h`, retaining the existing `lba_` C names for regression compatibility. The existing Int32 and masked Boolean APIs remain available.
- `lba_ref_descriptor`: `uint32_t version, permissions; uint64_t property_id, generation; uint16_t value_type, encoding; uint32_t reserved`. Version 1, type 3, owned-identity encoding 3, read/write bits 1/2 only, nonzero property ID/generation, zero reserved. No addresses or engine offsets.
- `lba_ref_provider`: owned context, `resolve(context, receiver, name, bytes, descriptor*)`, and `access(context, receiver, descriptor, operation, value, result*)`. Resolution returns 1 found, 0 absent, other failure. Access returns 0 success, other failure. READ receives canonical null and returns a current identity or canonical null. WRITE accepts an identity or canonical null; its result is ignored. Each single access transaction must revalidate current receiver/property/target identity, permissions and generation; failure must leave owned storage unchanged. Callbacks remain synchronous, bounded, non-reentrant C and may not call Lua or access engine/process memory.
- Canonical null is `{provider.owner_id, 0, 0}`. A nonnull identity has matching owner and nonzero object/generation. Null results become this instance's locked userdata, never Lua nil. Existing unresolved top-level lookup still returns nil; this leaf is not a complete UE4SS global API.
- `lba_install_with_refs(L, provider, optional_boolean, optional_reference, name, length)` preserves existing wrapper APIs. Installation remains protected and stack preserving, metadata copied, external owned contexts alive until Lua close.
- Property read precedence: reserved `IsValid`, null chain, reference, Boolean, Int32 method. Reference writes accept only this exact instance's userdata or nil; Boolean writes require exact Lua Boolean. Arbitrary userdata fields are never added.
- `o:IsValid()` accepts exactly one owned receiver, checks current provider status each time, returns true for exact 1 and false for 0 or canonical null; other status errors. It is available before the stale receiver access gate. Null property reads return another canonical-null userdata. Null writes and stale operations fail.
- Equality compares same-instance owner/object/generation without provider dereference; same-instance nulls are equal and foreign instances are unequal. This is owned identity equality, not an engine-address comparison.

## Implementation and focused evidence

1. Add independent behavioral tests first: valid chain, null chain, validity transitions, strict identity/type/permission/generation gates, read/write transaction failures, equality and legacy methods/Boolean masks. Record a behavioral failing baseline using the prior adapter plus a refusing reference stub.
2. Implement the shared C leaf without game or mod names. Keep sources of original mod archives unchanged.
3. Run real pinned Lua 5.4.7 host tests and legacy regression, then cross-compile/link the same product source with the retained PS5 SDK/Lua library and inspect ELF/imports. Native code remains unexecuted.
4. Review the binding and test/native evidence independently, fix material findings, record the exact scope and commit only this task's files. Do not link a fabricated engine provider into the Nexus app or claim Item Spawner works.

The remaining game runtime needs engine object/name discovery, GC-aware identities, reflection descriptors, argument conversion, actual game-thread hooks, device input and game function/UMG calls. This step makes one shared binding component usable when that provider exists; it does not translate Windows DLLs or establish broad game support.
