# Local l19 profile revision 2

R2 incorporates independently accepted native evidence into the authenticated game profile and fixes four selected binding-width lookups. Focused profile, schema, actual initializer, actual binding/parser, and selected-target compilation checks pass. The original saved staged declaration still contains only 17 member entries and one virtual entry. It has not been expanded or promoted, and the actual case still refuses missing staged facts and six unknown target facts before an engine call. There is no menu or spawn success claim.

All work was local. No console, account, network, upload, game configuration, payload, or system write was performed. V1's 60 selected source files remain byte-identical. Original mod bytes and l18 files were not edited.

The immutable selection is [SOURCE-SELECTION.json](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l19-local-profile-r2/reports/SOURCE-SELECTION.json), SHA256 `70e26759ff736d5dd167e5cae2d59700ff0dc1eee36ebe39fa0a784ddfc5cdd2`, containing 67 source pins and 23 external evidence pins. [FINAL-RECEIPT.json](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l19-local-profile-r2/reports/FINAL-RECEIPT.json) SHA256 is `5e441fb3d1db9949af4a057c0610dbbc8d33eb2421834cdfb137ce5ae228c1d9`. No selected source edits follow this freeze. This descriptive report is outside the selected source manifest.

The profile JSON SHA256 is `29932681a3b508e972759b37042ae9d234b1352d1d11bac82c3154e920c2751f`; its canonical digest is `83c29b672fd918d9e93b880025f0687d12a6ddc158a5d6ad2a035545e34466d3`. The generator authenticates the exact retained image SHA256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`, full size 194241424, and base 0x400000. Shipping and Stats-Off remain independently measured facts. Runtime authentication, explicit settings, staged identity, declared offsets, required facts, and collect-all refusal behavior remain in the unchanged initializer and generic preflight consumer.

## Added native evidence

The profile now knows 66 of 69 required member pins and four of seven required virtual slots. Both cast-width facts are resolved, leaving zero unknown required static width facts. These were added individually from measured evidence, with no broad offset shift or transplanted Windows template.

| Frozen source | SHA256 | Newly accepted facts |
| --- | --- | --- |
| `l19-unresolved-evidence/FIELDC-FACTS.json` | `76aa9df32429b26a378649719ce2bafffae21e100ac2dfa7e20b0475eb1e693c` | FFieldClass.FName 0/8B; CastFlags 0x10/8B; SuperClass 0x20/8B |
| `l19-unresolved-evidence/UCLASS-FACTS.json` | `10495e5740008dc389729934e6e1a36f00108f771fff52d58fb9c3754d3372f1` | UClass.ClassCastFlags 0xD8/8B; ClassDefaultObject 0x110/8B |
| `l19-class-size-evidence/CLASS-SIZE-FACTS.json` | `733f83ba4a4bd740f1834d13ae9476f626be1e629da6030a8d37488104a16c12` | Exact totals: UObjectBase 0x28, UField 0x30, UStruct 0xB0, UFunction 0xE0, UClass 0x200, FProperty 0x70; FProperty virtual byte offsets CopyValuesInternal 0xF8, DestroyValueInternal 0x140, GetMinAlignment 0x170 |
| `l19-property-subtype-evidence/SUBTYPE-FACTS.json` | `5429bed5e3e6a05c5d44f2baec856fdc221e30abced63c7a11ec6460147be033` | Exact totals Struct 0x78, Bool 0x78, Enum 0x80, Array 0x80; Struct.Struct 0x70, Array.Inner 0x78, Bool.ByteOffset/ByteMask/FieldMask 0x71/0x72/0x73, Enum.UnderlyingProp/Enum 0x70/0x78 |
| `l19-fact-summary-r2/SUBTYPE-AUDIT.json` | `9b0b9474bb537b13a82a7c2ec3c9f5fb7f3254356d1a60b19426a04d48d1a1d5` | Independent byte-window, named constructor callback, RELA, and field-role acceptance |

UObjectBase's exact total has a separately recorded two-sided native proof: the named Object registration bounds the complete base above at 0x28, and the confirmed allocation-to-base this-flow bounds it below at 0x28. Other listed totals use named registration or exact constructor allocation evidence. Lower bounds and static guard spacing were not substituted for exact totals.

Supplemental evidence identifies UClass.CreateDefaultObject at virtual byte offset 0x400. It remains outside the mandatory census because the selected startup graph has no call site. Native FField/FFieldClass lower bounds, candidate totals, and partial Script information remain unpromoted.

The six unknown mandatory facts remain UClass.Interfaces, FField.UEP_TotalSize, FFieldClass.UEP_TotalSize, UEngine.LoadMap, UEngine.Tick, and AGameModeBase.InitGameState. Each produces explicit refusal. Every required known pin is compared with the staged declaration, so an absent declaration also refuses.

The generator checks all instruction windows against mapped PT_LOAD bytes of the authenticated ELF. Additional data and relocation evidence is checked against exact recorded bytes and decoded relocation tuples. The generator supports the two frozen evidence report field spellings without changing their meaning. All inputs and selected public getter sources are pinned in the source selection.

## Binding width and virtual snapshot correction

The new private generic `NativeLayoutActiveWidths.hpp` derives widths from the genuine selected public getter value types with `decltype` and `WidthDetail::ValueWidth`. It executes no getter or engine call. Only these framework keys use the adapter:

| Key | Actual legacy selected-target provider | Actual public getter width |
| --- | --- | --- |
| UFunction.Func | unavailable | GetFunc(): 8B |
| UStruct.MinAlignment | 4B | GetMinAlignment(): 2B |
| UClass.ClassCastFlags | unavailable | GetClassCastFlags(): 8B |
| FFieldClass.CastFlags | unavailable | GetCastFlags(): 8B |

All other keys continue through the unchanged legacy provider. A missing or inaccessible selected public getter yields null and refuses. The helper has no game hash, game identifier, offset, size, literal width fallback, or additional runtime policy.

The actual target control supersedes an earlier source-type assumption that the old provider returned Func=16 and casts=8. The frozen supplement report is preserved; its native field evidence remains valid. `ACTIVE-WIDTHS-TARGET-ATTEMPT1.json` retains the failed assumptions. The corrected selected-target control compiles and its emitted facts are independently read as `(0,4,8,2,8,8)`: unavailable old Func/casts, old MinAlignment=4, then public Func/MinAlignment/ClassCastFlags/CastFlags=8/2/8/8.

The native UFunction.Func field is 0xD8/8B and the exact native class total is 0xE0. R2 stages that genuine boundary without inflating the total. The immutable V1 binding returns `Native accessor width unavailable: UFunction::Func` for the current target-provider model, establishing the primary RED. A separately retained conservative-16 host model also refuses 0xD8+16 beyond 0xE0; that is supplemental evidence rather than the actual target provider's result. R2's public getter width 8 stages 0xD8, while 0xD9 refuses the declared class boundary.

Each staged virtual snapshot entry now records `sizeof(void*)` as its width after the existing pointer-size-8 identity precondition. This corrects metadata that previously defaulted to zero. It does not alter the map, virtual offset, publication, or engine-call behavior. The actual Stage/GetStagedNativeLayoutFacts host check records GetMinAlignment byte offset 0x170 with width 8. Wrong and missing virtual entries still refuse the initializer preflight. Live member maps, total-size maps, presence bitmaps, and the tested virtual map remain empty during these staging controls.

## Focused RED and GREEN results

| Receipt | Result and scope |
| --- | --- |
| `ACTIVE-WIDTHS-RED.json` | Expected failure using immutable V1 actual binding: unavailable UFunction.Func width |
| `VIRTUAL-SNAPSHOT-RED.json` | Expected failure: staged virtual snapshot observed width 0, expected 8 |
| `PROFILE-ADDITIONS-GREEN.json` | All five new required field pins and both cast-width closures present |
| `ACTIVE-WIDTHS-GREEN.json` | 12 actual binding/parser host cases pass under fatal ASan/UBSan: four genuine-width boundaries, unknown key, known key with unavailable width, virtual snapshot, missing getter and fallback policy; all affected live maps and bitmaps stay empty |
| `ACTIVE-WIDTHS-TARGET.json` | Actual selected-target signatures and legacy-provider results pass; four missing-getter controls remain null |
| `INITIALIZER-PROFILE-GREEN.json` | 22 cases pass using extracted actual initializer code, including wrong cast offset, missing ClassDefaultObject, wrong/missing property virtual slot and specific collect-all refusal rows; no engine call follows refusal |
| `PROFILE-CHECKS-GREEN.json` | Generic profile controls pass in plain and fatal ASan/UBSan builds |
| `PROFILE-SCHEMA-GREEN.json` | 18 strict schema refusal cases pass |
| `NATIVE-FOCUS.json` | Actual selected initializer and binding translation units compile for the target with the new selected header |

The host binding controls substitute storage for the wide engine dependency and use nine exact selected schema rows plus the tested virtual row. Genuine target signatures are proved separately. The host FFieldClass total 0x40 is an owned boundary fixture, explicitly not an exact native sizeof claim. Negative cases assert their specific refusal text so unrelated parser failures do not pass them.

The nine unchanged retained XOM/cache/copyin/FName/lifecycle suites were not rerun. Their immutable V1 `HOST-ALL.json` receipt SHA256 `541fc81cf609c76f8af7e16c5b1df0e42803bd0a61b30954e3474b3336855560` is tied to the unchanged selected unit hashes. New binding/profile behavior has the focused checks above. XOM disk/live checks and fault refusal, cache budget, bank cleanup, padding, diagnostics scheduling, and original mod bytes are preserved.

## Exact R2 source difference

Added selected sources:

- `units/NativeLayoutActiveWidths.hpp`
- `tests/native_active_widths.py`
- `tests/active_width_target.py`
- `tests/target-active-width/active_width_controls.cpp`
- `tests/active-width-host/controls.cpp`
- `tests/active-width-host/NativeLayoutEngineIncludes.hpp`
- `tests/active-width-host/NativeLayoutSchema.inc`

Changed copied sources relative to immutable V1:

- `units/NativeLayoutBinding.cpp`: private width adapter and read-only virtual snapshot width.
- `units/RuntimeBuildProfile.hpp`: regenerated typed profile pins and provenance digest.
- `profiles/generate_profile.py`: accepted evidence merge and byte/relocation authentication.
- `profiles/townfall-build-facts.json`: regenerated accepted facts and evidence pins.
- `tests/focused_compile.py`: pins the new helper for target compilation.
- `tests/initializer_profile_integration.py` and generated `tests/initializer_profile_actual.cpp`: four new specific refusal cases.

The report directory retains exact patches for the binding, generator, generated profile/header, and initializer integration test. All other selected V1 sources are byte-identical.

The new provider is private inline `GetBindingAccessorWidth`; the staged-facts provider already introduced in V1 remains unchanged. No existing consumer struct layout changed. The new helper uses existing public engine declarations and the existing legacy width utility; it introduces no OS API. Parent full-build/import validation remains authoritative for the final linked import set. This local report makes no R2 full-pipeline claim; the parent owns that driver and independent final review.
