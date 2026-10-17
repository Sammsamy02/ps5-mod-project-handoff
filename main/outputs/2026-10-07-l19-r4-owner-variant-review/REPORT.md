# Independent FFieldVariant review

Final candidate review: PASS for the local source correction. Final header SHA256 `6891a6f15fa5124f5f0f57aafe081d501a5baec52e9001d2e3403d5a3f53ad3c`; final cpp SHA256 `f56f9d407975e4335bf0157919f62cfdf5c892d1d3ca4a028f40d9a5d51b2b47`. No candidate, frozen source, or parent receipt was edited by this reviewer. Original and final snapshots, test artifacts, and receipts are retained in this separate review directory.

The original requested candidate fixed five important modern paths: the UObject constructor tags a nonnull pointer at5.3+, Set writes only pointer storage at5.3+, native Owner reads use StaticSize8 rather than copying the adjacent Next pointer, and object traversal masks the tag. The local wrapper remains16 bytes for legacy behavior. The source contains genuine Version predicates and actual StaticSize; the host doubles replace their engine storage values for branch tests, not the production predicates.

## Findings in the original requested snapshot, now resolved

1. **P1: null owner traversal dereferenced null.** FFieldVariant::GetOwnerVariant called Container.Field->GetOwnerVariant for a null field variant; GetOutermostOwner looped without IsValid. Independent null direct, null outermost, and field-chain-to-null controls crashed plain or failed fatal UBSan. The final source first returns an invalid null variant for invalid input and bounds the outermost loop with IsValid. All three controls now pass.
2. **P2: pre4.25 local Set left the wrong owner kind.** Both local 4.24 Set direction controls failed: the prior guard only updated IsObject at4.25–5.2. The final source updates the local legacy bool on every version below5.3, retaining pointer-only writes at5.3+. Both 4.24 controls now pass.

The original findings are preserved in INDEPENDENT-CASES.json: five failing cases in both plain/sanitized executions, ten failures total. The follow-up source is isolated under final/. One independent final harness attempt encountered duplicate extracted methods after the parent expanded its own extractor; that harness error is preserved as final/INDEPENDENT-CASES-ATTEMPT1.json. It was corrected in the owned review script and is not a candidate defect.

## Independent verification

- Rebuilt and reran all25 fresh parent cases plain and fatal ASan/UBSan:50 executions, zero failures (52 commands including compiles).
- Rebuilt13 additional controls plain and fatal ASan/UBSan:26 executions, zero failures. These cover actual SetOwnerVariant object/field/null wrappers preserving Next,5.3 transition,5.2 legacy wrapper,4.24 local Set,4.24 owner-read UField fallback, null direct/outermost/chained traversal, nested valid owner traversal, and pointer hash.
- Independently compiled actual PS5 `unreal-ffield` and `unreal-src-property-ffieldpathproperty` with the final candidate header. Both compile and both dependency files select that header. Undefined symbol sets have zero additions/removals relative to the retained R3 objects. This is two-TU compilation/import evidence, not a full linked artifact or console execution claim.

The extracted class and method bodies are unchanged from the relevant snapshot. Engine object methods, Version storage, and GetOwner offset storage are host doubles. The modern native buffer fixtures expose exactly8 bytes or8-byte Owner plus a Next sentinel; the legacy fixtures provide the genuine16-byte wrapper. The test4.24 UField owner-read fallback executes the actual version branch with a mocked AsUFieldUnsafe pointer and UObject outer lookup. It does not prove every pre4.25 offset/layout or the legacy SetOwnerVariant storage bridge.

## Call-site and copy audit

The generated GetOwner overloads return references into mapped member storage. Within the selected coherent runtime, the direct callers are GetFFieldOwnerUnsafe, both SetOwnerVariant wrappers, and the typed GetOwner<T> template. GetFFieldOwnerUnsafe is now the single bounded native-to-local copy; the wrappers route to the corrected Set and do not modify the native adjacent pointer at5.3+. Typed GetOwner<T> uses IsA/ToUObjectUnsafe and does not copy the whole native owner. No remaining byvalue16 copy of raw native Owner was found in the selected runtime sources.

GetPathName uses IsValid and converts object owners with ToUObject; GetTypedOwner also checks IsValid. The corrected GetOutermostOwner now does so as well. IsA's UObject path and typed Get already mask via ToUObjectUnsafe. GetRawPointer and ToFieldUnsafe remain explicitly raw/unsafe accessors; no selected runtime client calling them was found. The selected LuaCustomProperty consumer uses GetOwnerVariant followed by ToUObject/ToField; CustomProperty uses SetOwnerVariant at4.25+ and the explicit UObject outer bridge below4.25; ObjectToString hashes the local variant; SDKGenerator uses GetOutermostOwner.

The369-object selected R3 defined-symbol census found the changed inline UObject FFieldVariant constructor emitted only in `unreal-ffield.o`, which the candidate replaces. The source search found no other selected explicit construction site requiring a changed constructor body. The symbol census does not discover every possible inlined instruction, so a future complete artifact build should still verify its selected sources and imports. The FField class/variant layout and public signatures do not change.

## Scope and ABI limits

This correction preserves native Owner storage and a legacy local wrapper. It is not a blanket claim that a16-byte FFieldVariant passed by value to an engine function has the native8-byte ABI. The declared native ConstructFn takes a const reference; all external variant-taking APIs still need their own signature/call proof when used. Generic TArray's version-aware FFieldVariant sizing exists, but no selected TArray<FFieldVariant> runtime instance was found or exercised. No new literal game offset or class total is introduced.

FField/FFieldClass exact totals and LoadMap/InitGameState slots remain independent unknowns. Existing R3 profile/declarations do not become complete from this patch. No original mod, profile, padding, game memory, console, network, upload, or deployment action occurred. Full-build and runtime acceptance remain with the parent.
