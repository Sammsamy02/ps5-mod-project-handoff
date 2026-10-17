The final PRX selects a compiled declaration pair only when its Program compile command defines `RC_NATIVE_LAYOUT_DECLARATIONS_HEADER` to an explicitly pinned generated header. Adding a header to an include directory does not select it. With no macro, the existing activation source or filesystem path loads the layout pair.

The generated header must define this constant after the generic helper has declared its type:

```cpp
#pragma once
namespace RC::Platform::NativeLayouts {
inline constexpr CompiledNativeLayoutDeclarations kCompiledNativeLayoutDeclarations{
    /* members: immutable complete MemberVariableLayout text */,
    /* vtables: immutable complete VTableLayout text */,
    /* selection_source: printable provenance identifier */,
    /* profile_digest: exact selected rc::runtimeprofile::kDigest */
};
}
```

All four fields are `std::string_view` and must reference static storage. The pair is single and immutable. Empty or malformed texts are still selected; actual staging refuses them, without falling back to external files. A mismatched profile digest fails compilation. Program logs the selected source and digest before staging.

The root generator must supply independently authenticated declarations, including matching NativeLayout identity in both texts, all 69 required member entries with proved class totals, and all seven required vtable entries. It must derive the digest from the same selected RuntimeBuildProfile. It must pin both text hashes, generated-header bytes, profile JSON/header, all evidence inputs, and the source identifier in its receipt. No generator may promote an extent bound or candidate total into an exact native size. The fixtures here contain only the current R3 authenticated 60 members and five slots with proved containing totals; they intentionally fail the actual required-fact preflight.

The final build must select the private `units/UE4SSProgram.cpp`, make `units/NativeLayoutDeclarations.hpp` a mandatory input, and make the generated header plus RuntimeBuildProfile mandatory Program dependencies. Its Program command must contain the exact macro selection. Two normal full builds and the existing import/provider/source gates still apply. The focused target receipt here proves compilation with a fixture header, rather than final artifact or runtime readiness.

Selection does not resolve axes or publish maps. `StageNativeLayoutTexts` parses exactly this pair and preserves its one-loaded-lifetime and existing-map refusals. The existing actual initializer authenticates fresh image/base/lifetime/deadline, resolves independent axes, checks explicit settings and all required metadata/values/widths, and logs every missing fact before publication or engine calls. Root must run the final initializer tests against its final profile and declaration pair.

No Lua, mod, pad, settings, or saved layout file changes are required for this loader. This task does not generate the final game header, edit any frozen source, or contact the console.
