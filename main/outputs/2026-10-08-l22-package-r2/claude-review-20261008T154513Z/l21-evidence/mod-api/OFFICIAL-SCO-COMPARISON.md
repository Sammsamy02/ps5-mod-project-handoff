# Official 5.6.1 SCO declaration followup

The official `FStaticConstructObjectParameters` declaration corroborates the **13 field names, types/order and two conditional remote fields** in the audited model. It also exposes an alignment distinction that the source's 48-byte placeholder does not express. This is additional static evidence, not a successful native call or build-readiness decision.

Sources are the exact EpicGames/UnrealEngine commit `6978b63c8951e57d97048d8424a0bebd637dde1d` supplied by the parent as the 5.6.1 release pin. Local hashes were rechecked:

| File | SHA256 | Evidence |
|---|---|---|
| official-headers/UObjectGlobals.h | dfee8ed1e1309b6ea1df6c7573f8d75d63a48c03a9b0e46bfe789034697084c0 | declaration1728–1784; API pointer/reference signature571 |
| official-headers/Function.h | 8dfd24e6526bf82a57ad0620a37218fe7fe27b6f901f5ce47362305a0aad3218 | inline policy27–35; storage198–203; callable/storage509–515; TFunction base707 |

UObjectGlobals.h was already fetched by the parent. This followup fetched only its named Templates/Function.h dependency, read-only through existing gh authentication; `FUNCTION-FETCH-RECEIPT.json` records commit, blob hash and file hash. No credentials, runtime files or configuration were changed. `REPORT.prior-official-header.md` and the prior field tables preserve the previous state.

## Declaration comparison

The official field sequence is Class, Outer, Name, SetFlags, InternalSetFlags, the two bools, Template, InstanceGraph, ExternalPackage, PropertyInitCallback, SerialNumber, optional remote fields, and private SubobjectOverrides. The selected `Above55` structure reproduces the ordinary field sequence; it substitutes `uint8[0x30]` for official `TFunction<void()>` and `void*` for private `FObjectInitializer::FOverrides*`. Pointer type/access-control differences do not change the ordinary pointer size; callback alignment requires separate consideration.

The official `StaticConstructObject_Internal` declaration takes `const FStaticConstructObjectParameters&` and returns `UObject*` (line571), consistent with the saved-image RDI pointer / RAX result observations. Header names now provide direct semantic corroboration for the native accessed field offsets rather than depending only on the port's field labels. Exact official-line references were added to `sco-parameter-layout.{json,tsv}` for all13 rows.

`UE_WITH_REMOTE_OBJECT_HANDLE` gates both RemoteId and RemoteSubObjectOverrides at lines1769–1774. They appear between SerialNumber and SubobjectOverrides. The selected header defaults that macro to0 and uses the same condition. Native reads place SerialNumber at0x70 and the subobject override pointer at0x78; this supports the remote-disabled field sequence when correlated with this official declaration. It is not a direct dump of the game's preprocessor definition. No offsets or sizes are claimed for the remote-enabled configuration.

## Exact controlling declarations

Official `UObjectGlobals.h:1764–1777` contains these field/conditional lines (intervening comments omitted; declarations unchanged):

```cpp
TFunction<void()> PropertyInitCallback;
int32 SerialNumber = 0;
#if UE_WITH_REMOTE_OBJECT_HANDLE
FRemoteObjectId RemoteId;
UE::RemoteObject::Serialization::FRemoteObjectConstructionOverrides* RemoteSubObjectOverrides = nullptr;
#endif // UE_WITH_REMOTE_OBJECT_HANDLE
private:
FObjectInitializer::FOverrides* SubobjectOverrides = nullptr;
```

Official `Function.h:27–36` gives the exact inline-storage conditional:

```cpp
#if defined(_WIN32) && !defined(_WIN64) && (!defined(ALLOW_TFUNCTION_INLINE_ALLOCATORS_ON_WIN32) || !ALLOW_TFUNCTION_INLINE_ALLOCATORS_ON_WIN32)
    // Don't use inline storage on Win32, because that will affect the alignment of TFunction, and we can't pass extra-aligned types by value on Win32.
    #define TFUNCTION_USES_INLINE_STORAGE 0
#elif !defined(NUM_TFUNCTION_INLINE_BYTES) || NUM_TFUNCTION_INLINE_BYTES == 0
    #define TFUNCTION_USES_INLINE_STORAGE 0
#else
    #define TFUNCTION_USES_INLINE_STORAGE 1
    #define TFUNCTION_INLINE_SIZE         NUM_TFUNCTION_INLINE_BYTES
    #define TFUNCTION_INLINE_ALIGNMENT    16
#endif
```

The aligned member is exactly `alignas(TFUNCTION_INLINE_ALIGNMENT) uint8 InlineAllocation[TFUNCTION_INLINE_SIZE];` at line200. This header does not supply a default nonzero `NUM_TFUNCTION_INLINE_BYTES`;24 is inferred from the native geometry, not asserted as an official universal default. The visualization default is0 only when `UE_BUILD_SHIPPING || UE_BUILD_TEST`, and only if `UE_ENABLE_TFUNCTIONREF_VISUALIZATION` was not already defined (18–24). The exact target overrides have not been retrieved. The selected port independently supplies `#ifndef UE_WITH_REMOTE_OBJECT_HANDLE` / `#define UE_WITH_REMOTE_OBJECT_HANDLE 0`; no universal official default for that macro is inferred from UObjectGlobals.h.

## TFunction and alignment

Function.h uses inline storage unless the platform disables it or `NUM_TFUNCTION_INLINE_BYTES` is undefined/zero (27–35). When enabled, it explicitly sets `TFUNCTION_INLINE_ALIGNMENT` to16 and declares InlineAllocation with that alignment (200). `FFunctionStorage` contains the inline array followed by HeapAllocation (203). The base stores Callable before Storage (509–511); an optional visualization member follows Storage (513–515). Shipping/test defaults visualization to0 (18–24), but the macro is overridable. TFunction adds no data members to this base (707 onward).

The native observations are callback Callable at parameter+0x40, inline object at+0x50, heap pointer at+0x68, SerialNumber at+0x70 and SubobjectOverrides at+0x78. With 8-byte pointers, these agree with a **24-byte inline array**, a **48-byte TFunction**, no visualization member, and remote handles disabled. For that official conditional model, TFunction alignment is16; the enclosing parameter type therefore has alignment16 and total size0x80. These are source/layout deductions corroborated by actual field accesses, not an independent target `sizeof`/`alignof` measurement or a proof of exact game build macros.

The selected port's zeroed `uint8 PropertyInitCallback[0x30]` has no `alignas(16)` and leaves the enclosing source type with only its other members' alignment8. It preserves the observed field offsets and size in the current source model, but **does not preserve the official inline callback's declared alignment**. Actual callsite storage might still be aligned16 due to surrounding stack placement; it has not been measured in this bounded followup. Therefore the declaration difference establishes a fact needing caller/ABI evidence, not a claim of an observed fault or a proposed patch.

The zero callback model also has narrower semantics than constructing arbitrary TFunction bindings. The native parameter constructor clears Callable at+0x40 and HeapAllocation at+0x68; ObjectInitializer checks Callable before touching/cloning the bound object. Selected zero initialization agrees with that empty state. Nonempty callable construction, cloning, destruction and lifetime are not exercised by the mod and are not certified by the placeholder.

## What this narrows

Closed as a source-evidence gap: official field identities/order, real callback type, conditional remote-field placement, and the official function's reference/pointer signature. The native48-byte callback geometry now has an official implementation explanation. Calling it wholly opaque is no longer accurate.

Still missing: actual target macro settings/custom engine modifications; independent whole native size/alignment confirmation; selected caller parameter address alignment; runtime identity/admission; first successful SCO execution; and the resulting widgets' engine ownership, GC and lifetime. The initializer still explicitly binds slot4 with identity/ABI unverified. None of the33 mod/helper reflected APIs gains console execution evidence from this source followup. The overall build gate remains failed elsewhere, as the parent reported.
