# L19 R3 local profile integration

R3 resolves the two independently measured runtime axes and checks every required profile/configuration/layout fact before seeded consumption. Shipping is `build_configuration=1`; independently classified STATS off is `stats=1`. No inference from Shipping supplies STATS. The current saved declaration still refuses before an engine call: it contains only 17 member entries and one virtual entry. No console configuration, runtime publication, upload, menu, or spawn success is claimed.

## Frozen identity

- SOURCE-SELECTION.json SHA256: `cc0043df2f68d8ca1a61b49a5cef6d34e2a598fd49ded91e47cf3640d5f22337` (75 selected source files, 36 external evidence files).
- Profile JSON SHA256: `66dc76fd41c96dc2faf9bc02bc098aaace20d05a0793fd633a81d977588c29af`.
- Canonical profile digest: `efa1c15e6da091298d9764b9d0fa18b1196d8825776b383799806ef0397329fd`.
- FINAL-RECEIPT.json SHA256: `2f4f5acf095a4c32229e95d2140449da0a3ed417ce4110b248660e98d476bc01`.
- Authenticated decrypted image SHA256: `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`, full file size 194241424 bytes, base `0x400000`.

The generic consumer compares the fresh authenticated image SHA/full size/base, declared native identity, explicit settings, independently measured axes, FName ABI, required member offsets/width minima, and virtual offsets/widths. A missing, unknown, conflicting, or incomplete required fact refuses startup. It gathers all independent failures before throwing. The seeded preflight is conditional on the actual `__PROSPERO__` / `bSeedsArmed` path; it precedes `consume_required_slot_seeds` and map publication. Tests increment a counter only after the extracted preflight returns: all 29 current cases throw first and leave that counter at zero. This proves the conditional preflight boundary in the host harness, not live console execution. Its completion shim counts callback invocations, not actual FD/bank retirement; unchanged lifecycle tests retain that separate responsibility.

## R3 source changes relative to frozen R2

`units/NativeLayoutActiveWidths.hpp` adds six narrowly keyed widths derived from genuine public getter types. It retains the existing four keys and delegates every other key to the unchanged legacy provider. A missing/inaccessible getter remains unavailable. These are unevaluated type queries: no getter, engine call, version branch, game offset, or literal width fallback executes.

| Required key | Genuine public getter | Target binding footprint bytes |
|---|---|---:|
| UFunction.Func | GetFunc() | 8 |
| UStruct.MinAlignment | GetMinAlignment() | 2 |
| UClass.ClassCastFlags | GetClassCastFlags() | 8 |
| FFieldClass.CastFlags | GetCastFlags() | 8 |
| UStruct.SuperStruct | GetSuperStruct() | 8 |
| FField.Owner | GetOwnerVariant() | 16 |
| FField.Next | GetNextFieldAsProperty() | 8 |
| FField.NamePrivate | GetFName() | 8 |
| FProperty.PropertyFlags | GetPropertyFlags() | 8 |
| UFunction.FunctionFlags | GetFunctionFlags() | 4 |

Native FField.Owner physical storage remains the independently measured 8-byte tagged pointer. The selected public wrapper copies a 16-byte FFieldVariant; 16 is the conservative compiled footprint checked against a declared class boundary. It does not establish native variant ABI equivalence or an exact FField total. The synthetic host Owner boundary fixture uses a declared total `0x30`; that test does not promote the unproved target total. The modern owner operations use the tagged pointer branch; legacy bool representation and arbitrary native variant operations remain outside this metadata correction.

The initial prototype using FField Unsafe methods failed target compilation because those methods are private. That attempt is preserved. The accepted public-method target proof yields `(8,16,8,8,8,4)` for the six additional keys. The actual old target provider had unavailable Func/cast widths; it must not be described as an actual target Func width of 16. The previous conservative host model is supplementary history only. Exact UFunction total `0xE0` plus native Func `0xD8`/8 fits without inflating the measured total.

`profiles/generate_profile.py`, `profiles/townfall-build-facts.json`, and generated `units/RuntimeBuildProfile.hpp` incorporate only accepted native facts: UClass.Interfaces `0x1D8`/16 and UEngine.Tick virtual byte offset `0x310`/8. The generator verifies saved instruction/data bytes and PT_LOAD mappings, relocation tuples, exact evidence hashes, and accepted audit identity. Tick's original pending-review receipt stays unchanged; the separately pinned accepted audit authorizes its single fact promotion. No template offset or blanket shift is used.

`tests/initializer_profile_integration.py` and its generated extraction add Interface/Tick missing/conflict cases, Owner footprint16 PASS versus width7 REFUSE, and the actual saved 17/1 declaration shape. `tests/active_width_target.py` and generated controls cover all ten public getters and ten absent-getter cases. New `tests/required_width_census.py` and `tests/required_binding_widths.py` check all required widths; generated target/host fixtures are selected and hashed. `tests/native_active_widths.py` delegates to the comprehensive actual-binding suite and retains the compatible receipt/fixture paths. Exact changed-file patches are saved as `reports/DIFF-*.patch`; every selected/new file hash is in SOURCE-SELECTION.json.

`NativeLayoutBinding.cpp`, `NativeLayoutBinding.hpp`, `UnrealInitializer.cpp`, and all other R2 unit files are byte-identical to R2. No existing struct layout changed. The two freshly compiled TUs are `unreal-unrealinitializer` and `native-layoutbinding`; the binding TU selects the flat own-directory `NativeLayoutActiveWidths.hpp`. The initializer and binding retain the flat own-directory `NativeLayoutBinding.hpp`. The generic provider adds no OS API calls. Linked imports and complete artifact builds belong to the parent's full pipeline and are pending this focused receipt.

## Verification

Meaningful RED results are preserved: the immutable R2 target helper failed the required census on the six provider gaps; the immutable R2 actual binding refused a valid SuperStruct declaration with `Native accessor width unavailable: UStruct::SuperStruct`. The private-method prototype failed before the accepted public-method implementation.

Final GREEN focused results:

- 29 extracted actual initializer scenarios passed under fatal ASan/UBSan. Intended failures assert exact named REFUSE checks; all required rows are evaluated. Wrong SHA/base/full size, missing authentication/declaration, contradictory settings/identity/version, missing/conflicting known members/slots, owner denial, deadline, and simultaneous faults refuse before the counter representing later engine consumption.
- 25 actual binding/parser host scenarios passed under fatal ASan/UBSan: ten positive/boundary pairs, unknown key, known-schema unavailable width, virtual metadata, all 69 required staged declarations with 54 non-total widths, and missing-getter policy. Empty live member maps/total maps/presence bitmaps and the tested virtual map prove staging did not publish them.
- All 54 required non-total widths are present in the actual selected-target object and cover the independently measured native minima. The emitted `AllRequiredWidthFacts` array is inspected directly; zero missing or narrower widths remain.
- 18 schema negative scenarios passed; plain and sanitized profile checks passed.
- Ten genuine public target getter controls and ten missing-getter controls passed; both actual target TUs compiled.

The virtual fact snapshot retains R2's `sizeof(void*)` metadata after the pointer-size-8 identity check. Correct slots stage with width8; wrong/missing slots still refuse preflight. There is no virtual map modification or engine call in the snapshot.

All 67 R2 selected sources and all 60 V1 selected sources remain byte-identical to their frozen manifests. Nine unchanged retained suites were hash-tied instead of rerun; retained V1 HOST-ALL.json SHA256 is `541fc81cf609c76f8af7e16c5b1df0e42803bd0a61b30954e3474b3336855560`. Existing XOM disk/live checks, fault refusal, budget, bank cleanup, padding/mod bytes, helper topology, and diagnostic scheduling remain unchanged. R2's parent full build did not compile because setup stopped early; this report makes no R2 full-build claim.

## Remaining refusal scope

67 of 69 required member pins and five of seven required virtual slots have validated target values. Required static width gaps are zero. The four remaining native facts remain explicitly unknown:

- FField::UEP_TotalSize.
- FFieldClass::UEP_TotalSize.
- UEngine::LoadMap virtual byte offset.
- AGameModeBase::InitGameState virtual byte offset.

Neither FField candidate `0x30` nor FFieldClass candidate `0x40` has the required exact-size semantic proof. They are not promoted from member extent, vtable similarity, or object/guard spacing.

The saved staged 17/1 pair is not expanded or promoted. It lacks 52 required member entries and six virtual entries, including 50 now-known member values and four now-known virtual values. The actual saved-declaration test resolves Shipping1 and STATS-off1 independently, then refuses those missing facts and the four unknown target facts before consumption/publication. A future accepted expanded declaration must still pass every required check. No console configuration has been generated or written.
